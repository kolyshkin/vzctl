/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <linux/vzcalluser.h>

#include "vzerror.h"
#include "logger.h"
#include "res.h"
#include "exec.h"
#include "dist.h"
#include "util.h"
#include "env.h"
#include "vps_configure.h"
#include "list.h"
#include "image.h"


static struct vps_state{
	char *name;
	int id;
} vps_states[] = {
	{"starting", STATE_STARTING},
	{"running", STATE_RUNNING},
	{"stopping", STATE_STOPPING},
	{"restoring", STATE_RESTORING},
	{"checkpointing", STATE_CHECKPOINTING},
};

const char *state2str(int state)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(vps_states); i++)
		if (vps_states[i].id == state)
			return vps_states[i].name;

	return NULL;
}

static const char *get_local_ip(vps_param *param)
{
	list_head_t *h = &param->res.net.ip;
	const char *ip, *mask;
	static char dst[INET6_ADDRSTRLEN];

	/* Use first IP in the CT config */
	if (param->g_param != NULL && !param->res.net.delall)
		h = &param->g_param->res.net.ip;
	if (list_empty(h))
		h = &param->res.net.ip;
	if (list_empty(h))
		return NULL;

	ip = list_first_entry(h, ip_param, list)->val;
	mask = strchr(ip, '/');
	if (!mask)
		return ip;
	return strncpy(dst, ip, mask - ip);
}

static int vps_hostnm_configure(vps_handler *h, envid_t veid,
	dist_actions *actions, const char *root,
	char *hostname,	const char *ip, int state)
{
	char *envp[5];
	const char *script;
	int ret;
	char vps_state[32];
	char hostnm[STR_SIZE];
	const char *str_state;
	char ipnm[STR_SIZE];

	if (hostname == NULL)
		return 0;
	script = actions->set_hostname;
	if (script == NULL) {
		logger(0, 0, "Warning: set_hostname action script is not"
			" specified");
		return 0;
	}
	str_state = state2str(state);
	snprintf(vps_state, sizeof(vps_state), "VE_STATE=%s", str_state);
	envp[0] = vps_state;
	snprintf(hostnm, sizeof(hostnm), "HOSTNM=%s", hostname);
	envp[1] = hostnm;
	envp[2] = ENV_PATH;
	envp[3] = NULL;
	if (ip != NULL) {
		snprintf(ipnm, sizeof(ipnm), "IP_ADDR=%s", ip);
		envp[3] = ipnm;
	}
	envp[4] = NULL;
	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);

	return ret;
}

struct resolv_conf_data {
	int set;
	char *nameserver;
	char *searchdomain;
};

/* Read nameserver and search/domain lines from HW's /etc/resolv.conf
 *
 * We have to process resolv.conf the same way as system does,
 * so this function is partly based on GLIBC implementation
 * (resolv/res_init.c:__res_vinit()) which in turn
 * is taken from BIND 8.
 */
static int read_resolv_conf(struct resolv_conf_data *r)
{
	const char *file = "/etc/resolv.conf";
	FILE *fp;
	char buf[STR_SIZE];
	char nsrv[STR_SIZE] = "";
	char *srch = NULL;

	if (r->set)
		return 0;

	r->set = 1;

#define MATCH(line, name) \
	(!strncmp(line, name, sizeof(name) - 1) && \
	 (line[sizeof(name) - 1] == ' ' || \
	  line[sizeof(name) - 1] == '\t'))

	fp = fopen(file, "r");
	if (!fp) {
		logger(-1, errno, "Can't open %s", file);
		return -1;
	}
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *cp, *ep;

		/* skip comments */
		if (*buf == ';' || *buf == '#')
			continue;
		/* read default domain name */
		if (MATCH(buf, "domain") || MATCH(buf, "search")) {
			cp = buf + 6;
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if ((*cp == '\0') || (*cp == '\n'))
				continue;
			if ((ep = strchr(cp, '\n')) != NULL)
				*ep = '\0';
			/* As per resolv.conf(5), if there's more than
			 * one line, only the last line is used
			 */
			if (srch)
				free(srch);
			srch = strdup(cp);
			continue;
		}
		if (MATCH(buf, "nameserver")) {
			cp = buf + 10;
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if ((*cp == '\0') || (*cp == '\n'))
				continue;
			if ((ep = strchr(cp, '\n')) != NULL)
				*ep = '\0';
			/* nameserver entries are accumulated */
			if (nsrv[0])
				*--cp=' ';
			strncat(nsrv, cp, sizeof(nsrv) - strlen(nsrv) - 1);
			continue;
		}
	}
#undef MATCH

	fclose(fp);

	asprintf(&r->nameserver, "NAMESERVER=%s", nsrv);
	asprintf(&r->searchdomain, "SEARCHDOMAIN=%s", srch);
	free(srch);

	return 0;
}

static void free_resolv_conf(struct resolv_conf_data *r)
{
	if (r->nameserver)
		free(r->nameserver);
	if (r->searchdomain)
		free(r->searchdomain);
}

