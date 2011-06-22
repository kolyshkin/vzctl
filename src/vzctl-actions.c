/*
 *  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <linux/vzcalluser.h>
#include <errno.h>
#include <signal.h>

#include "vzctl.h"
#include "vzctl_param.h"
#include "env.h"
#include "logger.h"
#include "exec.h"
#include "config.h"
#include "vzerror.h"
#include "create.h"
#include "destroy.h"
#include "util.h"
#include "lock.h"
#include "vps_configure.h"
#include "modules.h"
#include "io.h"

extern struct mod_action g_action;
extern int do_enter(vps_handler *h, envid_t veid, const char *root,
			int argc, char **argv);

static struct option set_opt[] = {
	{"save",	no_argument, NULL, PARAM_SAVE},
	{"force",	no_argument, NULL, PARAM_FORCE},
	{"applyconfig",	required_argument, NULL, PARAM_APPLYCONFIG},
	{"applyconfig_map",
			required_argument, NULL, PARAM_APPLYCONFIG_MAP},
	{"reset_ub",	no_argument, NULL, PARAM_RESET_UB},
	{"iptables",	required_argument, NULL, PARAM_IPTABLES},
	/*	UB	*/
	{"kmemsize",	required_argument, NULL, PARAM_KMEMSIZE},
	{"lockedpages",	required_argument, NULL, PARAM_LOCKEDPAGES},
	{"privvmpages",	required_argument, NULL, PARAM_PRIVVMPAGES},
	{"shmpages",	required_argument, NULL, PARAM_SHMPAGES},
	{"numproc",	required_argument, NULL, PARAM_NUMPROC},
	{"physpages",	required_argument, NULL, PARAM_PHYSPAGES},
	{"vmguarpages",	required_argument, NULL, PARAM_VMGUARPAGES},
	{"oomguarpages",required_argument, NULL, PARAM_OOMGUARPAGES},
	{"numtcpsock",	required_argument, NULL, PARAM_NUMTCPSOCK},
	{"numflock",	required_argument, NULL, PARAM_NUMFLOCK},
	{"numpty",	required_argument, NULL, PARAM_NUMPTY},
	{"numsiginfo",	required_argument, NULL, PARAM_NUMSIGINFO},
	{"tcpsndbuf",	required_argument, NULL, PARAM_TCPSNDBUF},
	{"tcprcvbuf",	required_argument, NULL, PARAM_TCPRCVBUF},
	{"othersockbuf",required_argument, NULL, PARAM_OTHERSOCKBUF},
	{"dgramrcvbuf",	required_argument, NULL, PARAM_DGRAMRCVBUF},
	{"numothersock",required_argument, NULL, PARAM_NUMOTHERSOCK},
	{"numfile",	required_argument, NULL, PARAM_NUMFILE},
	{"dcachesize",	required_argument, NULL, PARAM_DCACHESIZE},
	{"numiptent",	required_argument, NULL, PARAM_NUMIPTENT},
	{"avnumproc",	required_argument, NULL, PARAM_AVNUMPROC},
	{"swappages",	required_argument, NULL, PARAM_SWAPPAGES},
	/*	Capability */
	{"capability",	required_argument, NULL, PARAM_CAP},
	/*	Network	*/
	{"ipadd",	required_argument, NULL, PARAM_IP_ADD},
	{"ip",		required_argument, NULL, PARAM_IP_ADD},
	{"ipdel",	required_argument, NULL, PARAM_IP_DEL},
	{"skip_arpdetect",
			no_argument, NULL, PARAM_SKIPARPDETECT},
	{"netdev_add",	required_argument, NULL, PARAM_NETDEV_ADD},
	{"netdev_del",	required_argument, NULL, PARAM_NETDEV_DEL},
	{"hostname",	required_argument, NULL, PARAM_HOSTNAME},
	{"nameserver",	required_argument, NULL, PARAM_NAMESERVER},
	{"searchdomain",required_argument, NULL, PARAM_SEARCHDOMAIN},
	{"userpasswd",	required_argument, NULL, PARAM_USERPW},
	/*	Devices	*/
	{"devices",	required_argument, NULL, PARAM_DEVICES},
	{"devnodes",	required_argument, NULL, PARAM_DEVNODES},
	{"pci_add",	required_argument, NULL, PARAM_PCI_ADD},
	{"pci_del",	required_argument, NULL, PARAM_PCI_DEL},
	/*	fs param */
	{"root",	required_argument, NULL, PARAM_ROOT},
	{"private",	required_argument, NULL, PARAM_PRIVATE},
	{"noatime",	required_argument, NULL, PARAM_NOATIME},
	/*	template	*/
	{"ostemplate",	required_argument, NULL, PARAM_OSTEMPLATE},
	/*	Cpu	*/
	{"cpuunits",	required_argument, NULL, PARAM_CPUUNITS},
	{"cpuweight",	required_argument, NULL, PARAM_CPUWEIGHT},
	{"cpulimit",	required_argument, NULL, PARAM_CPULIMIT},
	{"cpus",	required_argument, NULL, PARAM_VCPUS},
	{"cpumask",	required_argument, NULL, PARAM_CPUMASK},
	/*	create param	*/
	{"onboot",	required_argument, NULL, PARAM_ONBOOT},
	{"setmode",	required_argument, NULL, PARAM_SETMODE},
	{"disabled",	required_argument, NULL, PARAM_DISABLED},
	/*	quota */
	{"diskquota",	required_argument, NULL, PARAM_DISK_QUOTA},
	{"diskspace",	required_argument, NULL, PARAM_DISKSPACE},
	{"diskinodes",	required_argument, NULL, PARAM_DISKINODES},
	{"quotatime",	required_argument, NULL, PARAM_QUOTATIME},
	{"quotaugidlimit",
			required_argument, NULL, PARAM_QUOTAUGIDLIMIT},
	{"meminfo",	required_argument, NULL, PARAM_MEMINFO},
	/*	netif	*/
	{"netif_add",	required_argument, NULL, PARAM_NETIF_ADD_CMD},
	{"netif_del",	required_argument, NULL, PARAM_NETIF_DEL_CMD},
	{"mac_filter",	required_argument, NULL, PARAM_NETIF_MAC_FILTER},

	{"mac",		required_argument, NULL, PARAM_NETIF_MAC},
	{"ifname",	required_argument, NULL, PARAM_NETIF_IFNAME},
	{"host_mac",	required_argument, NULL, PARAM_NETIF_HOST_MAC},
	{"host_ifname",	required_argument, NULL, PARAM_NETIF_HOST_IFNAME},
	{"bridge",	required_argument, NULL, PARAM_NETIF_BRIDGE},

	/*	name	*/
	{"name",	required_argument, NULL, PARAM_NAME},
	{"features",	required_argument, NULL, PARAM_FEATURES},
	{"ioprio",	required_argument, NULL, PARAM_IOPRIO},
	{"description",	required_argument, NULL, PARAM_DESCRIPTION},
	{"bootorder",	required_argument, NULL, PARAM_BOOTORDER},

	{NULL, 0, NULL, 0}
};

