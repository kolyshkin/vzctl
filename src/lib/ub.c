/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

#include "types.h"
#include "ub.h"
#include "env.h"
#include "vzctl_param.h"
#include "vzerror.h"
#include "logger.h"
#include "vzsyscalls.h"
#include "util.h"


static struct ubname2id {
	char *name;
	unsigned int id;
} ubname2id[] = {
	{"KMEMSIZE",	PARAM_KMEMSIZE},
	{"LOCKEDPAGES",	PARAM_LOCKEDPAGES},
	{"PRIVVMPAGES",	PARAM_PRIVVMPAGES},
	{"SHMPAGES",	PARAM_SHMPAGES},
	{"NUMPROC",	PARAM_NUMPROC},
	{"PHYSPAGES",	PARAM_PHYSPAGES},
	{"VMGUARPAGES",	PARAM_VMGUARPAGES},
	{"OOMGUARPAGES",PARAM_OOMGUARPAGES},
	{"NUMTCPSOCK",	PARAM_NUMTCPSOCK},
	{"NUMFLOCK",	PARAM_NUMFLOCK},
	{"NUMPTY",	PARAM_NUMPTY},
	{"NUMSIGINFO",	PARAM_NUMSIGINFO},
	{"TCPSNDBUF",	PARAM_TCPSNDBUF},
	{"TCPRCVBUF",	PARAM_TCPRCVBUF},
	{"OTHERSOCKBUF",PARAM_OTHERSOCKBUF},
	{"DGRAMRCVBUF",	PARAM_DGRAMRCVBUF},
	{"NUMOTHERSOCK",PARAM_NUMOTHERSOCK},
	{"NUMFILE",	PARAM_NUMFILE},
	{"DCACHESIZE",	PARAM_DCACHESIZE},
	{"NUMIPTENT",	PARAM_NUMIPTENT},
	{"AVNUMPROC",	PARAM_AVNUMPROC},
	{"SWAPPAGES",	PARAM_SWAPPAGES},
	{NULL, 0},
};

/** Check that all required UBC parameters are specified.
 *
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int check_ub(vps_handler *h, ub_param *ub)
{
	int ret = 0;

#define CHECK_UB(name)							\
if (ub->name == NULL) {							\
	logger(-1, 0, "Error: required UB parameter " #name " not set");\
	ret = VZ_NOTENOUGHUBCPARAMS;					\
}

	if (is_vswap_config(ub))
	{
		CHECK_UB(physpages);
		CHECK_UB(swappages);
		if (is_vz_kernel(h) && !is_vswap_mode()) {
			logger(-1, 0, "Error: detected vswap CT config but "
					"kernel does not support vswap");
			logger(-1, 0, "This means either old kernel "
					"or bad config (physpages NOT set "
					"to 0:unlimited)");
			ret = VZ_BAD_KERNEL;
		}
		return ret;
	}
	/* else
	 * non-vswap case, require all params except swappages */
	CHECK_UB(kmemsize)
	CHECK_UB(lockedpages)
	CHECK_UB(privvmpages)
	CHECK_UB(shmpages)
	CHECK_UB(numproc)
	CHECK_UB(physpages)
	CHECK_UB(vmguarpages)
	CHECK_UB(oomguarpages)
	CHECK_UB(numtcpsock)
	CHECK_UB(numflock)
	CHECK_UB(numpty)
	CHECK_UB(numsiginfo)
	CHECK_UB(tcpsndbuf)
	CHECK_UB(tcprcvbuf)
	CHECK_UB(othersockbuf)
	CHECK_UB(dgramrcvbuf)
	CHECK_UB(numothersock)
	CHECK_UB(numfile)
	CHECK_UB(dcachesize)
	CHECK_UB(numiptent)
#undef CHECK_UB

	return ret;
}

inline static int is_ub_empty(ub_param *ub)
{
#define CHECK_UB(name)	if (ub->name != NULL) return 0;
	FOR_ALL_UBC(CHECK_UB)
#undef CHECK_UB

	return 1;
}

int get_ub_resid(char *name)
{
	int i;

	for (i = 0; ubname2id[i].name != NULL; i++)
		if (!strcasecmp(name, ubname2id[i].name))
			return ubname2id[i].id;
	return -1;
}

#ifdef VZ_KERNEL_SUPPORTED
static inline int setublimit(uid_t uid, unsigned long resource,
	const unsigned long *rlim)
{
	return syscall(__NR_setublimit, uid, resource, rlim);
}

static const char *get_ub_name(unsigned int res_id)
{
	int i;

	for (i = 0; ubname2id[i].name != NULL; i++)
		if (ubname2id[i].id == res_id)
			return ubname2id[i].name;
	return NULL;
}

