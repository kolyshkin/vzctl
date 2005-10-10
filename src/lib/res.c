/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <asm/timex.h>
#include <linux/vzcalluser.h>

#include "vzerror.h"
#include "env.h"
#include "dev.h"
#include "logger.h"
#include "res.h"
#include "exec.h"
#include "cap.h"
#include "dist.h"
#include "util.h"
#include "quota.h"

const char *state2str(int state);


int vps_hostnm_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, char *hostname, int state)
{
	char *envp[4];
	const char *script;
	int ret;
	char vps_state[32];
	char hostnm[STR_SIZE];
	const char *str_state;

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
	logger(0, 0, "Set hostname: %s", hostname);
	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);

	return ret;
}

int vps_dns_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, misc_param *net, int state)
{
	char *envp[10];
	char *str;
	const char *script;
	int ret, i;

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
		str = list2str("SEARCHDOMAIN", &net->searchdomain);
		if (str != NULL)
			envp[i++] = str;
	}
	if (!list_empty(&net->nameserver)) {
		str = list2str("NAMESERVER", &net->nameserver);
		if (str != NULL)
			envp[i++] = str;
	}
	envp[i++] = strdup(ENV_PATH);
	envp[i] = NULL;
	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
		SCRIPT_EXEC_TIMEOUT);
	logger(0, 0, "File resolv.conf was modified");
	free_arg(envp);

	return ret;
}

int vps_pw_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, list_head_t *pw)
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
	if (str != NULL) free(str);
	return ret;
}

int vps_quota_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, dq_param *dq, int state)
{
	char *envp[6];
	const char *script;
	int ret, i;
	char buf[64];
	const char *str_state;
	struct stat st;
	dev_res dev;
	const char *fs_name;

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
	if (stat(root, &st)) {
		logger(0, errno, "Unable to stat %s", root);
		return -1;
	}
	memset(&dev, 0, sizeof(dev));
	dev.dev = st.st_dev;
	dev.type = S_IFBLK | VE_USE_MINOR;
	dev.mask = S_IXGRP;
	if ((ret = set_devperm(h, veid, &dev)))
		return ret;
	i = 0;
	str_state = state2str(state);
	snprintf(buf, sizeof(buf), "VE_STATE=%s", str_state);
	envp[i++] = strdup(buf);
	if (*dq->ugidlimit)  {
		fs_name = vz_fs_get_name();
		snprintf(buf, sizeof(buf), "DEVFS=%s", fs_name);
		envp[i++] = strdup(buf);
		snprintf(buf, sizeof(buf), "MINOR=%d", minor(st.st_dev));
		envp[i++] = strdup(buf);
		snprintf(buf, sizeof(buf), "MAJOR=%d", major(st.st_dev));
		envp[i++] = strdup(buf);
	}
	envp[i++] = strdup(ENV_PATH);
	envp[i] = NULL;
	logger(0, 0, "Setting quota ugidlimit: %d", *dq->ugidlimit);
	ret = vps_exec_script(h, veid, root, NULL, envp, script, DIST_FUNC,
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

int vps_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, int op, vps_res *res, int state)
{
	int ret;

	if (!need_configure(res))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to configure VPS is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if (actions == NULL) {	
		logger(0, 0, "Dist action not loaded");
		return -1;
	}
	if ((ret = vps_hostnm_configure(h, veid, actions, root,
		res->misc.hostname, state)))
	{
		return ret;
	}
	if ((ret = vps_dns_configure(h, veid, actions, root, &res->misc,
		state)))
	{
		return ret;
	}
	if ((ret = vps_quota_configure(h, veid, actions, root, &res->dq,
		state)))
	{
		return ret;
	}
	return 0;
}

int vps_setup_res(vps_handler *h, envid_t veid, dist_actions *actions,
	fs_param *fs, vps_param *param, int vps_state, skipFlags skip,
	struct mod_action *action)
{
	int ret;
	vps_res *res = &param->res;
	
	if (skip & SKIP_SETUP)
		return 0;
	if (vps_state != STATE_STARTING) {
		if ((ret = vps_set_ublimit(h, veid, &res->ub)))
			return ret;
	}
	if ((ret = vps_set_quota(veid, &res->dq)))
		return ret;
	if ((ret = vps_net_ctl(h, veid, DEL, &param->del_res.net, actions,
		fs->root, vps_state)))
	{
		return ret;
	}
	if ((ret = vps_net_ctl(h, veid, ADD, &res->net, actions, fs->root,
		vps_state)))
	{
		return ret;
	}
	if ((ret = vps_netdev_ctl(h, veid, DEL, &param->del_res.net)))
		return ret;
	if ((ret = vps_netdev_ctl(h, veid, ADD, &res->net)))
		return ret;
	if ((ret = vps_set_cpu(h, veid, &res->cpu)))
		return ret;
	if ((ret = vps_set_devperm(h, veid, fs->root, &res->dev)))
		return ret;
	if ((ret = vps_set_fs(fs, &res->fs)))
		return ret;
	if (vps_state == STATE_RUNNING && vps_is_run(h, veid)) {
		if (res->cap.on || res->cap.off)
			logger(0, 0, "Unable to set capability on running VPS");
		if (res->env.ipt_mask) {
			logger(0, 0, "Unable to set iptables on running VPS");
			return VZ_SET_IPTABLES;
		}
	}
	if (!(skip & SKIP_CONFIGURE))
		vps_configure(h, veid, actions, fs->root, ADD, res, vps_state);
	ret = mod_setup(h, veid, vps_state, skip, action, param);

	return ret;
}