static int parse_opt(envid_t veid, int argc, char *argv[], struct option *opt,
	vps_param *param)
{
	int c, ret;

	while (1) {
		int option_index = -1;

		c = getopt_long(argc, argv, PARAM_LINE, opt, &option_index);
		if (c == -1)
			break;
		if (c == '?')
			return VZ_INVALID_PARAMETER_VALUE;
		ret = vps_parse_opt(veid, opt, param, c, optarg, &g_action);
		switch (ret) {
			case 0:
			case ERR_DUP:
				continue;
			case ERR_INVAL:
			case ERR_INVAL_SKIP:
			case ERR_LONG_TRUNC:
				if (option_index < 0) {
					logger(-1, 0, "Bad parameter for"
						" -%c : %s", c, optarg);
					return VZ_INVALID_PARAMETER_VALUE;
				} else {
					logger(-1, 0, "Bad parameter for"
						" --%s: %s",
						opt[option_index].name,
						optarg);
					return VZ_INVALID_PARAMETER_VALUE;
				}
			case ERR_UNK:
				/* This shouldn't happen -- getopt_long()
				 * should have catched this already
				 */
				if (option_index < 0)
					logger(-1, 0, "Invalid option -%c", c);
				else
					logger(-1, 0, "Invalid option --%s",
						opt[option_index].name);
				return VZ_INVALID_PARAMETER_SYNTAX;
			case ERR_NOMEM:
				logger(-1, 0, "Not enough memory");
				return VZ_RESOURCE_ERROR;
			case ERR_OTHER:
				logger(-1, 0, "Error parsing options");
				return VZ_SYSTEM_ERROR;
		}
	}
	if (optind < argc) {
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
			printf ("%s ", argv[optind++]);
		printf ("\n");
		return VZ_INVALID_PARAMETER_SYNTAX;
	}
	return 0;
}

static int parse_startstop_opt(int argc, char **argv, vps_param *param,
		int start, int stop)
{
	int c, ret = 0;
	struct option start_options[] = {
		{"force", no_argument, NULL, PARAM_FORCE},
		{"skip_ve_setup", no_argument, NULL, PARAM_SKIP_VE_SETUP},
		{"wait", no_argument, NULL, PARAM_WAIT},
		{"fast", no_argument, NULL, PARAM_FAST},
		{ NULL, 0, NULL, 0 }
	};

	while (1) {
		c = getopt_long (argc, argv, "", start_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case PARAM_FORCE:
			if (start)
				param->opt.start_force = YES;
			else
				ret = VZ_INVALID_PARAMETER_SYNTAX;
			break;
		case PARAM_SKIP_VE_SETUP:
			if (start)
				param->opt.skip_setup = YES;
			else
				ret = VZ_INVALID_PARAMETER_SYNTAX;
			break;
		case PARAM_WAIT:
			if (start)
				param->res.misc.wait = YES;
			else
				ret = VZ_INVALID_PARAMETER_SYNTAX;
			break;
		case PARAM_FAST:
			if (stop)
				param->opt.fast_kill = YES;
			else
				ret = VZ_INVALID_PARAMETER_SYNTAX;
			break;
		default:
			ret = VZ_INVALID_PARAMETER_SYNTAX;
			break;
		}
	}
	return ret;
}

