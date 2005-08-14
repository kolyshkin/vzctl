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
#ifndef	_NET_H_
#define	_NET_H_

#include <linux/config.h>
#if defined(CONFIG_VZ_LIST) || defined(CONFIG_VZ_LIST_MODULE)
#include <linux/vzlist.h>
#if defined(VZCTL_GET_VEIDS) && defined(VZCTL_GET_VEIPS)
#define HAVE_VZLIST_IOCTL 1
#endif
#endif

#include "list.h"
#include "types.h"
#include "dist.h"

typedef struct str_struct net_dev_param;
typedef struct str_struct ip_param;

/** Data structure for network parameters.
 */
typedef struct {
	list_head_t ip;	/**< VPS ip adresses list. */
	list_head_t dev;	/**< VPS network devices list. */
	int delall;		/**< flag to delete all ip addresses. */
	int skip_route_cleanup;
	
} net_param;

/** Setup VPS network.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param op		operation (ADD|DEL).
 * @param net		network parameters.
 * @param actions	distribution action scripts.
 * @param root		VPS root.
 * @param state		VPS state (STATE_STARTING|STATE_RUNNING).
 * @retun		0 on success.
 */
int vps_net_ctl(vps_handler *h, envid_t veid, int op, net_param *net,
	dist_actions *actions, char *root, int state);

/** Setup access to Host system network devices.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param op		operation (ADD|DEL).
 * @param net		network parameters.
 * @return		0 on success.
 */
int vps_netdev_ctl(vps_handler *h, envid_t veid, int op, net_param *net);

int find_ip(list_head_t *ip_h,  char *ipaddr);

/** Abtain ip list belong to VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param ip_h		ip list head.
 * @return		0 on success.
 */
int get_vps_ip(vps_handler *h, envid_t veid, list_head_t *ip_h);
int run_net_script(envid_t veid, int op, list_head_t *ip_h, int state);
#endif
