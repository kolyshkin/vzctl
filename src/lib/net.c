/*
 *  Copyright (C) 2000-2006 SWsoft. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <asm/timex.h>
#include <linux/vzcalluser.h>
#include <linux/vzctl_venet.h>

#include "types.h"
#include "net.h"
#include "vzerror.h"
#include "logger.h"
#include "list.h"
#include "dist.h"
#include "exec.h"
#include "env.h"
#include "script.h"
#include "util.h"


int find_ip(list_head_t *ip_h, char *ipaddr)
{
	ip_param *ip;

	if (list_empty(ip_h))
		return 0;
	list_for_each(ip, ip_h, list) {
		if (!strcmp(ip->val, ipaddr))
			return 1;
	}
	return 0;
}

static inline int _ip_ctl(vps_handler *h, envid_t veid, int op,
	unsigned int ip)
{
	struct sockaddr_in addr;
	struct vzctl_ve_ip_map ip_map;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip;
	addr.sin_port = 0;

	ip_map.veid = veid;
	ip_map.op = op;
	ip_map.addr = (struct sockaddr*) &addr;
	ip_map.addrlen = sizeof(addr);

	return ioctl(h->vzfd, VENETCTL_VE_IP_MAP, &ip_map);
}

int ip_ctl(vps_handler *h, envid_t veid, int op, char *ip)
{
	int ret;
	unsigned int ipaddr;

	if ((ret = get_ipaddr(ip, &ipaddr)))
		return 0;
	ret = _ip_ctl(h, veid, op, ipaddr);
	if (ret) {
		switch (errno) {
			case EADDRINUSE	:
				ret = VZ_IP_INUSE;
				break;
			case ESRCH	:
	                        ret = VZ_VE_NOT_RUNNING;
				break;
			case EADDRNOTAVAIL:
				if (op == VE_IP_DEL)
					return 0;
				ret = VZ_IP_NA;
				break;
			default:
				ret = VZ_CANT_ADDIP;
				break;
		}
		logger(0, errno, "Unable to %s IP %s",
			op == VE_IP_ADD ? "add" : "del", ip);
	}
	return ret;
}

static struct vps_state{
	char *name;
	int id;
} vps_states[] = {
	{"starting", STATE_STARTING},
	{"running", STATE_RUNNING}
};

const char *state2str(int state)
{
	int i;

	for (i = 0; i < sizeof(vps_states) / sizeof(*vps_states); i++)
		if (vps_states[i].id == state)
			return vps_states[i].name;

	return NULL;		
}

/*
 * Setup (add/delete) ip addreses inside VPS
 */
int vps_ip_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, int op, net_param *net, int state)
{
	char *envp[5];
	char *str;
	const char *script = NULL;
	int ret, i;
	char vps_state[32];
	const char *str_state;
	const char *delall = "IPDELALL=yes";

	if (list_empty(&net->ip) && !net->delall && state != STATE_STARTING) 
		return 0;
	if (actions == NULL)
		return 0;
	switch (op) {
		case ADD:
			script = actions->add_ip;
			if (script == NULL) {
				logger(0, 0, "Warning: add_ip action script"
					" is not specified");
				return 0;
			}
			break;
		case DEL:
			script = actions->del_ip;
			if (script == NULL) {
				logger(0, 0, "Warning: del_ip action script"
					" is not specified");
				return 0;
			}
			break;
	}
	i = 0;
	str_state = state2str(state);
	snprintf(vps_state, sizeof(vps_state), "VE_STATE=%s", str_state);
	envp[i++] = vps_state;
	str = list2str("IP_ADDR", &net->ip);
	if (str != NULL)
		envp[i++] = str;
	if (net->delall)
		envp[i++] = (char *) delall;
	envp[i++] = ENV_PATH;
	envp[i] = NULL;

	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);
	if (str != NULL) free(str);

	return ret;
}