static int start(vps_handler *h, envid_t veid, vps_param *g_p,
	vps_param *cmd_p)
{
	int ret;

	if (g_p->opt.start_disabled == YES &&
		cmd_p->opt.start_force != YES)
	{
		logger(-1, 0, "Container start disabled");
		return VZ_VE_START_DISABLED;
	}
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "Container is already running");
		return VZ_VE_RUNNING;
	}
	g_p->res.misc.wait = cmd_p->res.misc.wait;
	ret = vps_start(h, veid, g_p,
		cmd_p->opt.skip_setup == YES ? SKIP_SETUP : 0, &g_action);
	return ret;
}

static int stop(vps_handler *h, envid_t veid, vps_param *g_p, vps_param *cmd_p)
{
	int ret;
	int stop_mode;

	if (cmd_p->opt.fast_kill == YES)
		stop_mode = M_KILL;
	else
		stop_mode = M_HALT;
	ret = vps_stop(h, veid, g_p, stop_mode, 0, &g_action);

	return ret;
}

static int restart(vps_handler *h, envid_t veid, vps_param *g_p,
	vps_param *cmd_p)
{
	int ret;

	logger(0, 0, "Restarting container");

	if (vps_is_run(h, veid)) {
		ret = stop(h, veid, g_p, cmd_p);
		if (ret != 0)
			return ret;
	}

	return start(h, veid, g_p, cmd_p);
}

static int parse_create_opt(envid_t veid, int argc, char **argv,
	vps_param *param)
{
	int ret;
	struct option *opt;
	struct option create_options[] = {
	{"ostemplate",	required_argument, NULL, PARAM_OSTEMPLATE},
	{"config",	required_argument, NULL, PARAM_CONFIG},
	{"private",	required_argument, NULL, PARAM_PRIVATE},
	{"root",	required_argument, NULL, PARAM_ROOT},
	{"ipadd",	required_argument, NULL, PARAM_IP_ADD},
	{"hostname",	required_argument, NULL, PARAM_HOSTNAME},
	{"name",	required_argument, NULL, PARAM_NAME},
	{"description",	required_argument, NULL, PARAM_DESCRIPTION},
	{ NULL, 0, NULL, 0 }
};

	opt = mod_make_opt(create_options, &g_action, NULL);
	if (opt == NULL)
		return VZ_RESOURCE_ERROR;
	ret = parse_opt(veid, argc, argv, opt, param);
	free(opt);

	return ret;
}

static int create(vps_handler *h, envid_t veid, vps_param *vps_p,
	vps_param *cmd_p)
{
	int ret;

	ret = vps_create(h, veid, vps_p, cmd_p, &g_action);

	return ret;
}

static int destroy(vps_handler *h, envid_t veid, vps_param *g_p,
	vps_param *cmd_p)
{
	int ret;

	ret = vps_destroy(h, veid, &g_p->res.fs);
	if (!ret)
		remove_names(veid);
	return ret;
}

static int parse_chkpnt_opt(int argc, char **argv, vps_param *vps_p)
{
	int c, ret;
	int option_index;
	cpt_param *cpt = &vps_p->res.cpt;
	static struct option chkpnt_options[] = {
	/*	sub commands	*/
	{"dump",	no_argument, NULL, PARAM_DUMP},
	{"suspend",	no_argument, NULL, PARAM_SUSPEND},
	{"resume",	no_argument, NULL, PARAM_RESUME},
	{"kill",	no_argument, NULL, PARAM_KILL},
	{"skip_arpdetect", no_argument, NULL, PARAM_SKIPARPDETECT},
	/*	flags		*/
	{"flags",	required_argument, NULL, PARAM_CPU_FLAGS},
	{"context",	required_argument, NULL, PARAM_CPTCONTEXT},
	{"dumpfile",	required_argument, NULL, PARAM_DUMPFILE},
	{ NULL, 0, NULL, 0 }
	};

	ret = 0;
	while (1) {
		option_index = -1;
		c = getopt_long (argc, argv, "", chkpnt_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case PARAM_DUMPFILE:
			cpt->dumpfile = strdup(optarg);
			break;
		case PARAM_CPTCONTEXT:
			cpt->ctx = strtoul(optarg, NULL, 16);
			break;
		case PARAM_CPU_FLAGS:
			cpt->cpu_flags = strtoul(optarg, NULL, 0);
			break;
		case PARAM_DUMP:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_DUMP;
			break;
		case PARAM_KILL:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_KILL;
			break;
		case PARAM_RESUME:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_RESUME;
			break;
		case PARAM_SUSPEND:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_SUSPEND;
			break;
		case PARAM_SKIPARPDETECT:
			vps_p->res.net.skip_arpdetect = YES;
			break;
		default:
			if (option_index < 0)
				logger(-1, 0, "Invalid option -%c", c);
			else
				logger(-1, 0, "Invalid option --%s",
					chkpnt_options[option_index].name);
			return VZ_INVALID_PARAMETER_SYNTAX;
		}
	}
	/* Do full checkpointing */
	if (!cpt->cmd)
		cpt->cmd = CMD_CHKPNT;
	return 0;

err_syntax:
	logger(-1, 0, "Invalid syntax: only one sub command may be used");
	return VZ_INVALID_PARAMETER_SYNTAX;
}