int set_ublimit(vps_handler *h, envid_t veid, ub_param *ub)
{
	const unsigned long *res;

#define SET_UB_LIMIT(name, id)						\
res = ub->name;								\
if (res != NULL) {							\
	if (setublimit(veid, id, res)) {				\
		logger(-1, errno, "setublimit %s %lu:%lu failed",	\
			get_ub_name(id), res[0], res[1]);		\
		return VZ_SETUBC_ERROR;					\
	}								\
}

	SET_UB_LIMIT(kmemsize, UB_KMEMSIZE)
	SET_UB_LIMIT(lockedpages, UB_LOCKEDPAGES)
	SET_UB_LIMIT(privvmpages, UB_PRIVVMPAGES)
	SET_UB_LIMIT(shmpages, UB_SHMPAGES)
	SET_UB_LIMIT(numproc, UB_NUMPROC)
	SET_UB_LIMIT(physpages, UB_PHYSPAGES)
	SET_UB_LIMIT(vmguarpages, UB_VMGUARPAGES)
	SET_UB_LIMIT(oomguarpages, UB_OOMGUARPAGES)
	SET_UB_LIMIT(numtcpsock, UB_NUMTCPSOCK)
	SET_UB_LIMIT(numflock, UB_NUMFLOCK)
	SET_UB_LIMIT(numpty, UB_NUMPTY)
	SET_UB_LIMIT(numsiginfo, UB_NUMSIGINFO)
	SET_UB_LIMIT(tcpsndbuf, UB_TCPSNDBUF )
	SET_UB_LIMIT(tcprcvbuf, UB_TCPRCVBUF)
	SET_UB_LIMIT(othersockbuf, UB_OTHERSOCKBUF)
	SET_UB_LIMIT(dgramrcvbuf, UB_DGRAMRCVBUF)
	SET_UB_LIMIT(numothersock, UB_NUMOTHERSOCK)
	SET_UB_LIMIT(numfile, UB_NUMFILE)
	SET_UB_LIMIT(dcachesize, UB_DCACHESIZE)
	SET_UB_LIMIT(numiptent, UB_IPTENTRIES)
	if (ub->swappages &&
	    setublimit(veid, UB_SWAPPAGES, ub->swappages) == -1)
	{
		if (errno == EINVAL) {
			logger(-1, ENOSYS, "failed to set swappages");
		} else {
			logger(-1, errno, "failed to set swappages");
			return VZ_SETUBC_ERROR;
		}
	}
#undef SET_UB_LIMIT

	return 0;
}
#endif

/** Apply UBC resources.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param ubc		UBC parameters
 * @return		0 on success
 */
int vps_set_ublimit(vps_handler *h, envid_t veid, ub_param *ub)
{
	int ret;

	if (is_ub_empty(ub))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to apply UBC parameters: "
			"container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if ((ret = h->setlimits(h, veid, ub)))
		return ret;
	logger(0, 0, "UB limits were set successfully");
	return 0;
}

void free_ub_param(ub_param *ub)
{
#define FREE_P(x) free(ub->x); ub->x = NULL;
	if (ub == NULL)
		return;
	FOR_ALL_UBC(FREE_P)
	FREE_P(vm_overcommit)
#undef FREE_P
}

void add_ub_limit(struct ub_struct *ub, int res_id, unsigned long *limit)
{
#define ADD_UB_PARAM(name, id)					\
if (res_id == id) {						\
	ub->name = limit;					\
	return;							\
}

	ADD_UB_PARAM(kmemsize, PARAM_KMEMSIZE)
	ADD_UB_PARAM(lockedpages, PARAM_LOCKEDPAGES)
	ADD_UB_PARAM(privvmpages, PARAM_PRIVVMPAGES)
	ADD_UB_PARAM(shmpages, PARAM_SHMPAGES)
	ADD_UB_PARAM(numproc, PARAM_NUMPROC)
	ADD_UB_PARAM(physpages, PARAM_PHYSPAGES)
	ADD_UB_PARAM(vmguarpages, PARAM_VMGUARPAGES)
	ADD_UB_PARAM(oomguarpages, PARAM_OOMGUARPAGES)
	ADD_UB_PARAM(numtcpsock, PARAM_NUMTCPSOCK)
	ADD_UB_PARAM(numflock, PARAM_NUMFLOCK)
	ADD_UB_PARAM(numpty, PARAM_NUMPTY)
	ADD_UB_PARAM(numsiginfo, PARAM_NUMSIGINFO)
	ADD_UB_PARAM(tcpsndbuf, PARAM_TCPSNDBUF)
	ADD_UB_PARAM(tcprcvbuf, PARAM_TCPRCVBUF)
	ADD_UB_PARAM(othersockbuf, PARAM_OTHERSOCKBUF)
	ADD_UB_PARAM(dgramrcvbuf, PARAM_DGRAMRCVBUF)
	ADD_UB_PARAM(numothersock, PARAM_NUMOTHERSOCK)
	ADD_UB_PARAM(numfile, PARAM_NUMFILE)
	ADD_UB_PARAM(dcachesize, PARAM_DCACHESIZE)
	ADD_UB_PARAM(numiptent, PARAM_NUMIPTENT)
	ADD_UB_PARAM(avnumproc, PARAM_AVNUMPROC)
	ADD_UB_PARAM(swappages, PARAM_SWAPPAGES)
#undef ADD_UB_PARAM
}

