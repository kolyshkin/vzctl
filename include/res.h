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
#ifndef	_RES_H_
#define _RES_H_

#include "types.h"
#include "net.h"
#include "cpu.h"
#include "dev.h"
#include "ub.h"
#include "cap.h"
#include "quota.h"
#include "fs.h"
#include "dist.h"

typedef struct {
	envid_t veid;
	unsigned long ipt_mask;
} env_param;

typedef struct {
	list_head_t userpw;
	list_head_t nameserver;
	list_head_t searchdomain;
	char *hostname;
} misc_param;

struct mod_action;

/** Data structure for VPS resources.
 */

typedef struct {
	fs_param fs;		/**< file system parameters. */
	tmpl_param tmpl;	/**< template parameters. */
	env_param env;		/**< environment parameters. */
	net_param net;		/**< network parameters. */
	cpu_param cpu;		/**< cpu parameters. */
	dev_param dev;		/**< device parameters. */
	ub_param ub;		/**< UBC aprameters. */
	cap_param cap;		/**< capability parameters. */
	dq_param dq;		/**< disk quota parameters. */
	misc_param misc;	
} vps_res;

typedef struct {
        int save;
        int fast_kill;
        int skip_lock;
        int skip_setup;
        int start_disabled;
        int start_force;
        int setmode;
        int onboot;
        char *config;
        char *origin_sample;
        char *lockdir;
        char *apply_cfg;
} vps_opt;

struct log_s {
        char *log_file;
        int level;
        int enable;
};

struct vps_param {
	struct log_s log;
	vps_res res;
	vps_res del_res;
	vps_opt opt;
	struct mod_action *mod;
	struct vps_param *g_param;
};
typedef struct vps_param vps_param;

/** Setting VPS resources.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param actions	distribution action scripts.
 * @param fs		VPS FS parameters
 * @param param		VPS parameters.
 * @param vps_state	VPS state.
 * @param action	external modules.
 * @return		0 on success.
 */
int vps_setup_res(vps_handler *h, envid_t veid, dist_actions *actions,
	fs_param *fs, vps_param *param, int vps_state, skipFlags skip,
	struct mod_action *action);

/** Setup user password
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param actions	distribution action scripts.
 * @param root		VPS root.
 * @param pw		list of user:password parameters.
 * @return		0 on success.
 */
int vps_pw_configure(vps_handler *h, envid_t veid, dist_actions *actions,
	char *root, list_head_t *pw);
int need_configure(vps_res *res);
#endif