static int parse_restore_opt(int argc, char **argv, vps_param *vps_p)
{
	int c;
	int option_index;
	cpt_param *cpt = &vps_p->res.cpt;
	static struct option restore_options[] = {
	/*	sub commands	*/
	{"undump",	no_argument, NULL, PARAM_UNDUMP},
	{"kill",	no_argument, NULL, PARAM_KILL},
	{"resume",	no_argument, NULL, PARAM_RESUME},
	/*	flags		*/
	{"dumpfile",	required_argument, NULL, PARAM_DUMPFILE},
	{"flags",	required_argument, NULL, PARAM_CPU_FLAGS},
	{"context",	required_argument, NULL, PARAM_CPTCONTEXT},
	{"skip_arpdetect", no_argument, NULL, PARAM_SKIPARPDETECT},
	{ NULL, 0, NULL, 0 }
	};

	while (1) {
		option_index = -1;
		c = getopt_long (argc, argv, "", restore_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case PARAM_DUMPFILE:
			cpt->dumpfile = strdup(optarg);
			break;
		case PARAM_CPTCONTEXT:
			cpt->ctx = strtoul(optarg, NULL, 16);
			break;
		case PARAM_CPU_FLAGS:
			cpt->cpu_flags = strtoul(optarg, NULL, 0);
			break;
		case PARAM_UNDUMP:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_UNDUMP;
			break;
		case PARAM_KILL:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_KILL;
			break;
		case PARAM_RESUME:
			if (cpt->cmd)
				goto err_syntax;
			cpt->cmd = CMD_RESUME;
			break;
		case PARAM_SKIPARPDETECT:
			vps_p->res.net.skip_arpdetect = YES;
			break;
		default:
			if (option_index < 0)
				logger(-1, 0, "Invalid option -%c", c);
			else
				logger(-1, 0, "Invalid option --%s",
					restore_options[option_index].name);
			return VZ_INVALID_PARAMETER_SYNTAX;
		}
	}
	/* Do full restore */
	if (!cpt->cmd)
		cpt->cmd = CMD_RESTORE;
	return 0;
err_syntax:
	logger(-1, 0, "Invalid syntax: only one sub command may be used");
	return VZ_INVALID_PARAMETER_SYNTAX;
}

static int check_set_ugidlimit(unsigned long *cur, unsigned long *old,
		int loud)
{
	unsigned long c, o = 0;

	if (!cur)
		return 0;
	c = *cur;
	if (old)
		o = *old;

	if (c != 0 && o == 0) {
		if (loud)
			logger(-1, 0, "Unable to turn on second-level"
				" disk quota on a running container");
		return 1;
	}

	if (c == 0 && o != 0) {
		if (loud)
			logger(-1, 0, "Unable to turn off second-level"
				" disk quota on a running container");
		return 1;
	}

	return 0;
}

/* Check parameters that can't be set on running CT */
static int check_set_mode(vps_handler *h, envid_t veid, int setmode, int apply,
	vps_res *new_res, vps_res *old_res)
{
	int found = 0;
	int loud = (setmode != SET_RESTART);

	/* If some caps are set and they differ from the old ones */
	if ( (new_res->cap.on &&
				(new_res->cap.on != old_res->cap.on)) ||
		(new_res->cap.off &&
				(new_res->cap.off != old_res->cap.off)) ) {
		if (loud)
			logger(-1, 0, "Unable to set capability "
					"on a running container");
		found++;
	}
	/* If iptables mask is set and it differs from the old one.
	 * FIXME: we don't catch the case when the new set is empty
	 * and the old one is not (vzctl set --iptables '') */
	if (new_res->env.ipt_mask &&
			new_res->env.ipt_mask != old_res->env.ipt_mask)
	{
		if (loud)
			logger(-1, 0, "Unable to set iptables "
					"on a running container");
		found++;
	}
	/* If some features are set (to either on or off)
	 * and the new mask differs from the old one */
	if (new_res->env.features_known &&
			new_res->env.features_mask !=
			old_res->env.features_mask)
	{
		if (loud)
			logger(-1, 0, "Unable to set features "
					"on a running container");
		found++;
	}
	/* Turning quota ugid limit on/off */
	found += check_set_ugidlimit(new_res->dq.ugidlimit,
			old_res->dq.ugidlimit, loud);
	/* Enabling/disabling DISK_QUOTA */
	if ( (new_res->dq.enable) &&
			(new_res->dq.enable != old_res->dq.enable) ) {
		if (loud)
			logger(-1, 0, "Unable to switch DISK_QUOTA on or off "
					"on a running container");
		found++;
	}

	if (!found)
		return 0;

	switch (setmode) {
		case SET_RESTART:
			return 1;
		case SET_IGNORE:
			return -1;
		case SET_NONE:
		default:
			logger(-1, 0, "WARNING: Some of the parameters could "
				"not be applied to a running container.\n"
				"\tPlease consider using --setmode option");
			return 0;
	}
}

