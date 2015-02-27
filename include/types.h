/*
 *  Copyright (C) 2000-2011, Parallels, Inc. All rights reserved.
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
#ifndef	_TYPES_H_
#define	_TYPES_H_

#define GLOBAL_CFG		PKGCONFDIR "/vz.conf"
#define OSRELEASE_CFG		PKGCONFDIR "/osrelease.conf"
#define DIST_DIR		PKGCONFDIR "/dists"
#define VENAME_DIR		PKGCONFDIR "/names"

#define VZCTLDEV		"/dev/vzctl"

#define VZFIFO_FILE		"/.vzfifo"

#define VPS_NET_ADD		SCRIPTDIR "/vps-net_add"
#define VPS_NET_DEL		SCRIPTDIR "/vps-net_del"
#define VPS_NETNS_DEV_ADD	SCRIPTDIR "/vps-netns_dev_add"
#define VPS_NETNS_DEV_DEL	SCRIPTDIR "/vps-netns_dev_del"
#define VPS_PCI			SCRIPTDIR "/vps-pci"
#define VPS_PRESTART		SCRIPTDIR "/vps-prestart"

/* User-defined scripts are in VPSCONFDIR */
#define USER_CREATE_SCRIPT	VPSCONFDIR "/vps.create"

#ifndef __ENVID_T_DEFINED__
typedef unsigned envid_t;
#define __ENVID_T_DEFINED__
#endif

#define STR_SIZE	512
#define PATH_LEN	256

#define NONE		0
#define YES		1
#define NO		2

#define CFG		2
#define ADD		0
#define DEL		1

/* Default environment variable PATH */
#define DEF_PATH	"/usr/local/sbin:/usr/local/bin:" \
			"/usr/sbin:/usr/bin:/sbin:/bin"
#define ENV_PATH	"PATH=" DEF_PATH

/* Defaults for checkpointing */
#define DEF_DUMPDIR	VZDIR "/dump"
#define DEF_DUMPFILE	"Dump.%d"

/* CT states */
enum {
	STATE_STARTING = 1,
	STATE_RUNNING = 2,
	STATE_STOPPED = 3,
	STATE_STOPPING = 4,
	STATE_RESTORING = 5,
	STATE_CHECKPOINTING = 6,
};

/* Parse error codes */
#define ERR_DUP		-1
#define ERR_INVAL	-2
#define ERR_UNK		-3
#define ERR_NOMEM	-4
#define ERR_OTHER	-5
#define ERR_INVAL_SKIP	-6
#define ERR_LONG_TRUNC  -7

struct arg_start;
struct fs_param;
struct arg_start;
struct ub_struct;
struct dev_res;
struct cpu_param;
struct veth_dev;
struct meminfo_param;
struct cpt_param;
struct vps_param;
struct fs_param;

typedef enum {
	SKIP_NONE =		0,
	SKIP_SETUP =		(1<<0),
	SKIP_CONFIGURE =	(1<<1),
	SKIP_ACTION_SCRIPT =	(1<<2),
	SKIP_UMOUNT =		(1<<3),
	SKIP_REMOUNT =		(1<<4),
	SKIP_DUMPFILE_UNLINK =	(1<<5),
	SKIP_VETH_CREATE =	(1<<6),
	SKIP_FSCK =		(1<<7),
} skipFlags;

/** CT handler.
 */
typedef struct vps_handler {
	int vzfd;	/**< /dev/vzctl file descriptor. */
	int stdfd;
	int can_join_pidns; /* can't enter otherwise */
	int can_join_userns; /* can't run non privileged otherwise */
	int (*is_run)(struct vps_handler *h, envid_t veid);
	int (*enter)(struct vps_handler *h, envid_t veid, const char *root, int flags);
	int (*destroy)(struct vps_handler *h, envid_t veid);
	int (*env_create)(struct arg_start *arg);
	int (*env_chkpnt)(struct vps_handler *h, envid_t veid,
			  const struct fs_param *fs, int cmd, struct cpt_param *param);
	int (*env_restore)(struct vps_handler *h, envid_t veid, struct vps_param *vps_p,
			   int cmd, struct cpt_param *param, skipFlags skip);
	int (*setlimits)(struct vps_handler *h, envid_t, struct ub_struct *ub);
	int (*setcpus)(struct vps_handler *h, envid_t, struct cpu_param *cpu);
	int (*setcontext)(envid_t veid);
	int (*setdevperm)(struct vps_handler *h, envid_t veid, struct dev_res *res);
	int (*netdev_ctl)(struct vps_handler *h, envid_t veid, int op, char *name);
	int (*ip_ctl)(struct vps_handler *h, envid_t veid, int op, const char *ipstr);
	int (*veth_ctl)(struct vps_handler *h, envid_t veid, int op, struct veth_dev *veth);
} vps_handler;

static inline int is_vz_kernel(vps_handler *h)
{
	return h && h->vzfd != -1;
}

typedef int (* execFn)(void *data);

/* Some kernels use a few IDs close to INT_MAX (2147483647) */
#define VEID_MAX	2147483644

#endif
