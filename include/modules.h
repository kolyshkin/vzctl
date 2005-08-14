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
#ifndef	_MODULES_H_
#define _MODULES_H_

#include "types.h"
#include "res.h"

#define	TYPE_CMD	0
#define	TYPE_CONF	1

#define MOD_DIR		"/usr/lib/vzctl/modules/"

#define MOD_INFO_SYM    "vz_get_mod_info"

#define MOD_IDBITS	16
#define MOD2ID(id)	(id << MOD_IDBITS)
#define MOD_PARAM2ID(m_id, p_id)				\
			(MOD2ID(m_id) | (0xffff & p_id))
#define PARAM2MOD(x)	(x >> MOD_IDBITS)

#define GET_MOD_INFO(name)		\
struct mod_info *vz_get_mod_info()	\
{					\
	return &name;			\
}

typedef struct {
	void *cfg;
	void *opt;
} data_param;

struct mod_info {
	void *handle;
	char **actions;
	int id;
	char *desc;
	data_param *(*alloc_data)(void);
	int (*init)(data_param *data);
	int (*parse_cfg)(envid_t veid, data_param *data, const char *name,
		const char *rval);
	int (*parse_opt)(envid_t veid, data_param *data, int opt,
		const char *rval);
	int (*store)(data_param *data, list_head_t *conf_head);
	int (*setup)(vps_handler *h, envid_t veid, data_param *data,
		int vps_state, int skip, vps_param *param);
	int (*cleanup)(vps_handler *h, envid_t veid, data_param *data,
		vps_param *param);
	void (*free_data)(data_param *data);
	struct option *(*get_opt)(data_param *data, char *action);
	const char *(*get_usage)();
};

struct mod {
	char *fname;
	data_param *data;
	struct mod_info *mod_info;
};

struct mod_action {
	int mod_count;
	char *action;
	struct mod *mod_list;
};

struct option *mod_make_opt(struct option *opt, struct mod_action *action,
	const char *name);
int mod_save_config(struct mod_action *action, list_head_t *conf);
int mod_parse(envid_t veid, struct mod_action *action, const char *name,
	int opt, const char *rval);
int mod_setup(vps_handler *h, envid_t veid, int vps_state, skipFlags skip,
	struct mod_action *action, vps_param *param);
int mod_cleanup(vps_handler *h, envid_t veid, struct mod_action *action,
	vps_param *param);
void mod_print_usage(struct mod_action *action);

#endif	/* _MODULES_H_ */
