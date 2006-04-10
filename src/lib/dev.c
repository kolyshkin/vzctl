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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#include <asm/timex.h>
#include <linux/vzcalluser.h>

#include "vzerror.h"
#include "util.h"
#include "dev.h"
#include "env.h"
#include "logger.h"

static int dev_create(char *root, dev_res *dev)
{
	char buf1[STR_SIZE];
	char buf2[STR_SIZE];
	struct stat st;
	int ret;

	if (!dev->name[0])
		return 0;
	if (check_var(root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	/* If device does not exist inside VE get
	* information from VE0 and create it
	*/
	snprintf(buf1, sizeof(buf1), "%s/dev/%s", root, dev->name);
	ret = lstat(buf1, &st);
	if (ret && errno != ENOENT) {
		logger(0, errno, "Unable to stat device %s", buf1);
		return VZ_SET_DEVICES;
	} else if (!ret)
		return 0;
	snprintf(buf2, sizeof(buf2), "/dev/%s", dev->name);
	if (lstat(buf2, &st)) {
		if (errno == ENOENT)
			logger(0, 0, "Incorrect name or no such device %s",
				buf2);
		else
			logger(0, errno, "Unable to stat  device %s", buf2);
		return VZ_SET_DEVICES;
	}
	if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) {
		logger(0, 0, "The %s is not block or character device", buf2);
		return VZ_SET_DEVICES;
	}
	if (make_dir(buf1, 0))
		return VZ_SET_DEVICES;
	if (mknod(buf1, st.st_mode, st.st_rdev)) {
		logger(0, errno, "Unable to create device %s", buf1);
		return VZ_SET_DEVICES;
	}
	return 0;
}

int set_devperm(vps_handler *h, envid_t veid, dev_res *dev)
{
	int ret;
	struct vzctl_setdevperms devperms;

	devperms.veid = veid;
	devperms.dev = dev->dev;
	devperms.mask = dev->mask;
	devperms.type = dev->type;

	if ((ret = ioctl(h->vzfd, VZCTL_SETDEVPERMS, &devperms)))
		logger(0, errno, "Unable to set devperms");

	return ret;
}

/** Allow/disallow access to devices on host system from VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param root		VPS root.
 * @param dev		devices list.
 * @return		0 on success.
 */
int vps_set_devperm(vps_handler *h, envid_t veid, char *root, dev_param *dev)
{
	int ret = 0;
	dev_res *res;
	list_head_t *dev_h = &dev->dev;

	if (list_empty(dev_h))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to apply devperm: VPS is not running");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Setting devices");
	list_for_each(res, dev_h, list) {
		if (res->name[0])
			if ((ret = dev_create(root, res)))
				break;
		if ((ret = set_devperm(h, veid, res))) 
			break;
	}
	return ret;
}

int add_dev_param(dev_param *dev, dev_res *res)
{
	dev_res *tmp;

	if (list_is_init(&dev->dev))
		list_head_init(&dev->dev);
	tmp = malloc(sizeof(*tmp));
	if (tmp == NULL)
		return -1;
	memcpy(tmp, res, sizeof(*tmp));
	list_add_tail(&tmp->list, &dev->dev);

	return 0;
}

static void free_dev(list_head_t  *head)
{
	dev_res *cur;

	while(!list_empty(head)) {
		list_for_each(cur, head, list) {
			list_del(&cur->list);
			free(cur);
			break;
		}
	}
	list_head_init(head);
}

void free_dev_param(dev_param *dev)
{
	free_dev(&dev->dev);
}
