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
#ifndef	_VPS_CONFIGURE_H_
#define _VPS_CONFIGURE_H_
#include "res.h"

/** Setup user password
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param actions	distribution action scripts.
 * @param root		CT root.
 * @param pw		list of user:password parameters.
 * @return		0 on success.
 */
int vps_pw_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	const char *root, list_head_t *pw);
int vps_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	const fs_param *fs, vps_param *param, int state);
int vps_ip_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	const char *root, int op, net_param *net, int state);
const char *state2str(int state);
#endif