int run_net_script(envid_t veid, int op, list_head_t *ip_h, int state)
{
	char *argv[2];
	char *envp[10];
	const char *script;
	int ret;
	char buf[STR_SIZE];
	int i = 0;

	if (list_empty(ip_h))
		return 0;
	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_STATE=%s", state2str(state));
	envp[i++] = strdup(buf);
	envp[i++] = list2str("IP_ADDR", ip_h);
	envp[i++] = strdup(ENV_PATH);
	envp[i] = NULL;
	switch (op) {
		case ADD:
			script = VPS_NET_ADD;
			break;
		case DEL:
			script = VPS_NET_DEL;
			break;
		default:
			return 0;
	}
	argv[0] = (char *)script;
	argv[1] = NULL;
	ret = run_script(script, argv, envp, 0);
	free_arg(envp);

	return ret;
}

static inline int invert_ip_op(int op)
{
	if (op == VE_IP_ADD)
		return VE_IP_DEL;
	else
		return VE_IP_ADD;
}

static int vps_ip_ctl(vps_handler *h, envid_t veid, int op,
		list_head_t *ip_h, int rollback)
{
	int ret = 0;
	ip_param *ip;
	int inv_op;

	list_for_each(ip, ip_h, list) {
		if ((ret = ip_ctl(h, veid, op, ip->val))) 
			break;
	}
	if (ret && rollback) {
		/* restore original ip state op of error */
		inv_op = invert_ip_op(op);
		list_for_each_prev(ip, ip_h, list) {
			ip_ctl(h, veid, inv_op, ip->val);
		}
	}

	return ret;
}

static inline int vps_add_ip(vps_handler *h, envid_t veid,
	list_head_t *ip_h)
{
	char *str;
	int ret;

	if ((str = list2str(NULL, ip_h)) != NULL) {
		logger(0, 0, "Adding IP address(es): %s", str);
		free(str);
	}
	if ((ret = vps_ip_ctl(h, veid, VE_IP_ADD, ip_h, 1)))
		return ret;
	if ((ret = run_net_script(veid, ADD, ip_h, STATE_RUNNING)))
		vps_ip_ctl(h, veid, VE_IP_DEL, ip_h, 0);
	
	return ret;
}

static inline int vps_del_ip(vps_handler *h, envid_t veid,
	list_head_t *ip_h)
{
	char *str;
	int ret;

	if ((str = list2str(NULL, ip_h)) != NULL) {
		logger(0, 0, "Deleting IP address(es): %s", str);
		free(str);
	}
	if ((ret = vps_ip_ctl(h, veid, VE_IP_DEL, ip_h, 1)))
		return ret;
	run_net_script(veid, DEL, ip_h, STATE_RUNNING);

	return ret;
}

static inline int vps_set_ip(vps_handler *h, envid_t veid,
	list_head_t *ip_h)
{
	int ret;
	list_head_t oldip;

	list_head_init(&oldip);
	if (get_vps_ip(h, veid, &oldip) < 0)
		return VZ_GET_IP_ERROR;
	if (!(ret = vps_del_ip(h, veid, &oldip))) {
		if ((ret = vps_add_ip(h, veid, ip_h)))
			vps_add_ip(h, veid, &oldip);
	}
	free_str_param(&oldip);

	return ret;
}

static int netdev_ctl(vps_handler *h, int veid, int op, char *name)
{
	struct vzctl_ve_netdev ve_netdev;

	ve_netdev.veid = veid;
	ve_netdev.op = op;
	ve_netdev.dev_name = name;
	if (ioctl(h->vzfd, VZCTL_VE_NETDEV, &ve_netdev) < 0) 
		return VZ_NETDEV_ERROR;
	return 0;
}

int vps_netdev_ctl(vps_handler *h, envid_t veid, int op, net_param *net)
{
	int ret = 0;
	list_head_t *dev_h = &net->dev;
	net_dev_param *dev;
	int cmd;

	if (list_empty(dev_h))
		return 0;
	if (!vps_is_run(h, veid)){
		logger(0, 0, "Unable to setup network devices:"
			" VPS is not running");
		return VZ_VE_NOT_RUNNING;
	}
	cmd = (op == ADD) ? VE_NETDEV_ADD : VE_NETDEV_DEL;
	list_for_each(dev, dev_h, list) {
		if ((ret = netdev_ctl(h, veid, cmd, dev->val))) {
			logger(0, errno, "Unable to %s netdev %s",
				(op == ADD) ? "add": "del", dev->val);
			break;
		}
	}
	return ret;
}

