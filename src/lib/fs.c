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
#include <unistd.h>
#include <errno.h>
#include <mntent.h>
#include <string.h>
#include <sys/mount.h>
#include <limits.h>

#include "fs.h"
#include "util.h"
#include "logger.h"
#include "vzerror.h"
#include "script.h"
#include "quota.h"
#include "image.h"
#include "list.h"

int vps_is_run(vps_handler *h, envid_t veid);

int fsmount(envid_t veid, fs_param *fs, dq_param *dq, int fsck)
{
	int ret;

	/* Create VE_ROOT mount point if not exist */
	if (make_dir(fs->root, 1)) {
		logger(-1, 0, "Can't create mount point %s", fs->root);
		return VZ_FS_MPOINTCREATE;
	}
	if (ve_private_is_ploop(fs)) {
		if (! is_ploop_supported())
			return VZ_PLOOP_UNSUP;
#ifdef HAVE_PLOOP
		{
			struct vzctl_mount_param param = {};

			param.target = fs->root;
			param.quota = is_2nd_level_quota_on(dq);
			param.mount_data = fs->mount_opts;
			param.fsck = fsck;
			ret = vzctl_mount_image(fs->private, &param);
		}
#else
		/* Check for is_ploop_supported() above will log and return
		 * an error. This part is for compiler to be happy.
		 */
		ret = VZ_PLOOP_UNSUP;
#endif
	}
	else {
		if ((ret = vps_quotaon(veid, fs->private, dq)))
			return ret;
		if ((ret = vz_mount(fs, 0)))
			vps_quotaoff(veid, dq);
	}
	return ret;
}

#define DELETED_STR	" (deleted)"

static int umount_submounts(const char *root)
{
	FILE *fp;
	struct mntent *mnt;
	int len;
	char path[PATH_MAX + 1];
	list_head_t head;
	str_param *it;

	if (realpath(root, path) == NULL) {
		logger(-1, errno, "realpath(%s) failed", root);
		return -1;
	}
	if ((fp = setmntent("/proc/mounts", "r")) == NULL) {
		logger(-1, errno, "Unable to open /proc/mounts");
		return -1;
	}
	list_head_init(&head);
	strcat(path, "/"); /* skip base mountpoint */
	len = strlen(path);
	while ((mnt = getmntent(fp)) != NULL) {
		const char *p = mnt->mnt_dir;

		if (strncmp(p, DELETED_STR, sizeof(DELETED_STR) - 1) == 0)
			p += sizeof(DELETED_STR) - 1;

		if (strncmp(path, p, len) == 0)
			add_str_param(&head, p);
	}
	endmntent(fp);

	list_for_each_prev(it, &head, list) {
		if (umount(it->val))
			logger(-1, errno, "Cannot umount %s",
						it->val);
	}
	free_str_param(&head);

	return 0;
}

int fsumount(envid_t veid, const fs_param *fs)
{
	umount_submounts(fs->root);

	if (ve_private_is_ploop(fs)) {
		if (! is_ploop_supported())
			return VZ_PLOOP_UNSUP;
#ifdef HAVE_PLOOP
		return vzctl_umount_image(fs->private);
#else
		/* Check for is_ploop_supported() above will log and return
		 * an error. This part is for compiler to be happy.
		 */
		return VZ_PLOOP_UNSUP;
#endif
	}
	/* simfs case */
	if (umount(fs->root) != 0) {
		logger(-1, errno, "Can't umount %s", fs->root);
		return VZ_FS_CANTUMOUNT;
	}

	if (is_vzquota_available() && !quota_ctl(veid, QUOTA_STAT))
		return quota_off(veid, 0);

	return 0;
}

int vps_mount(vps_handler *h, envid_t veid, fs_param *fs, dq_param *dq,
	skipFlags skip)
{
	char buf[PATH_LEN];
	int ret, i;
	int fsck = ! (skip & SKIP_FSCK);

	if (check_var(fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (check_var(fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (vps_is_mounted(fs->root, fs->private) == 1) {
		logger(-1, 0, "Container is already mounted");
		return 0;
	}
	/* Execute pre mount scripts */
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), "%s/vps.%s", VPSCONFDIR,
			PRE_MOUNT_PREFIX);
		for (i = 0; i < 2; i++) {
			if (run_pre_script(veid, buf)) {
				logger(-1, 0, "Error executing "
						"premount script %s", buf);
				return VZ_ACTIONSCRIPT_ERROR;
			}
			snprintf(buf, sizeof(buf), "%s/%d.%s", VPSCONFDIR,
				veid, PRE_MOUNT_PREFIX);
		}
	}

	if (stat_file(fs->private) != 1) {
		logger(-1, 0, "Container private area %s does not exist",
				fs->private);
		return VZ_FS_NOPRVT;
	}

	if ((ret = fsmount(veid, fs, dq, fsck)))
		return ret;
	/* Execute per-CT & global mount scripts */
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), "%s/vps.%s", VPSCONFDIR,
			MOUNT_PREFIX);
		for (i = 0; i < 2; i++) {
			if (run_pre_script(veid, buf)) {
				logger(-1, 0, "Error executing mount script %s",
					buf);
				fsumount(veid, fs);
				return VZ_ACTIONSCRIPT_ERROR;
			}
			snprintf(buf, sizeof(buf), "%s/%d.%s", VPSCONFDIR,
				veid, MOUNT_PREFIX);
		}
	}
	logger(0, 0, "Container is mounted");

	return 0;
}

int vps_umount(vps_handler *h, envid_t veid, const fs_param *fs,
		skipFlags skip)
{
	char buf[PATH_LEN];
	int ret, i;

	if (vps_is_mounted(fs->root, fs->private) == 0) {
		logger(-1, 0, "CT is not mounted");
		return VZ_FS_NOT_MOUNTED;
	}
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "Container is running -- stop it first");
		return VZ_VE_RUNNING;
	}
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), "%s/%d.%s", VPSCONFDIR,
			veid, UMOUNT_PREFIX);
		for (i = 0; i < 2; i++) {
			if (run_pre_script(veid, buf)) {
				logger(-1, 0, "Error executing umount script %s",
					buf);
				return VZ_ACTIONSCRIPT_ERROR;
			}
			snprintf(buf, sizeof(buf), "%s/vps.%s", VPSCONFDIR,
				UMOUNT_PREFIX);
		}
	}
	if (!(ret = fsumount(veid, fs)))
		logger(0, 0, "Container is unmounted");
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), "%s/%d.%s", VPSCONFDIR,
			veid, POST_UMOUNT_PREFIX);
		for (i = 0; i < 2; i++) {
			if (run_pre_script(veid, buf)) {
				logger(-1, 0, "Error executing "
						"postumount script %s",	buf);
				return VZ_ACTIONSCRIPT_ERROR;
			}
			snprintf(buf, sizeof(buf), "%s/vps.%s", VPSCONFDIR,
				POST_UMOUNT_PREFIX);
		}
	}

	return ret;
}

int vps_set_fs(fs_param *g_fs, fs_param *fs)
{
	/* This function is currently unused */
	return 0;

	if (check_var(g_fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (check_var(g_fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (vps_is_mounted(g_fs->root, g_fs->private) == 0) {
		logger(-1, 0, "Container is not mounted");
		return VZ_FS_NOT_MOUNTED;
	}

	return vz_mount(g_fs, MS_REMOUNT | fs->flags);
}
