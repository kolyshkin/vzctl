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
#ifndef _MEMINFO_H_
#define _MEMINFO_H_

typedef struct meminfo_param {
	int mode;
#define VE_MEMINFO_NONE 0
#define VE_MEMINFO_PAGES 1
#define VE_MEMINFO_PRIVVMPAGES 2
	unsigned long val;
} meminfo_param;

struct vps_param;
int vps_meminfo_set(vps_handler *h, envid_t veid, meminfo_param *param,
	struct vps_param *vps_p, int state);
int get_meminfo_mode(char *name);
const char *get_meminfo_mode_nm(int id);
#endif /* _MEMINFO_H_ */
