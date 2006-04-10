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
#include "vzerror.h"
#include "logger.h"

#ifndef __NR_setublimit
#ifdef __ia64__
#define __NR_setublimit	1507
#elif __x86_64__
#define __NR_setublimit	502
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
	{"KMEMSIZE",	UB_KMEMSIZE},
	{"LOCKEDPAGES",	UB_LOCKEDPAGES},
	{"PRIVVMPAGES",	UB_PRIVVMPAGES},
	{"SHMPAGES",	UB_SHMPAGES},
	{"NUMPROC",	UB_NUMPROC},
	{"PHYSPAGES",	UB_PHYSPAGES},
	{"VMGUARPAGES",	UB_VMGUARPAGES},
	{"OOMGUARPAGES",UB_OOMGUARPAGES},
	{"NUMTCPSOCK",	UB_NUMTCPSOCK},
	{"NUMFLOCK",	UB_NUMFLOCK},
	{"NUMPTY",	UB_NUMPTY},
	{"NUMSIGINFO",	UB_NUMSIGINFO},
	{"TCPSNDBUF",	UB_TCPSNDBUF},
	{"TCPRCVBUF",	UB_TCPRCVBUF},
	{"OTHERSOCKBUF",UB_OTHERSOCKBUF},
	{"DGRAMRCVBUF",	UB_DGRAMRCVBUF},
	{"NUMOTHERSOCK",UB_NUMOTHERSOCK},
	{"NUMFILE",	UB_NUMFILE}, 
	{"DCACHESIZE",	UB_DCACHESIZE},
	{"NUMIPTENT",	UB_IPTENTRIES},
	{"AVNUMPROC",	UB_DUMMY},
	{NULL, 0},
};

/** Check that all required parameters are specified in ub.
 *
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int check_ub(ub_param *ub)
{
	int i, ret = 0;

	for (i = 0; ubname2id[i].name != NULL; i++) {
		int j, found = 0;
		if (ubname2id[i].id == UB_DUMMY)
			continue;
		for (j = 0; j < ub->num_res; j++)
			if (ub->ub[j].res_id == ubname2id[i].id) {
				found = 1;
				break;
		}
		if (!found) {
			logger(0, 0, "UB parameter %s not set",
				ubname2id[i].name);
			ret = VZ_NOTENOUGHUBCPARAMS;
		}
	}
	return ret;
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

unsigned long *get_ub_limit(ub_param *param, char *name)
{
	int id, i;

	if ((id = get_ub_resid(name)) < 0)
		return NULL;
	for (i = 0; i < param->num_res; i++) {
		if (param->ub[i].res_id == id)
			return param->ub[i].limit;
	}
	return NULL;
}

static inline int _set_ublimit(int veid, unsigned long res_id,
	unsigned long *rlim)
{
	if (setublimit(veid, res_id, rlim) == -1)
		return VZ_SETUBC_ERROR;
	return 0;
}

int set_ublimit(vps_handler *h, envid_t veid, ub_param *ubc)
{
	int i, ret;

	if (!ubc->num_res)
		return 0;
	for (i = 0; i < ubc->num_res; i++) {
		ub_res *ub = ubc->ub;
		if (ub[i].res_id == UB_DUMMY)
			continue;
		if ((ret = _set_ublimit(veid, ub[i].res_id, ub[i].limit))) {
			logger(0, errno, "setublimit %d %lu:%lu failed",
				ub[i].res_id, ub[i].limit[0], ub[i].limit[1]);
			return ret;
		}
	}
        return 0;
}

/** Apply UBC resources.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param ubc		UBC parameters
 * @return		0 on success
 */
int vps_set_ublimit(vps_handler *h, envid_t veid, ub_param *ubc)
{
	int ret;

	if (!ubc->num_res)
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to apply UBC parameters,"
			" VPS is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if ((ret = set_ublimit(h, veid, ubc)))
		return ret;
	logger(0, 0, "UB limits were set successefully");
	return 0;
		

}

