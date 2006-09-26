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

#include <linux/vzcalluser.h>
#include <linux/vzctl_veth.h>

#include "vzerror.h"
#include "util.h"
#include "veth.h"
#include "env.h"
#include "logger.h"

static void free_veth(list_head_t *head);

static int veth_dev_create(vps_handler *h, envid_t veid, veth_dev *dev)
{
	struct vzctl_ve_hwaddr veth;
	int ret;

	if (!dev->dev_name[0] || dev->addrlen != ETH_ALEN)
		return EINVAL;
	if (dev->addrlen_ve != 0 && dev->addrlen_ve != ETH_ALEN)
		return EINVAL;
	veth.op = VE_ETH_ADD;
	veth.veid = veid;
	veth.addrlen = dev->addrlen;
	veth.addrlen_ve = dev->addrlen_ve;
	memcpy(veth.dev_addr, dev->dev_addr, ETH_ALEN);
	memcpy(veth.dev_addr_ve, dev->dev_addr_ve, ETH_ALEN);
	memcpy(veth.dev_name, dev->dev_name, IFNAMSIZE);
	memcpy(veth.dev_name_ve, dev->dev_name_ve, IFNAMSIZE);
	ret = ioctl(h->vzfd, VETHCTL_VE_HWADDR, &veth);
	if (ret) {
		if (errno == ENOTTY) {
			logger(-1, 0, "Warning: veth feature is"
				" not supported by kernel, skipped"
				" veth configure");
			ret = 0;
		} else {
			logger(-1, errno, "Unable to create veth");
			ret = VZ_VETH_ERROR;
		}
	}
	return ret;
}

static int veth_dev_remove(vps_handler *h, envid_t veid, veth_dev *dev)
{
	struct vzctl_ve_hwaddr veth;
	int ret;

	if (!dev->dev_name[0])
		return EINVAL;
	veth.op = VE_ETH_DEL;
	veth.veid = veid;
	memcpy(veth.dev_name, dev->dev_name, IFNAMSIZE);
	ret = ioctl(h->vzfd, VETHCTL_VE_HWADDR, &veth);
	if (ret) {
		if (errno != ENODEV) {
			logger(-1, errno, "Unable to remove veth");
			ret = VZ_VETH_ERROR;
		} else
			ret = 0;
	}
	return ret;
}

/** Create/remove veth devices for VE.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param dev		devices list.
 * @return		0 on success.
 */
static int veth_ctl(vps_handler *h, envid_t veid, int op, veth_param *list,
	int rollback)
{
	int ret = 0;
	veth_dev *tmp;
	list_head_t *dev_h = &list->dev;

	if (list_empty(dev_h))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to %s veth: VE is not running",
			op == ADD ? "create" : "remove");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Processing veth devices");
	list_for_each(tmp, dev_h, list) {
		if (op == ADD) {
			if ((ret = veth_dev_create(h, veid, tmp)))
				break;
		} else {
			if ((ret = veth_dev_remove(h, veid, tmp))) 
				break;
		}
		tmp->flags = 1;
	}
	/* If operation failed remove added devices. 
	 * Remove devices from list to skip saving.
         */
	if (ret && rollback) {
		list_for_each(tmp, dev_h, list) {
			if (op == ADD && tmp->flags == 1)
				veth_dev_remove(h, veid, tmp);
		}
		free_veth(dev_h);
	}
	return ret;
}

int vps_setup_veth(vps_handler *h, envid_t veid, vps_param *param)
{
	int ret;

        if ((ret = veth_ctl(h, veid, DEL, &param->del_res.veth, 0)))
                return ret;
        if ((ret = veth_ctl(h, veid, ADD, &param->res.veth, 1)))
                return ret;
	return 0;
}

int add_veth_param(veth_param *list, veth_dev *dev)
{
	veth_dev *tmp;

	if (list_is_init(&list->dev))
		list_head_init(&list->dev);
	tmp = malloc(sizeof(*tmp));
	if (tmp == NULL)
		return -1;
	memcpy(tmp, dev, sizeof(*tmp));
	list_add_tail(&tmp->list, &list->dev);

	return 0;
}

veth_dev *find_veth(list_head_t *head, veth_dev *dev)
{
	veth_dev *tmp;

	if (list_empty(head))
		return NULL;
	list_for_each(tmp, head, list) {
		if (!strcmp(tmp->dev_name, dev->dev_name))
			return dev;
	}
	return NULL;
}

int merge_veth_list(list_head_t *old, list_head_t *add, list_head_t *del,
	veth_param *merged)
{
	veth_dev *dev;

	list_for_each(dev, old, list) {
		/* Skip old devices that was added or deleted */
		if (find_veth(del, dev) != NULL ||
			find_veth(add, dev) != NULL)
		{
			continue;
		}
		/* Add old devices */
		if (add_veth_param(merged, dev))
			return 1;
	}
	list_for_each(dev, add, list) {
		/* Add new devices */
		if (add_veth_param(merged, dev))
			return 1;
	}
	return 0;
}

static void free_veth(list_head_t *head)
{
	veth_dev *cur;

	while (!list_empty(head)) {
		list_for_each(cur, head, list) {
			list_del(&cur->list);
			free(cur);
			break;
		}
	}
	list_head_init(head);
}

void free_veth_param(veth_param *dev)
{
	free_veth(&dev->dev);
}

int copy_veth_param(veth_param *dst, veth_param *src)
{
	veth_dev *dev;

	list_for_each(dev, &src->dev, list) {
		if (add_veth_param(dst, dev))
			return 1;
	}
	return 0;
}
