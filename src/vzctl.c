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
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "vzctl.h"
#include "env.h"
#include "logger.h"
#include "exec.h"
#include "config.h"
#include "vzerror.h"
#include "types.h"
#include "util.h"
#include "modules.h"

LOG_DATA

struct mod_action g_action;
char *_proc_title;
int _proc_title_len;

void init_modules(struct mod_action *action, const char *name);
void free_modules(struct mod_action *action);
int parse_action_opt(envid_t veid, int action, int argc, char *argv[],
	vps_param *param, const char *name);
int run_action(envid_t veid, int action, vps_param *g_p, vps_param *vps_p,
        vps_param *cmd_p, int argc, char **argv, int skiplock);

void version()
{
	fprintf(stdout, "vzctl version " VERSION "\n");
}

void usage()
{
	struct mod_action mod;

	version();
	fprintf(stdout, "Copyright (C) 2000-2005 SWsoft.\n");
	fprintf(stdout, "This program may be distributed under the terms of the Q Public License.\n\n");
	fprintf(stdout, "Usage: vzctl [options] <command> <veid> [parameters]\n"
"vzctl destroy | mount | umount | stop | status | enter <veid>\n"
"vzctl create <veid> {--ostemplate <name>] [--config <name>]\n"
"   [--private <path>] [--root <path>] [--ipadd <addr>] | [--hostname <name>]\n"
"vzctl start <veid> [--force]\n"
"vzctl exec | exec2 <veid> <command> [arg ...]\n"
"vzctl runscript <veid> <script>\n"
"vzctl chkpnt <vpsid> [--dumpfile <name>]\n"
"vzctl restore <vpsid> [--dumpfile <name>]\n"
"vzctl set veid [--save] [--setmode restart|ignore]\n"
"   [--ipadd <addr>] [--ipdel <addr>|all] [--hostname <name>]\n"
"   [--nameserver <addr>] [--searchdomain <name>] [--onboot yes|no]\n"
"   [--userpasswd <user>:<passwd>] [--cpuunits <N>] [--cpulimit <N>]\n"
"   [--diskspace <soft>[:<hard>]] [--diskinodes <soft>[:<hard>]]\n"
"   [--quotatime <N>] [--quotaugidlimit <N>]\n"
"   [--noatime yes|no] [--capability <name>:on|off ...]\n"
"   [--devices b|c:major:minor|all:r|w|rw]\n"
"   [--devnodes device:r|w|rw|none]\n"
"   [--applyconfig <name>]\n");
   
	fprintf(stdout, "   [--iptables <name>] [--disabled <yes|no>]\n");
	fprintf(stdout, "   [UBC parameters]\n"
"UBC parameters (N - items, P - pages, B - bytes):\n"
"Two numbers divided by colon means barrier:limit.\n"
"In case the limit is not given it is set to the same value as the barrier.\n"
"   --numproc N[:N]	--numtcpsock N[:N]	--numothersock N[:N]\n"
"   --vmguarpages P[:P]	--kmemsize B[:B]	--tcpsndbuf B[:B]\n"
"   --tcprcvbuf B[:B]	--othersockbuf B[:B]	--dgramrcvbuf B[:B]\n"
"   --oomguarpages P[:P]	--lockedpages P[:P]	--privvmpages P[:P]\n"
"   --shmpages P[:P]	--numfile N[:N]		--numflock N[:N]\n"
"   --numpty N[:N]	--numsiginfo N[:N]	--dcachesize N[:N]\n"
"   --numiptent N[:N]	--physpages P[:P]	--avnumproc N[:N]\n");
	memset(&mod, 0, sizeof(mod));
	set_log_level(0);
	init_modules(&mod, NULL);
	mod_print_usage(&mod);
	free_modules(&mod);
}

