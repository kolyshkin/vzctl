/*
 *  Copyright (C) 2000-2011, Parallels, Inc. All rights reserved.
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
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
#include "vps_configure.h"


char *find_ip(list_head_t *ip_h, const char *ipaddr)
{
	ip_param *ip;
	char *slash, *ip_slash, *ip_only;
	int len;

	if (list_empty(ip_h))
		return NULL;

	slash = strchr(ipaddr, '/');
	if (slash) {
		len = slash - ipaddr + 1;
		ip_slash = strndupa(ipaddr, len);
		ip_only = strndupa(ipaddr, len - 1);
	}
	else {
		ip_only = NULL;
		len = asprintf(&ip_slash, "%s/", ipaddr);
	}

	list_for_each(ip, ip_h, list) {
		/* Match complete IP or IP/mask string */
		if (!strcmp(ip->val, ipaddr))
			return ip->val;
		/* Match only IP */
		if ((ip_only != NULL) && (!strcmp(ip->val, ip_only)))
			return ip->val;
		/* Match IP/ (same IP, different mask) */
		if (!strncmp(ip->val, ip_slash, len))
			return ip->val;
	}
	return NULL;
}

int merge_ip_list(int delall, list_head_t *old, list_head_t *add,
		list_head_t *del, list_head_t *merged)
{
	return __merge_str_list(delall, old, add, del, merged, find_ip);
}

static int ip_ctl(vps_handler *h, envid_t veid, int op, char *str)
{
	const char *ip, *mask;

	mask = strchr(str, '/');
	if (mask) {
		ip = strndupa(str, mask - str);
		mask++;
	}
	else {
		ip = str;
	}

	return h->ip_ctl(h, veid, op, ip);
}

int run_net_script(envid_t veid, int op, list_head_t *ip_h, int state,
	int skip_arpdetect)
{
	char *argv[3];
	char *envp[10];
	char *script;
	int ret;
	char buf[STR_SIZE];
	int i = 0;
	char *skip_str = "SKIP_ARPDETECT=yes";

	if (list_empty(ip_h))
		return 0;
	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_STATE=%s", state2str(state));
	envp[i++] = strdup(buf);
	envp[i++] = list2str("IP_ADDR", ip_h);
	envp[i++] = strdup(ENV_PATH);
	if (skip_arpdetect)
		envp[i++] = strdup(skip_str);
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
	argv[0] = script;
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

static int vps_add_ip(vps_handler *h, envid_t veid,
	net_param *net, int state)
{
	char *str;
	int ret;
	list_head_t *ip_h = &net->ip;

	if ((str = list2str(NULL, ip_h)) != NULL) {
		if (str[0] != '\0')
			logger(0, 0, "Adding IP address(es): %s", str);
		free(str);
	}
	if ((ret = vps_ip_ctl(h, veid, VE_IP_ADD, ip_h, 1)))
		return ret;
	if ((ret = run_net_script(veid, ADD, ip_h, state, net->skip_arpdetect)))
		vps_ip_ctl(h, veid, VE_IP_DEL, ip_h, 0);

	return ret;
}

static int vps_del_ip(vps_handler *h, envid_t veid,
	net_param *net, int state)
{
	char *str;
	int ret;
	list_head_t *ip_h = &net->ip;

	if (net->delall)
		/* Add existing VE IPs to the list */
		if (get_vps_ip(h, veid, ip_h) < 0)
			return VZ_GET_IP_ERROR;

	str = list2str(NULL, ip_h);
	if ((str != NULL) || (net->delall)) {
		logger(0, 0, "Deleting %sIP address(es): %s",
				(net->delall) ? "all " : "",
				str ? str : "");
		free(str);
	}
	if ((ret = vps_ip_ctl(h, veid, VE_IP_DEL, ip_h, 1)))
		return ret;
	run_net_script(veid, DEL, ip_h, state, net->skip_arpdetect);

	return ret;
}

static int set_netdev(vps_handler *h, envid_t veid, int cmd, net_param *net)
{
	int ret = 0;
	list_head_t *dev_h = &net->dev;
	net_dev_param *dev;

	if (list_empty(dev_h))
		return 0;
	list_for_each(dev, dev_h, list) {
		if ((ret = h->netdev_ctl(h, veid, cmd, dev->val))) {
			logger(-1, errno, "Unable to %s netdev %s",
				(cmd == VE_NETDEV_ADD ) ? "add": "del",
				dev->val);
			break;
		}
	}
	return ret;
}

int vps_set_netdev(vps_handler *h, envid_t veid, ub_param *ub,
		net_param *net_add, net_param *net_del)
{
	int ret, pid, pid1, status;

	if (list_empty(&net_add->dev) && list_empty(&net_del->dev))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to setup network devices: "
			"container is not running");
		return VZ_VE_NOT_RUNNING;
	}

	if ((ret = set_netdev(h, veid, VE_NETDEV_DEL, net_del)))
		return ret;

	/* Adding device(s) to CT should be done under setluid() */
	if ((pid1 = fork()) < 0) {
		logger(-1, errno, "Can't fork");
		return VZ_RESOURCE_ERROR;
	} else if (pid1 == 0) {

		if ((ret = h->setcontext(veid)))
			exit(ret);
		exit(set_netdev(h, veid, VE_NETDEV_ADD, net_add));
	}
	while ((pid = waitpid(pid1, &status, 0)) == -1)
		if (errno != EINTR) {
			logger(-1, errno, "Error in waitpid()");
			break;
		}
	ret = VZ_SYSTEM_ERROR;
	if (pid == pid1) {
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			logger(-1, 0, "Got signal %d", WTERMSIG(status));
	} else if (pid < 0)
		logger(-1, errno, "Error in waitpid()");

	return ret;
}

