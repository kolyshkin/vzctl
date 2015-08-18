/*
 *  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <getopt.h>
#include "res.h"
#include "list.h"
#include "modules.h"

typedef struct str_struct conf_struct;

typedef struct vps_config {
	char *name;	/* CT config parameters name */
	char *alias;	/* alias name for parameter */
	int id;		/* command line option id */
} vps_config;

int vps_parse_config(envid_t veid, const char *path, vps_param *vps_p,
	struct mod_action *action);
int vps_parse_opt(envid_t veid, struct option *opts, vps_param *param,
	int opt, char *rval, struct mod_action *action);
int vps_save_config(envid_t veid, const char *path, vps_param *new_p,
	vps_param *old_p, struct mod_action *action);
vps_param *reread_vps_config(envid_t veid);

vps_param *init_vps_param();
ub_res *get_ub_res(ub_param *ub, int res_id);
int merge_vps_param(vps_param *dst, vps_param *src);
int merge_global_param(vps_param *dst, vps_param *src);
void free_vps_param(vps_param *param);
int get_veid_by_name(const char *name);
int set_name(int veid, char *new_name, char *old_name);
int save_ve_layout(int veid, vps_param *param, int layout);

#endif