int main(int argc, char *argv[], char *envp[])
{
	int action = 0;
	int verbose = 0;
	int quiet = 0;
	int veid, ret, skiplock = 0;
	char buf[256];
	vps_param *gparam = NULL, *vps_p = NULL, *cmd_p = NULL;
	const char *action_nm;

	_proc_title = argv[0];
	_proc_title_len = envp[0] - argv[0];

	gparam = init_vps_param();
	vps_p = init_vps_param();
	cmd_p = init_vps_param();

	while (argc > 1) {
		if (!strcmp(argv[1], "--verbose")) {
			verbose = 2;
		} else if (!strcmp(argv[1], "--quiet"))
			quiet = 1;
		else if (!strcmp(argv[1], "--version")) {
			version();
			exit(0);
		} else if (!strcmp(argv[1], "--skiplock"))
			skiplock = YES;
		else
			break;
		argc--; argv++;
	}
	if (argc <= 1)	{
		usage();
		exit(VZ_INVALID_PARAMETER_SYNTAX);
	}
	action_nm = argv[1];
	init_log(NULL, 0, 1, verbose, 0, NULL);
	if (!strcmp(argv[1], "set")) {
		init_modules(&g_action, "set");
		action = ACTION_SET;
	//	status = ST_SET;
	} else if (!strcmp(argv[1], "create")) {
		init_modules(&g_action, "create");
		action = ACTION_CREATE;
	//	status = ST_CREATE;
	} else if (!strcmp(argv[1], "start")) {
		init_modules(&g_action, "set");
		action = ACTION_START;
	//	status = ST_START;
	} else if (!strcmp(argv[1], "stop")) {
		init_modules(&g_action, "set");
		action = ACTION_STOP;
	//	status = ST_STOP;
	} else if (!strcmp(argv[1], "restart")) {
		action = ACTION_RESTART;
	//	status = ST_RESTART;
	} else if (!strcmp(argv[1], "destroy")) {
		action = ACTION_DESTROY;
	//	status = ST_DESTROY;
	} else if (!strcmp(argv[1], "mount")) {
		action = ACTION_MOUNT;
	//	status = ST_MOUNT;
	} else if (!strcmp(argv[1], "umount")) {
		action = ACTION_UMOUNT;
	//	status = ST_UMOUNT;
	} else if (!strcmp(argv[1], "exec3")) {
		action = ACTION_EXEC3;
	} else if (!strcmp(argv[1], "exec2")) {
		action = ACTION_EXEC2;
	} else if (!strcmp(argv[1], "exec")) {
		action = ACTION_EXEC;
	} else if (!strcmp(argv[1], "runscript")) {
		action = ACTION_RUNSCRIPT;
	} else if (!strcmp(argv[1], "enter")) {
		action = ACTION_ENTER;
	} else if (!strcmp(argv[1], "status")) {
		action = ACTION_STATUS;
		quiet = 1;
	} else if (!strcmp(argv[1], "chkpnt")) {
		action = ACTION_CHKPNT;
	} else if (!strcmp(argv[1], "restore")) {
		action = ACTION_RESTORE;
	} else if (!strcmp(argv[1], "--help")) {
		usage();
		exit(0);
	} else {
		init_modules(&g_action, action_nm);
		action = ACTION_CUSTOM;
		if (!g_action.mod_count) {
			fprintf(stderr, "Bad command: %s\n", argv[1]);
			ret = VZ_INVALID_PARAMETER_SYNTAX;
			goto error;
		}
	}
	if (argc < 3) {
		fprintf(stderr, "VPS id is not given\n");
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	}
	if (parse_int(argv[2], &veid)) {
		fprintf(stderr, "Bad VPS id %s\n", argv[2]);
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	}
	argc -= 2; argv += 2;
	/* Read global config file */
	if (vps_parse_config(veid, GLOBAL_CFG, gparam, &g_action)) {
		fprintf(stderr, "Global configuration file %s not found\n",
			GLOBAL_CFG);
		ret = VZ_NOCONFIG;
		goto error;
	}
	init_log(gparam->log.log_file, veid, gparam->log.enable != NO,
		verbose ? gparam->log.level + 2 : gparam->log.level,
		quiet, "vzctl");
	if ((ret = parse_action_opt(veid, action, argc, argv, cmd_p,
		action_nm)))
	{
		goto error;
	}
	if (veid == 0 && action == ACTION_SET) {
		if (!cmd_p->res.cpu.units) {
			fprintf(stderr, "Only option cpuunits can be used in"
				" VE0\n");
                        ret = VZ_INVALID_PARAMETER_VALUE;
			goto error;
		}
	} else if (veid <= 0) {
		fprintf(stderr, "Bad VPS id %d\n", veid);
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	}
	get_vps_conf_path(veid, buf, sizeof(buf));
	if (stat_file(buf)) {
		if (vps_parse_config(veid, buf, vps_p, &g_action)) {
			logger(0, 0, "Error in config file %s",
				buf);
	                exit(VZ_NOCONFIG);
        	}
	} else if (action != ACTION_CREATE &&
			action != ACTION_STATUS &&
			action != ACTION_SET)
	{
		logger(0, 0, "VPS config file does not exist");
		ret = VZ_NOVECONFIG;
		goto error;
	}
	merge_vps_param(gparam, vps_p);
	merge_global_param(cmd_p, gparam);
	ret = run_action(veid, action, gparam, vps_p, cmd_p, argc-1, argv+1,
		skiplock);

error:
	free_modules(&g_action);
	free_vps_param(gparam);
	free_vps_param(vps_p);
	free_vps_param(cmd_p);
	free_log();

	return ret;
}
