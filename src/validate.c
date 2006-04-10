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

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <ub/beancounter.h>

#include "res.h"
#include "validate.h"
#include "vzerror.h"
#include "logger.h"
#include "util.h"

static int page_size = 4096;

static struct {
	char *name;
	int id;
} validate_act2str[] = {
	{"none",	ACT_NONE},
	{"warning",	ACT_WARN},
	{"error",	ACT_ERROR},
	{"fix",		ACT_FIX},
};

int action2id(char *mode)
{
	int i;

	if (mode == NULL)
		return ACT_NONE;
	for (i = 0; i < sizeof(validate_act2str) / sizeof(*validate_act2str);
		i++)
	{
		if (!strcmp(validate_act2str[i].name, mode))
			return validate_act2str[i].id;
	}
	return ACT_NONE;
}



static int read_yn()
{
	char buf[1024];

	fprintf(stderr, " (y/n) [y] ");
	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		if (buf[0] == 'y' || buf[0] == '\n') {
			return 1;
		}
		else if (buf[0] == 'n')
			return 	0;
		fprintf(stderr, " (y/n) [y] ");
	} 
	return 0;
}

static int check_param(struct ub_struct *param, int log)
{
	int ret = 0;	
#define CHECKPARAM(name) 						\
	if (param->name == NULL) {					\
		if (log)						\
			logger(0, 0, "Error: parameter " #name		\
				" not found\n");			\
		ret = 1;						\
	}								\

	CHECKPARAM(numproc);
	CHECKPARAM(numtcpsock);
	CHECKPARAM(numothersock);
	CHECKPARAM(oomguarpages)
	CHECKPARAM(vmguarpages);
	CHECKPARAM(kmemsize);
	CHECKPARAM(tcpsndbuf);
	CHECKPARAM(tcprcvbuf);
	CHECKPARAM(othersockbuf);
	CHECKPARAM(dgramrcvbuf);
	CHECKPARAM(privvmpages);
	CHECKPARAM(numfile);
	CHECKPARAM(dcachesize);
	CHECKPARAM(physpages)
	CHECKPARAM(numpty)

	return ret;
}

