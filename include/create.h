/*
 *  Copyright (C) 2000-2007 SWsoft. All rights reserved.
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
#ifndef	_CREATE_H_
#define	_CREATE_H_
#include "res.h"

#define VPS_CREATE	LIB_SCRIPTS_DIR "vps-create"

struct mod_action;

int vps_postcreate(envid_t veid, fs_param *fs, tmpl_param *tmpl);
int vps_create(vps_handler *h, envid_t veid, vps_param *vps_p, vps_param *cmd_p,
	struct mod_action *action);
int vps_destroy(vps_handler *h, envid_t veid, fs_param *fs);
int fs_create(envid_t veid, fs_param *fs, tmpl_param *tmpl, dq_param *dq,
	char *tar_nm);
#endif

