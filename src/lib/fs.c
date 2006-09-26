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
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>

#include "fs.h"
#include "util.h"
#include "logger.h"
#include "vzerror.h"
#include "script.h"
#include "quota.h"

int vps_is_run(vps_handler *h, envid_t veid);

/** Get VE mount status.
 *
 * @param root		VE root.
 * @return		 1 - VE mounted
 *			 0 - VE unmounted.
 *			-1 - error
 */
int vps_is_mounted(char *root)
{
	return vz_fs_is_mounted(root);
}

/** Mount VE.
 *
 * @param veid		VE id.
 * @param fs		file system parameters.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int fsmount(envid_t veid, fs_param *fs, dq_param *dq)
{
	int ret;

	/* Create VE_ROOT mount point if not exist */
	if (make_dir(fs->root, 1)) {
		logger(-1, 0, "Can't create mount point %s", fs->root);
		return VZ_FS_MPOINTCREATE;
	}
	if ((ret = vps_quotaon(veid, fs->private, dq)))
		return ret;
	if ((ret = vz_mount(fs, 0))) 
		vps_quotaoff(veid, dq);
	return ret;
}

static int real_umount(envid_t veid, char *root)
{
	int i, ret, n;

	n = 0;
	for (i = 0; i < 2; i++) {
		while (1) {
			ret = umount2(root, MNT_DETACH);
			if (ret < 0) {
				if (n > 0 && errno == EINVAL)
					ret = 0;
				break;
			}
			n++;
		}
		if (ret == 0 || (ret < 0 && errno != EBUSY))
			break;
		sleep(1);
	}
	if (ret) {
		logger(-1, errno, "Can't umount: %s", root);
		ret = VZ_FS_CANTUMOUNT;
	}
	return ret;
}

/** Unmount VE.
 *
 * @param veid		VE id.
 * @param root		VE root.
 * @return		0 on success.
 */
int fsumount(envid_t veid, char *root)
{
	int ret;

	if (!(ret = real_umount(veid, root))) {
		if (!quota_ctl(veid, QUOTA_STAT))
			ret = quota_off(veid, 0);
	}
	return ret;
}

/** Mount VE and run mount action script if exists.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param fs		file system parameters.
 * @param dq		disk quota parameters.
 * @param skip		skip mount action scrips
 * @return		0 on success.
 */
int vps_mount(vps_handler *h, envid_t veid, fs_param *fs, dq_param *dq,
	skipFlags skip)
{
	char buf[PATH_LEN];
	int ret, i;

	if (check_var(fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (check_var(fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (!stat_file(fs->private)) {
		logger(-1, 0, "VE private area %s does not exist", fs->private);
		return VZ_FS_NOPRVT;
	}
	if (vps_is_mounted(fs->root)) {
		logger(-1, 0, "VE is already mounted");
		return 0;
	}
	if ((ret = fsmount(veid, fs, dq)))
		return ret;
	/* Execute per VE & global mount scripts */
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), "%svps.%s", VPS_CONF_DIR,
			MOUNT_PREFIX);
		for (i = 0; i < 2; i++) {
			if (run_pre_script(veid, buf)) {
				logger(-1, 0, "Error executing mount script %s",
					buf);
				fsumount(veid, fs->root);
				return VZ_ACTIONSCRIPT_ERROR;
			}
			snprintf(buf, sizeof(buf), "%s%d.%s", VPS_CONF_DIR,
				veid, MOUNT_PREFIX);
        	}
	}
	logger(0, 0, "VE is mounted");

	return 0;
}

/** Unmount VE and run unmount action script if exists.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param root		VE root.
 * @param skip		skip unmount action scrips
 * @return		0 on success.
 */
int vps_umount(vps_handler *h, envid_t veid, char *root, skipFlags skip)
{
        char buf[PATH_LEN];
	int ret, i;

	if (!vps_is_mounted(root)) {
		logger(-1, 0, "VE is not mounted");
		return VZ_FS_NOT_MOUNTED;
	}
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "VE is running. Stop VE first");
		return 0;
	}
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), "%svps.%s", VPS_CONF_DIR,
			UMOUNT_PREFIX);
		for (i = 0; i < 2; i++) {
			if (run_pre_script(veid, buf)) {
				logger(-1, 0, "Error executing umount script %s",
					buf);
				return VZ_ACTIONSCRIPT_ERROR;
			}
			snprintf(buf, sizeof(buf), "%s%d.%s", VPS_CONF_DIR,
				veid, UMOUNT_PREFIX);
		}
	}
	if (!(ret = fsumount(veid, root)))
		logger(0, 0, "VE is unmounted");

	return 0;
}

int vps_set_fs(fs_param *g_fs, fs_param *fs)
{
	if (fs->noatime != YES)
		return 0;
	if (check_var(g_fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (check_var(g_fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (!vps_is_mounted(g_fs->root)) {
		logger(-1, 0, "VE is not mounted");
		return VZ_FS_NOT_MOUNTED;
	}
	g_fs->noatime = fs->noatime;
	return vz_mount(g_fs, 1);
}
