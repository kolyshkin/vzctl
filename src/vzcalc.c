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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "logger.h"
#include "ub.h"
#include "config.h"
#include "util.h"

LOG_DATA

void usage()
{
	printf("Usage: vzcalc [-v] <veid>\n");
}

int check_param(struct ub_struct *param)
{
	int ret = 0;	
#define CHECKPARAM(name, str_name) \
if (param->name == NULL) { \
	fprintf(stderr, "Error: parameter " #str_name " not found\n"); \
	ret = 1; \
}
	CHECKPARAM(numproc, NUMPROC);
	CHECKPARAM(vmguarpages, VMGUARPAGES);
	CHECKPARAM(kmemsize, KMEMSIZE);
	CHECKPARAM(tcpsndbuf, TCPSNDBUF);
	CHECKPARAM(tcprcvbuf, TCPRCVBUF);
	CHECKPARAM(othersockbuf, OTHERSOCKBUF);
	CHECKPARAM(dgramrcvbuf, DGRAMRCVBUF);
	CHECKPARAM(privvmpages, PRIVVMPAGES);

	return ret;
}

int calculate(int veid, ub_param *ub, int verbose)
{
	ub_param ub_cur;
	struct ub_struct ubs;
	struct ub_struct ubs_cur;
	char tmp[STR_SIZE];
	int ret = 0;
	double kmem_net_cur, lm_cur, tr_cur, ms_cur, al_cur, np_cur;
	double kmem_net_max, lm_max, lm_prm, ms_prm, al_prm, al_max, np_max;
	double cur, prm, max;
	unsigned long long lowmem;
	unsigned long long ram, swap = 1;
	int run, thrmax;
	int page;

	ub2ubs(ub, &ubs);
	if (check_param(&ubs))
		return 1;
	if (get_mem(&ram))
		return 1;
	if (get_thrmax(&thrmax))
		return 1;
	get_swap(&swap);
	if (get_lowmem(&lowmem))
		return 1;
	page = get_pagesize();
	lm_cur = tr_cur = ms_cur = al_cur = np_cur = 0;
	lowmem *= 0.4;
	memset(&ub_cur, 0, sizeof(ub_cur));
	if (!(run = vps_read_ubc(veid, &ub_cur))) {
		ub2ubs(&ub_cur, &ubs_cur);
		if (check_param(&ubs_cur))
			return 1;
		kmem_net_cur = (double)ubs_cur.kmemsize[0] + 
			(double)ubs_cur.tcprcvbuf[0] + 
			(double)ubs_cur.tcpsndbuf[0] +
			(double)ubs_cur.dgramrcvbuf[0] +
			(double)ubs_cur.othersockbuf[0];
		/*	Low memory	*/
		lm_cur = kmem_net_cur / lowmem;
		/*	Total RAM	*/
		tr_cur = ((double)ubs_cur.physpages[0] * page + kmem_net_cur) 
					/ ram;
		/*	Mem + Swap	*/
		ms_cur = ((double)ubs_cur.oomguarpages[0] * page +
				kmem_net_cur) /	(ram + swap);
		/*	Allocated	*/
		al_cur = ((double)ubs_cur.privvmpages[0] * page +
			       kmem_net_cur) / (ram + swap);
		/*	Numproc		*/
		np_cur = (double)ubs_cur.numproc[0] / thrmax;	
	}
	kmem_net_max = (double)ubs.kmemsize[1] + 
		(double)ubs.tcprcvbuf[1] +
		(double)ubs.tcpsndbuf[1] +
		(double)ubs.dgramrcvbuf[1] +
		(double)ubs.othersockbuf[1];
	
	/*      Low memory      */
	lm_max = kmem_net_max / lowmem;
	lm_prm = lm_max;
	/*      Mem + Swap      */
	ms_prm = ((double)ubs.oomguarpages[0] * page + kmem_net_max) /
			(ram + swap);
	/*      Allocated       */
	al_prm = ((double)ubs.vmguarpages[0] * page + kmem_net_max) / 
			(ram + swap);
	al_max = ((double)ubs.privvmpages[1] * page + kmem_net_max) /
			(ram + swap);
	/*	Numproc		*/
	np_max = (double)ubs.numproc[0] / thrmax;

	/* Calculate maximum for current */
	cur = lm_cur;
	if (tr_cur > cur)
		cur = tr_cur;
	if (ms_cur > cur)
		cur = ms_cur;
	if (al_cur > cur)
		cur = al_cur;
	if (np_cur > cur)
		cur = np_cur;
	/* Calculate maximum for promised */
	prm = lm_prm;
	if (ms_prm > prm)
		prm = ms_prm;
	if (al_prm > prm)
		prm = al_prm;
	/* Calculate maximum for Max */
	max = lm_max;
	if (al_max > max)
		max = al_max;
	if (np_max > max)
		max = np_max;
	sprintf(tmp, "n/a");
	printf("Resource     Current(%%)  Promised(%%)  Max(%%)\n");
	if (verbose) {
		if (!run)
			sprintf(tmp, "%10.2f", lm_cur * 100);
		printf("Low Mem    %10s %10.2f %10.2f\n",
				tmp, lm_prm * 100, lm_max * 100);
		if (!run)
			sprintf(tmp, "%10.2f", tr_cur * 100);
		printf("Total RAM  %10s        n/a        n/a \n", tmp);
		if (!run)
			sprintf(tmp, "%10.2f", ms_cur * 100);
		printf("Mem + Swap %10s %10.2f        n/a\n",
				tmp, ms_prm * 100);
		if (!run)
			sprintf(tmp, "%10.2f", al_cur * 100);
		printf("Alloc. Mem %10s %10.2f %10.2f\n",
				tmp , al_prm * 100, al_max * 100);
		if (!run)
			sprintf(tmp, "%10.2f", np_cur * 100);
		printf("Num. Proc  %10s        n/a %10.2f\n",
				tmp, np_max * 100);
		printf("--------------------------------------------\n");
	}
	if (!run)
		sprintf(tmp, "%10.2f", cur * 100);
	printf("Memory     %10s %10.2f %10.2f\n", tmp, prm * 100, max * 100);
	free_ub_param(&ub_cur);
	return ret;
}

int main(int argc, char **argv)
{
	struct stat st;
	char path[64];
	int ret, opt, veid, verbose = 0;
	vps_param *param;

	while ((opt = getopt(argc, argv, "vh")) > 0) {
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		case 'h':
			usage();
			exit(1);
			break;
		default	:
			exit(1);
		}
	}
	if (optind == argc) {
		usage();
		exit(1);
	}
	if (parse_int(argv[optind], &veid)) {
		fprintf(stderr, "Invalid veid: %s\n", argv[optind]);
		exit(1);
	}
	get_vps_conf_path(veid, path, sizeof(path));
	if (stat(path, &st)) {
		fprintf(stderr, "VPS configuration file: %s not found\n", path);
		exit(1);
	}
	param = init_vps_param();
	if (vps_parse_config(veid, path, param, NULL))
		exit(1);
	ret = calculate(veid, &param->res.ub, verbose);
	free_vps_param(param);
	exit(ret);
}
