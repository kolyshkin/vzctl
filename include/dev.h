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
