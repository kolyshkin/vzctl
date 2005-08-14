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
#ifndef	_CREATE_H_
#define	_CREATE_H_
#include "res.h"

#define VPS_CREATE	VPS_SCRIPTS_DIR "vps-create"

struct mod_action;

int vps_postcreate(envid_t veid, fs_param *fs, tmpl_param *tmpl);
int vps_create(vps_handler *h, envid_t veid, vps_param *vps_p, vps_param *cmd_p,
	struct mod_action *action);
int vps_destroy(vps_handler *h, envid_t veid, fs_param *fs);
int fs_create(envid_t veid, fs_param *fs, tmpl_param *tmpl, dq_param *dq,
	char *tar_nm);
#endif