static void merge_apply_param(vps_param *old, vps_param *new, char *cfg)
{
#define FREE_STR(x)	free(x); x = NULL;

	FREE_STR(new->res.fs.root_orig)
	FREE_STR(new->res.fs.private_orig)
	FREE_STR(new->res.tmpl.ostmpl)
	FREE_STR(new->res.tmpl.dist)
	FREE_STR(new->res.misc.hostname)
	FREE_STR(new->res.misc.description)
#undef FREE_STR
	free_str_param(&new->res.net.ip);
	if (new->opt.origin_sample == NULL)
		new->opt.origin_sample = strdup(cfg);
	merge_vps_param(old, new);
}

static int apply_param_from_cfg(int veid, vps_param *param, char *cfg)
{
	char conf[STR_SIZE];
	vps_param *new;

	if (cfg == NULL)
		return 0;
	snprintf(conf, sizeof(conf), VPS_CONF_DIR "ve-%s.conf-sample", cfg);
	if (!stat_file(conf)) {
		logger(-1, 0, "Sample config file does not found: %s", conf);
		return VZ_APPLY_CONFIG_ERROR;
	}
	new = init_vps_param();
	vps_parse_config(veid, conf, new, &g_action);
	merge_apply_param(param, new, cfg);
	free_vps_param(new);
	return 0;
}

static int parse_set_opt(envid_t veid, int argc, char *argv[],
	vps_param *param)
{
	int ret;
	struct option *opt;

	opt = mod_make_opt(set_opt, &g_action, NULL);
	if (opt == NULL)
		return VZ_RESOURCE_ERROR;
	ret = parse_opt(veid, argc, argv, opt, param);
	free(opt);

	return ret;
}

static int set_ve0(vps_handler *h, vps_param *g_p,
	vps_param *vps_p, vps_param *cmd_p)
{
	int ret;
	ub_param *ub;
	cpu_param *cpu = NULL;

	if (cmd_p->opt.reset_ub == YES) {
		/* Apply parameters from config */
		ub = &vps_p->res.ub;
		cmd_p->opt.save = NO; // suppress savewarning
	} else {
		/* Apply parameters from command line */
		ub = &cmd_p->res.ub;
		cpu = &cmd_p->res.cpu;
	}
	ret = vps_set_ublimit(h, 0, ub);
	if (ret)
		return ret;
	if ((ret = ve_ioprio_set(h, 0, &cmd_p->res.io)))
		return ret;
	if (cpu != NULL)
		if ((ret = hn_set_cpu(cpu)))
			return ret;
	return 0;
}

