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
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <getopt.h>
#include "res.h"
#include "list.h"
#include "modules.h"

typedef struct str_struct conf_struct;

typedef struct vps_config {
	char *name;	/* VPS config parameters name */
	char *alias;	/* alias name for parameter */
	int id;		/* command line option id */
} vps_config;

int vps_parse_config(envid_t veid, char *path, vps_param *vps_p,
	struct mod_action *action);
int vps_parse_opt(envid_t veid, vps_param *param, int opt,
	const char *rval, struct mod_action *action);
int vps_save_config(envid_t veid, char *path, vps_param *new_p,
	vps_param *old_p, struct mod_action *action);

vps_param *init_vps_param();
ub_res *get_ub_res(ub_param *ub, int res_id);
int check_ub(ub_param *ub);
int merge_vps_param(vps_param *dst, vps_param *src);
int merge_global_param(vps_param *dst, vps_param *src);
void free_vps_param(vps_param *param);
const vps_config *conf_get_by_name(const vps_config *conf, const char *name);
const vps_config *conf_get_by_id(const vps_config *conf, int id);
int opt_get_by_id(struct option *opt, int id);
struct option *get_set_opt(void);
int conf_parse_yesno(int *dst, const char *val, int checkdup);
int conf_parse_strlist(list_head_t *list, const char *val, int checkdup);

#endif