int add_ub_param(ub_param *ub, ub_res *res)
{
	if (ub->ub == NULL)
		ub->num_res = 0;
	ub->ub = realloc(ub->ub, sizeof(ub_res) * (ub->num_res + 1));
	if (ub->ub == NULL)
		return -1;
	memcpy(&ub->ub[ub->num_res], res, sizeof(ub_res));
	ub->num_res++;
	
	return 0;
}

void free_ub_param(ub_param *ub)
{
	if (!ub->num_res)
		return;
	free(ub->ub);
	ub->ub = NULL;
	ub->num_res = 0;
}

ub_res *get_ub_res(ub_param *ub, int res_id)
{
	int i;

	if (!ub->num_res)
		return NULL;
	for (i = 0; i < ub->num_res; i++) 
		if (ub->ub[i].res_id == res_id)
			return &ub->ub[i];

	return NULL;
}

void merge_ub(ub_param *dst, ub_param *src)
{
	int i;

	if (!src->num_res)
		return;
	for (i = 0; i < src->num_res; i++) {
		ub_res *res_p;

		if ((res_p = get_ub_res(dst, src->ub[i].res_id)) == NULL) {
			add_ub_param(dst, &src->ub[i]);
		} else {
			memcpy(res_p, &src->ub[i], sizeof(ub_res));
		}
	}	
}

void free_ubs_limit(struct ub_struct *ubs)
{
#define FREE_P(x) if (ubs->x != NULL) {free(ubs->x); ubs->x = NULL;}
	if (ubs == NULL)
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
}

void add_ubs_limit(struct ub_struct *ubs, int res_id, unsigned long *limit)
{

	switch (res_id) {
	case UB_KMEMSIZE:
		ubs->kmemsize = limit;
		break;
	case UB_LOCKEDPAGES:
		ubs->lockedpages = limit;
		break;
	case UB_PRIVVMPAGES:
		ubs->privvmpages = limit;
		break;
	case UB_SHMPAGES:
		ubs->shmpages = limit;
		break;
	case UB_NUMPROC:
		ubs->numproc = limit;
		break;
	case UB_PHYSPAGES:
		ubs->physpages = limit;
		break;
	case UB_VMGUARPAGES:
		ubs->vmguarpages = limit;
		break;
	case UB_OOMGUARPAGES:
		ubs->oomguarpages = limit;
		break;
	case UB_NUMTCPSOCK:
		ubs->numtcpsock = limit;
		break;
	case UB_NUMFLOCK:
		ubs->numflock = limit;
		break;
	case UB_NUMPTY:
		ubs->numpty = limit;
		break;
	case UB_NUMSIGINFO:
		ubs->numsiginfo = limit;
		break;
	case UB_TCPSNDBUF:
		ubs->tcpsndbuf = limit;
		break;
	case UB_TCPRCVBUF:
		ubs->tcprcvbuf = limit;
		break;
	case UB_OTHERSOCKBUF:
		ubs->othersockbuf = limit;
		break;
	case UB_DGRAMRCVBUF:
		ubs->dgramrcvbuf = limit;
		break;
	case UB_NUMOTHERSOCK:
		ubs->numothersock = limit;
		break;
	case UB_NUMFILE:
		ubs->numfile = limit;
		break;
	case UB_DCACHESIZE:
		ubs->dcachesize = limit;
		break;
	case UB_IPTENTRIES:
		ubs->numiptent = limit;
		break;
	case UB_DUMMY:
		/* UB_DUMMY: is alias for AVNUMPROC */
		ubs->avnumproc = limit;
		break;
	}
}

void ub2ubs(ub_param *ub, struct ub_struct *ubs)
{
	int i;

	memset(ubs, 0, sizeof(*ubs));
	for (i = 0; i < ub->num_res; i++) {
		ub_res *res = &ub->ub[i];
		add_ubs_limit(ubs, res->res_id, res->limit);
	}
}

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