static int set(vps_handler *h, envid_t veid, vps_param *g_p, vps_param *vps_p,
	vps_param *cmd_p)
{
	int ret, is_run;
	dist_actions *actions = NULL;
	char *dist_name;

	ret = 0;

	if (!list_empty(&cmd_p->res.veth.dev) ||
	    !list_empty(&cmd_p->del_res.veth.dev))
	{
		ret = check_veth_param(veid, &vps_p->res.veth, &cmd_p->res.veth,
			&cmd_p->del_res.veth);
		if (ret) {
			cmd_p->opt.save = NO;
			return ret;
		}
	}

	cmd_p->g_param = g_p;
	if (cmd_p->opt.apply_cfg_map == APPCONF_MAP_NAME) {
		ret = set_name(veid, g_p->res.name.name, g_p->res.name.name);
		if (ret != 0)
			return ret;
	}
	/* Reset UB parameters from config  */
	if (cmd_p->opt.reset_ub == YES && h != NULL) {
		ret = vps_set_ublimit(h, veid, &vps_p->res.ub);
		cmd_p->opt.save = NO; // suppress savewarning
		return ret;
	}
	if (cmd_p->opt.setmode == SET_RESTART && !cmd_p->opt.save) {
		logger(-1, 0, "Error: --setmode restart doesn't make sense"
			" without --save flag");
		return VZ_INVALID_PARAMETER_SYNTAX;
	}
	if (cmd_p->res.name.name != NULL) {
		if (!cmd_p->opt.save) {
			logger(-1, 0, "Error: unable to use"
				 " --name option without --save");
			ret =  VZ_SET_NAME_ERROR;
		} else {
			ret = set_name(veid, cmd_p->res.name.name,
				vps_p->res.name.name);
		}
		if (ret) {
			cmd_p->opt.save = NO;
			return ret;
		}
	}
	is_run = h != NULL && vps_is_run(h, veid);
	if (is_run) {
		if (cmd_p->res.fs.private_orig != NULL) {
			free(cmd_p->res.fs.private_orig);
			cmd_p->res.fs.private_orig = NULL;
			logger(-1, 0,"Unable to change VE_PRIVATE "
					"on a running container");
			return VZ_VE_RUNNING;
		}
		if (cmd_p->res.fs.root_orig != NULL) {
			free(cmd_p->res.fs.root_orig);
			cmd_p->res.fs.root_orig = NULL;
			logger(-1, 0, "Unable to change VE_ROOT "
					"on a running container");
			return VZ_VE_RUNNING;
		}
	}
	if (need_configure(&cmd_p->res) ||
		need_configure(&cmd_p->del_res) ||
		!list_empty(&cmd_p->res.misc.userpw))
	{
		actions = vz_malloc(sizeof(*actions));
		if (!actions)
			return VZ_RESOURCE_ERROR;
		dist_name = get_dist_name(&g_p->res.tmpl);
		if ((ret = read_dist_actions(dist_name, DIST_DIR, actions)))
			return ret;
		free(dist_name);
	}
	/* Setup password */
	if (h != NULL && !list_empty(&cmd_p->res.misc.userpw)) {
		if (!is_run)
			if ((ret = vps_start(h, veid, g_p,
				SKIP_SETUP|SKIP_ACTION_SCRIPT, NULL)))
			{
				goto err;
			}
		ret = vps_pw_configure(h, veid, actions, g_p->res.fs.root,
			&cmd_p->res.misc.userpw);
		if (!is_run)
			vps_stop(h, veid, g_p, M_KILL, SKIP_ACTION_SCRIPT,
				NULL);
		if (ret)
			goto err;
	}
	if (cmd_p->opt.apply_cfg != NULL) {
		if ((ret = apply_param_from_cfg(veid, cmd_p,
			cmd_p->opt.apply_cfg)))
		{
			goto err;
		}
	}
	/* Skip applying parameters on stopped CT */
	if (cmd_p->opt.save && !is_run) {
		ret = mod_setup(h, veid, STATE_STOPPED, SKIP_NONE, &g_action,
			cmd_p);
		return ret;
	}
	if (is_run) {
		ret = check_set_mode(h, veid, cmd_p->opt.setmode,
			cmd_p->opt.save, &cmd_p->res, &g_p->res);
		if (ret) {
			if (ret < 0) {
				ret = VZ_VE_RUNNING;
				goto err;
			} else {
				merge_vps_param(g_p, cmd_p);
				ret = restart(h, veid, g_p, cmd_p);
				goto err;
			}
		}
	}
	ret = vps_setup_res(h, veid, actions, &g_p->res.fs, cmd_p,
		STATE_RUNNING, SKIP_NONE, &g_action);
err:
	free_dist_actions(actions);
	free(actions);

	return ret;
}

static int mount(vps_handler *h, envid_t veid, vps_param *g_p,
	vps_param *cmd_p)
{
	return vps_mount(h, veid, &g_p->res.fs, &g_p->res.dq, 0);
}

static int umount(vps_handler *h, envid_t veid, vps_param *g_p,
	vps_param *cmd_p)
{
	return vps_umount(h, veid, g_p->res.fs.root, 0);
}

