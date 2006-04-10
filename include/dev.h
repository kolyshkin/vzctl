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
#ifndef	_DEV_H_
#define	_DEV_H_

#include "list.h"

#define DEV_MODE_READ		1
#define DEV_MODE_WRITE		2

/** Data structure for devices.
 */
typedef struct {
	list_elem_t list;		/**< next element. */
	char name[32];			/**< device name. */
	unsigned int dev;		/**< device number. */
	unsigned int type;		/**< S_IFBLK | S_IFCHR. */
	unsigned int mask;		/**< access mode. */
	int use_major;			/**< VE_USE_MAJOR | VE_USE_MINOR. */
} dev_res;

/** Devices list.
 */
typedef struct {
	list_head_t dev;
} dev_param;

/** Allow/disallow access to devices on host system from VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param root		VPS root.
 * @param dev		devices list.
 * @return		0 on success.
 */
int vps_set_devperm(vps_handler *h, envid_t veid, char *root, dev_param *dev);
int set_devperm(vps_handler *h, envid_t veid, dev_res *dev);
int add_dev_param(dev_param *dev, dev_res *res);
void free_dev_param(dev_param *dev);

#endif
