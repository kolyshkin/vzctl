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
#include <getopt.h>

#include "vzconfig.h"
#include "validate.h"
#include "logger.h"
#include "util.h"

const char progname[] = "vzcfgvalidate";
extern int page_size;

static void usage(int rc)
{
	fprintf(rc ? stderr : stdout,
"Usage: %s [-r|-i] [-v yes|no] <configfile>\n"
"	-r		repair mode\n"
"	-i		interactive repair mode\n"
"	-v yes|no	force VSwap/non-VSwap mode (auto-detect by default)\n",
		progname);

	exit(rc);
}

int main(int argc, char **argv)
{
	vps_param *param, *gparam;
	struct stat st;
	char *infile = NULL;
	int opt, recover = 0, ask = 0;
	int ret = 1;
	int vswap = -1;

	init_log(NULL, 0, 1, 0, 0, progname);

	while ((opt = getopt(argc, argv, "rihv:")) > 0) {
		switch(opt) {
		case 'r'	:
			recover = 1;
			break;
		case 'i'	:
			ask = 1;
			break;
		case 'h'	:
			usage(0);
		case 'v':
			switch (yesno2id(optarg)) {
				case YES:
					vswap = 1;
					break;
				case NO:
					vswap = 0;
					break;
				default:
					logger(-1, 0, "Invalid argument "
							"for -v: %s", optarg);
					usage(1);
			}
			break;
		default	:
			usage(1);
		}
	}
	if (optind >= argc)
		usage(1);

	if ((page_size = get_pagesize()) < 0)
		return 1;

	/* Read global config */
	gparam = init_vps_param();
	if (vps_parse_config(0, GLOBAL_CFG, gparam, NULL)) {
		logger(-1, 0, "WARNING: Global configuration file %s "
			"not found", GLOBAL_CFG);
	}

	/* Read container config */
	infile = strdup(argv[optind]);
	if (!infile) {
		logger(-1, 0, "No free memory");
		goto err;
	}
	if (stat(infile, &st)) {
		logger(-1, 0, "Container configuration file %s not found",
			infile);
		goto err;
	}
	param = init_vps_param();
	if (vps_parse_config(0, infile, param, NULL))
		goto err;

	/* Merge configs (needed for DISK_QUOTA value, maybe others) */
	merge_vps_param(gparam, param);

	if (vswap == -1)
		vswap = is_vswap_config(&param->res.ub);

	if (!(ret = validate(&param->res, recover, ask, vswap))) {
		if (recover || ask)
			if (vps_save_config(0, infile, param, NULL, NULL))
				goto err;
		logger(-1, 0, "Validation completed: success");
		ret = 0;
	}
	else
		ret = 2;

err:
	free(infile);
	exit(ret);
}
