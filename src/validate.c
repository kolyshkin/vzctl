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
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "ub.h"
#include "res.h"
#include "validate.h"
#include "vzctl_param.h"
#include "vzerror.h"
#include "logger.h"
#include "util.h"

int page_size = -1;

static int read_yn()
{
	char buf[1024];

	fprintf(stderr, " (y/n) [y] ");
	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		if (buf[0] == 'y' || buf[0] == '\n') {
			return 1;
		}
		else if (buf[0] == 'n')
			return 0;
		fprintf(stderr, " (y/n) [y] ");
	}
	return 0;
}

static int check_param(struct ub_struct *param, int log)
{
	int ret = 0;
#define CHECKPARAM(name)						\
	if (param->name == NULL) {					\
		if (log)						\
			logger(-1, 0, "Error: parameter " #name		\
				" not found");				\
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

#define SET_MES(val)	logger(0, 0, "set to %lu", val);
#define SET2_MES(val1, val2) logger(0, 0, "set to %lu:%lu", val1, val2);

/** Validate vswap config
 * Current ideas are:
 * 1. only check physpages and swappages
 * 2. barrier should be zero for both
 * 3. physpages.limit should not exceed host RAM
 * 4. swappages.limit should not exceed host swap
 */
static int validate_vswap(vps_res *param, int recover, int ask)
{
	struct ub_struct *ub = &param->ub;
	int changed = 0;
	int ret = 0;
	unsigned long long tmp;
	unsigned long ram, swap;

#define CHECK_VSWAP(name, limit)					\
	if (ub->name != NULL) {						\
		if (ub->name[0] != 0) {					\
			logger(-1, 0, "Error: " #name ".bar should be "	\
					"= 0 (currently, %lu)",		\
					ub->name[0]);			\
			if (ask || recover) {				\
				SET_MES((unsigned long) 0);		\
				if (ask)				\
					recover = read_yn();		\
				if (recover) {				\
					ub->name[0] = 0;		\
					changed++;			\
				}					\
			}						\
			if (!recover) ret = 1;				\
		}							\
		if (ub->name[1] > limit) {				\
			logger(-1, 0, "Warning: " #name ".lim should "	\
					"be <= %lu (currently, %lu)",	\
					limit, ub->name[1]);		\
			if (ask || recover) {				\
				SET_MES((unsigned long) limit);		\
				if (ask)				\
					recover = read_yn();		\
				if (recover) {				\
					ub->name[1] = limit;		\
					changed++;			\
				}					\
			}						\
			if (!recover) ret = 1;				\
		}							\
	} else {							\
		logger(-1, 0, "Error: parameter " #name " not found");	\
		ret = 1;						\
	}

	if (get_mem(&tmp) < 0)
		return -1;
	ram = tmp / page_size;

	if (get_swap(&tmp) < 0)
		return -1;
	swap = tmp / page_size;

	CHECK_VSWAP(physpages, ram);
	CHECK_VSWAP(swappages, swap);

	return ret;
}

int validate(vps_res *param, int recover, int ask, int vswap)
{
	unsigned long avnumproc;
	int ret = 0;
	unsigned long val, val1;
	unsigned long tmp_val0, tmp_val1;
	int changed = 0;
	struct ub_struct *ub;

	if (vswap)
		return validate_vswap(param, recover, ask);

#define CHECK_BL(x, name)						\
if (x != NULL) {							\
	if (x[0] > x[1]) {						\
		logger(-1, 0, "Error: barrier should be <= limit for "	\
			#name " (currently, %lu:%lu)",			\
			x[0], x[1]);					\
		if (ask || recover) {					\
			tmp_val1 = x[0];				\
			tmp_val0 = x[0];				\
			SET2_MES(tmp_val0, tmp_val1)			\
			if (ask) recover = read_yn();			\
			if (recover) {					\
				x[1] = tmp_val1;			\
				changed++;				\
			}						\
		}							\
		if (!recover) ret = 1;					\
	}								\
} else {								\
	logger(-1, 0, "Error: parameter "  #name " not found");		\
	ret = 1;							\
}

#define CHECK_B(name)							\
if (ub->name != NULL) {							\
	if ((ub->name[0] != ub->name[1])) {				\
		logger(-1, 0, "Error: barrier should be equal to limit" \
			" for " #name " (currently, %lu:%lu)",		\
			ub->name[0], ub->name[1]);			\
		if (ask || recover) {					\
			tmp_val0 = max_ul(ub->name[0], ub->name[1]);	\
			tmp_val1 = tmp_val0;				\
			SET2_MES(tmp_val0, tmp_val1)			\
			if (ask) recover = read_yn();			\
			if (recover) {					\
				ub->name[0] = tmp_val0;			\
				ub->name[1] = tmp_val1;			\
				changed++;				\
			}						\
		}							\
		if (!recover) ret = 1;					\
	}								\
} else {								\
	logger(-1, 0, "Error: parameter "  #name " not found");		\
	ret = 1;							\
}

	if (param == NULL)
		return 1;
	ub = &param->ub;
	if (check_param(ub, 1))
		return 1;
	if (ub->avnumproc != NULL)
		avnumproc = ub->avnumproc[0];
	else
		avnumproc = ub->numproc[0] / 2;
/*	1 Check barrier & limit	*/
	/* Primary */
	CHECK_B(numproc)
	CHECK_B(numtcpsock)
	CHECK_B(numothersock)
	if (ub->vmguarpages != NULL) {
		if (ub->vmguarpages[1] != LONG_MAX) {
			logger(-1, 0, "Error: limit should be = %lu for"
				" vmguarpages (currently, %lu)", LONG_MAX,
				ub->vmguarpages[1]);
			if (ask || recover) {
				SET_MES((unsigned long) LONG_MAX);
				if (ask)
					recover = read_yn();
				if (recover) {
					ub->vmguarpages[1] = LONG_MAX;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
	} else {
		logger(-1, 0, "Error: parameter vmguarpages not found");
		ret = 1;
	}
	/* Secondary */
	CHECK_BL(ub->kmemsize, kmemsize)
	CHECK_BL(ub->tcpsndbuf, tcpsndbuf)
	CHECK_BL(ub->tcprcvbuf, tcprcvbuf)
	CHECK_BL(ub->othersockbuf, othersockbuf)
	CHECK_BL(ub->dgramrcvbuf, dgramrcvbuf)
	if (ub->oomguarpages != NULL) {
		if (ub->oomguarpages[1] != LONG_MAX) {
			logger(-1, 0, "Error: limit should be = %lu for"
				" oomguarpages (currently, %lu)", LONG_MAX,
				ub->oomguarpages[1]);
			if (ask || recover) {
				SET_MES((unsigned long) LONG_MAX);
				if (ask)
					recover = read_yn();
				if (recover) {
					ub->oomguarpages[1] = LONG_MAX;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
	} else {
		logger(-1, 0, "Error: parameter oomguarpages not found");
		ret = 1;
	}
	CHECK_BL(ub->privvmpages, privvmpages)
	/* Auxiliary */
	CHECK_BL(ub->lockedpages, lockedpages)
	CHECK_B(shmpages)
	if (ub->physpages != NULL) {
		if (ub->physpages[0] != 0) {
			logger(-1, 0, "Error: barrier should be = 0 for"
				" physpages (currently, %lu)",
				ub->physpages[0]);
			if (ask || recover) {
				SET_MES((unsigned long) 0);
				if (ask)
					recover = read_yn();
				if (recover) {
					ub->physpages[0] = 0;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
		if (ub->physpages[1] != LONG_MAX) {
			logger(-1, 0, "Error: limit should be = %lu for"
				" physpages (currently, %lu)", LONG_MAX,
				ub->physpages[1]);
			if (ask || recover) {
				SET_MES((unsigned long) LONG_MAX);
				if (ask)
					recover = read_yn();
				if (recover) {
					ub->physpages[1] = LONG_MAX;
					changed++;
				}
			}
			if (!recover) ret = 1;
//			if (!ask) fprintf(stderr, "\n");
		}
	} else {
		logger(-1, 0, "Error: parameter physpages not found");
		ret = 1;
	}
	CHECK_B(numfile)
	CHECK_BL(ub->numflock, numflock)
	CHECK_B(numpty)
	CHECK_B(numsiginfo)
	CHECK_BL(ub->dcachesize, dcachesize)
	CHECK_B(numiptent)
	if (param->dq.enable == YES) {
		CHECK_BL(param->dq.diskspace, diskspace)
		CHECK_BL(param->dq.diskinodes, diskinodes)
	}

/*	2 Check formulas			*/
	val = ub->numfile[0] * 384;
	val &= LONG_MAX;
	if (ub->dcachesize[1] < val) {
		logger(-1, 0, "Warning: dcachesize.lim should be > %lu"
				" (currently, %lu)", val,
				ub->dcachesize[1]);
		if (ask || recover) {
			SET_MES(val);
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->dcachesize[1] = val;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}

	val = (40 * 1024 * avnumproc) + ub->dcachesize[1];
	val &= LONG_MAX;
	if (ub->kmemsize[0] < val) {
		logger(-1, 0, "Error: kmemsize.bar should be > %lu"
				" (currently, %lu)", val, ub->kmemsize[0]);
		if (ask || recover) {
			tmp_val1 = ub->kmemsize[1] + val - ub->kmemsize[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1);
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->kmemsize[1] = tmp_val1;
				ub->kmemsize[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	if (ub->privvmpages[0] < ub->vmguarpages[0]) {
		logger(-1, 0, "Warning: privvmpages.bar should be > %lu"
				" (currently, %lu)", ub->vmguarpages[0],
				ub->privvmpages[0]);
		if (ask || recover) {
			tmp_val0 = ub->vmguarpages[0];
			tmp_val1 = ub->privvmpages[1] < tmp_val0 ?
				tmp_val0 : ub->vmguarpages[1];
			SET_MES(tmp_val0);
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->privvmpages[0] = tmp_val0;
				ub->privvmpages[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 2.5 * 1024 * ub->numtcpsock[0];
	val &= LONG_MAX;
	if (ub->tcpsndbuf[1] - ub->tcpsndbuf[0] < val) {
		logger(-1, 0, "Error: tcpsndbuf.lim-tcpsndbuf.bar"
				" should be > %lu (currently, %lu-%lu=%lu)",
				val, ub->tcpsndbuf[1], ub->tcpsndbuf[0],
				ub->tcpsndbuf[1]-ub->tcpsndbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->tcpsndbuf[0] + val;
			tmp_val0 = ub->tcpsndbuf[0];
			SET2_MES(tmp_val0, tmp_val1);
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->tcpsndbuf[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 2.5 * 1024 * ub->numothersock[0];
	val &= LONG_MAX;
	if (ub->othersockbuf[1] - ub->othersockbuf[0] < val) {
		logger(-1, 0, "Error: othersockbuf.lim-othersockbuf.bar"
				" should be > %lu (currently, %lu-%lu=%lu)",
				val, ub->othersockbuf[1], ub->othersockbuf[0],
				ub->othersockbuf[1]-ub->othersockbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->othersockbuf[0] + val;
			tmp_val0 = ub->othersockbuf[0];
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->othersockbuf[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val =  2.5 * 1024 * ub->numtcpsock[0];
	val &= LONG_MAX;
	if (ub->tcprcvbuf[1] - ub->tcprcvbuf[0] < val) {
		logger(-1, 0, "Warning: tcprcvbuf.lim-tcprcvbuf.bar"
				" should be > %lu (currently, %lu-%lu=%lu)",
				val, ub->tcprcvbuf[1], ub->tcprcvbuf[0],
				ub->tcprcvbuf[1] - ub->tcprcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->tcprcvbuf[0] + val;
			tmp_val0 = ub->tcprcvbuf[0];
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->tcprcvbuf[1] = tmp_val1;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 64 * 1024;
	if (ub->tcprcvbuf[0] < val) {
		logger(-1, 0, "Warning: tcprcvbuf.bar should be > %lu"
				" (currently, %lu)", val,
				ub->tcprcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->tcprcvbuf[1]+val-ub->tcprcvbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->tcprcvbuf[1] = tmp_val1;
				ub->tcprcvbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 64 * 1024;
	if (ub->tcpsndbuf[0] <  val) {
		logger(-1, 0, "Warning: tcpsndbuf.bar should be > %lu"
				" (currently, %lu)", val,
				ub->tcpsndbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->tcpsndbuf[1]+val-ub->tcpsndbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->tcpsndbuf[1] = tmp_val1;
				ub->tcpsndbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}
	val = 32 * 1024;
	val1 = 129 * 1024;
	if (ub->dgramrcvbuf[0] < val) {
		logger(-1, 0, "Warning: dgramrcvbuf.bar should be >"
				" %lu (currently, %lu)", val,
				ub->dgramrcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->dgramrcvbuf[1] + val -
					ub->dgramrcvbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->dgramrcvbuf[1] = tmp_val1;
				ub->dgramrcvbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	} else if (ub->dgramrcvbuf[0] < val1) {
		logger(-1, 0, "Recommendation: dgramrcvbuf.bar should be >"
				" %lu (currently, %lu)", val1,
				ub->dgramrcvbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->dgramrcvbuf[1] + val1 -
					ub->dgramrcvbuf[0];
			tmp_val0 = val1;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->dgramrcvbuf[1] = tmp_val1;
				ub->dgramrcvbuf[0] = tmp_val0;
				changed++;
			}
		}
//		if (!ask) fprintf(stderr, "\n");
	}
	val =  32 * 1024;
	val1 = 129 * 1024;
	if (ub->othersockbuf[0] < val) {
		logger(-1, 0, "Warning: othersockbuf.bar should be >"
				" %lu (currently, %lu)", val,
				ub->othersockbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->othersockbuf[1] + val -
					ub->othersockbuf[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->othersockbuf[1] = tmp_val1;
				ub->othersockbuf[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	} else if (ub->othersockbuf[0] < val1) {
		logger(-1, 0, "Recommendation: othersockbuf.bar should be >"
				" %lu (currently, %lu)", val1,
				ub->othersockbuf[0]);
		if (ask || recover) {
			tmp_val1 = ub->othersockbuf[1] + val1 -
					ub->othersockbuf[0];
			tmp_val0 = val1;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->othersockbuf[1] = tmp_val1;
				ub->othersockbuf[0] = tmp_val0;
				changed++;
			}
		}
//		if (!ask) fprintf(stderr, "\n");
	}
	val = avnumproc * 32;
	val1 = ub->numtcpsock[0] + ub->numothersock[0] + ub->numpty[0];
	if (val1 > val)
		val = val1;
	val &= LONG_MAX;
	if (ub->numfile[0] < val) {
		logger(-1, 0, "Warning: numfile should be > %lu"
				" (currently, %lu)", val, ub->numfile[0]);
		if (ask || recover) {
			tmp_val1 = ub->numfile[1] + val - ub->numfile[0];
			tmp_val0 = val;
			SET2_MES(tmp_val0, tmp_val1)
			if (ask)
				recover = read_yn();
			if (recover) {
				ub->numfile[1] = tmp_val1;
				ub->numfile[0] = tmp_val0;
				changed++;
			}
		}
		if (!recover) ret = 1;
//		if (!ask) fprintf(stderr, "\n");
	}

	return ret;
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
