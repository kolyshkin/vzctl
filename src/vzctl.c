/*
 *  Copyright (C) 2000-2012, Parallels, Inc. All rights reserved.
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
#include <signal.h>
#include <limits.h>

#include "version.h"
#include "vzctl.h"
#include "env.h"
#include "logger.h"
#include "exec.h"
#include "vzconfig.h"
#include "vzerror.h"
#include "types.h"
#include "util.h"
#include "modules.h"

struct mod_action g_action;
char *_proc_title;
int _proc_title_len;

void init_modules(struct mod_action *action, const char *name);
void free_modules(struct mod_action *action);
int parse_action_opt(envid_t veid, act_t action, int argc, char *argv[],
	vps_param *param, const char *name);
int run_action(envid_t veid, act_t action, vps_param *g_p, vps_param *vps_p,
	vps_param *cmd_p, int argc, char **argv, int skiplock);

static void version(FILE *fp)
{
	fprintf(fp, "vzctl version " VERSION "\n");
}

static void usage(int rc)
{
	struct mod_action mod;
	FILE *fp = rc ? stderr : stdout;

	version(fp);
	fprintf(fp,
"Copyright (C) 2000-2012, Parallels, Inc.\n"
"This program may be distributed under the terms of the GNU GPL License.\n"
"\n"
"Usage: vzctl [options] <command> <ctid> [parameters]\n"
"\n"
"vzctl create <ctid> [--ostemplate <name>] [--config <name>]\n"
"   [--layout ploop|simfs] [--hostname <name>] [--name <name>] [--ipadd <addr>]\n"
"   [--diskspace <kbytes>] [--private <path>] [--root <path>]\n"
"vzctl start <ctid> [--force] [--wait]\n"
"vzctl destroy | mount | umount | stop | restart | status <ctid>\n"
#ifdef HAVE_PLOOP
"vzctl convert <ctid> [--layout ploop[:mode]] [--diskspace <kbytes>]\n"
"vzctl compact <ctid>\n"
"vzctl snapshot <ctid> [--id <uuid>] [--name <name>] [--description <desc>]\n"
"   [--skip-suspend]\n"
"vzctl snapshot-switch | snapshot-delete <ctid> --id <uuid>\n"
"vzctl snapshot-list <ctid> [-H] [-o field[,field...]] [--id <uuid>]\n"
#endif
"vzctl quotaon | quotaoff | quotainit <ctid>\n"
"vzctl console <ctid> [ttyno]\n"
"vzctl enter <ctid> [--exec <command> [arg ...]]\n"
"vzctl exec | exec2 <ctid> <command> [arg ...]\n"
"vzctl runscript <ctid> <script>\n"
"vzctl suspend | resume <ctid> [--dumpfile <name>]\n"
"vzctl set <ctid> [--save] [--force] [--setmode restart|ignore]\n"
"   [--ram <bytes>[KMG]] [--swap <bytes>[KMG]]\n"
"   [--ipadd <addr>] [--ipdel <addr>|all] [--hostname <name>]\n"
"   [--nameserver <addr>] [--searchdomain <name>]\n"
"   [--onboot yes|no] [--bootorder <N>]\n"
"   [--userpasswd <user>:<passwd>]\n"
"   [--cpuunits <N>] [--cpulimit <N>] [--cpus <N>] [--cpumask <cpus>]\n"
"   [--diskspace <soft>[:<hard>]] [--diskinodes <soft>[:<hard>]]\n"
"   [--quotatime <N>] [--quotaugidlimit <N>] [--mount_opts <opt>[,<opt>...]]\n"
"   [--capability <name>:on|off ...]\n"
"   [--devices b|c:major:minor|all:r|w|rw]\n"
"   [--devnodes device:r|w|rw|none]\n"
"   [--netif_add <ifname[,mac,host_ifname,host_mac,bridge]]>]\n"
"   [--netif_del <ifname>]\n"
"   [--applyconfig <name>] [--applyconfig_map <name>]\n"
"   [--features <name:on|off>] [--name <vename>] [--ioprio <N>]\n"
"   [--pci_add [<domain>:]<bus>:<slot>.<func>] [--pci_del <d:b:s.f>]\n"
"   [--iptables <name>] [--disabled <yes|no>]\n"
"   [UBC parameters]\n"
"\n"
"UBC parameters (N - items, P - pages, B - bytes):\n"
"Two numbers divided by colon means barrier:limit.\n"
"In case the limit is not given it is set to the same value as the barrier.\n"
"   --numproc N[:N]	--numtcpsock N[:N]	--numothersock N[:N]\n"
"   --vmguarpages P[:P]	--kmemsize B[:B]	--tcpsndbuf B[:B]\n"
"   --tcprcvbuf B[:B]	--othersockbuf B[:B]	--dgramrcvbuf B[:B]\n"
"   --oomguarpages P[:P]	--lockedpages P[:P]	--privvmpages P[:P]\n"
"   --shmpages P[:P]	--numfile N[:N]		--numflock N[:N]\n"
"   --numpty N[:N]	--numsiginfo N[:N]	--dcachesize N[:N]\n"
"   --numiptent N[:N]	--physpages P[:P]	--avnumproc N[:N]\n"
"   --swappages P[:P]\n"
);
	memset(&mod, 0, sizeof(mod));
	set_log_level(0);
	init_modules(&mod, NULL);
	mod_print_usage(&mod);
	free_modules(&mod);
	exit(rc);
}

int main(int argc, char *argv[], char *envp[])
{
	act_t action = -1;
	int verbose = 0;
	int verbose_custom = 0;
	int quiet = 0;
	int veid, ret, skiplock = 0;
	char buf[256];
	vps_param *gparam = NULL, *vps_p = NULL, *cmd_p = NULL;
	const char *action_nm;
	struct sigaction act;
	char *name = NULL, *opt;

	_proc_title = argv[0];
	_proc_title_len = envp[0] - argv[0];

	gparam = init_vps_param();
	vps_p = init_vps_param();
	cmd_p = init_vps_param();

	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);

	while (argc > 1) {
		opt = argv[1];

		if (!strcmp(opt, "--verbose")) {
			verbose++;
			verbose_custom = 1;
		} else if (!strcmp(opt, "--quiet"))
			quiet = 1;
		else if (!strcmp(opt, "--version")) {
			version(stdout);
			exit(0);
		} else if (!strcmp(opt, "--skiplock"))
			skiplock = YES;
		else
			break;
		argc--; argv++;
	}
	if (argc <= 1)
		usage(VZ_INVALID_PARAMETER_SYNTAX);
	action_nm = argv[1];
	init_log(NULL, 0, 1, verbose, quiet, NULL);
	if (!strcmp(argv[1], "set")) {
		init_modules(&g_action, "set");
		action = ACTION_SET;
	} else if (!strcmp(argv[1], "create")) {
		init_modules(&g_action, "create");
		action = ACTION_CREATE;
	} else if (!strcmp(argv[1], "start")) {
		init_modules(&g_action, "set");
		action = ACTION_START;
	} else if (!strcmp(argv[1], "stop")) {
		init_modules(&g_action, "set");
		action = ACTION_STOP;
	} else if (!strcmp(argv[1], "restart")) {
		action = ACTION_RESTART;
	} else if (!strcmp(argv[1], "destroy") || !strcmp(argv[1], "delete")) {
		action = ACTION_DESTROY;
	} else if (!strcmp(argv[1], "mount")) {
		action = ACTION_MOUNT;
	} else if (!strcmp(argv[1], "umount")) {
		action = ACTION_UMOUNT;
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
	} else if (!strcmp(argv[1], "console")) {
		action = ACTION_CONSOLE;
#ifdef HAVE_PLOOP
	} else if (!strcmp(argv[1], "convert")) {
		action = ACTION_CONVERT;
	} else if (!strcmp(argv[1], "compact")) {
		action = ACTION_COMPACT;
#endif
	} else if (!strcmp(argv[1], "status")) {
		action = ACTION_STATUS;
		quiet = 1;
	} else if (!strcmp(argv[1], "suspend") || !strcmp(argv[1], "chkpnt")) {
		action = ACTION_SUSPEND;
	} else if (!strcmp(argv[1], "resume") || !strcmp(argv[1], "restore")) {
		action = ACTION_RESUME;
	} else if (!strcmp(argv[1], "quotaon")) {
		action = ACTION_QUOTAON;
	} else if (!strcmp(argv[1], "quotaoff")) {
		action = ACTION_QUOTAOFF;
	} else if (!strcmp(argv[1], "quotainit")) {
		action = ACTION_QUOTAINIT;
#ifdef HAVE_PLOOP
	} else if (!strcmp(argv[1], "snapshot")) {
		action = ACTION_SNAPSHOT_CREATE;
	} else if (!strcmp(argv[1], "snapshot-switch")) {
		action = ACTION_SNAPSHOT_SWITCH;
	} else if (!strcmp(argv[1], "snapshot-delete")) {
		action = ACTION_SNAPSHOT_DELETE;
	} else if (!strcmp(argv[1], "snapshot-list")) {
		action = ACTION_SNAPSHOT_LIST;
#endif
	} else if (!strcmp(argv[1], "--help")) {
		usage(0);
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
		fprintf(stderr, "CT ID missing\n");
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	}
	if (parse_int(argv[2], &veid)) {
		name = strdup(argv[2]);
		veid = get_veid_by_name(name);
	}
	if (veid < 0 || veid > VEID_MAX) {
		fprintf(stderr, "Bad CT ID %s\n", argv[2]);
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	}

	argc -= 2; argv += 2;
	/* getopt_long() prints argv[0] when reporting errors */
	argv[0] = _proc_title;

	/* Read global config file */
	if (vps_parse_config(veid, GLOBAL_CFG, gparam, &g_action)) {
		ret = VZ_NOCONFIG;
		goto error;
	}
	init_log(gparam->log.log_file, veid, gparam->log.enable != NO,
		gparam->log.level, quiet, "vzctl");
	/* Set verbose level from global config if not overwriten
	   by --verbose
	*/
	if (!verbose_custom && gparam->log.verbose != NULL) {
		verbose = *gparam->log.verbose;
		verbose_custom = 1;
	}
	if (verbose < -1)
		verbose = -1;
	if (verbose_custom)
		set_log_verbose(verbose);
	if ((ret = parse_action_opt(veid, action, argc, argv, cmd_p,
		action_nm)))
	{
		goto error;
	}
	if (veid == 0 && action != ACTION_SET) {
		fprintf(stderr, "Only set actions are allowed for CT0\n");
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	} else if (veid < 0) {
		fprintf(stderr, "Bad CT ID %d\n", veid);
		ret = VZ_INVALID_PARAMETER_VALUE;
		goto error;
	}
	get_vps_conf_path(veid, buf, sizeof(buf));
	if (stat_file(buf)) {
		if (vps_parse_config(veid, buf, vps_p, &g_action)) {
			ret = VZ_NOCONFIG;
			goto error;
		}
		if (name != NULL &&
		    vps_p->res.name.name != NULL &&
		    strcmp(name, vps_p->res.name.name))
		{
			logger(-1, 0, "Unable to find container by name %s",
					name);
			ret = VZ_INVALID_PARAMETER_VALUE;
			goto error;
		}
	} else if (action != ACTION_CREATE &&
			action != ACTION_STATUS &&
			action != ACTION_SET)
	{
		logger(-1, 0, "Container config file does not exist");
		ret = VZ_NOVECONFIG;
		goto error;
	}
	merge_vps_param(gparam, vps_p);
	merge_global_param(cmd_p, gparam);
	ret = run_action(veid, action, gparam, vps_p, cmd_p, argc, argv,
		skiplock);

error:
	free_modules(&g_action);
	free_vps_param(gparam);
	free_vps_param(vps_p);
	free_vps_param(cmd_p);
	free_log();
	free(name);

	return ret;
}
