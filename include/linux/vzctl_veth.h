/*
 *  include/linux/vzctl_veth.h
 *
 *  Copyright (C) 2006-2008, Parallels, Inc. All rights reserved.
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
 *
 */

#ifndef _VZCTL_VETH_H
#define _VZCTL_VETH_H

#include <linux/types.h>
#include <linux/ioctl.h>

#ifndef __ENVID_T_DEFINED__
typedef unsigned envid_t;
#define __ENVID_T_DEFINED__
#endif

struct vzctl_ve_hwaddr {
	envid_t veid;
	int op;
#define VE_ETH_ADD			1
#define VE_ETH_DEL			2
#define VE_ETH_ALLOW_MAC_CHANGE		3
#define VE_ETH_DENY_MAC_CHANGE		4
	unsigned char	dev_addr[6];
	int addrlen;
	char		dev_name[16];
	unsigned char	dev_addr_ve[6];
	int addrlen_ve;
	char		dev_name_ve[16];
};

#define VETHCTLTYPE '['

#define VETHCTL_VE_HWADDR	_IOW(VETHCTLTYPE, 3,			\
					struct vzctl_ve_hwaddr)

#endif