int vps_net_ctl(vps_handler *h, envid_t veid, int op, net_param *net,
	dist_actions *actions, char *root, int state)
{
	list_head_t *ip_h = &net->ip;
	int ret = 0;

	if (list_empty(ip_h) &&	!net->delall) {
		/* make initial network setup on VPS start*/
		if (state == STATE_STARTING && op == ADD)
			vps_ip_configure(h, veid, actions, root, op, net,state);
		return 0;
	}
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to apply network parameters: VPS is not"
			" running");
		return VZ_VE_NOT_RUNNING;
	}
	if (op == ADD) {
		if (net->delall == YES)
			ret = vps_set_ip(h, veid, ip_h);
		else
			ret = vps_add_ip(h, veid, ip_h);
	} else if (op == DEL) {
		ret = vps_del_ip(h, veid, ip_h);
	}
	if (!ret)
		vps_ip_configure(h, veid, actions, root, op, net, state);
	return ret;
}

#define	PROC_VEINFO	"/proc/vz/veinfo"
static inline int get_vps_ip_proc(envid_t veid, list_head_t *ip_h)
{
	FILE *fd;
	char str[16384];
	char *token;
	int id, cnt = 0;

	if ((fd = fopen(PROC_VEINFO, "r")) == NULL) {
		logger(0, errno, "Unable to open %s", PROC_VEINFO);
		return -1;
	}
	while (!feof(fd)) {
		if (fgets(str, sizeof(str), fd) == NULL)
			break;
		token = strtok(str, " ");
		if (token == NULL)
			continue;
		if (parse_int(token, &id))
			continue;	
		if (veid != id)
			continue;
		if ((token = strtok(NULL, " ")) != NULL)
			token = strtok(NULL, " ");
		if (token == NULL)
			break;
		while ((token = strtok(NULL, " \t\n")) != NULL) {
			if (add_str_param(ip_h, token)) {
				free_str_param(ip_h);
				cnt = -1;
				break;
			}
			cnt++;
		}
		break;
	}
	fclose(fd);
	return cnt;
}

#if HAVE_VZLIST_IOCTL
#ifndef NIPQUAD
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]
#endif

static inline int get_vps_ip_ioctl(vps_handler *h, envid_t veid,
	list_head_t *ip_h)
{
	int ret = -1;
	struct vzlist_veipv4ctl veip;
	uint32_t *addr;
	char buf[16];
	int i;

	veip.veid = veid;
	veip.num = 256;
	addr = malloc(veip.num * sizeof(*veip.ip));
	if (addr == NULL)
		return -1;
	for (;;) {
		veip.ip = addr;
		ret = ioctl(h->vzfd, VZCTL_GET_VEIPS, &veip);
		if (ret < 0) 
			goto out;
		else if (ret <= veip.num)
			break;
		veip.num = ret;
		addr = realloc(addr, veip.num * sizeof(*veip.ip));
		if (addr == NULL)
			return -1;
	}
	if (ret > 0) {
		for (i = ret - 1; i >= 0; i--) {
			snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
				NIPQUAD(addr[i]));
			if ((ret = add_str_param(ip_h, buf)))
				break;
		}
	}
out:
	free(addr);
	return ret;
}

int get_vps_ip(vps_handler *h, envid_t veid, list_head_t *ip_h)
{
	int ret;
	ret = get_vps_ip_ioctl(h, veid, ip_h);
	if (ret < 0) 
		ret = get_vps_ip_proc(veid, ip_h);
	if (ret < 0)
		free_str_param(ip_h);
	return ret;
}
#else

int get_vps_ip(vps_handler *h, envid_t veid, list_head_t *ip_h)
{
	int ret;

	if ((ret = get_vps_ip_proc(veid, ip_h)) < 0)
		free_str_param(ip_h);
	return ret;
}
#endif

