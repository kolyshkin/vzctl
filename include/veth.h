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
#ifndef	_VETH_H_
#define	_VETH_H_

#include "list.h"
#include "dist.h"

#define IFNAMSIZE	16
#define ETH_ALEN	6
#define MAC_SIZE	3*ETH_ALEN - 1
#define VZNETCFG	SBINDIR "/vznetcfg"

#define PROC_VETH	"/proc/vz/veth"

#define SW_OUI		0x001851
#define DEF_VETHNAME	"veth$VEID"


/** Data structure for devices.
 */
typedef struct veth_dev {
	list_elem_t list;		/**< prev/next element. */
	char dev_addr[ETH_ALEN];	/**< device MAC address. */
	int addrlen;			/**< device MAC address length. */
	char dev_name[IFNAMSIZE];	/**< device name. */
	char dev_addr_ve[ETH_ALEN];	/**< device MAC address in CT. */
	int addrlen_ve;			/**< CT device MAC address length. */
	char dev_name_ve[IFNAMSIZE];	/**< device name in CT. */
	int flags;
	int active;
	int configure;
	int mac_filter;
	char dev_bridge[IFNAMSIZE];	/**< bridge name. */
} veth_dev;

/** Devices list.
 */
typedef struct {
	list_head_t dev;
	int delall;
} veth_param;

/** Create/remove veth devices for CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @return		0 on success.
 */
struct vps_param;
int vps_setup_veth(vps_handler *h, envid_t veid, dist_actions *actions,
	const char *root, veth_param *veth_add, veth_param *veth_del,
	int state, int skip);
int add_veth_param(veth_param *list, veth_dev *dev);
void free_veth_param(veth_param *dev);
int copy_veth_param(veth_param *dst, veth_param *src);
int merge_veth_list(list_head_t *old, list_head_t *add, list_head_t *del,
	veth_param *merged);
void generate_mac(int veid, char *dev_name, char *mac);
void free_veth_dev(veth_dev *dev);
veth_dev *find_veth_by_ifname_ve(list_head_t *head, char *name);
veth_dev *find_veth_configure(list_head_t *head);
int merge_veth_list(list_head_t *old, list_head_t *add, list_head_t *del,
	veth_param *merged);
int parse_hwaddr(const char *str, char *addr);
int check_veth_param(envid_t veid, veth_param *veth_old, veth_param *veth_new,
	veth_param *veth_del);

#endif	/* _VETH_H_ */
