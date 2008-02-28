/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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
#ifndef _VZCTL_VENET_H
#define _VZCTL_VENET_H

#include <linux/types.h>
#include <linux/ioctl.h>

#ifndef __ENVID_T_DEFINED__
typedef unsigned envid_t;
#define __ENVID_T_DEFINED__
#endif

struct vzctl_ve_ip_map {
	envid_t veid;
	int op;
#define VE_IP_ADD	1
#define VE_IP_DEL	2
	struct sockaddr *addr;
	int addrlen;
};

#define VENETCTLTYPE '('

#define VENETCTL_VE_IP_MAP	_IOW(VENETCTLTYPE, 3,			\
					struct vzctl_ve_ip_map)

#endif
