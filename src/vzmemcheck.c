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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "validate.h"
#include "ub.h"
#include "util.h"
#include "logger.h"

extern int page_size;

static void usage(int rc)
{
	fprintf(rc ? stderr : stdout, "Usage: vzmemcheck [-v] [-A]\n");
	exit(rc);
}

static void header(int verbose, int param)
{
	if (param)
		printf("Output values in Mbytes\n");
	else
		printf("Output values in %%\n");
	if (verbose)
		printf("CTID       ");
	printf(" LowMem  LowMem     RAM MemSwap MemSwap   Alloc   Alloc   Alloc\n");
	if (verbose)
		printf("           ");
	printf("   util  commit    util    util  commit    util  commit   limit\n");
	return;
}

static void inc_rusage(struct CRusage *rusagetotal, struct CRusage *rusage)
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

static void shift_ubs_param(struct ub_struct *param)
{
#define SHIFTPARAM(name)						\
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
#undef SHIFTPARAM
}

static int calculate(int numerator, int verbose)
{
	struct CRusage rutotal_comm, rutotal_utl;
	struct CRusage ru_comm, ru_utl;
	char str[STR_SIZE];
	char name[STR_SIZE];
	const char *fmt = NULL;
	int veid = 0, res, id, ret = 0, found = 0, new_veid = 0;
	int exited = 0;
	double r, rs, lm, k;
	struct mem_struct mem;
	unsigned long held, maxheld, barrier, limit;
	FILE *fd;
	struct ub_struct ub_s;
	envid_t *velist;
	int venum;

	if (get_mem(&mem.ram))
		return 1;
	if (get_swap(&mem.swap))
		mem.swap = 1;
	if (get_lowmem(&mem.lowmem))
		return 1;

	venum = get_running_ve_list(&velist);
	if (venum < 0) {
		fprintf(stderr, "Can't get list of running CTs: %s\n",
				strerror(-venum));
		return 1;
	}

	if ((fd = fopen(PROCUBC, "r")) == NULL) {
		fprintf(stderr, "Unable to open file " PROCUBC ": %s\n",
			strerror(errno));
		free(velist);
		return 1;
	}

	mem.lowmem *= 0.4;
	memset(&ub_s, 0, sizeof(ub_s));
	memset(&rutotal_comm, 0, sizeof(rutotal_comm));
	memset(&rutotal_utl, 0, sizeof(rutotal_utl));
	if (numerator) {
		/* Convert to Mb */
		k = 1.0 / (1024 * 1024);
	} else {
		/* Convert to % */
		k = 100;
	}
	header(verbose, numerator);
	while (!exited)	{
		if (fgets(str, sizeof(str), fd) == NULL) {
			veid = new_veid;
			found = 1;
			str[0] = 0;
			exited = 1;
		} else {
			if ((res = sscanf(str, "%d:", &id)) == 1) {
				fmt =  "%*lu:%s%lu%lu%lu%lu";
				veid = new_veid;
				new_veid = id;
				found = 1;
			} else {
				fmt = "%s%lu%lu%lu%lu";
				found = 0;
			}
		}
		if (found && veid && ve_in_list(velist, venum, veid)) {
			if (!calc_ve_utilization(&ub_s, &ru_utl, &mem,
				numerator))
			{
				ru_utl.low_mem *= k;
				ru_utl.total_ram *= k;
				ru_utl.mem_swap *= k;
				ru_utl.alloc_mem *= k;
			}
			shift_ubs_param(&ub_s);
			if (!calc_ve_commitment(&ub_s, &ru_comm, &mem,
				 numerator))
			{
				ru_comm.low_mem *= k;
				ru_comm.total_ram *= k;
				ru_comm.mem_swap *= k;
				ru_comm.alloc_mem *= k;
				ru_comm.alloc_mem_lim *= k;
			}
			if (verbose) {
				printf("%-10d %7.2f %7.2f %7.2f %7.2f %7.2f"
					" %7.2f %7.2f %7.2f\n", veid,
					ru_utl.low_mem,
					ru_comm.low_mem,
					ru_utl.total_ram,
					ru_utl.mem_swap,
					ru_comm.mem_swap,
					ru_utl.alloc_mem,
					ru_comm.alloc_mem,
					ru_comm.alloc_mem_lim);
			}
			inc_rusage(&rutotal_utl, &ru_utl);
			inc_rusage(&rutotal_comm, &ru_comm);
			free_ub_param(&ub_s);
		}
		if (!exited) {
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
				add_ub_limit(&ub_s, res, par);
			}
		}
	}
	fclose(fd);
	free(velist);
	if (verbose) {
		printf("--------------------------------------------------------------------------\n");
		printf("Summary:   ");
	}
	printf("%7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f\n",
			rutotal_utl.low_mem,
			rutotal_comm.low_mem,
			rutotal_utl.total_ram,
			rutotal_utl.mem_swap,
			rutotal_comm.mem_swap,
			rutotal_utl.alloc_mem,
			rutotal_comm.alloc_mem,
			rutotal_comm.alloc_mem_lim);
	if (numerator) {
		lm = mem.lowmem / (1024 * 1024);
		r = mem.ram / (1024 * 1024);
		rs = (mem.ram + mem.swap) / (1024 * 1024);
		if (verbose)
			printf("           ");
		printf("%7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f\n",
			lm, lm, r, rs, rs, rs, rs, rs);
	}
	return ret;
}

int main(int argc, char **argv)
{
	int ret, opt, verbose = 0, numerator = 0;

	while ((opt = getopt(argc, argv, "Avh")) > 0) {
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		case 'A':
			numerator = 1;
			break;
		case 'h':
			usage(0);
		default:
			usage(1);
		}
	}
	init_log(NULL, 0, 0, 0, 0, NULL);
	page_size = get_pagesize();
	if (page_size < 0)
		return 1;
	ret = calculate(numerator, verbose);
	exit(ret);
}
