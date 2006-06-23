/*
 *  Copyright (C) 2000-2006 SWsoft. All rights reserved.
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
#include "cpt.h"
#include "meminfo.h"
#include "veth.h"

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
typedef struct vps_res {
	fs_param fs;		/**< file system parameters. */
	tmpl_param tmpl;	/**< template parameters. */
	env_param env;		/**< environment parameters. */
	net_param net;		/**< network parameters. */
	cpu_param cpu;		/**< cpu parameters. */
	dev_param dev;		/**< device parameters. */
	ub_param ub;		/**< UBC aprameters. */
	cap_param cap;		/**< capability parameters. */
	dq_param dq;		/**< disk quota parameters. */
	cpt_param cpt;		/**< chekpointing parameters */
	meminfo_param meminfo;
	veth_param veth;	/**< veth parameters */
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
	int reset_ub;
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
int vps_cleanup_res(vps_handler *h, envid_t veid, vps_param *param,
	int vps_state);
int setup_resource_management(vps_handler *h, envid_t veid, vps_res *res);
int need_configure(vps_res *res);

#endif