int validate(vps_res *param, int recover, int ask)
{
	unsigned long avnumproc;
	int ret = 0;
	unsigned long val, val1;
	unsigned long tmp_val0, tmp_val1;
	int changed = 0;
	struct ub_struct ubs;

#define SET_MES(val)	logger(0, 0, "set to %lu", val);
#define SET2_MES(val1, val2) logger(0, 0,"set to %lu:%lu", val1, val2);

#define CHECK_BL(x, name) \
if (x != NULL) { 						\
	if (x[0] > x[1]) {				\
		logger(0, 0, "Error: barrier should be <= limit for " \
			#name " (currently, %lu:%lu)",			\
			x[0], x[1]);		\
		if (ask || recover) {					\
			tmp_val1 = x[0];			\
			tmp_val0 = x[0];			\
			SET2_MES(tmp_val0, tmp_val1)			\
			if (ask) recover = read_yn();			\
			if (recover) {					\
				x[1] = tmp_val1;		\
			       	changed++;				\
			}						\
		}							\
		if (!recover) ret = 1;					\
	}								\
} else {								\
	logger(0, 0, "Error: parameter "  #name " not found\n");	\
	ret = 1;							\
}

#define CHECK_B(name)							\
if (ubs.name != NULL) {						\
	if ((ubs.name[0] != ubs.name[1])) {			\
		logger(0, 0, "Error: barrier should be equal limit for " \
			#name " (currently, %lu:%lu)",			\
			ubs.name[0], ubs.name[1]);		\
		if (ask || recover) {					\
			tmp_val0 = max_ul(ubs.name[0], ubs.name[1]); \
			tmp_val1 = tmp_val0;				\
			SET2_MES(tmp_val0, tmp_val1)			\
			if (ask) recover = read_yn();			\
			if (recover) {					\
				ubs.name[0] = tmp_val0;		\
				ubs.name[1] = tmp_val1;		\
				changed++;				\
			}						\
		}							\
		if (!recover) ret = 1;					\
	}								\
} else {								\
	logger(0, 0, "Error: parameter "  #name " not found\n");	\
	ret = 1;							\
}

	if (param == NULL)
		return 1;
	ub2ubs(&param->ub, &ubs);
	if (check_param(&ubs, 1))
		return 1;
	if (ubs.avnumproc != NULL)
		avnumproc = ubs.avnumproc[0];
	else
		avnumproc = ubs.numproc[0] / 2;
/*	1 Check barrier & limit	*/
	/* Primary */
	CHECK_B(numproc)
	CHECK_B(numtcpsock)
	CHECK_B(numothersock)
	if (ubs.vmguarpages != NULL) {
		if (ubs.vmguarpages[1] != LONG_MAX) {
			logger(0, 0, "Error: limit should be = %lu for"
			       " vmguarpages (currently, %lu)", LONG_MAX,
				ubs.vmguarpages[1]);
			if (ask || recover) {
				SET_MES((unsigned long) LONG_MAX);
				if (ask)
					recover = read_yn();
				if (recover) {
					ubs.vmguarpages[1] = LONG_MAX;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
	} else {
		logger(0, 0, "Error: parameter vmguarpages not found\n");
		ret = 1;
	}
	/* Secondary */
	CHECK_BL(ubs.kmemsize, kmemsize)
	CHECK_BL(ubs.tcpsndbuf, tcpsndbuf)
	CHECK_BL(ubs.tcprcvbuf, tcprcvbuf)
	CHECK_BL(ubs.othersockbuf, othersockbuf)
	CHECK_BL(ubs.dgramrcvbuf, dgramrcvbuf)
	if (ubs.oomguarpages != NULL) {
		if (ubs.oomguarpages[1] != LONG_MAX) {
			logger(0, 0, "Error: limit should be = %lu for"
			       " oomguarpages (currently, %lu)", LONG_MAX,
				ubs.oomguarpages[1]);
			if (ask || recover) {
				SET_MES((unsigned long) LONG_MAX);
				if (ask)
					recover = read_yn();
				if (recover) {
					ubs.oomguarpages[1] = LONG_MAX;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
	} else {
		logger(0, 0, "Error: parameter oomguarpages not found\n");
		ret = 1;
	}
	CHECK_BL(ubs.privvmpages, privvmpages)
	/* Auxiliary */
	CHECK_BL(ubs.lockedpages, lockedpages)
	CHECK_B(shmpages)
	if (ubs.physpages != NULL) {
		if (ubs.physpages[0] != 0) {
			logger(0, 0, "Error: barrier should be = 0 for"
			       " physpages (currently, %lu)",
				ubs.physpages[0]);
			if (ask || recover) {
				SET_MES((unsigned long) 0);
				if (ask)
					recover = read_yn();
				if (recover) {
					ubs.physpages[0] = 0;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
		if (ubs.physpages[1] != LONG_MAX) {
			logger(0, 0, "Error: limit should be = %lu for"
			       " physpages (currently, %lu)", LONG_MAX,
				ubs.physpages[1]);
			if (ask || recover) {
				SET_MES((unsigned long) LONG_MAX);
				if (ask)
					recover = read_yn();
				if (recover) {
					ubs.physpages[1] = LONG_MAX;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
	} else {
		logger(0, 0, "Error: parameter physpages not found\n");
		ret = 1;
	}
	CHECK_B(numfile)
	CHECK_BL(ubs.numflock, numflock)
	CHECK_B(numpty)
	CHECK_B(numsiginfo)
	CHECK_BL(ubs.dcachesize, dcachesize)
	CHECK_B(numiptent)
	CHECK_BL(param->dq.diskspace, diskspace)
	CHECK_BL(param->dq.diskinodes, diskinodes)

/*	2 Check formulas			*/
	val = (40 * 1024 * avnumproc) + ubs.dcachesize[1];
	val &= LONG_MAX;
	if (ubs.kmemsize[0] < val) {
		logger(0, 0, "Error: kmemsize.bar should be > %lu"
				" (currently, %lu)", val, ubs.kmemsize[0]);
		if (ask || recover) {
			tmp_val1 = ubs.kmemsize[1] + val - ubs.kmemsize[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1);
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.kmemsize[1] = tmp_val1;
				ubs.kmemsize[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	if (ubs.privvmpages[0] < ubs.vmguarpages[0]) {
		logger(0, 0, "Warning: privvmpages.bar should be > %lu"
				" (currently, %lu)", ubs.vmguarpages[0],
				ubs.privvmpages[0]);
		if (ask || recover) {
			tmp_val0 = ubs.vmguarpages[0];
			tmp_val1 = ubs.privvmpages[1] < tmp_val0 ?
				tmp_val0 : ubs.vmguarpages[1];
			SET_MES(tmp_val0);
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.privvmpages[0] = tmp_val0;
                                ubs.privvmpages[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 2.5 * 1024 * ubs.numtcpsock[0];
	val &= LONG_MAX;
	if (ubs.tcpsndbuf[1] - ubs.tcpsndbuf[0] < val) {
		logger(0, 0, "Error: tcpsndbuf.lim-tcpsndbuf.bar"
				" should be > %lu (currently, %lu)", val,
				ubs.tcpsndbuf[1]-ubs.tcpsndbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.tcpsndbuf[0] + val;
			tmp_val0 = ubs.tcpsndbuf[0];
			SET2_MES(tmp_val0, tmp_val1);
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.tcpsndbuf[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 2.5 * 1024 * ubs.numothersock[0];
	val &= LONG_MAX;
	if (ubs.othersockbuf[1] - ubs.othersockbuf[0] < val) {
		logger(0, 0, "Error: othersockbuf.lim-othersockbuf.bar"
			       " should be > %lu (currently, %lu)", val,
				ubs.othersockbuf[1]-ubs.othersockbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.othersockbuf[0] + val;
			tmp_val0 = ubs.othersockbuf[0];
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.othersockbuf[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val =  2.5 * 1024 * ubs.numtcpsock[0];
	val &= LONG_MAX;
	if (ubs.tcprcvbuf[1] - ubs.tcprcvbuf[0] < val) {
		logger(0, 0, "Warning: tcprcvbuf.lim-tcprcvbuf.bar"
			       " should be > %lu (currently, %lu)", val,
				ubs.tcprcvbuf[1] - ubs.tcprcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.tcprcvbuf[0] + val;
			tmp_val0 = ubs.tcprcvbuf[0];
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.tcprcvbuf[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 64 * 1024;
	if (ubs.tcprcvbuf[0] < val) {
		logger(0, 0, "Warning: tcprcvbuf.bar should be > %lu\n"
				" (currently, %lu)", val,
				ubs.tcprcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.tcprcvbuf[1]+val-ubs.tcprcvbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.tcprcvbuf[1] = tmp_val1;
				ubs.tcprcvbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 64 * 1024;
	if (ubs.tcpsndbuf[0] <  val) {
		logger(0, 0, "Warning: tcpsndbuf.bar should be > %lu"
				" (currently, %lu)", val,
				ubs.tcpsndbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.tcpsndbuf[1]+val-ubs.tcpsndbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.tcpsndbuf[1] = tmp_val1;
				ubs.tcpsndbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 32 * 1024;
	val1 = 129 * 1024;
	if (ubs.dgramrcvbuf[0] < val) {
		logger(0, 0, "Warning: dgramrcvbuf.bar should be >"
				" %lu (currently, %lu)", val,
				ubs.dgramrcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.dgramrcvbuf[1] + val - 
					ubs.dgramrcvbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.dgramrcvbuf[1] = tmp_val1;
				ubs.dgramrcvbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	} else if (ubs.dgramrcvbuf[0] < val1) {
		logger(0, 0, "Recommendation: dgramrcvbuf.bar should be >"
				" %lu (currently, %lu)", val1,
				ubs.dgramrcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.dgramrcvbuf[1] + val1 -
					ubs.dgramrcvbuf[0];
			tmp_val0 = val1;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.dgramrcvbuf[1] = tmp_val1;
				ubs.dgramrcvbuf[0] = tmp_val0;
				changed++;
			}
		}
//		if (!ask) fprintf(stderr, "\n");
	}
	val =  32 * 1024;
	val1 = 129 * 1024;
	if (ubs.othersockbuf[0] < val) {
		logger(0, 0, "Warning: othersockbuf.bar should be >"
				" %lu (currently, %lu)", val, 
				ubs.othersockbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.othersockbuf[1] + val - 
					ubs.othersockbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.othersockbuf[1] = tmp_val1;
				ubs.othersockbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	} else if (ubs.othersockbuf[0] < val1) {
		logger(0, 0,"Recommendation: othersockbuf.bar should be >"
				" %lu (currently, %lu)", val1, 
				ubs.othersockbuf[0]);
		if (ask || recover) {
			tmp_val1 = ubs.othersockbuf[1] + val1 - 
					ubs.othersockbuf[0];
			tmp_val0 = val1;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.othersockbuf[1] = tmp_val1;
				ubs.othersockbuf[0] = tmp_val0;
				changed++;
			}
		}
//		if (!ask) fprintf(stderr, "\n");
	}
	val = avnumproc * 32;
	val1 = ubs.numtcpsock[0] + ubs.numothersock[0] + ubs.numpty[0];
	if (val1 > val)
		val = val1;
	val &= LONG_MAX;
	if (ubs.numfile[0] < val) {
		logger(0, 0, "Warning: numfile should be > %lu"
				" (currently, %lu)", val, ubs.numfile[0]);
		if (ask || recover) {
			tmp_val1 = ubs.numfile[1] + val - ubs.numfile[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.numfile[1] = tmp_val1;
				ubs.numfile[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = ubs.numfile[0] * 384;
	val &= LONG_MAX;
	if (ubs.dcachesize[1] < val) {
		logger(0, 0, "Warning: dcachesize.lim should be > %lu"
				" (currently, %lu)", val,
				ubs.dcachesize[1]);
		if (ask || recover) {
			SET_MES(val);
			if (ask)
				recover = read_yn();
			if (recover) {
				ubs.dcachesize[1] = val;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}

	return ret;
}

int vps_validate(vps_res *param, int mode)
{
	int ret;

	if (mode == ACT_NONE)
		return 0;	
	logger(0, 0, "Validate VPS:");
	ret  = validate(param, mode == ACT_FIX, 0);
	if (mode == ACT_ERROR && ret)
		return VZ_VALIDATE_ERROR;
	return 0;
}

int calc_ve_utilization(struct ub_struct *param, struct CRusage *rusage,
	struct mem_struct *mem, int numerator)
{
	double kmem_net;

	memset(rusage, 0, sizeof(struct CRusage));
	if (param == NULL)
		return -1;
	if (check_param(param, 1))
		return -1;
	kmem_net = (double)param->kmemsize[0] + 
		(double)param->tcprcvbuf[0] + 
		(double)param->tcpsndbuf[0] +
		(double)param->dgramrcvbuf[0] +
		(double)param->othersockbuf[0];
	/*	Low memory	*/
	rusage->low_mem = kmem_net;
	if (!numerator)
		rusage->low_mem /= mem->lowmem;
	/*	Total RAM	*/
	rusage->total_ram = ((double)param->physpages[0] * page_size +
		kmem_net);
	if (!numerator)
		rusage->total_ram /= mem->ram;
	/*	Mem + Swap	*/
	rusage->mem_swap = ((double)param->oomguarpages[0] * page_size +
		kmem_net);
	if (!numerator)
			rusage->mem_swap /= (mem->ram + mem->swap);
	/*	Allocated memory	*/
	rusage->alloc_mem = ((double)param->privvmpages[0] * page_size +
		kmem_net);
	if (!numerator)
		rusage->alloc_mem /= (mem->ram + mem->swap);

	return 0;
}

int calc_ve_commitment(struct ub_struct *param, struct CRusage *rusage,
	struct mem_struct *mem, int numerator)
{
	double kmem_net;

	memset(rusage, 0, sizeof(struct CRusage));
	if (param == NULL)
		return -1;
	if (check_param(param, 1))
		return -1;
	kmem_net = (double)param->kmemsize[1] +
		(double)param->tcprcvbuf[1] +
		(double)param->tcpsndbuf[1] +
		(double)param->dgramrcvbuf[1] +
		(double)param->othersockbuf[1];
	/*	Low memory	*/
	rusage->low_mem = kmem_net;
	if (!numerator)
		rusage->low_mem /= mem->lowmem;
	/*	Total RAM	*/
	rusage->total_ram = ((double)param->physpages[0] * page_size +
		kmem_net);
	if (!numerator)
		rusage->total_ram /= mem->ram;
	/*	Mem + Swap	*/
	rusage->mem_swap = ((double)param->oomguarpages[0] * page_size +
		kmem_net);
	if (!numerator)
		rusage->mem_swap /= (mem->ram + mem->swap);
	/*	Allocated memory	*/
	rusage->alloc_mem = ((double)param->vmguarpages[0] * page_size +
		kmem_net);
	if (!numerator)
		rusage->alloc_mem /= (mem->ram + mem->swap);
	/*	Allocated memory limit	*/
	rusage->alloc_mem_lim = ((double)param->privvmpages[1] * page_size +
		kmem_net);
	if (!numerator)
		rusage->alloc_mem_lim /= (mem->ram + mem->swap);
	/*	Max Allocated memory limit	*/
	rusage->alloc_mem_max_lim = ((double)param->privvmpages[1] * page_size +
		kmem_net);
	if (!numerator)
		rusage->alloc_mem_max_lim /= mem->ram;
	return 0;
}

void inc_rusage(struct CRusage *rusagetotal, struct CRusage *rusage)
{
	if (rusagetotal == NULL || rusage == NULL)
		return;
	rusagetotal->low_mem += rusage->low_mem;
	rusagetotal->total_ram += rusage->total_ram;
	rusagetotal->mem_swap += rusage->mem_swap;
	rusagetotal->alloc_mem += rusage->alloc_mem;
	rusagetotal->alloc_mem_lim += rusage->alloc_mem_lim;
	if (rusage->alloc_mem_max_lim > rusagetotal->alloc_mem_max_lim)
		rusagetotal->alloc_mem_max_lim = rusage->alloc_mem_max_lim;
}

void mul_rusage(struct CRusage *rusage, int k)
{
	if (rusage == NULL)
		return; 
	rusage->low_mem *= k;
	rusage->total_ram *= k;
	rusage->mem_swap *= k;
	rusage->alloc_mem *= k;
	rusage->alloc_mem_lim *= k;
	rusage->alloc_mem_max_lim *= k;
}

void shift_ubs_param(struct ub_struct *param)
{
#define SHIFTPARAM(name) 						\
if (param->name != NULL) {						\
	param->name[0] = param->name[1];				\
	param->name[1] = param->name[2];				\
} 
	SHIFTPARAM(numproc);
	SHIFTPARAM(numtcpsock);
	SHIFTPARAM(numothersock);
	SHIFTPARAM(oomguarpages)
	SHIFTPARAM(vmguarpages);
	SHIFTPARAM(kmemsize);
	SHIFTPARAM(tcpsndbuf);
	SHIFTPARAM(tcprcvbuf);
	SHIFTPARAM(othersockbuf);
	SHIFTPARAM(dgramrcvbuf);
	SHIFTPARAM(privvmpages);
	SHIFTPARAM(numfile);
	SHIFTPARAM(dcachesize);
	SHIFTPARAM(physpages)
	SHIFTPARAM(numpty)
}

int calc_hn_rusage(struct CRusage *ru_comm, struct CRusage *ru_utl)
{
	FILE *fd;
	struct CRusage utl, comm;
	char str[STR_SIZE];
	char name[STR_SIZE];
	const char *fmt;
	unsigned long held, maxheld, barrier, limit;
	int found = 0;
	int id, res;
	int veid = 0;
	ub_param ub;
	struct ub_struct ub_s;
	struct mem_struct mem;
	
	if ((fd = fopen(PROCUBC, "r")) == NULL) {
		logger(0, errno, "Unable open " PROCUBC);
		return -1;
	}
	if (ru_comm != NULL)
		memset(ru_comm, 0, sizeof(*ru_comm));
	if (ru_utl != NULL)
		memset(ru_utl, 0, sizeof(*ru_utl));
	memset(&ub, 0, sizeof(ub));
	memset(&ub_s, 0, sizeof(ub_s));
	while (fgets(str, sizeof(str), fd)) {
		if ((res = sscanf(str, "%d:", &id)) == 1) {
			fmt =  "%*lu:%s%lu%lu%lu%lu";
			found = 1;
			if (veid) {
				if (ru_utl != NULL) {
					calc_ve_utilization(&ub_s, &utl, &mem, 0);
					inc_rusage(ru_utl, &utl);
				}
				if (ru_comm != NULL) {
					shift_ubs_param(&ub_s);
					calc_ve_commitment(&ub_s, &comm, &mem, 0);
					inc_rusage(ru_comm, &comm);
				}
			}
			veid = id;
			free_ubs_limit(&ub_s);
		} else {
			fmt = "%s%lu%lu%lu%lu";
		}
		if (found) {
			if ((res = sscanf(str, fmt, name, &held, &maxheld,
				&barrier, &limit)) != 5)
			{
				continue;
			}
			if ((res = get_ub_resid(name)) >= 0) {
				unsigned long *par;
				par = malloc(sizeof(*par) * 3);
				par[0] = held;
				par[1] = barrier;
				par[2] = limit;
				add_ubs_limit(&ub_s, res, par);
			}
		}
	}
	/* Last VE in /proc/user_beancounters */
	if (veid) {
		if (ru_utl != NULL) {
			calc_ve_utilization(&ub_s, &utl, &mem, 0);
			inc_rusage(ru_utl, &utl);
		}
		if (ru_comm != NULL) {
			shift_ubs_param(&ub_s);
			calc_ve_commitment(&ub_s, &comm, &mem, 0);
			inc_rusage(ru_comm, &comm);
		}
		free_ubs_limit(&ub_s);
	}
	fclose(fd);
	return 0;
}

int check_hn_overcommitment(int veid, struct ub_struct *param,
	struct ovrc *ovrc)
{
	struct CRusage ru_comm;
	struct CRusage rusage_ve;
	int actid;
	int ret = 0;
	struct mem_struct mem;

	if (param == NULL)
		return 0;
	actid = ovrc->action;
	if (ovrc->action == ACT_NONE)
		return 0;
	memset(&ru_comm, 0, sizeof(ru_comm));
	/* Calculate current HN overcommitment */
	if (calc_hn_rusage(&ru_comm, NULL))
		return 0;
	/* Add VE resource usage */
	calc_ve_commitment(param, &rusage_ve, &mem, 0);
	inc_rusage(&ru_comm, &rusage_ve);
	/* Convert to % */
	mul_rusage(&ru_comm, 100);
	if (ovrc->level_low_mem != NULL &&
		ru_comm.low_mem > *ovrc->level_low_mem)
	{
		logger(0, 0, "%s: node is overcommited.",
				actid == ACT_ERROR ? "Error" : "Warning");
		logger(0, 0, "\tLow Memory commitment level (%.3f%%)"
			       " exceeds configured (%.3f%%)",
				ru_comm.low_mem, *ovrc->level_low_mem);
		ret = 1;
	}
	if (ovrc->level_mem_swap != NULL &&
		ru_comm.mem_swap > *ovrc->level_mem_swap)
	{
		if (!ret)
			logger(0, 0, "%s: node is overcommited.",
				actid == ACT_ERROR ? "Error" : "Warning");
		logger(0, 0, "\tMemory and Swap commitment level (%.3f%%)"
				" exceeds configured (%.3f%%)",
				ru_comm.mem_swap, *ovrc->level_mem_swap);
		ret = 1;
	}
	if (ovrc->level_alloc_mem != NULL &&
		ru_comm.alloc_mem > *ovrc->level_alloc_mem)
	{
		if (!ret)
			logger(0, 0, "%s: node is overcommited.",
				actid == ACT_ERROR ? "Error" : "Warning");
		logger(0, 0, "\tAllocated Memory commitment level (%.3f%%)"
				" exceeds configured (%.3f%%)",
				ru_comm.alloc_mem, *ovrc->level_alloc_mem);
		ret = 1;
	}
	if (ovrc->level_alloc_mem_lim != NULL &&
		ru_comm.alloc_mem_lim > *ovrc->level_alloc_mem_lim)
	{
		if (!ret)
			logger(0, 0, "%s: node is overcommited.",
				actid == ACT_ERROR ? "Error" : "Warning");
		logger(0, 0, "\tTotal Alloc Limit commitment level (%.3f%%)"
				" exceeds configured (%.3f%%)",
				ru_comm.alloc_mem_lim,
				*ovrc->level_alloc_mem_lim);
		ret = 1;
	}
	return actid == ACT_ERROR ? VZ_OVERCOMMIT_ERROR : 0;
}

