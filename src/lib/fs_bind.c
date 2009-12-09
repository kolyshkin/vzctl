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
#include <sys/mount.h>
#include <sys/vfs.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "fs.h"
#include "logger.h"
#include "vzerror.h"

/*  Check is fs mounted
 *  return: 1 - yes
 *	    0 - no
 *	  < 0 - error
 */
int vz_fs_is_mounted(const char *root)
{
	FILE *fp;
	char buf[512];
	char mnt[512];
	int ret = 0;

	if ((fp = fopen("/proc/mounts", "r")) == NULL) {
		logger(-1, errno,  "unable to open /proc/mounts");
		return -1;
	}
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if (sscanf(buf, "%*[^ ] %s ", mnt) != 1)
			continue;
		if (!strcmp(mnt, root)) {
			ret = 1;
			break;
		}
	}
	fclose(fp);
	return ret;
}

int vz_mount(fs_param *fs, int remount)
{
	int mntopt = MS_BIND;

	if (fs->noatime == YES)
		mntopt |= MS_NOATIME;
	if (remount)
		mntopt |= MS_REMOUNT;
	logger(2, 0,  "Mounting root: %s %s", fs->private, fs->root);

	if (mount(fs->private, fs->root, "none", mntopt, NULL) < 0)
	{
		logger(-1, errno, "Can't mount: %s %s",
			fs->private, fs->root);
		return VZ_FS_CANTMOUNT;
	}
	return 0;
}