/** Add UBC resource in ub_res format to UBC struct
 *
 * @param ub		UBC parameters.
 * @param res		UBC resource in ub_res format.
 * @return		0 on success.
 */
int add_ub_param(ub_param *ub, ub_res *res)
{
	unsigned long *limit;

	limit = malloc(sizeof(*limit) * 2);
	if (limit == NULL)
		return ERR_NOMEM;
	limit[0] = res->limit[0];
	limit[1] = res->limit[1];

	add_ub_limit(ub, res->res_id, limit);

	return 0;
}

/** Set secondary beancounters in vswap mode,
 *  if they were not set explicitly,
 *  based on ram, swap and overcommit.
 *
 *  @param cfg		UBC parameters from CT config
 *  @param cmd		UBC parameters from command line
 */
int fill_vswap_ub(ub_param *cfg, ub_param *cmd)
{
	float ovc = 0;
	unsigned long ram, swap;

#define SET(param, bar, lim) do {					\
	if (!cmd->param && !cfg->param) {				\
		cmd->param = vz_malloc(sizeof(unsigned long)*2);	\
		if (!cmd->param)					\
			goto enomem;					\
		cmd->param[0] = (bar);					\
		cmd->param[1] = (lim);					\
	} } while (0)

	if (!is_vswap_config(cfg) && !is_vswap_config(cmd))
		return 0;

	ram = (cmd->physpages ? : cfg->physpages)[1];
	if (cmd->swappages)
		swap = cmd->swappages[1];
	else if (cfg->swappages)
		swap = cfg->swappages[1];
	else {/* swap not set, but required */
		logger(-1, 0, "Error: required UB parameter (swap) not set");
		return VZ_NOTENOUGHUBCPARAMS;
	}

	if (cmd->vm_overcommit)
		ovc = *cmd->vm_overcommit;
	else if (cfg->vm_overcommit)
		ovc = *cfg->vm_overcommit;

	SET(lockedpages, ram, ram);
	SET(oomguarpages, ram, LONG_MAX);
	SET(vmguarpages, ram + swap, LONG_MAX);
	if (ovc)
		SET(privvmpages, (ram + swap) * ovc, (ram + swap) * ovc);
	else
		SET(privvmpages, LONG_MAX, LONG_MAX);
#undef SET

	return 0;

enomem:
	return VZ_RESOURCE_ERROR;
}

/** Read UBC resources current usage from /proc/user_beancounters
 *
 * @param veid		CT ID.
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int vps_read_ubc(envid_t veid, ub_param *ub)
{
	FILE *fd;
	char str[STR_SIZE];
	char name[64];
	const char *fmt = NULL; /* make gcc happy */
	int ret, found;
	envid_t id;
	unsigned long held, maxheld, barrier, limit;
	ub_res res;

	fd = fopen(PROCUBC, "r");
	if (fd == NULL) {
		logger(-1, errno, "Unable to open " PROCUBC);
		return -1;
	}
	found = 0;
	while (fgets(str, sizeof(str), fd)) {
		if ((ret = sscanf(str, "%d:", &id)) == 1) {
			if (id == veid) {
				fmt =  "%*lu:%s%lu%lu%lu%lu";
				found = 1;
			} else {
				if (found)
					break;
			}
		} else {
			fmt = "%s%lu%lu%lu%lu";
		}
		if (!found)
			continue;
		if ((ret = sscanf(str, fmt,
			name, &held, &maxheld, &barrier, &limit)) != 5)
		{
			continue;
		}
		if ((res.res_id = get_ub_resid(name)) >= 0) {
			res.limit[0] = held;
			res.limit[1] = held;
			add_ub_param(ub, &res);
		}
	}
	fclose(fd);
	return !found;
}

/* If you want to modify this function, don't forget print_vswap() */
int is_vswap_config(const ub_param *param)
{
	/* Dirty hack: treat INT_MAX (i.e. 32 bit LONG_MAX) as unlimited.
	 * Required for old CTs migrated from 32 bit to 64 bit host.
	 */
	return  (param != NULL) &&
		(param->physpages != NULL) &&
		(param->physpages[1] != LONG_MAX) &&
		(param->physpages[1] != INT_MAX);
}
