/*
 *  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/cpt_ioctl.h>
#include <linux/vzcalluser.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>


#include "cpt.h"
#include "env.h"
#include "exec.h"
#include "script.h"
#include "vzconfig.h"
#include "vzerror.h"
#include "logger.h"
#include "util.h"

int cpt_cmd(vps_handler *h, envid_t veid, const char *root,
		int action, int cmd, unsigned int ctx)
{
	int fd;
	int err, ret = 0;
	const char *file;

	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if (action == CMD_CHKPNT) {
		file = PROC_CPT;
		err = VZ_CHKPNT_ERROR;
	} else if (action == CMD_RESTORE) {
		file = PROC_RST;
		err = VZ_RESTORE_ERROR;
	} else {
		logger(-1, 0, "cpt_cmd: Unsupported cmd");
		return -1;
	}
	if ((fd = open(file, O_RDWR)) < 0) {
		if (errno == ENOENT)
			logger(-1, errno, "Error: No checkpointing"
				" support, unable to open %s", file);
		else
			logger(-1, errno, "Unable to open %s", file);
		return err;
	}
	if ((ret = ioctl(fd, CPT_JOIN_CONTEXT, ctx ? : veid)) < 0) {
		logger(-1, errno, "Can not join cpt context %d", ctx ? : veid);
		goto err;
	}
	switch (cmd) {
	case CMD_KILL:
		logger(0, 0, "Killing...");
		if ((ret = ioctl(fd, CPT_KILL, 0)) < 0) {
			logger(-1, errno, "Can not kill container");
			goto err;
		}
		break;
	case CMD_RESUME:
		logger(0, 0, "Resuming...");
		clean_hardlink_dir(root);
		if ((ret = ioctl(fd, CPT_RESUME, 0)) < 0) {
			logger(-1, errno, "Can not resume container");
			goto err;
		}
		break;
	}
	if (!ctx) {
		logger(2, 0, "\tput context");
		if ((ret = ioctl(fd, CPT_PUT_CONTEXT, 0)) < 0) {
			logger(-1, errno, "Can not put context");
			goto err;
		}
	}
err:
	close(fd);
	return ret ? err : 0;
}

int vps_chkpnt(vps_handler *h, envid_t veid, const fs_param *fs,
		int cmd, cpt_param *param)
{
	const char *root = fs->root;

	if (root == NULL) {
		logger(-1, 0, "Container root (VE_ROOT) is not set");
		return VZ_VE_ROOT_NOTSET;
	}
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to setup checkpointing: "
			"container is not running");
		return VZ_VE_NOT_RUNNING;
	}

	return h->env_chkpnt(h, veid, fs, cmd, param);
}

int vps_restore(vps_handler *h, envid_t veid, vps_param *vps_p, int cmd,
	cpt_param *param, skipFlags skip)
{
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to perform restore: "
			"container already running");
		return VZ_VE_RUNNING;
	}

	return h->env_restore(h, veid, vps_p, cmd, param, skip);
}
