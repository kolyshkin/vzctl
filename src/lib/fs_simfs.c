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
int vz_fs_is_mounted(char *root)
{
	FILE *fp;
	char buf[512];
	char mnt[512];
	int ret = 0; 

	if ((fp = fopen("/proc/mounts", "r")) == NULL) {
		logger(0, errno,  "unable to open /proc/mounts");
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
	int mntopt = 0;

        if (fs->noatime == YES)
                mntopt |= MS_NOATIME;
        if (remount)
		mntopt |= MS_REMOUNT;
        logger(2, 0,  "Mounting root: %s %s", fs->root, fs->private);
        if (mount(fs->private, fs->root, "simfs", mntopt,
			remount ? "" : fs->private) < 0)
	{
                logger(0, errno, "Can't mount: %s %s", fs->root, fs->private);
                return VZ_FS_CANTMOUNT;
        }
	return 0;
}