static int enter(vps_handler *h, envid_t veid, const char *root,
		int argc, char **argv)
{
	set_log_file(NULL);
	if (check_var(root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if (argc > 0) {
		/* omit "--exec" argument */
		argc -= 1; argv += 1;
	}
	return do_enter(h, veid, root, argc, argv);
}

static int exec(vps_handler *h, act_t action, envid_t veid, const char *root,
		int argc, char **argv)
{
	int mode;
	char **arg = NULL;
	char *argv_bash[] = {"bash", "-c", NULL, NULL};
	char *buf = NULL;
	int i, ret;
	int len, totallen = 0;

	/* disable logging on terminal  */
	set_log_verbose(0);
	mode = MODE_BASH;
	if (action == ACTION_EXEC3) {
		arg = argv;
		mode = MODE_EXEC;
	} else if (argc && strcmp(argv[0], "-")) {
		for (i = 0; i < argc; i++) {
			len = strlen(argv[i]);
			if (len) {
				buf = (char*)realloc(buf, totallen + len + 2);
				sprintf(buf + totallen, "%s ", argv[i]);
			} else {
				/* set empty argument */
				len = 2;
				buf = (char*)realloc(buf, totallen + len + 2);
				sprintf(buf + totallen, "\'\' ");
			}
			totallen += len + 1;
			buf[totallen] = 0;
		}
		argv_bash[2] = buf;
		arg = argv_bash;
	}
	ret = vps_exec(h, veid, root, mode, arg, NULL, NULL, 0);

	return ret;
}

static int chkpnt(vps_handler *h, envid_t veid, vps_param *g_p, vps_param *cmd_p)
{
	int cmd;

	cmd = cmd_p->res.cpt.cmd;
	merge_vps_param(g_p, cmd_p);
	if (cmd == CMD_KILL || cmd == CMD_RESUME)
		return cpt_cmd(h, veid, CMD_CHKPNT, &cmd_p->res.cpt, g_p);
	return vps_chkpnt(h, veid, g_p, cmd, &cmd_p->res.cpt);
}

static int restore(vps_handler *h, envid_t veid, vps_param *g_p,
	vps_param *cmd_p)
{
	int cmd;

	cmd = cmd_p->res.cpt.cmd;
	merge_vps_param(g_p, cmd_p);
	if (cmd == CMD_KILL || cmd == CMD_RESUME)
		return cpt_cmd(h, veid, CMD_RESTORE, &cmd_p->res.cpt, g_p);
	return vps_restore(h, veid, g_p, cmd, &cmd_p->res.cpt);
}

static int show_status(vps_handler *h, envid_t veid, vps_param *param)
{
	int exist = 0, mounted = 0, run = 0, suspended = 0;
	char buf[STR_SIZE];
	fs_param *fs = &param->res.fs;

	get_vps_conf_path(veid, buf, sizeof(buf));
	if (fs->private != NULL && stat_file(fs->private) && stat_file(buf))
		exist = 1;
	mounted = vps_is_mounted(fs->root);
	run = vps_is_run(h, veid);
	if (exist == 1) {
		get_dump_file(veid, param->res.cpt.dumpdir, buf, sizeof(buf));
		if (stat_file(buf))
			suspended = 1;
	}
	printf("CTID %d %s %s %s%s\n", veid,
		exist ? "exist" : "deleted",
		mounted ? "mounted" : "unmounted",
		run ? "running" : "down",
		suspended ? " suspended" : "");
	return 0;
}

static int parse_custom_opt(envid_t veid, int argc, char **argv,
	vps_param *param, const char *name)
{
	int ret;
	struct option *opt;

	opt = mod_make_opt(NULL, &g_action, name);
	if (opt == NULL)
		return VZ_RESOURCE_ERROR;
	ret = parse_opt(veid, argc, argv, opt, param);
	free(opt);

	return ret;
}

int parse_action_opt(envid_t veid, act_t action, int argc, char *argv[],
	vps_param *param, const char *name)
{
	int ret = 0;

	switch (action)	{
	case ACTION_SET:
		ret = parse_set_opt(veid, argc, argv, param);
		break;
	case ACTION_CREATE:
		ret = parse_create_opt(veid, argc, argv, param);
		break;
	case ACTION_START:
		ret = parse_startstop_opt(argc, argv, param, 1, 0);
		break;
	case ACTION_STOP:
		ret = parse_startstop_opt(argc, argv, param, 0, 1);
		break;
	case ACTION_RESTART:
		ret = parse_startstop_opt(argc, argv, param, 1, 1);
		break;
	case ACTION_ENTER:
		if (argc >= 2) {
			if (!strcmp(argv[1], "--exec")) {
				if ((argc == 2) || (*argv[2] == '\0')) {
					fprintf(stderr,
						"No command line "
						"given for --exec\n");
					ret = VZ_INVALID_PARAMETER_SYNTAX;
				}
			} else {
				fprintf(stderr,
					"Invalid option '%s'\n", argv[1]);
				ret = VZ_INVALID_PARAMETER_SYNTAX;
			}
		}
		break;
	case ACTION_DESTROY:
		break;
	case ACTION_MOUNT:
		break;
	case ACTION_EXEC:
	case ACTION_EXEC2:
	case ACTION_EXEC3:
		if (argc < 2) {
			fprintf(stderr, "No command line given for exec\n");
			ret = VZ_INVALID_PARAMETER_SYNTAX;
		}
		break;
	case ACTION_RUNSCRIPT:
		if (argc < 2) {
			fprintf(stderr, "Script not specified\n");
			ret = VZ_INVALID_PARAMETER_SYNTAX;
		}
		break;
	case ACTION_CUSTOM:
		ret = parse_custom_opt(veid, argc, argv, param, name);
		break;
	case ACTION_CHKPNT:
		ret = parse_chkpnt_opt(argc, argv, param);
		break;
	case ACTION_RESTORE:
		ret = parse_restore_opt(argc, argv, param);
		break;
	default :
		if ((argc - 1) > 0) {
			fprintf (stderr, "Invalid options: ");
			while (--argc)
				fprintf(stderr, "%s ", *(++argv));
			fprintf (stderr, "\n");
			ret = VZ_INVALID_PARAMETER_SYNTAX;
		}
	}
	return ret;
}

int run_action(envid_t veid, act_t action, vps_param *g_p, vps_param *vps_p,
	vps_param *cmd_p, int argc, char **argv, int skiplock)
{
	vps_handler *h = NULL;
	int ret, lock_id = -1;
	struct sigaction act;
	char fname[STR_SIZE];

	ret = 0;
	if ((h = vz_open(veid)) == NULL) {
		/* Accept to run "set --save --force" on non-openvz
		 * kernel */
		if (action != ACTION_SET ||
		    cmd_p->opt.save_force != YES ||
		    cmd_p->opt.save != YES)
		{
			return VZ_BAD_KERNEL;
		}
	}
	if (action != ACTION_EXEC &&
		action != ACTION_EXEC2 &&
		action != ACTION_EXEC3 &&
		action != ACTION_ENTER &&
		action != ACTION_STATUS)
	{
		if (skiplock != YES) {
			lock_id = vps_lock(veid, g_p->opt.lockdir, "");
			if (lock_id > 0) {
				logger(-1, 0, "Container already locked");
				ret = VZ_LOCKED;
				goto err;
			} else if (lock_id < 0) {
				logger(-1, 0, "Unable to lock container");
				ret = VZ_SYSTEM_ERROR;
				goto err;
			}
		}
		sigemptyset(&act.sa_mask);
		act.sa_handler = SIG_IGN;
		act.sa_flags = 0;
		sigaction(SIGINT, &act, NULL);
	}
	switch (action) {
	case ACTION_CREATE:
		ret = create(h, veid, g_p, cmd_p);
		break;
	case ACTION_DESTROY:
		ret = destroy(h, veid, g_p, cmd_p);
		break;
	case ACTION_MOUNT:
		ret = mount(h, veid, g_p, cmd_p);
		break;
	case ACTION_UMOUNT:
		ret = umount(h, veid, g_p, cmd_p);
		break;
	case ACTION_START:
		ret = start(h, veid, g_p, cmd_p);
		break;
	case ACTION_STOP:
		ret = stop(h, veid, g_p, cmd_p);
		break;
	case ACTION_RESTART:
		ret = restart(h, veid, g_p, cmd_p);
		break;
	case ACTION_SET:
		if (veid == 0)
			ret = set_ve0(h, g_p, vps_p, cmd_p);
		else
			ret = set(h, veid, g_p, vps_p, cmd_p);
		if (cmd_p->opt.save == YES) {
			get_vps_conf_path(veid, fname, sizeof(fname));
			/* Warn if config does not exist */
			if (!stat_file(fname))
				logger(-1, errno, "WARNING: %s not found",
					fname);
			ret = vps_save_config(veid, fname,
					cmd_p, vps_p, &g_action);
		} else if (cmd_p->opt.save != NO) {
			if (list_empty(&cmd_p->res.misc.userpw)) {
				logger(0, 0, "WARNING: Settings were not saved"
				" and will be reset to original values at the"
				" next start (use --save flag)");
			}
		}
		break;
	case ACTION_STATUS:
		ret = show_status(h, veid, g_p);
		break;
	case ACTION_ENTER:
		ret = enter(h, veid, g_p->res.fs.root, argc, argv);
		break;
	case ACTION_EXEC:
	case ACTION_EXEC2:
	case ACTION_EXEC3:
		ret = exec(h, action, veid, g_p->res.fs.root, argc, argv);
		if (ret && action == ACTION_EXEC)
			ret = VZ_COMMAND_EXECUTION_ERROR;
		break;
	case ACTION_CHKPNT:
		ret = chkpnt(h, veid, g_p, cmd_p);
		break;
	case ACTION_RESTORE:
		ret = restore(h, veid, g_p, cmd_p);
		break;
	case ACTION_RUNSCRIPT:
		ret = vps_run_script(h, veid, argv[0], g_p);
		break;

#define CHECK_DQ(r) if (g_p->res.dq.enable != YES) { \
	ret = r; \
	logger(0, 0, "Disk quota is not enabled, skipping operation"); \
	break; \
}
	case ACTION_QUOTAON:
		CHECK_DQ(VZ_DQ_ON)
		ret = vps_quotaon(veid, g_p->res.fs.private, &g_p->res.dq);
		break;
	case ACTION_QUOTAOFF:
		CHECK_DQ(VZ_DQ_OFF)
		ret = vps_quotaoff(veid, &g_p->res.dq);
		break;
	case ACTION_QUOTAINIT:
		CHECK_DQ(VZ_DQ_INIT)
		ret = quota_init(veid, g_p->res.fs.private, &g_p->res.dq);
		break;
#undef CHECK_DQ

	case ACTION_CUSTOM:
		ret = mod_setup(h, veid, 0, 0, &g_action, g_p);
		break;
	}
err:
	/* Unlock CT in case lock taken */
	if (skiplock != YES && !lock_id)
		vps_unlock(veid, g_p->opt.lockdir);
	vz_close(h);
	return ret;
}