static int remove_ipv6_addr(net_param *net)
{
	list_head_t *head = &net->ip;
	ip_param *ip, *tmp;
	int cnt;

	cnt = 0;
	list_for_each_safe(ip, tmp, head, list) {
		if (get_addr_family(ip->val) == AF_INET6) {
			free(ip->val);
			list_del(&ip->list);
			free(ip);
			cnt++;
		}
	}
	return cnt;
}

int vps_net_ctl(vps_handler *h, envid_t veid, int op, net_param *net,
	dist_actions *actions, const char *root, int state, int skip)
{
	int ret = 0;

	if (list_empty(&net->ip) && /* Skip if no IPs in list, */
			/* except for these cases:
			 * (1) starting, do always run ADD */
			!(state == STATE_STARTING && op == ADD) &&
			/* (2) deleting all, do delete */
			!(op == DEL && net->delall))
		return 0;

	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to apply network parameters: "
			"container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if (net->ipv6_net != YES) {
		if (remove_ipv6_addr(net))
			logger(0, 0, "WARNING: IPv6 support is disabled");
	}
	if (op == ADD) {
		ret = vps_add_ip(h, veid, net, state);
	} else if (op == DEL) {
		ret = vps_del_ip(h, veid, net, state);
	}
	if (!ret && !(skip & SKIP_CONFIGURE))
		vps_ip_configure(h, veid, actions, root, op, net, state);
	return ret;
}

#define	PROC_VEINFO	"/proc/vz/veinfo"
static int get_vps_ip_proc(envid_t veid, list_head_t *ip_h)
{
	FILE *fd;
	char str[16384];
	char data[16];
	char *token;
	int id, cnt = 0;

	if ((fd = fopen(PROC_VEINFO, "r")) == NULL) {
		logger(-1, errno, "Unable to open %s", PROC_VEINFO);
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
		if (veid != (envid_t)id)
			continue;
		if ((token = strtok(NULL, " ")) != NULL)
			token = strtok(NULL, " ");
		if (token == NULL)
			break;
		while ((token = strtok(NULL, " \t\n")) != NULL) {
			/* Canonicalize IPv6 addresses */
			if ((get_addr_family(token) == AF_INET6) &&
			    inet_pton(AF_INET6, token, data) > 0 &&
			    !inet_ntop(AF_INET6, data, token, strlen(token)+1))
				break;

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

static int get_vps_ip_ioctl(vps_handler *h, envid_t veid,
	list_head_t *ip_h)
{
	int ret = -1;
	struct vzlist_veipv4ctl veip;
	uint32_t *addr, *tmp;
	char buf[64];
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
		tmp = realloc(addr, veip.num * sizeof(*veip.ip));
		if (tmp == NULL) {
			ret = -1;
			goto out;
		}
		addr = tmp;
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
