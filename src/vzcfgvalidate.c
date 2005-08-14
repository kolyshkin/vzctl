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
#include <getopt.h>

#include "config.h"
#include "validate.h"
#include "logger.h"

log_param g_log;

void usage()
{
	printf("Usage: vzcfgvalidate [-r|-i] <configfile>\n");
	printf("	-r repair mode\n");
	printf("	-i interactive repair mode\n");
}

int main(int argc, char **argv)
{
	vps_param *param;
	struct stat st;
	char *infile = NULL;
	int ret, opt, recover = 0, ask = 0;

	while ((opt = getopt(argc, argv, "ri")) > 0)
	{
		switch(opt) {
		case 'r'	:
			recover = 1;
			break;
		case 'i'	:
			ask = 1;
			break;
		default	:
			usage();
			exit(1);
		}
	}
	if (optind >= argc) {
		usage();
		exit(1);
	}
	infile = strdup(argv[optind]);
	if (stat(infile, &st)) {
		fprintf(stderr,"VPS configuration file: %s not found\n", infile);
		free(infile);
		exit(1);
	}
	init_log(NULL, 0, 1, 0, 0, NULL);
	param = init_vps_param();
	if (vps_parse_config(0, infile, param, NULL))
		exit(1);
	if (!(ret = validate(&param->res, recover, ask)))	{
		if (recover || ask)
			if (vps_save_config(0, infile, param, NULL, NULL))
				return 1;
		fprintf(stderr, "Validation completed: success\n");
	}
	exit(ret);
}
