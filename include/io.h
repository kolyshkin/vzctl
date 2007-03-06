/*
 *  Copyright (C) 2007 SWsoft. All rights reserved.
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

#define VE_IOPRIO_MIN		0
#define VE_IOPRIO_MAX		7
#define IOPRIO_WHO_UBC		1000

#define IOPRIO_CLASS_SHIFT	13
#define IOPRIO_CLASS_BE		2

extern int ve_ioprio_set(vps_handler *h, envid_t veid,
				io_param *io_param, vps_param *vps_p);
