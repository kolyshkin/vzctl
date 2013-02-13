/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

#include <sys/types.h>
#include "list.h"

#define DEV_MODE_READ		1
#define DEV_MODE_WRITE		2

/** Data structure for devices.
 */
typedef struct dev_res {
	list_elem_t list;		/**< prev/next elements. */
	char *name;			/**< device name. */
	dev_t dev;			/**< device number. */
	unsigned int type;		/**< S_IFBLK | S_IFCHR. */
	unsigned int mask;		/**< access mode. */
	int use_major;			/**< VE_USE_MAJOR | VE_USE_MINOR. */
} dev_res;

/** Devices list.
 */
typedef struct {
	list_head_t dev;
} dev_param;

typedef struct {
	list_head_t list;
} pci_param;

int create_static_dev(const char *root, const char *name, const char *alias,
		mode_t mode, dev_t dev);

/** Allow/disallow access to devices on host system from CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param root		CT root.
 * @param dev		devices list.
 * @return		0 on success.
 */
int vps_set_devperm(vps_handler *h, envid_t veid, const char *root,
		dev_param *dev);
int set_devperm(vps_handler *h, envid_t veid, dev_res *dev);
int add_dev_param(dev_param *dev, dev_res *res);
void free_dev_param(dev_param *dev);

int vps_set_pci(vps_handler *h, envid_t veid, int op, const char *root,
		pci_param *pci);

#endif
