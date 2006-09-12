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

#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "types.h"
#include "ub.h"
#include "env.h"
#include "vzctl_param.h"
#include "vzerror.h"
#include "logger.h"

#ifndef __NR_setublimit
#ifdef __ia64__
#define __NR_setublimit	1507
#elif __x86_64__
#define __NR_setublimit	502
#elif __powerpc__
#define __NR_setublimit 412
#else
#define __NR_setublimit	512
#endif
#endif

static inline int setublimit(uid_t uid, unsigned long resource,
        unsigned long *rlim)
{
	return syscall(__NR_setublimit, uid, resource, rlim);
}

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
	{NULL, 0},
};

/** Check that all required UBC parameters are specified.
 *
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int check_ub(ub_param *ub)
{
	int ret = 0;

#define CHECK_UB(name)							\
if (ub->name == NULL) {							\
	logger(0, 0, "UB parameter " #name " not set");			\
	ret = VZ_NOTENOUGHUBCPARAMS;					\
}

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

const char *get_ub_name(int res_id)
{
	int i;

	for (i = 0; ubname2id[i].name != NULL; i++)
		if (ubname2id[i].id == res_id)
			return ubname2id[i].name;
        return NULL;
}

int set_ublimit(vps_handler *h, envid_t veid, ub_param *ub)
{
	
#define SET_UB_LIMIT(name, id)						\
if (ub->name != NULL) {							\
	if (setublimit(veid, id, ub->name)) {				\
		logger(0, errno, "setublimit %s %lu:%lu failed",	\
			get_ub_name(id), ub->name[0], ub->name[1]);	\
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
#undef SET_UB_LIMIT

	return 0;
}

/** Apply UBC resources.
 *
 * @param h		VE handler.
 * @param veid		VE id.
 * @param ubc		UBC parameters
 * @return		0 on success
 */
int vps_set_ublimit(vps_handler *h, envid_t veid, ub_param *ub)
{
	int ret;

	if (is_ub_empty(ub))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to apply UBC parameters: "
			"VE is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if ((ret = set_ublimit(h, veid, ub)))
		return ret;
	logger(0, 0, "UB limits were set successefully");
	return 0;
}

void free_ub_param(ub_param *ub)
{
#define FREE_P(x) if (ub->x != NULL) {free(ub->x); ub->x = NULL;}
	if (ub == NULL)
		return;
	FREE_P(kmemsize)
	FREE_P(lockedpages)
	FREE_P(privvmpages)
	FREE_P(shmpages)
	FREE_P(numproc)
	FREE_P(physpages)
	FREE_P(vmguarpages)
	FREE_P(oomguarpages)
	FREE_P(numtcpsock)
	FREE_P(numflock)
	FREE_P(numpty)
	FREE_P(numsiginfo)
	FREE_P(tcpsndbuf)
	FREE_P(tcprcvbuf)
	FREE_P(othersockbuf)
	FREE_P(dgramrcvbuf)
	FREE_P(numothersock)
	FREE_P(numfile)
	FREE_P(dcachesize)
	FREE_P(numiptent)
	FREE_P(avnumproc)
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
#undef ADD_UB_PARAM
}

/** Add UBC resource in ub_res format to UBC struct
 *
 * @param ub            UBC parameters.
 * @param res           UBC resource in ub_res format.
 * @return              0 on success.
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

void merge_ub(ub_param *dst, ub_param *src)
{
#define MERGE_P2(x)						\
if ((src->x) != NULL) { 					\
	if ((dst->x) == NULL) 					\
		dst->x = malloc(sizeof(*(dst->x)) * 2);	 	\
	dst->x[0] = src->x[0];					\
	dst->x[1] = src->x[1];					\
}

	MERGE_P2(kmemsize)
	MERGE_P2(lockedpages)
	MERGE_P2(privvmpages)
	MERGE_P2(shmpages)
	MERGE_P2(numproc)
	MERGE_P2(physpages)
	MERGE_P2(vmguarpages)
	MERGE_P2(oomguarpages)
	MERGE_P2(numtcpsock)
	MERGE_P2(numflock)
	MERGE_P2(numpty)
	MERGE_P2(numsiginfo)
	MERGE_P2(tcpsndbuf)
	MERGE_P2(tcprcvbuf)
	MERGE_P2(othersockbuf)
	MERGE_P2(dgramrcvbuf)
	MERGE_P2(numothersock)
	MERGE_P2(numfile)
	MERGE_P2(dcachesize)
	MERGE_P2(numiptent)
	MERGE_P2(avnumproc)
#undef MERGE_P2
}

/** Read UBC resources current usage from /proc/user_beancounters
 *
 * @param veid		VE id.
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int vps_read_ubc(envid_t veid, ub_param *ub)
{
	FILE *fd;
	char str[STR_SIZE];
	char name[64];
	const char *fmt;
	int ret, found, id;
	unsigned long held, maxheld, barrier, limit;
	ub_res res;

	fd = fopen(PROCUBC, "r");
	if (fd == NULL) {
		logger(0, errno, "Unable to open " PROCUBC);
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

