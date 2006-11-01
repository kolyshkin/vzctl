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
#ifndef	_NET_H_
#define	_NET_H_

/* #include <linux/vzlist.h> */
#define HAVE_VZLIST_IOCTL 0

#include "list.h"
#include "types.h"
#include "dist.h"

typedef struct str_struct net_dev_param;
typedef struct str_struct ip_param;

/** Data structure for network parameters.
 */
typedef struct {
	list_head_t ip;		/**< VE IP adresses list. */
	list_head_t dev;	/**< VE network devices list. */
	int delall;		/**< flag to delete all ip addresses. */
	int skip_arpdetect;
	int skip_route_cleanup;
	int ipv6_net;
	
} net_param;

/** Setup VE network.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param op		operation (ADD|DEL).
 * @param net		network parameters.
 * @param actions	distribution action scripts.
 * @param root		VE root.
 * @param state		VE state (STATE_STARTING|STATE_RUNNING).
 * @retun		0 on success.
 */
int vps_net_ctl(vps_handler *h, envid_t veid, int op, net_param *net,
	dist_actions *actions, char *root, int state, int skip_arpdetect);

/** Setup access to Host system network devices.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param op		operation (ADD|DEL).
 * @param net		network parameters.
 * @return		0 on success.
 */
int vps_netdev_ctl(vps_handler *h, envid_t veid, int op, net_param *net);

int find_ip(list_head_t *ip_h,  char *ipaddr);

/** Obtain list of IP addresses belonging to the VE.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param ip_h		IP list head.
 * @return		0 on success.
 */
int get_vps_ip(vps_handler *h, envid_t veid, list_head_t *ip_h);
int run_net_script(envid_t veid, int op, list_head_t *ip_h, int state,
	int skip_arpdetect);
#endif