static int vps_dns_configure(vps_handler *h, envid_t veid,
	dist_actions *actions, const char *root, misc_param *net, int state)
{
	char *envp[10];
	char *str;
	const char *script;
	int ret, i;
	struct resolv_conf_data resolv_conf = {};

	if (list_empty(&net->searchdomain) &&
		list_empty(&net->nameserver))
	{
		return 0;
	}
	script = actions->set_dns;
	if (script == NULL) {
		logger(0, 0, "Warning: set_dns action script is not specified");
		return 0;
	}
	i = 0;
	if (!list_empty(&net->searchdomain)) {
		str = list_first_entry(&net->searchdomain, str_param, list)->val;
		if (strcmp(str, "inherit") == 0) {
			read_resolv_conf(&resolv_conf);
			str = strdup(resolv_conf.searchdomain);
		}
		else
			str = list2str("SEARCHDOMAIN", &net->searchdomain);

		if (str != NULL)
			envp[i++] = str;
	}
	if (!list_empty(&net->nameserver)) {
		str = list_first_entry(&net->nameserver, str_param, list)->val;
		if (strcmp(str, "inherit") == 0) {
			read_resolv_conf(&resolv_conf);
			str = strdup(resolv_conf.nameserver);
		}
		else
			str = list2str("NAMESERVER", &net->nameserver);

		if (str != NULL)
			envp[i++] = str;
	}
	envp[i++] = strdup(ENV_PATH);
	envp[i] = NULL;
	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);
	free_arg(envp);
	free_resolv_conf(&resolv_conf);

	return ret;
}

int vps_pw_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	const char *root, list_head_t *pw)
{
	char *envp[3];
	const char *script;
	int ret;
	char *str;

	if (list_empty(pw) || actions == NULL)
		return 0;
	script = actions->set_userpass;
	if (script == NULL) {
		logger(0, 0, "Warning: set_userpass action script is not"
			" specified");
		return 0;
	}
	str = list2str("USERPW", pw);
	envp[0] = str;
	envp[1] = ENV_PATH;
	envp[2] = NULL;
	if ((ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT)))
	{
		ret = VZ_CHANGEPASS;
		logger(0, 0, "Password change failed");
	}
	free(str);
	return ret;
}

static int vps_quota_configure(vps_handler *h, envid_t veid,
	const dist_actions *actions, const fs_param *fs, const dq_param *dq,
	int state)
{
	char *envp[6];
	const char *script;
	int ret, i;
	char buf[64];
	const char *str_state;

	if (dq->enable == NO)
		return 0;
	if (dq->ugidlimit == NULL)
		return 0;
	script = actions->set_ugid_quota;
	if (script == NULL) {
		logger(0, 0, "Warning: set_ugid_quota action script is not"
			" specified");
		return 0;
	}
	i = 0;
	str_state = state2str(state);
	snprintf(buf, sizeof(buf), "VE_STATE=%s", str_state);
	envp[i++] = strdup(buf);
	if (*dq->ugidlimit)  {
		if (!ve_private_is_ploop(fs)) {
			/* simfs/vzquota case */
			struct stat st;
			const char *fs_name;

			if (stat(fs->root, &st)) {
				logger(-1, errno, "Unable to stat %s",
						fs->root);
				return VZ_ERROR_SET_USER_QUOTA;
			}
			fs_name = vz_fs_get_name();
			snprintf(buf, sizeof(buf), "DEVFS=%s", fs_name);
			envp[i++] = strdup(buf);
			snprintf(buf, sizeof(buf), "MINOR=%d", minor(st.st_dev));
			envp[i++] = strdup(buf);
			snprintf(buf, sizeof(buf), "MAJOR=%d", major(st.st_dev));
			envp[i++] = strdup(buf);
		}
		else {
			/* ploop/native quota case */
			snprintf(buf, sizeof(buf), "UGIDLIMIT=%lu",
					*dq->ugidlimit);
			envp[i++] = strdup(buf);
		}
	}
	envp[i++] = strdup(ENV_PATH);
	envp[i] = NULL;
	ret = vps_exec_script(h, veid, fs->root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);
	free_arg(envp);

	return ret;
}

int need_configure(vps_res *res)
{
	if (res->misc.hostname == NULL &&
		!res->net.delall &&
		list_empty(&res->net.ip) &&
		list_empty(&res->misc.nameserver) &&
		list_empty(&res->misc.searchdomain) &&
		res->dq.ugidlimit == NULL)
	{
		return 0;
	}
	return 1;
}

/*
 * Setup (add/delete) IP address(es) inside container
 */
int vps_ip_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	const char *root, int op, net_param *net, int state)
{
	char *envp[6];
	char *str;
	const char *script = NULL;
	int ret, i;
	char vps_state[32];
	char *ipv6_net = "IPV6=yes";
	const char *str_state;
	char *delall = "IPDELALL=yes";

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
		envp[i++] = delall;
	if (net->ipv6_net == YES)
		envp[i++] = ipv6_net;
	envp[i++] = ENV_PATH;
	envp[i] = NULL;
	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);
	free(str);

	return ret;
}



int vps_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	const fs_param *fs, vps_param *param, int state)
{
	int ret;
	vps_res *res = &param->res;

	if (!need_configure(res))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to configure container: not running");
		return VZ_VE_NOT_RUNNING;
	}
	if (actions == NULL) {
		logger(0, 0, "Dist action not loaded");
		return -1;
	}
	if ((ret = vps_hostnm_configure(h, veid, actions, fs->root,
		res->misc.hostname, get_local_ip(param), state)))
	{
		return ret;
	}
	if ((ret = vps_dns_configure(h, veid, actions, fs->root, &res->misc,
		state)))
	{
		return ret;
	}
	if ((ret = vps_quota_configure(h, veid, actions, fs, &res->dq,
		state)))
	{
		return ret;
	}
	return 0;
}
