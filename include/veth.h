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
#ifndef	_VETH_H_
#define	_VETH_H_

#include "list.h"

#define IFNAMSIZE	16
#define ETH_ALEN	6
#define VZNETCFG	"/usr/sbin/vznetcfg"

/** Data structure for devices.
 */
typedef struct {
	list_elem_t list;		/**< next element. */
	char dev_addr[ETH_ALEN];	/**< device MAC address. */
	int addrlen;			/**< device MAC address length. */
	char dev_name[IFNAMSIZE];	/**< device name. */
	char dev_addr_ve[ETH_ALEN];	/**< device MAC address in VE. */
	int addrlen_ve;			/**< VE device MAC address length. */
	char dev_name_ve[IFNAMSIZE];	/**< device name in VE. */
	int flags;
} veth_dev;

/** Devices list.
 */
typedef struct {
	list_head_t dev;
} veth_param;

/** Create/remove veth devices for VE.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @return		0 on success.
 */
struct vps_param;
int vps_setup_veth(vps_handler *h, envid_t veid, struct vps_param *param);
int add_veth_param(veth_param *list, veth_dev *dev);
void free_veth_param(veth_param *dev);
int copy_veth_param(veth_param *dst, veth_param *src);
int merge_veth_list(list_head_t *old, list_head_t *add, list_head_t *del,
	veth_param *merged);
#endif	/* _VETH_H_ */
