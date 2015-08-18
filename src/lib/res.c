/*
 *  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
#include "image.h"
#include "script.h"

static int fill_2quota_param(struct setup_env_quota_param *p,
		const struct fs_param *fs)
{
	struct stat st;

	if (!ve_private_is_ploop(fs)) {
		/* simfs case */
		if (stat(fs->root, &st)) {
			logger(-1, errno, "%s: Can't stat %s",
				       __func__, fs->root);
			return VZ_ERROR_SET_USER_QUOTA;
		}
		p->dev_name[0] = 0;
		p->dev = st.st_dev;

		return 0;
	}

	/* ploop case */
	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;
#ifdef HAVE_PLOOP
	if (vzctl_get_ploop_dev(fs->root, p->dev_name, sizeof(p->dev_name))) {
		logger(-1, 0, "Unable to find ploop device for %s", fs->root);
		return VZ_ERROR_SET_USER_QUOTA;
	}
#endif
	if (stat(p->dev_name, &st)) {
		logger(-1, errno, "%s: Can't stat %s", __func__, p->dev_name);
		return VZ_ERROR_SET_USER_QUOTA;
	}
	p->dev = st.st_rdev;

	return 0;
}

/** Give Container permissions to do quotactl operations on its root.
 *
 */
static int vps_2quota_perm(vps_handler *h, int veid, dev_t device)
{
	dev_res dev = { };

	dev.dev = device;
	dev.type = S_IFBLK | VE_USE_MINOR;
	dev.mask = S_IXGRP;
	return h->setdevperm(h, veid, &dev);
}

int vps_setup_res(vps_handler *h, envid_t veid, dist_actions *actions,
	fs_param *fs, ub_param *ub, vps_param *param,
	int vps_state, skipFlags skip,
	struct mod_action *action)
{
	int ret;
	vps_res *res = &param->res;

	if (skip & SKIP_SETUP)
		return 0;
	if (vps_state != STATE_STARTING) {
		if ((ret = vps_set_ublimit(h, veid, ub ? : &res->ub)))
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
	if ((ret = vps_meminfo_set(h, veid, &res->meminfo, param, vps_state)))
		return ret;
	if ((ret = vps_set_io(h, veid, &res->io)))
		return ret;
	/* Setup 2nd-level quota */
	if (is_2nd_level_quota_on(&res->dq)) {
		struct setup_env_quota_param qp;
		if ((ret = fill_2quota_param(&qp, fs)))
			return ret;
		if ((ret = vps_2quota_perm(h, veid, qp.dev)))
			return ret;
		if ((ret = vps_execFn(h, veid, fs->root,
					(execFn)setup_env_quota, &qp,
					VE_SKIPLOCK)))
			return ret;
	}

	if (vps_state == STATE_STARTING &&
			!(skip & SKIP_CONFIGURE) &&
			actions->set_console &&
			is_vz_kernel(h) &&
			check_min_kernel_version("2.6.32") == 0 &&
			vps_exec_script(h, veid, res->fs.root, NULL, NULL,
				actions->set_console, NULL, 0)) {
		return VZ_ACTIONSCRIPT_ERROR;
	}

	if (!(skip & SKIP_CONFIGURE))
		vps_configure(h, veid, actions, fs, param, vps_state);
	/* Setup quota limits after configure steps */
	if (!ve_private_is_ploop(fs)) {
		if ((ret = vps_set_quota(veid, &res->dq)))
			return ret;
	}
	if ((ret = vps_setup_veth(h, veid, actions,  fs->root, &res->veth,
			&param->del_res.veth, vps_state, skip)))
	{
		return ret;
	}
	ret = mod_setup(h, veid, vps_state, skip, action, param);

	return ret;
}
