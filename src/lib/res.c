/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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
#include "vps_configure.h"
#include "io.h"

/** Function called on CT start to setup resource management
 *
 */
int setup_resource_management(vps_handler *h, envid_t veid, vps_res *res)
{
	int ret;

	if ((ret = check_ub(&res->ub)))
		return ret;
	if ((ret = set_ublimit(h, veid, &res->ub)))
		return ret;
	return 0;
}

/** Give Container permissions to do quotactl operations on its root.
 *
 */
static int vps_2quota_perm(vps_handler *h, int veid,
				const char *root, dq_param *dq)
{
	dev_res dev;
	struct stat st;

	if (dq->enable == NO || dq->ugidlimit == NULL ||  *dq->ugidlimit == 0)
		return 0;

	if (stat(root, &st)) {
		logger(-1, errno, "Unable to stat %s", root);
		return VZ_ERROR_SET_USER_QUOTA;
	}

	memset(&dev, 0, sizeof(dev));
	dev.dev = st.st_dev;
	dev.type = S_IFBLK | VE_USE_MINOR;
	dev.mask = S_IXGRP;
	return set_devperm(h, veid, &dev);
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
	if ((ret = vps_net_ctl(h, veid, DEL, &param->del_res.net, actions,
		fs->root, vps_state, skip)))
	{
		return ret;
	}
	if ((ret = vps_net_ctl(h, veid, ADD, &res->net, actions, fs->root,
		vps_state, skip)))
	{
		return ret;
	}
	if ((ret = vps_set_netdev(h, veid, &res->ub,
					&res->net, &param->del_res.net)))
		return ret;
	if ((ret = vps_set_cpu(h, veid, &res->cpu)))
		return ret;
	if ((ret = vps_set_devperm(h, veid, fs->root, &res->dev)))
		return ret;
	if ((ret = vps_set_pci(h, veid, ADD, fs->root, &res->pci)))
		return ret;
	if ((ret = vps_set_pci(h, veid, DEL, fs->root, &param->del_res.pci)))
		return ret;
	if ((ret = vps_set_fs(fs, &res->fs)))
		return ret;
	if((ret = vps_meminfo_set(h, veid, &res->meminfo, param, vps_state)))
		return ret;
	if ((ret = ve_ioprio_set(h, veid, &res->io)))
		return ret;
	if ((ret = vps_2quota_perm(h, veid, fs->root, &res->dq)))
		return ret;

	if (!(skip & SKIP_CONFIGURE))
		vps_configure(h, veid, actions, fs->root, param, vps_state);
	/* Setup quota limits after configure steps */
	if ((ret = vps_set_quota(veid, &res->dq)))
		return ret;
	if ((ret = vps_setup_veth(h, veid, actions,  fs->root, &res->veth,
			&param->del_res.veth, vps_state, skip)))
	{
		return ret;
	}
	ret = mod_setup(h, veid, vps_state, skip, action, param);

	return ret;
}
