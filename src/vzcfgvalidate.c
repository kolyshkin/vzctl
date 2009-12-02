/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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

#include "config.h"
#include "validate.h"
#include "logger.h"
#include "util.h"

extern int page_size;

void usage(int rc)
{
	FILE *fp = rc ? stderr : stdout;
	fprintf(fp, "Usage: vzcfgvalidate [-r|-i] <configfile>\n");
	fprintf(fp, "	-r repair mode\n");
	fprintf(fp, "	-i interactive repair mode\n");
	exit(rc);
}

int main(int argc, char **argv)
{
	vps_param *param, *gparam;
	struct stat st;
	char *infile = NULL;
	int ret, opt, recover = 0, ask = 0;

	while ((opt = getopt(argc, argv, "rih")) > 0) {
		switch(opt) {
		case 'r'	:
			recover = 1;
			break;
		case 'i'	:
			ask = 1;
			break;
		case 'h'	:
			usage(0);
		default	:
			usage(1);
		}
	}
	if (optind >= argc)
		usage(1);

	init_log(NULL, 0, 1, 0, 0, NULL);

	if ((page_size = get_pagesize()) < 0)
		return 1;

	/* Read global config */
	gparam = init_vps_param();
	if (vps_parse_config(0, GLOBAL_CFG, gparam, NULL)) {
		fprintf(stderr, "WARNING: Global configuration file %s "
				"not found\n", GLOBAL_CFG);
	}

	/* Read container config */
	infile = strdup(argv[optind]);
	if (stat(infile, &st)) {
		fprintf(stderr,"Container configuration file %s not found\n",
				infile);
		free(infile);
		exit(1);
	}
	param = init_vps_param();
	if (vps_parse_config(0, infile, param, NULL))
		exit(1);

	/* Merge configs (needed for DISK_QUOTA value, maybe others) */
	merge_vps_param(gparam, param);

	if (!(ret = validate(&param->res, recover, ask)))	{
		if (recover || ask)
			if (vps_save_config(0, infile, param, NULL, NULL))
				return 1;
		fprintf(stderr, "Validation completed: success\n");
	}
	exit(ret);
}
