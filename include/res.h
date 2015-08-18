/*
 *  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
#include "meminfo.h"
#include "veth.h"

struct env_param {
	envid_t veid;
	unsigned long long ipt_mask;
	unsigned long nf_mask;	/* netfilter */

	/*
	 * features_known is the set of features that are present
	 * in config file. this make vzctl work with new kernel
	 * that supports features vzctl still doesn't know yet.
	 * features that kernel support and that are not present
	 * in this mask kernel sets to default values.
	 */
	unsigned long long features_mask;
	unsigned long long features_known;
	char *osrelease;
};
typedef struct env_param env_param_t;

typedef struct {
	list_head_t userpw;
	list_head_t nameserver;
	list_head_t searchdomain;
	char *hostname;
	char *description;
	int onboot;
	unsigned long *bootorder;
	int wait;
	unsigned long *local_uid;
	unsigned long *local_gid;
	int stop_timeout;
} misc_param;

struct mod_action;

typedef struct {
	int ioprio;
	int iolimit;
	int iopslimit;
} io_param;

typedef struct name_param {
	int veid;
	char *name;
} name_param;

typedef struct cpt_param {
	char *dumpdir;
	char *dumpfile;
	unsigned int ctx;
	unsigned int cpu_flags;
	int cmd;
	int rst_fd;
} cpt_param;

/** Data structure for CT resources.
 */
typedef struct vps_res {
	fs_param fs;		/**< file system parameters. */
	tmpl_param tmpl;	/**< template parameters. */
	env_param_t env;	/**< environment parameters. */
	net_param net;		/**< network parameters. */
	cpu_param cpu;		/**< cpu parameters. */
	dev_param dev;		/**< device parameters. */
	ub_param ub;		/**< UBC parameters. */
	cap_param cap;		/**< capability parameters. */
	dq_param dq;		/**< disk quota parameters. */
	cpt_param cpt;		/**< chekpointing parameters */
	meminfo_param meminfo;
	veth_param veth;	/**< veth parameters */
	misc_param misc;
	name_param name;
	io_param io;
	pci_param pci;
} vps_res;

enum {
	APPCONF_MAP_NAME = 0x01,
};

enum {
	VE_LAYOUT_SIMFS = 1,
	VE_LAYOUT_PLOOP = 2,
};

typedef struct {
	int save;
	int save_force;
	int fast_kill;
	int skip_lock;
	int skip_setup;
	int start_disabled;
	int start_force;
	int setmode;
	char *config;
	char *origin_sample;
	char *lockdir;
	char *apply_cfg;
	int apply_cfg_map;
	int reset_ub;
	int skip_umount;
	int skip_fsck;
	int skip_remount;
} vps_opt;

struct log_s {
	char *log_file;
	int level;
	int enable;
	int *verbose;
};

struct vzctl_snapshot_param {
	char *guid;
	char *name;
	char *desc;
#define SNAPSHOT_SKIP_SUSPEND	1 << 1
#define SNAPSHOT_SKIP_CONFIG	1 << 2
#define SNAPSHOT_SKIP_RESUME	1 << 3
#define SNAPSHOT_MUST_RESUME	1 << 4
	int flags;
	char *target;	/* snapshot mount point */
};

struct vps_param {
	struct log_s log;
	vps_res res;
	vps_res del_res;
	vps_opt opt;
	struct vzctl_snapshot_param snap;
	struct mod_action *mod;
	struct vps_param *g_param;
};
typedef struct vps_param vps_param;

/** Setting CT resources.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param actions	distribution action scripts.
 * @param fs		CT FS parameters
 * @param ub		CT UBC parameters (if NULL taken from param)
 * @param param		CT parameters.
 * @param vps_state	CT state.
 * @param action	external modules.
 * @return		0 on success.
 */
int vps_setup_res(vps_handler *h, envid_t veid, dist_actions *actions,
	fs_param *fs, ub_param *ub, vps_param *param,
	int vps_state, skipFlags skip,
	struct mod_action *action);
int need_configure(vps_res *res);

#endif
