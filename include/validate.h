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
#ifndef _VALIDATE_H_
#define _VALIDATE_H_

#define ACT_NONE	0
#define ACT_WARN	1
#define ACT_ERROR	2
#define ACT_FIX		3

#define UTILIZATION	0
#define COMMITMENT	1

#define PROCUBC		"/proc/user_beancounters"

struct CRusage {
	double low_mem;
	double total_ram;
	double mem_swap;
	double alloc_mem;
	double alloc_mem_lim;
	double alloc_mem_max_lim;
	double cpu;
};

struct ovrc {
	int action;
	float *level_low_mem;
	float *level_total_ram;
	float *level_mem_swap;
	float *level_alloc_mem;
	float *level_alloc_mem_lim;
	float *level_alloc_mem_max_lim;
};

struct mem_struct {
	unsigned long long ram;
	unsigned long long swap;
	unsigned long long lowmem;
};

struct ub_struct;

int calc_ve_utilization(struct ub_struct *param, struct CRusage *rusage,
	struct mem_struct *mem, int numerator);
int calc_ve_commitment(struct ub_struct *param, struct CRusage *rusage,
	struct mem_struct *mem, int numerator);
void add_ubs_limit(struct ub_struct *ubs, int res_id, unsigned long *limit);
void free_ubs_limit(struct ub_struct *ubs);
void shift_ubs_param(struct ub_struct *param);
void inc_rusage(struct CRusage *rusagetotal, struct CRusage *rusage);
#endif

