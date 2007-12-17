/*
 *  Copyright (C) 2000-2007 SWsoft. All rights reserved.
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
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/vzcalluser.h>
#include <unistd.h>
#include <fcntl.h>

#include "logger.h"
#include "list.h"
#include "config.h"
#include "vzctl_param.h"
#include "vzerror.h"
#include "util.h"
#include "ub.h"
#include "vzctl.h"
#include "res.h"
#include "iptables.h"
#include "meminfo.h"
#include "vzfeatures.h"
#include "io.h"

static int _page_size;
static int check_name(char *name);

static vps_config config[] = {
/*	Op	*/
{"LOCKDIR",	NULL, PARAM_LOCKDIR},
{"DUMPDIR",	NULL, PARAM_DUMPDIR},
/*	Log	*/
{"LOGGING",	NULL, PARAM_LOGGING},
{"LOG_LEVEL",	NULL, PARAM_LOGLEVEL},
{"LOGFILE",	NULL, PARAM_LOGFILE},
{"VERBOSE",	NULL, PARAM_VERBOSE},

{"IPTABLES",	NULL, PARAM_IPTABLES},
/*	UB	*/
{"KMEMSIZE",	NULL, PARAM_KMEMSIZE},
{"LOCKEDPAGES",	NULL, PARAM_LOCKEDPAGES},
{"PRIVVMPAGES",	NULL, PARAM_PRIVVMPAGES},
{"SHMPAGES",	NULL, PARAM_SHMPAGES},
{"NUMPROC",	NULL, PARAM_NUMPROC},
{"PHYSPAGES",	NULL, PARAM_PHYSPAGES},
{"VMGUARPAGES",	NULL, PARAM_VMGUARPAGES},
{"OOMGUARPAGES",NULL, PARAM_OOMGUARPAGES},
{"NUMTCPSOCK",	NULL, PARAM_NUMTCPSOCK},
{"NUMFLOCK",	NULL, PARAM_NUMFLOCK},
{"NUMPTY",	NULL, PARAM_NUMPTY},
{"NUMSIGINFO",	NULL, PARAM_NUMSIGINFO},
{"TCPSNDBUF",	NULL, PARAM_TCPSNDBUF},
{"TCPRCVBUF",	NULL, PARAM_TCPRCVBUF},
{"OTHERSOCKBUF",NULL, PARAM_OTHERSOCKBUF},
{"DGRAMRCVBUF",	NULL, PARAM_DGRAMRCVBUF},
{"NUMOTHERSOCK",NULL, PARAM_NUMOTHERSOCK},
{"NUMFILE",	NULL, PARAM_NUMFILE},
{"DCACHESIZE",	NULL, PARAM_DCACHESIZE},
{"DCACHE",	"DCACHESIZE", -1},
{"NUMIPTENT",	NULL, PARAM_NUMIPTENT},
{"IPTENTRIES",	"NUMIPTENT", -1},
{"AVNUMPROC",	NULL, PARAM_AVNUMPROC},
/*	Capability */
{"CAPABILITY",	NULL, PARAM_CAP},
/*	Network	*/
{"IP_ADDRESS",	NULL, PARAM_IP_ADD},
{"",		NULL, PARAM_IP_DEL},
{"NETDEV",	NULL, PARAM_NETDEV_ADD},
{"",		NULL, PARAM_NETDEV_DEL},
{"HOSTNAME",	NULL, PARAM_HOSTNAME},
{"NAMESERVER",	NULL, PARAM_NAMESERVER},
{"IPV6",	NULL, PARAM_IPV6NET},
{"SEARCHDOMAIN",NULL, PARAM_SEARCHDOMAIN},
{"",		NULL, PARAM_USERPW},
/*	Devices	*/
{"DEVICES",	NULL, PARAM_DEVICES},
{"DEVNODES",	NULL, PARAM_DEVNODES},
/*	fs param */
{"VE_ROOT",	NULL, PARAM_ROOT},
{"VE_PRIVATE",	NULL, PARAM_PRIVATE},
{"TEMPLATE",	NULL, PARAM_TEMPLATE},
{"NOATIME",	NULL, PARAM_NOATIME},
/*	template     */
{"OSTEMPLATE",	NULL, PARAM_OSTEMPLATE},
{"DEF_OSTEMPLATE", NULL, PARAM_DEF_OSTEMPLATE},
/*	CPU	*/
{"CPUUNITS",	NULL, PARAM_CPUUNITS},
{"CPUUWEIGHT",	NULL, PARAM_CPUWEIGHT},
{"CPULIMIT",	NULL, PARAM_CPULIMIT},
{"CPUS",	NULL, PARAM_VCPUS},
/* create param	*/
{"ONBOOT",	NULL, PARAM_ONBOOT},
{"CONFIGFILE",	NULL, PARAM_CONFIG},
{"ORIGIN_SAMPLE",NULL,PARAM_CONFIG_SAMPLE},
{"DISABLED",	NULL, PARAM_DISABLED},
/* quota */
{"DISK_QUOTA",	NULL, PARAM_DISK_QUOTA},
{"DISKSPACE",	NULL, PARAM_DISKSPACE},
{"DISKINODES",	NULL, PARAM_DISKINODES},
{"QUOTATIME",	NULL, PARAM_QUOTATIME},
{"QUOTAUGIDLIMIT",NULL, PARAM_QUOTAUGIDLIMIT},
{"MEMINFO",	NULL, PARAM_MEMINFO},
{"VETH",	NULL, PARAM_VETH_ADD},
{"NETIF",	NULL, PARAM_NETIF_ADD},
{"VEID",	NULL, PARAM_VEID},
{"NAME",	NULL, PARAM_NAME},

{"FEATURES",	NULL, PARAM_FEATURES},
{"IOPRIO",	NULL, PARAM_IOPRIO},
{NULL		,NULL, -1}
};

static struct option set_opt[] = {
{"save",	no_argument, NULL, PARAM_SAVE},
{"applyconfig",	required_argument, NULL, PARAM_APPLYCONFIG},
{"applyconfig_map",	required_argument, NULL, PARAM_APPLYCONFIG_MAP},
{"config",	required_argument, NULL, PARAM_CONFIG},
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
{"oomguarpagess",required_argument, NULL, PARAM_OOMGUARPAGES},
{"numtcpsock",	required_argument, NULL, PARAM_NUMTCPSOCK},
{"numflock",	required_argument, NULL, PARAM_NUMFLOCK},
{"numpty",	required_argument, NULL, PARAM_NUMPTY},
{"numsiginfo",	required_argument, NULL, PARAM_NUMSIGINFO},
{"tcpsndbuf",	required_argument, NULL, PARAM_TCPSNDBUF},
{"tcprcvbuf",	required_argument, NULL, PARAM_TCPRCVBUF},
{"othersockbuf",required_argument, NULL, PARAM_OTHERSOCKBUF},
{"dgramrcvbuf",	required_argument, NULL, PARAM_DGRAMRCVBUF},
{"numothersockk",required_argument, NULL, PARAM_NUMOTHERSOCK},
{"numfile",	required_argument, NULL, PARAM_NUMFILE},
{"dcachesize",	required_argument, NULL, PARAM_DCACHESIZE},
{"numiptent",	required_argument, NULL, PARAM_NUMIPTENT},
{"avnumproc",	required_argument, NULL, PARAM_AVNUMPROC},
/*	Capability */
{"capability",	required_argument, NULL, PARAM_CAP},
/*	Network	*/
{"ipadd",	required_argument, NULL, PARAM_IP_ADD},
{"ip",		required_argument, NULL, PARAM_IP_ADD},
{"ipdel",	required_argument, NULL, PARAM_IP_DEL},
{"skip_arpdetect",no_argument, NULL, PARAM_SKIPARPDETECT},
{"netdev_add",	required_argument, NULL, PARAM_NETDEV_ADD},
{"netdev_del",	required_argument, NULL, PARAM_NETDEV_DEL},
{"hostname",	required_argument, NULL, PARAM_HOSTNAME},
{"nameserver",	required_argument, NULL, PARAM_NAMESERVER},
{"searchdomain",required_argument, NULL, PARAM_SEARCHDOMAIN},
{"userpasswd",	required_argument, NULL, PARAM_USERPW},
/*	Devices	*/
{"devices",	required_argument, NULL, PARAM_DEVICES},
{"devnodes",	required_argument, NULL, PARAM_DEVNODES},
/*	fs param */
{"root",	required_argument, NULL, PARAM_ROOT},
{"private",	required_argument, NULL, PARAM_PRIVATE},
{"noatime",	required_argument, NULL, PARAM_NOATIME},
/*	template	*/
{"ostemplate",	required_argument, NULL, PARAM_OSTEMPLATE},
{"pkgset",	required_argument, NULL, PARAM_PKGSET},
{"pkgver",	required_argument, NULL, PARAM_PKGVER},
/*	Cpu	*/
{"cpuunits",	required_argument, NULL, PARAM_CPUUNITS},
{"cpuweight",	required_argument, NULL, PARAM_CPUWEIGHT},
{"cpulimit",	required_argument, NULL, PARAM_CPULIMIT},
{"cpus",	required_argument, NULL, PARAM_VCPUS},
/*	create param	*/
{"onboot",	required_argument, NULL, PARAM_ONBOOT},
{"setmode",	required_argument, NULL, PARAM_SETMODE},
{"disabled",	required_argument, NULL, PARAM_DISABLED},
/*	quota */
{"diskspace",	required_argument, NULL, PARAM_DISKSPACE},
{"diskinodes",	required_argument, NULL, PARAM_DISKINODES},
{"quotatime",	required_argument, NULL, PARAM_QUOTATIME},
{"quotaugidlimit", required_argument, NULL, PARAM_QUOTAUGIDLIMIT},
{"meminfo",	required_argument, NULL, PARAM_MEMINFO},
/*	veth	*/
{"veth_add",	required_argument, NULL, PARAM_VETH_ADD_CMD},
{"veth_del",	required_argument, NULL, PARAM_VETH_DEL_CMD},
/*	netif	*/
{"netif_add",	required_argument, NULL, PARAM_NETIF_ADD_CMD},
{"netif_del",	required_argument, NULL, PARAM_NETIF_DEL_CMD},

{"mac",		required_argument, NULL, PARAM_NETIF_MAC},
{"ifname",	required_argument, NULL, PARAM_NETIF_IFNAME},
{"host_mac",	required_argument, NULL, PARAM_NETIF_HOST_MAC},
{"host_ifname",	required_argument, NULL, PARAM_NETIF_HOST_IFNAME},

/*	name	*/
{"name",	required_argument, NULL, PARAM_NAME},
{"features",	required_argument, NULL, PARAM_FEATURES},
{"ioprio",	required_argument, NULL, PARAM_IOPRIO},

{NULL, 0, NULL, 0}
};

struct option *get_set_opt(void)
{
	return set_opt;
}

const vps_config *conf_get_by_name(const vps_config *conf, const char *name)
{
	const vps_config *p;

	if (conf == NULL)
		return NULL;
	for (p = conf; p->name != NULL; p++) {
		if (!strcmp(p->name, name)) {
			if (p->alias != NULL)
				return conf_get_by_name(conf, p->alias);
			return p;
		}
	}
	return NULL;
}

const vps_config *conf_get_by_id(const vps_config *conf, int id)
{
	const vps_config *p;

	if (conf == NULL)
		return NULL;
	for (p = conf; p->name != NULL; p++)
		if (p->id == id)
			return p;
	return NULL;
}

int opt_get_by_id(struct option *opt, int id)
{
	struct option *p;

	for (p = opt; p->name != NULL; p++)
		if (p->val == id)
			return p->val;
	return -1;
}

static conf_struct *find_conf_line(list_head_t *head, char *name,
	char delim)
{
	conf_struct *conf;
	char *p;
	int len;

	len = strlen(name);
	list_for_each(conf, head, list) {
		if (strncmp(conf->val, name, len))
			continue;
		p = conf->val + len;
		if (*p == delim)
			return conf;
	}
	return NULL;
}

static int parse_setmode(vps_param *vps_p, const char *val)
{
	if (!strcmp(val, "ignore"))
		vps_p->opt.setmode = SET_IGNORE;
	else if (!strcmp(val, "restart"))
		vps_p->opt.setmode = SET_RESTART;
	else
		return ERR_INVAL;
	return 0;
}

int conf_parse_strlist(list_head_t *list, const char *val, int checkdup)
{
	if (add_str2list(list, val))
		return ERR_NOMEM;
	return 0;
}

int conf_parse_str(char **dst, const char *val, int checkdup)
{
	if (*dst != NULL) {
		if (checkdup)
			return ERR_DUP;
		free(*dst);
	}
	*dst = strdup(val);
	if (*dst == NULL)
		return ERR_NOMEM;
	return 0;
}

int conf_parse_yesno(int *dst, const char *val, int checkdup)
{
	int ret;

	if (*dst && checkdup)
		return ERR_DUP;
	if ((ret = yesno2id(val)) < 0)
		return ERR_INVAL;
	*dst = ret;
	return 0;
}

int conf_store_strlist(list_head_t *conf, char *name, list_head_t *val)
{
	char *str;

	if (list_empty(val))
		return 0;
	if ((str = list2str_c(name, '"', val)) == NULL)
		return ERR_NOMEM;;
	if (add_str_param2(conf, str)) {
		free(str);
		return ERR_NOMEM;
	}
	return 0;
}

int conf_store_str(list_head_t *conf, char *name, const char *val)
{
	char *buf;
	int len;

	if (val == NULL)
		return 0;
	len = strlen(name) + strlen(val) + 3;
	buf = malloc(len + 1);
	if (buf == NULL)
		return ERR_NOMEM;
	sprintf(buf, "%s=\"%s\"", name,	val);
	if (add_str_param2(conf, buf))
		return ERR_NOMEM;
	return 0;
}

int conf_store_yesno(list_head_t *conf, char *name, int val)
{
	char *buf;
	int len;

	if (!val)
		return 0;
	len = strlen(name) + 6;
	buf = malloc(len + 1);
	if (buf == NULL)
		return ERR_NOMEM;
	sprintf(buf, "%s=\"%s\"", name,	val == YES ? "yes" : "no");
	if (add_str_param2(conf, buf))
		return ERR_NOMEM;
	return 0;
}

/******************** Features *************************/
static int parse_features(env_param *env, char *val)
{
	int ret;
	char *token;
	struct feature_s *feat;

	if ((token = strtok(val, "\t ")) == NULL)
		return 0;

	ret = 0;
	do {
		feat = find_feature(token);
		if (feat == NULL) {
			logger(0, 0, "Warning: Unknown feature: %s", token);
			ret = ERR_INVAL_SKIP;
			continue;
		}

		if (feat->on)
			env->features_mask |= feat->mask;
		env->features_known |= feat->mask;
	} while ((token = strtok(NULL, "\t ")) != NULL);
	return ret;
}

static void store_features(unsigned long long mask, unsigned long long known,
		vps_config *conf, list_head_t *conf_h)
{
	char buf[2 * STR_SIZE];
	int r;

	r = snprintf(buf, sizeof(buf) - 1, "%s=\"", conf->name);
	features_mask2str(mask, known, buf + r, sizeof(buf) - r - 2);
	strcat(buf, "\"");
	add_str_param(conf_h, buf);
}

int parse_ioprio(int id, io_param *io, char *val)
{
	if (parse_int(val, &io->ioprio))
		return ERR_INVAL;
	if (io->ioprio < VE_IOPRIO_MIN || io->ioprio > VE_IOPRIO_MAX)
		return ERR_INVAL;
	return 0;
}

static int store_ioprio(vps_param *old_p, vps_param *vps_p,
				 vps_config *conf, list_head_t *conf_h)
{
	io_param *io_param;
	char buf[20];

	io_param = &vps_p->res.io;

	if (conf->id != PARAM_IOPRIO || io_param->ioprio < 0)
		return 0;

	snprintf(buf, sizeof(buf), "%s=\"%d\"", conf->name, io_param->ioprio);
	add_str_param(conf_h, buf);

	return 0;
}

/******************** Iptables *************************/
static int parse_iptables(env_param *env, char *val)
{
	char *token;
	struct iptables_s *ipt;
	int ret;

	if ((token = strtok(val, "\t ")) == NULL)
		return 0;
	ret = 0;
	do {
		if ((ipt = find_ipt(token)) == NULL) {
			logger(0, 0, "Warning: Unknown iptable module: %s,"
				" skipped", token);
			ret = ERR_INVAL_SKIP;
			continue;
		}
		env->ipt_mask |= (unsigned long) ipt->mask;
	} while ((token = strtok(NULL, "\t ")));
	return ret;
}

static void store_iptables(unsigned long ipt_mask, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	int r;

	r = snprintf(buf, sizeof(buf), "%s=\"", conf->name);
	ipt_mask2str(ipt_mask, buf + r, sizeof(buf) - r - 1);
	strcat(buf, "\"");
	add_str_param(conf_h, buf);
}

static int store_env(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	env_param *env = &vps_p->res.env;

	switch (conf->id) {
	case PARAM_IPTABLES:
		if (!env->ipt_mask)
			break;
		store_iptables(env->ipt_mask, conf, conf_h);
		break;
	case PARAM_FEATURES:
		if (!env->features_known)
			break;
		store_features((env->features_mask & env->features_known) |
			(old_p->res.env.features_mask & ~env->features_known),
			old_p->res.env.features_known | env->features_known,
			conf, conf_h);
		break;
	}
	return 0;
}

/********************** UB **************************************/
static int get_mul(char c)
{
	switch (c) {
	case 'G':
	case 'g':
		return 1024 * 1024 * 1024;
	case 'M':
	case 'm':
		return 1024 * 1024;
	case 'K':
	case 'k':
		return 1024;
	case 'P':
	case 'p':
		return _page_size;
	case 'B':
	case 'b':
		return 1;
	}
	return -1;
}

/* This function parses string in form xxx[GMKPB]
 */
static const char *parse_ul_sfx(const char *str, unsigned long long *val, int divisor)
{
	int n;
	char *tail;

	if (!str || !val)
		return NULL;
	if (!strncmp(str, "unlimited", 9)) {
		*val = LONG_MAX;
		return str + 9;
	}
	errno = 0;
	*val = strtoull(str, &tail, 10);
	if (errno == ERANGE)
		return NULL;
	if (*tail != ':' && *tail != '\0') {
		if (!divisor)
			return NULL;
		if ((n = get_mul(*tail)) < 0)
			return NULL;
		*val = (*val) * n / divisor;
		++tail;
	}
	return tail;
}

/* This function parses string in form xxx[GMKPB]:yyy[GMKPB]
 * If :yyy is omitted, it is set to xxx.
 */
static int parse_twoul_sfx(const char *str, unsigned long *val, int divisor)
{
	unsigned long long tmp;
	int ret = 0;

	if (!(str = parse_ul_sfx(str, &tmp, divisor)))
		return ERR_INVAL;
	if (tmp > LONG_MAX) {
		tmp = LONG_MAX;
		ret = ERR_LONG_TRUNC;
	}
	val[0] = tmp;
	if (*str == ':') {
		str = parse_ul_sfx(++str, &tmp, divisor);
		if (!str || *str != '\0')
			return ERR_INVAL;
		if (tmp > LONG_MAX) {
			tmp = LONG_MAX;
			ret = ERR_LONG_TRUNC;
		}
		val[1] = tmp;
	} else if (*str == '\0') {
		val[1] = val[0];
	} else
		return ERR_INVAL;
	return ret;
}

/******************** totalmem *************************/
int parse_meminfo(meminfo_param *param, const char *val)
{
	int mode;
	char mode_nm[32];
	unsigned long meminfo_val;
	int ret;

	if (*val == 0)
		return 0;
	meminfo_val = 0;
	ret = sscanf(val, "%31[^:]:%lu", mode_nm, &meminfo_val);
	if (ret != 2 && ret != 1)
		return ERR_INVAL;
	if ((mode = get_meminfo_mode(mode_nm)) < 0)
		return ERR_INVAL;
	if ((mode != VE_MEMINFO_NONE && ret !=2) ||
	    (mode == VE_MEMINFO_NONE && ret == 2))
		return ERR_INVAL;
	if((mode != VE_MEMINFO_NONE) && meminfo_val == 0)
		return ERR_INVAL;
	param->mode = mode;
	param->val = meminfo_val;

	return 0;
}

static int store_meminfo(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[64];
	const char *mode_nm;
	meminfo_param *meminfo = &vps_p->res.meminfo;

	if (conf->id != PARAM_MEMINFO || meminfo->mode < 0)
		return 0;
	mode_nm = get_meminfo_mode_nm(meminfo->mode);
	if (mode_nm == NULL)
		return 0;
	if (meminfo->mode == VE_MEMINFO_NONE) {
		snprintf(buf, sizeof(buf), "%s=\"%s\"",
		conf->name, mode_nm);
	} else {
		snprintf(buf, sizeof(buf), "%s=\"%s:%lu\"",
		conf->name, mode_nm, meminfo->val);
	}
	add_str_param(conf_h, buf);
	return 0;
}

int parse_ub(vps_param *vps_p, const char *val, int id, int divisor)
{
	int ret;
	ub_res res;
	const vps_config *conf;

	if ((conf = conf_get_by_id(config, id)) == NULL)
		return ERR_INVAL;
	ret = parse_twoul_sfx(val, res.limit, divisor);
	if (ret && ret != ERR_LONG_TRUNC)
		return ret;
	res.res_id = id;
	if (add_ub_param(&vps_p->res.ub, &res))
		return ERR_NOMEM;
	return ret;
}

static int store_ub(vps_param *old_p, vps_param *vps_p,
	list_head_t *conf_h)
{
	const vps_config *conf;
	ub_param *ub = &vps_p->res.ub;
	char buf[128];

#define ADD_UB_PARAM(res, id)						\
if (ub->res != NULL) {							\
	conf = conf_get_by_id(config, id);				\
	snprintf(buf, sizeof(buf), "%s=\"%lu:%lu\"", conf->name,	\
		ub->res[0], ub->res[1]);				\
	if (add_str_param(conf_h, buf))					\
		return ERR_NOMEM;					\
}

	ADD_UB_PARAM(kmemsize, PARAM_KMEMSIZE)
	ADD_UB_PARAM(lockedpages, PARAM_LOCKEDPAGES)
	ADD_UB_PARAM(privvmpages, PARAM_PRIVVMPAGES)
	ADD_UB_PARAM(shmpages, PARAM_SHMPAGES)
	ADD_UB_PARAM(numproc, PARAM_NUMPROC)
	ADD_UB_PARAM(physpages,	PARAM_PHYSPAGES)
	ADD_UB_PARAM(vmguarpages, PARAM_VMGUARPAGES)
	ADD_UB_PARAM(oomguarpages, PARAM_OOMGUARPAGES)
	ADD_UB_PARAM(numtcpsock, PARAM_NUMTCPSOCK)
	ADD_UB_PARAM(numflock, PARAM_NUMFLOCK)
	ADD_UB_PARAM(numpty, PARAM_NUMPTY)
	ADD_UB_PARAM(numsiginfo, PARAM_NUMSIGINFO)
	ADD_UB_PARAM(tcpsndbuf,	PARAM_TCPSNDBUF)
	ADD_UB_PARAM(tcprcvbuf, PARAM_TCPRCVBUF)
	ADD_UB_PARAM(othersockbuf, PARAM_OTHERSOCKBUF)
	ADD_UB_PARAM(dgramrcvbuf, PARAM_DGRAMRCVBUF)
	ADD_UB_PARAM(numothersock, PARAM_NUMOTHERSOCK)
	ADD_UB_PARAM(numfile, PARAM_NUMFILE)
	ADD_UB_PARAM(dcachesize, PARAM_DCACHESIZE)
	ADD_UB_PARAM(numiptent, PARAM_NUMIPTENT)
	ADD_UB_PARAM(avnumproc, PARAM_AVNUMPROC)
#undef ADD_UB_PARAM

	return 0;
}

/********************** Capability ********************/

static int parse_cap(char *str, cap_param *cap)
{
	int len;
	char *p, *token;
	char cap_nm[128];
	unsigned long *mask;

	if ((token = strtok(str, "\t ")) == NULL)
		return 0;
	do {
		if ((p = strrchr(token, ':')) == NULL) {
			logger(-1, 0, "Invalid syntaxes in %s:"
				" capname:on|off", token);
			return ERR_INVAL;
		}
		if (!strcmp(p + 1, "off"))
			mask = &cap->off;
		else if (!strcmp(p + 1, "on"))
			mask = &cap->on;
		else {
			logger(-1, 0, "Invalid syntaxes in %s:"
				" capname:on|off", token);
			return ERR_INVAL;
		}
		len = p - token;
		strncpy(cap_nm, token,
			len < sizeof(cap_nm) ? len : sizeof(cap_nm));
		cap_nm[len] = 0;
		if (get_cap_mask(cap_nm, mask)) {
			logger(-1, 0, "Capability %s is unknown", cap_nm);
			return ERR_INVAL;
		}
	} while ((token = strtok(NULL, " ")));

	return 0;
}

static int store_cap(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	char *p;
	int len;
	cap_param *cap = &vps_p->res.cap;

	if (conf->id != PARAM_CAP)
		return 0;
	if (!cap->on && !cap->off)
		return 0;
	p = buf;
	p += sprintf(buf, "%s=", conf->name);
	len = buf + sizeof(buf) - p - 1;
	build_cap_str(cap, &old_p->res.cap, p, len);
	add_str_param(conf_h, buf);

	return 0;
}
/********************** Network ************************/

int check_ip_dot(char *ip)
{
	int i;
	char *str = ip;
	char *p;

	for (i = 0; i < 5; i++) {
		if ((p = strchr(str, '.')) == NULL)
			break;
		str = p + 1;
	}
	if (i != 3)
		return VZ_BADIP;
	return 0;
}

static int parse_ip(vps_param *vps_p, char *val, int id)
{
	char *token;
	char dst[64];
	unsigned int ip[16];
	int family;
	net_param *net;

	if (id == PARAM_IP_ADD)
		net = &vps_p->res.net;
	else if (id == PARAM_IP_DEL)
		net = &vps_p->del_res.net;
	else
		return 0;

	if ((token = strtok(val, "\t ")) == NULL)
		return 0;
	do {
		if (id == PARAM_IP_DEL && !strcmp(token, "all")) {
			vps_p->res.net.delall = YES;
			continue;
		}
		if (!strcmp(token, "0.0.0.0"))
			continue;
		if (!strcmp(token, "::"))
			continue;
		if (!strcmp(token, "::0"))
			continue;
		if (!strchr(token, ':')) {
			if (check_ip_dot(token))
				return ERR_INVAL;
		}
		if ((family = get_netaddr(token, ip)) < 0)
			return ERR_INVAL;
		if (inet_ntop(family, ip, dst, sizeof(dst)) == NULL)
			return ERR_INVAL;
		if (!find_ip(&net->ip, dst))
			add_str_param(&net->ip, dst);
	} while ((token = strtok(NULL, " ")));

	return 0;
}

static int store_ip(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	list_head_t ip;
	char *str;

	list_head_init(&ip);
	if (conf->id != PARAM_IP_ADD)
		return 0;
	if (!vps_p->res.net.delall &&
		list_empty(&vps_p->res.net.ip) &&
		list_empty(&vps_p->del_res.net.ip))
	{
		return 0;
	}
	merge_str_list(vps_p->res.net.delall, &old_p->res.net.ip,
		&vps_p->res.net.ip, &vps_p->del_res.net.ip, &ip);
	str = list2str_c(conf->name, '"', &ip);
	free_str_param(&ip);
	if (str == NULL)
		return -1;
	add_str_param(conf_h, str);
	free(str);

	return 0;
}

static int check_netdev(const char *devname)
{
	int i, len;
	const char *name;
	static char *netdev_strict[] = {"venet", "tun", "tap", "lo", NULL};

	for (i = 0; netdev_strict[i] != NULL; i++) {
		name = netdev_strict[i];
		len = strlen(name);
		if (!strncmp(name, devname, len))
			return 1;
	}
	return 0;
}

int add_netdev(net_param *net, char *val)
{
	char *token;

	if ((token = strtok(val, "\t ")) == NULL)
		return 0;
	do {
		if (check_netdev(token))
			return ERR_INVAL;
		add_str_param(&net->dev, token);
	} while ((token = strtok(NULL, "\t ")));

	return 0;
}

static int store_netdev(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	list_head_t dev;
	char *str;

	list_head_init(&dev);
	if (conf->id != PARAM_NETDEV_ADD)
		return 0;
	if (list_empty(&vps_p->res.net.dev) &&
		list_empty(&vps_p->del_res.net.dev))
	{
		return 0;
	}
	merge_str_list(0, &old_p->res.net.dev, &vps_p->res.net.dev,
		 &vps_p->del_res.net.dev, &dev);
	str = list2str_c(conf->name, '"', &dev);
	free_str_param(&dev);
	if (str == NULL)
		return -1;
	add_str_param(conf_h, str);
	free(str);

	return 0;
}

/********************* fs *******************************/

static int store_fs(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	fs_param *fs = &vps_p->res.fs;
	int ret;

	ret = 0;
	switch (conf->id) {
	case PARAM_ROOT:
		ret = conf_store_str(conf_h, conf->name, fs->root_orig);
		break;
	case PARAM_PRIVATE:
		ret = conf_store_str(conf_h, conf->name, fs->private_orig);
		break;
	case PARAM_NOATIME:
		ret = conf_store_yesno(conf_h, conf->name, fs->noatime);
		break;
	}
	return ret;
}

static int store_tmpl(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	int ret;

	ret = 0;
	switch (conf->id) {
	case PARAM_OSTEMPLATE:
		ret = conf_store_str(conf_h, conf->name,
			vps_p->res.tmpl.ostmpl);
		break;
	}
	return ret;
}

/***********************Quota***************************/
static int parse_dq(unsigned long **param, const char *val, int sfx)
{
	int ret;
	unsigned long *tmp;

	tmp = malloc(sizeof(unsigned long) * 2);
	if (tmp == NULL)
		return ERR_NOMEM;
	ret = parse_twoul_sfx(val, tmp, sfx ? 1024 : 0);
	if (ret && ret != ERR_LONG_TRUNC) {
		free(tmp);
		return ret;
	}
	*param = tmp;
	return ret;
}

static int store_dq(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	dq_param *param = &vps_p->res.dq;

	switch (conf->id) {
	case PARAM_DISKSPACE:
		if (param->diskspace == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu:%lu\"", conf->name,
			param->diskspace[0], param->diskspace[1]);
		add_str_param(conf_h, buf);
		break;
	case PARAM_DISKINODES:
		if (param->diskinodes == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu:%lu\"", conf->name,
			param->diskinodes[0], param->diskinodes[1]);
		add_str_param(conf_h, buf);
		break;
	case PARAM_QUOTATIME:
		if (param->exptime == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu\"", conf->name,
			 param->exptime[0]);
		add_str_param(conf_h, buf);
		break;
	case PARAM_QUOTAUGIDLIMIT:
		if (param->ugidlimit == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu\"", conf->name,
			param->ugidlimit[0]);
		add_str_param(conf_h, buf);
		break;
	}
	return 0;
}

/*********************Devices***************************/
/*
 * Parse device permission string and set corresponding bits in the permission
 * mask.  The caller is responsible for clearing the mask before the call.
 */
static int parse_dev_perm(const char *str, unsigned int *perms)
{
	const char *ch;

	if (strcmp(str, "none")) {
		for (ch = str; *ch; ch++) {
			if (*ch == 'r')
				*perms |= S_IROTH;
			else if (*ch == 'w')
				*perms |= S_IWOTH;
			else if (*ch == 'q')
				*perms |= S_IXGRP;
			else
				return 1;
		}
	}
	return 0;
}

static int parse_devices_str(const char *str, dev_res *dev)
{
	int ret;
	unsigned long val, major;
	char type[2];
	char minor[32];
	char mode[3];

	ret = sscanf(str, "%1[^:]:%lu:%16[^:]:%2s", type, &major, minor, mode);
	if (ret != 4)
		return 1;
	memset(dev, 0, sizeof(*dev));
	if (!strcmp(type, "b"))
		dev->type = S_IFBLK;
	else if (!strcmp(type, "c"))
		dev->type = S_IFCHR;
	else
		return -1;
	if (!strcmp(minor, "all")) {
		dev->use_major = VE_USE_MAJOR;
		dev->type |= VE_USE_MAJOR;
		dev->dev = makedev(major, 0);
	} else {
		dev->type |= VE_USE_MINOR;
		if (parse_ul(minor, &val))
			return -1;
		dev->dev = makedev(major, val);
	}
	ret = parse_dev_perm(mode, &dev->mask);
	return 0;
}

static int parse_dev(vps_param *vps_p, char *val)
{
	char *token;
	dev_res dev;

	if ((token = strtok(val, " ")) == NULL)
		return 0;
	do {
		if (parse_devices_str(token, &dev))
			return ERR_INVAL;
		if (add_dev_param(&vps_p->res.dev, &dev))
			return ERR_NOMEM;
	} while ((token = strtok(NULL, " ")));

	return 0;
}

static int store_dev(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	dev_param *dev = &vps_p->res.dev;
	dev_res *res;
	int r;
	char *sp, *ep;

	if (conf->id != PARAM_DEVICES)
		return 0;
	if (list_empty(&dev->dev))
		return 0;
	sp = buf;
	ep = buf + sizeof(buf) - 1;
	list_for_each(res, &dev->dev, list) {
		int major, minor, i = 0;
		char mask[3];

		if (res->name[0])
			continue;
		if (sp == buf)
			sp += snprintf(buf, sizeof(buf), "%s=\"", conf->name);
		major = major(res->dev);
		minor = minor(res->dev);
		if (res->mask & S_IROTH)
			mask[i++] = 'r';
		if (res->mask & S_IWOTH)
			mask[i++] = 'w';
		mask[i] = 0;
		if (res->use_major) {
			r = snprintf(sp, ep - sp,"%c:%d:all:%s ",
				S_ISBLK(res->type) ? 'b' : 'c', major, mask);
		} else {
			r = snprintf(sp, ep - sp,"%c:%d:%d:%s ",
				S_ISBLK(res->type) ? 'b' : 'c', major, minor,
				mask);
		}
		sp += r;
		if ((r < 0) || (sp >= ep))
			break;
	}
	if (sp != buf)
		strcat(buf, "\"");
	add_str_param(conf_h, buf);
	return 0;
}

static int parse_devnodes_str(const char *str, dev_res *dev)
{
	char *ch;
	int len;
	char buf[64];
	struct stat st;

	if ((ch = strchr(str, ':')) == NULL)
		return ERR_INVAL;
	ch++;
	len = ch - str;
	if (len > sizeof(dev->name))
		return ERR_INVAL;
	memset(dev, 0, sizeof(*dev));
	snprintf(dev->name, len, "%s", str);
	snprintf(buf, sizeof(buf), "/dev/%s", dev->name);
	if (stat(buf, &st)) {
		logger(-1, errno, "Incorrect device name %s", buf);
		return ERR_INVAL;
	}
	if (S_ISCHR(st.st_mode))
		dev->type = S_IFCHR;
	else if (S_ISBLK(st.st_mode))
		dev->type = S_IFBLK;
	else {
		logger(-1, 0, "The %s is not block or character device", buf);
		return ERR_INVAL;
	}
	dev->dev = st.st_rdev;
	dev->type |= VE_USE_MINOR;
	if (parse_dev_perm(ch, &dev->mask))
		return ERR_INVAL;
	return 0;
}

static int parse_devnodes(vps_param *vps_p, char *val)
{
	char *token;
	dev_res dev;

	if ((token = strtok(val, " ")) == NULL)
		return 0;
	do {
		if (parse_devnodes_str(token, &dev))
			return ERR_INVAL;
		if (add_dev_param(&vps_p->res.dev, &dev))
			return ERR_NOMEM;
	} while ((token = strtok(NULL, " ")));

	return 0;
}

static int store_devnodes(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	dev_param *dev = &vps_p->res.dev;
	dev_res *res;
	int r, i;
	char *sp, *ep;
	char mask[3];

	if (conf->id != PARAM_DEVNODES)
		return 0;
	if (list_empty(&dev->dev))
		return 0;
	sp = buf;
	*sp = 0;
	ep = buf + sizeof(buf) - 1;
	list_for_each (res, &dev->dev, list) {
		i = 0;
		if (!res->name[0])
			continue;
		if (sp == buf)
			sp += snprintf(buf, sizeof(buf), "%s=\"", conf->name);
		if (res->mask & S_IROTH)
			mask[i++] = 'r';
		if (res->mask & S_IWOTH)
			mask[i++] = 'w';
		mask[i] = 0;
		r = snprintf(sp, ep - sp,"%s:%s ", res->name, mask);
		sp += r;
		if ((r < 0) || (sp >= ep))
			break;
	}
	if (sp != buf)
		strcat(buf, "\"");
	add_str_param(conf_h, buf);
	return 0;
}

static int store_misc(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	misc_param *misc = &vps_p->res.misc;
	int ret;

	ret = 0;
	switch (conf->id) {
	case PARAM_ONBOOT:
		ret = conf_store_yesno(conf_h, conf->name, vps_p->opt.onboot);
		break;
	case PARAM_DISABLED:
		ret = conf_store_yesno(conf_h, conf->name,
			vps_p->opt.start_disabled);
		break;
	case PARAM_CONFIG_SAMPLE:
		ret = conf_store_str(conf_h, conf->name,
			vps_p->opt.origin_sample);
		break;
	case PARAM_HOSTNAME:
		ret = conf_store_str(conf_h, conf->name, misc->hostname);
		break;
	case PARAM_NAMESERVER:
		ret = conf_store_strlist(conf_h, conf->name, &misc->nameserver);
		break;
	case PARAM_SEARCHDOMAIN:
		ret = conf_store_strlist(conf_h, conf->name,
			&misc->searchdomain);
		break;
	}
	return ret;
}

/****************** CPU**********************************/

static int parse_cpulimit(unsigned long **param, const char *str)
{
	char *tail;
	int ncpu;
	unsigned long val;

	errno = 0;
	val = (int)strtoul(str, (char **)&tail, 10);
	if (*tail == '%' && *(tail + 1) == '\0') {
		if (val > 100)
			return ERR_INVAL;
		ncpu = get_num_cpu();
		val *= ncpu;
	} else if (*tail != '\0' || errno == ERANGE)
		return ERR_INVAL;

	*param = malloc(sizeof(unsigned long));
	**param = val;
	return 0;
}

static int store_cpu(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	cpu_param *cpu = &vps_p->res.cpu;

	switch (conf->id) {
	case PARAM_CPUUNITS:
		if (cpu->units == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu\"",
				conf->name, *cpu->units);
		add_str_param(conf_h, buf);
		break;
	case PARAM_CPUWEIGHT:
		if (cpu->weight == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu\"",
			conf->name, *cpu->weight);
		add_str_param(conf_h, buf);
		break;
	case PARAM_CPULIMIT:
		if (cpu->limit == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu\"",
			conf->name, *cpu->limit);
		add_str_param(conf_h, buf);
		break;
	case PARAM_VCPUS:
		if (cpu->vcpus == NULL)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%lu\"",
			conf->name, *cpu->vcpus);
		add_str_param(conf_h, buf);
		break;

	}
	return 0;
}

/******************** VETH *********************************/
static int parse_veth_str(const char *str, veth_dev *dev, int operation)
{
	char *ch, *tmp;
	int len, err;

	memset(dev, 0, sizeof(*dev));
	if (!operation) {
		/* Removing veth device */
		/* Parsing veth device name in VE0 */
		if ((ch = strchr(str, ',')) != NULL)
			return ERR_INVAL;
		len = strlen(str) + 1;
		if (len > IFNAMSIZE)
			return ERR_INVAL;
		snprintf(dev->dev_name, len, "%s", str);
		return 0;
	}
	/* Creating veth device */
	/* Parsing veth device name in VE0 */
	if ((ch = strchr(str, ',')) == NULL)
		return ERR_INVAL;
	ch++;
	len = ch - str;
	if (len > IFNAMSIZE)
		return ERR_INVAL;
	snprintf(dev->dev_name, len, "%s", str);
	tmp = ch;

	/* Parsing veth MAC address in VE0 */
	if ((ch = strchr(tmp, ',')) == NULL)
		return ERR_INVAL;
	len = ch - tmp;
	if (len != 3*ETH_ALEN -1)
		return ERR_INVAL;
	dev->addrlen = ETH_ALEN;
	err = parse_hwaddr(tmp, dev->dev_addr);
	if (err)
		return ERR_INVAL;
	ch++;
	tmp = ch;

	/* Parsing veth name in VE */
	if ((ch = strchr(tmp, ',')) == NULL)
		return ERR_INVAL;
	ch++;
	len = ch - tmp;
	if (len > IFNAMSIZE)
		return ERR_INVAL;
	snprintf(dev->dev_name_ve, len, "%s", tmp);

	/* Parsing veth MAC address in VE */
	len = strlen(ch);
	if (len != 3*ETH_ALEN -1)
		return ERR_INVAL;
	dev->addrlen_ve = ETH_ALEN;
	err = parse_hwaddr(ch, dev->dev_addr_ve);
	if (err)
		return ERR_INVAL;

	return 0;
}

static int parse_veth(vps_param *vps_p, char *val, int operation)
{
	int ret;
	char *token;
	veth_dev dev;

	if ((token = strtok(val, " ")) == NULL)
		return 0;
	do {
		if (parse_veth_str(token, &dev, operation))
			return ERR_INVAL;
		if (operation)
			ret = add_veth_param(&vps_p->res.veth, &dev);
		else
			ret = add_veth_param(&vps_p->del_res.veth, &dev);

	} while ((token = strtok(NULL, " ")));

	return 0;
}

/********************* NETIF **********************************/
void generate_veth_name(int veid, char *dev_name_ve,  char *dev_name, int len)
{
	char *name;
	int id = 0;

	name = subst_VEID(veid, DEF_VETHNAME);
	sscanf(dev_name_ve, "%*[^0-9]%d", &id);
	snprintf(dev_name, len, "%s.%d", name, id);
	free(name);
}

static int add_netif_param(veth_param *veth, int opt, char *str)
{
	veth_dev *dev;
	int len;

	dev = NULL;
	dev = find_veth_configure(&veth->dev);
	if (dev == NULL) {
		dev = calloc(1, sizeof(veth_dev));
		dev->configure = 1;
		list_add_tail(&dev->list, &veth->dev);
	}
	len = strlen(str);
	switch (opt) {
	case PARAM_NETIF_IFNAME:
		if (dev->dev_name_ve[0] != 0) {
			logger(-1, 0,"Multiple use of"
				" --ifname option not allowed");
			return ERR_INVAL;
		}
		if (len > IFNAMSIZE)
			return ERR_INVAL;
		strcpy(dev->dev_name_ve, str);
		break;
	case PARAM_NETIF_MAC:
		if (parse_hwaddr(str, dev->dev_addr_ve))
			return ERR_INVAL;
		dev->addrlen_ve = ETH_ALEN;
		break;
	case PARAM_NETIF_HOST_IFNAME:
		if (len > IFNAMSIZE)
			return ERR_INVAL;
		strcpy(dev->dev_name, str);
		break;
	case PARAM_NETIF_HOST_MAC:
		if (parse_hwaddr(str, dev->dev_addr))
			return ERR_INVAL;
		dev->addrlen = ETH_ALEN;
		break;
	}
	return 0;
}

static int parse_netif_str(envid_t veid, const char *str, veth_dev *dev)
{
	const char *p, *next, *ep;
	int len, err;
	char tmp[256];

	memset(dev, 0, sizeof(*dev));
	next = p = str;
	ep = p + strlen(str);
	do {
		while (*next != '\0' && *next != ',') next++;
		if (!strncmp("ifname=", p, 7)) {
			p += 7;
			len = next - p;
			if (len > IFNAMSIZE)
				return ERR_INVAL;
			if (dev->dev_name_ve[0] == '\0')
				strncpy(dev->dev_name_ve, p, len);
		} else if (!strncmp("host_ifname=", p, 12)) {
			p += 12;
			len = next - p;
			if (len > IFNAMSIZE)
				return ERR_INVAL;
			if (dev->dev_name[0] == '\0')
				strncpy(dev->dev_name, p, len);
		} else if (!strncmp("mac=", p, 4)) {
			p += 4;
			len = next - p;
			if (len >= sizeof(tmp))
				return ERR_INVAL;
			strncpy(tmp, p, len);
			tmp[len] = 0;
			err = parse_hwaddr(tmp, dev->dev_addr_ve);
			if (err)
				return err;
			dev->addrlen_ve = ETH_ALEN;
		} else if (!strncmp("host_mac=", p, 9)) {
			p += 9;
			len = next - p;
			if (len >= sizeof(tmp))
				return ERR_INVAL;
			strncpy(tmp, p, len);
			tmp[len] = 0;
			err = parse_hwaddr(tmp, dev->dev_addr);
			if (err)
				return err;
			dev->addrlen = ETH_ALEN;
		}
		p = ++next;
	} while (p < ep);
	if (dev->dev_name_ve[0] == 0)
		return ERR_INVAL;
	if (dev->dev_name[0] == 0)
		generate_veth_name(veid, dev->dev_name_ve, dev->dev_name,
			sizeof(dev->dev_name));
	if (dev->addrlen_ve == 0) {
		generate_mac(veid, dev->dev_name, dev->dev_addr_ve);
		dev->addrlen_ve = ETH_ALEN;
	}
	if (dev->addrlen == 0) {
		memcpy(dev->dev_addr, dev->dev_addr_ve, sizeof(dev->dev_addr));
		dev->addrlen = ETH_ALEN;
	}
	return 0;
}


static int parse_netif(envid_t veid, veth_param *veth, char *val)
{
	int ret;
	char *token;
	veth_dev dev;

	if ((token = strtok(val, ";")) == NULL)
		return 0;
	do {
		if (parse_netif_str(veid, token, &dev)) {
			free_veth_dev(&dev);
			continue;
		}
		if (find_veth_by_ifname_ve(&veth->dev,
				dev.dev_name_ve) == NULL)
		{
			ret = add_veth_param(veth, &dev);
		}
		free_veth_dev(&dev);

	} while ((token = strtok(NULL, ";")));

	return 0;
}

static int store_netif(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE * 10];
	veth_dev *dev;
	char *sp, *ep, *prev;
	veth_param merged;
	veth_param *veth_add = &vps_p->res.veth;
	veth_param *veth_del = &vps_p->del_res.veth;


#define STR2MAC(dev)			\
	((unsigned char *)dev)[0],	\
	((unsigned char *)dev)[1],	\
	((unsigned char *)dev)[2],	\
	((unsigned char *)dev)[3],	\
	((unsigned char *)dev)[4],	\
	((unsigned char *)dev)[5]
	if (conf->id != PARAM_NETIF_ADD)
		return 0;
	if (list_empty(&veth_add->dev) && list_empty(&veth_del->dev) &&
		!veth_add->delall)
	{
		return 0;
	}
	memset(&merged, 0, sizeof(merged));
	list_head_init(&merged.dev);
	if (old_p != NULL) {
		if (merge_veth_list(veth_add->delall == YES ? NULL :
							&old_p->res.veth.dev,
				&veth_add->dev, &veth_del->dev, &merged))
		{
			return 0;
		}
	} else {
		copy_veth_param(&merged, veth_add);
	}
	*buf = 0;
	sp = buf;
	ep = buf + sizeof(buf) - 2;
	sp += snprintf(buf, sizeof(buf), "%s=\"", conf->name);
	prev = sp;
	list_for_each(dev, &merged.dev, list) {
		if (prev != sp)
			*(sp-1) = ';';
		prev = sp;
		if (dev->dev_name_ve[0] != 0) {
			sp += snprintf(sp, ep - sp, "ifname=%s,",
				dev->dev_name_ve);
			if (sp >= ep)
				break;
		}
		if (dev->addrlen_ve != 0) {
			sp += snprintf(sp, ep - sp,
				"mac=%02X:%02X:%02X:%02X:%02X:%02X,",
				STR2MAC(dev->dev_addr_ve));
			if (sp >= ep)
				break;
		}
		if (dev->dev_name[0] != 0) {
			sp += snprintf(sp, ep - sp, "host_ifname=%s,",
				dev->dev_name);
			if (sp >= ep)
				break;
		}
		if (dev->addrlen != 0) {
			sp += snprintf(sp, ep - sp,
				"host_mac=%02X:%02X:%02X:%02X:%02X:%02X,",
				STR2MAC(dev->dev_addr));
			if (sp >= ep)
				break;
		}
		if (prev != sp)
			*(sp-1) = 0;
		if (sp >= ep)
			break;
	}
	free_veth_param(&merged);
	strcat(buf, "\"");
	add_str_param(conf_h, buf);
	return 0;
#undef STR2MAC
}

static int parse_netif_str_cmd(envid_t veid, const char *str, veth_dev *dev)
{
	const char *ch, *tmp, *ep;
	int len, err;

	memset(dev, 0, sizeof(*dev));
	ep = str + strlen(str);
	/* Creating veth device */
	/* Parsing veth device name in VE */
	if ((ch = strchr(str, ',')) == NULL) {
		ch = ep;
		len = ep - str;
	} else {
		len = ch - str;
		ch++;
	}
	if (len > IFNAMSIZE)
		return ERR_INVAL;
	snprintf(dev->dev_name_ve, len + 1, "%s", str);
	tmp = ch;
	if (ch == ep) {
		generate_veth_name(veid, dev->dev_name_ve, dev->dev_name,
			sizeof(dev->dev_name));
		generate_mac(veid, dev->dev_name, dev->dev_addr);
		dev->addrlen_ve = ETH_ALEN;
		generate_mac(veid, dev->dev_name_ve, dev->dev_addr_ve);
		dev->addrlen = ETH_ALEN;
		return 0;
	}
	/* Parsing veth MAC address in VE */
	if ((ch = strchr(tmp, ',')) == NULL) {
		ch = ep;
		len = ch - tmp;
	} else {
		len = ch - tmp;
		ch++;
	}
	if (len != MAC_SIZE) {
		logger(-1, 0, "Invalid VE MAC address length");
		return ERR_INVAL;
	}
	err = parse_hwaddr(tmp, dev->dev_addr_ve);
	if (err) {
		logger(-1, 0, "Invalid VE MAC address format");
		return ERR_INVAL;
	}
	dev->addrlen = ETH_ALEN;
	tmp = ch;
	if (ch == ep) {
		generate_veth_name(veid, dev->dev_name_ve, dev->dev_name,
			sizeof(dev->dev_name));
		memcpy(dev->dev_addr, dev->dev_addr_ve, ETH_ALEN);
		dev->addrlen = ETH_ALEN;
		return 0;
	}
	/* Parsing veth name in VE0 */
	if ((ch = strchr(tmp, ',')) == NULL) {
		ch = ep;
		len = ch - tmp;
	} else {
		len = ch - tmp;
		ch++;
	}
	if (len > IFNAMSIZE)
		return ERR_INVAL;
	snprintf(dev->dev_name, len + 1, "%s", tmp);
	if (ch == ep) {
		memcpy(dev->dev_addr, dev->dev_addr_ve, ETH_ALEN);
		dev->addrlen = ETH_ALEN;
		return 0;
	}
	/* Parsing veth MAC address in VE */
	len = strlen(ch);
	if (len != MAC_SIZE) {
		logger(-1, 0, "Invalid host MAC address");
		return ERR_INVAL;
	}
	err = parse_hwaddr(ch, dev->dev_addr);
	if (err) {
		logger(-1, 0, "Invalid host MAC address");
		return ERR_INVAL;
	}
	dev->addrlen_ve = ETH_ALEN;
	return 0;
}

static int parse_netif_cmd(envid_t veid, veth_param *veth, char *val)
{
	int ret;
	char *token;
	veth_dev dev;

	if ((token = strtok(val, " ")) == NULL)
		return 0;
	do {
		if (parse_netif_str_cmd(veid, token, &dev))
			return ERR_INVAL;
		ret = add_veth_param(veth, &dev);

	} while ((token = strtok(NULL, " ")));

	return 0;
}

static int store_name(vps_param *old_p, vps_param *vps_p, vps_config *conf,
		list_head_t *conf_h)
{
	char buf[STR_SIZE];

	switch (conf->id) {
	case PARAM_VEID:
		if (vps_p->res.name.veid <= 0)
			break;
		snprintf(buf, sizeof(buf), "%s=\"%d\"", conf->name,
			vps_p->res.name.veid);
		add_str_param(conf_h, buf);
		break;
	case PARAM_NAME:
		conf_store_str(conf_h, conf->name, vps_p->res.name.name);
		break;
	}
	return 0;
}

static int parse(envid_t veid, vps_param *vps_p, char *val, int id)
{
	int ret;
	int int_id;
	unsigned long uid;

	ret = 0;
	if (!_page_size) {
		_page_size = get_pagesize();
		if (_page_size == -1)
			return -1;
	}
	switch (id) {
	case PARAM_CONFIG:
		ret = conf_parse_str(&vps_p->opt.config, val, 1);
		break;
	case PARAM_CONFIG_SAMPLE:
		ret = conf_parse_str(&vps_p->opt.origin_sample, val, 1);
		break;
	case PARAM_SAVE:
		vps_p->opt.save = YES;
		break;
	case PARAM_RESET_UB:
		vps_p->opt.reset_ub = YES;
		break;
	case PARAM_ONBOOT:
		ret = conf_parse_yesno(&vps_p->opt.onboot, val, 1);
		break;
	case PARAM_DISABLED:
		ret = conf_parse_yesno(&vps_p->opt.start_disabled, val, 1);
		break;
	case PARAM_HOSTNAME:
		ret = conf_parse_str(&vps_p->res.misc.hostname, val, 1);
		break;
	case PARAM_NAMESERVER:
		ret = conf_parse_strlist(&vps_p->res.misc.nameserver, val, 1);
		break;
	case PARAM_SEARCHDOMAIN:
		ret = conf_parse_strlist(&vps_p->res.misc.searchdomain, val, 1);
		break;
	case PARAM_USERPW:
		ret = conf_parse_strlist(&vps_p->res.misc.userpw, val, 1);
		break;
	case PARAM_APPLYCONFIG_MAP:
		if (!strcmp(val, "name"))
			vps_p->opt.apply_cfg_map = APPCONF_MAP_NAME;
		else
			ret = ERR_INVAL;
		break;
	case PARAM_APPLYCONFIG:
		ret = conf_parse_str(&vps_p->opt.apply_cfg, val, 1);
		break;
	case PARAM_SETMODE:
		ret = parse_setmode(vps_p, val);
		break;
	case PARAM_LOGFILE:
		ret = conf_parse_str(&vps_p->log.log_file, val, 1);
		break;
	case PARAM_LOCKDIR:
		ret = conf_parse_str(&vps_p->opt.lockdir, val, 1);
		break;
	case PARAM_DUMPDIR:
		ret = conf_parse_str(&vps_p->res.cpt.dumpdir, val, 1);
		break;
	case PARAM_LOGGING:
		ret = conf_parse_yesno(&vps_p->log.enable, val, 1);
		break;
	case PARAM_LOGLEVEL:
		if (parse_int(val, &int_id))
			break;
		vps_p->log.level = int_id;
		break;
	case PARAM_VERBOSE:
		if (vps_p->log.verbose != NULL)
			break;
		if (parse_int(val, &int_id))
			break;
		vps_p->log.verbose = malloc(sizeof(*vps_p->log.verbose));
		if (vps_p->log.verbose == NULL)
			return VZ_RESOURCE_ERROR;
		*vps_p->log.verbose = int_id;
		break;
	case PARAM_IPTABLES:
		ret = parse_iptables(&vps_p->res.env, val);
		break;
	case PARAM_LOCKEDPAGES:
	case PARAM_PRIVVMPAGES:
	case PARAM_SHMPAGES:
	case PARAM_PHYSPAGES:
	case PARAM_VMGUARPAGES:
	case PARAM_OOMGUARPAGES:
		ret = parse_ub(vps_p, val, id, _page_size);
		break;
	case PARAM_NUMPROC:
	case PARAM_NUMTCPSOCK:
	case PARAM_NUMFLOCK:
	case PARAM_NUMPTY:
	case PARAM_NUMSIGINFO:
	case PARAM_NUMOTHERSOCK:
	case PARAM_NUMFILE:
	case PARAM_NUMIPTENT:
	case PARAM_AVNUMPROC:
		ret = parse_ub(vps_p, val, id, 0);
		break;
	case PARAM_KMEMSIZE:
	case PARAM_TCPSNDBUF:
	case PARAM_TCPRCVBUF:
	case PARAM_OTHERSOCKBUF:
	case PARAM_DGRAMRCVBUF:
	case PARAM_DCACHESIZE:
		ret = parse_ub(vps_p, val, id, 1);
		break;
	case PARAM_CAP:
		ret = parse_cap(val, &vps_p->res.cap);
		break;
	case PARAM_IP_ADD:
	case PARAM_IP_DEL:
		ret = parse_ip(vps_p, val, id);
		break;
	case PARAM_SKIPARPDETECT:
		vps_p->res.net.skip_arpdetect = YES;
		break;
	case PARAM_IPV6NET:
		ret = conf_parse_yesno(&vps_p->res.net.ipv6_net, val, 1);
		vps_p->del_res.net.ipv6_net = vps_p->res.net.ipv6_net;
		break;
	case PARAM_NETDEV_ADD:
		ret = add_netdev(&vps_p->res.net, val);
		break;
	case PARAM_NETDEV_DEL:
		ret = add_netdev(&vps_p->del_res.net, val);
		break;
	case PARAM_ROOT:
		if (!(ret = conf_parse_str(&vps_p->res.fs.root_orig, val, 1)))
			vps_p->res.fs.root = subst_VEID(veid, val);
		break;
	case PARAM_PRIVATE:
		if (!(ret = conf_parse_str(&vps_p->res.fs.private_orig, val,1)))
			vps_p->res.fs.private = subst_VEID(veid, val);
		break;
	case PARAM_TEMPLATE:
		ret = conf_parse_str(&vps_p->res.fs.tmpl, val, 1);
		break;
	case PARAM_NOATIME:
		ret = conf_parse_yesno(&vps_p->res.fs.noatime, val, 1);
		break;
	case PARAM_DEF_OSTEMPLATE:
		ret = conf_parse_str(&vps_p->res.tmpl.def_ostmpl, val, 1);
		break;
	case PARAM_PKGSET:
		ret = conf_parse_str(&vps_p->res.tmpl.pkgset, val, 1);
		break;
	case PARAM_PKGVER:
		ret = conf_parse_str(&vps_p->res.tmpl.pkgver, val, 1);
		break;
	case PARAM_OSTEMPLATE:
		ret = conf_parse_str(&vps_p->res.tmpl.ostmpl, val, 1);
		break;
	case PARAM_DISK_QUOTA:
		ret = conf_parse_yesno(&vps_p->res.dq.enable, val, 1);
		break;
	case PARAM_DISKSPACE:
		if (vps_p->res.dq.diskspace != NULL)
			break;
		if (parse_dq(&vps_p->res.dq.diskspace, val, 1))
			ret = ERR_INVAL;
		break;
	case PARAM_DISKINODES:
		if (vps_p->res.dq.diskinodes != NULL)
			break;
		if (parse_dq(&vps_p->res.dq.diskinodes, val, 0))
			ret = ERR_INVAL;
		break;
	case PARAM_QUOTATIME:
		if (vps_p->res.dq.exptime != NULL)
			break;
		vps_p->res.dq.exptime = malloc(sizeof(unsigned long));
		if (parse_ul(val, vps_p->res.dq.exptime)) {
			free(vps_p->res.dq.exptime);
			vps_p->res.dq.exptime = NULL;
			ret = ERR_INVAL;
		}
		break;
	case PARAM_QUOTAUGIDLIMIT:
		if (vps_p->res.dq.ugidlimit != NULL)
			break;
		vps_p->res.dq.ugidlimit = malloc(sizeof(unsigned long));
		if (parse_ul(val, vps_p->res.dq.ugidlimit)) {
			free(vps_p->res.dq.ugidlimit);
			vps_p->res.dq.ugidlimit = NULL;
			ret = ERR_INVAL;
		}
		break;
	case PARAM_DEVICES:
		ret = parse_dev(vps_p, val);
		break;
	case PARAM_DEVNODES:
		ret = parse_devnodes(vps_p, val);
		break;
	case PARAM_CPUUNITS:
		if (vps_p->res.cpu.units != NULL)
			break;
		if (parse_ul(val, &uid))
			return ERR_INVAL;
		if (uid < MINCPUUNITS || uid > MAXCPUUNITS)
			return ERR_INVAL;
		vps_p->res.cpu.units = malloc(sizeof(unsigned long));
		if (vps_p->res.cpu.units == NULL)
			return ERR_NOMEM;
		*vps_p->res.cpu.units = uid;
		break;
	case PARAM_CPUWEIGHT:
		if (vps_p->res.cpu.weight != NULL)
			break;
		if (parse_ul(val, &uid))
			return ERR_INVAL;
		vps_p->res.cpu.weight = malloc(sizeof(unsigned long));
		if (vps_p->res.cpu.weight == NULL)
			return ERR_NOMEM;
		*vps_p->res.cpu.weight = uid;
		break;
	case PARAM_CPULIMIT:
		if (vps_p->res.cpu.limit != NULL)
			break;
		if (parse_cpulimit(&vps_p->res.cpu.limit, val))
			return ERR_INVAL;
		break;
	case PARAM_VCPUS:
		if (vps_p->res.cpu.vcpus != NULL)
			break;
		if (parse_ul(val, &uid))
			return ERR_INVAL;
		vps_p->res.cpu.vcpus = malloc(sizeof(unsigned long));
		if (vps_p->res.cpu.vcpus == NULL)
			return ERR_NOMEM;
		*vps_p->res.cpu.vcpus = uid;
		break;
	case PARAM_MEMINFO:
		ret = parse_meminfo(&vps_p->res.meminfo, val);
		break;
	case PARAM_VETH_ADD:
		if (!list_empty(&vps_p->res.veth.dev))
			break;
		ret = parse_veth(vps_p, val, 1);
		vps_p->res.veth.version = 1;
		break;
	case PARAM_VETH_ADD_CMD:
		logger(0, 0, "Warning: --veth_add option is outdated use"
			" --netif_add instead");
		ret = parse_veth(vps_p, val, 1);
		break;
	case PARAM_VETH_DEL_CMD:
		logger(0, 0, "Warning: --veth_del option is outdated use"
			" --netif_del instead");
		ret = parse_veth(vps_p, val, 0);
		break;
	case PARAM_NETIF_ADD:
		/* Skip VETH parameter if NETIF exists */
		if (vps_p->res.veth.version)
			free_veth_param(&vps_p->res.veth);
		if (!list_empty(&vps_p->res.veth.dev))
			break;
		ret = parse_netif(veid, &vps_p->res.veth, val);
		vps_p->res.veth.version = 0;
		break;
	case PARAM_NETIF_ADD_CMD:
		ret = parse_netif_cmd(veid, &vps_p->res.veth, val);
		break;
	case PARAM_NETIF_DEL_CMD: {
		veth_dev dev;

		memset(&dev, 0, sizeof(dev));
		if (strlen(val) > IFNAMSIZE) {
			ret = ERR_INVAL;
			break;
		}
		if (!strcmp(val, "all")) {
			vps_p->res.veth.delall = YES;
			break;
		}
		if (find_veth_by_ifname_ve(&vps_p->del_res.veth.dev, val) != NULL)
			break;
		strcpy(dev.dev_name_ve, val);
		add_veth_param(&vps_p->del_res.veth, &dev);

		break;
	}
	case PARAM_NETIF_IFNAME:
	case PARAM_NETIF_MAC:
	case PARAM_NETIF_HOST_IFNAME:
	case PARAM_NETIF_HOST_MAC:
		ret = add_netif_param(&vps_p->res.veth, id, val);
		break;
	case PARAM_NAME:
		if (vps_p->res.name.name != NULL)
			break;
		if (check_name(val))
			return ERR_INVAL;
		ret = conf_parse_str(&vps_p->res.name.name, val, 1);
		break;
	case PARAM_VEID:
		if (parse_int(val, &int_id))
			return ERR_INVAL;
		vps_p->res.name.veid = int_id;
		break;
	case PARAM_FEATURES:
		ret = parse_features(&vps_p->res.env, val);
		break;
	case PARAM_IOPRIO:
		if (parse_ioprio(id, &vps_p->res.io, val))
			return ERR_INVAL;
		break;
	default:
		logger(10, 0, "Not handled parameter %d %s", id, val);
		break;
	}
	return ret;
}

static int store(vps_param *old_p, vps_param *vps_p, list_head_t *conf_h)
{
	vps_config *conf;

	store_ub(old_p, vps_p, conf_h);
	for (conf = config; conf->name != NULL; conf++) {
		store_env(old_p, vps_p, conf, conf_h);
		store_cap(old_p, vps_p, conf, conf_h);
		store_ip(old_p, vps_p, conf, conf_h);
		store_netdev(old_p, vps_p, conf, conf_h);
		store_fs(old_p, vps_p, conf, conf_h);
		store_tmpl(old_p, vps_p, conf, conf_h);
		store_dq(old_p, vps_p, conf, conf_h);
		store_dev(old_p, vps_p, conf, conf_h);
		store_devnodes(old_p, vps_p, conf, conf_h);
		store_misc(old_p, vps_p, conf, conf_h);
		store_cpu(old_p, vps_p, conf, conf_h);
		store_meminfo(old_p, vps_p, conf, conf_h);
		store_netif(old_p, vps_p, conf, conf_h);
		store_name(old_p, vps_p, conf, conf_h);
		store_ioprio(old_p, vps_p, conf, conf_h);
	}
	return 0;
}

/********************************************************/
/*	VE parse config stuff				*/
/********************************************************/

int vps_parse_config(envid_t veid, char *path, vps_param *vps_p,
	struct mod_action *action)
{
	char *str;
	char ltoken[STR_SIZE];
	FILE *fp;
	int line = 0;
	int ret;
	char *rtoken;
	struct stat st;
	int len = 4096;
	int err = 0;
	const vps_config *conf;

	if ((fp = fopen(path, "r")) == NULL) {
		logger(-1, errno, "Unable to open %s", path);
		return 1;
	}
	if (!stat(path, &st))
		len = st.st_size;
	if (len > 4096)
		str = malloc(len);
	else
		str = alloca(len);
	if (str == NULL)
		return VZ_RESOURCE_ERROR;
	while (fgets(str, len, fp)) {
		line++;
		if ((rtoken = parse_line(str, ltoken, sizeof(ltoken))) == NULL)
			continue;
		if ((conf = conf_get_by_name(config, ltoken)) != NULL) {
			ret = parse(veid, vps_p, rtoken, conf->id);
		} else if (action != NULL)
			ret = mod_parse(veid, action, ltoken, -1, rtoken);
		else
			continue;
		if (!ret) {
			continue;
		} else if (ret == ERR_INVAL_SKIP) {
			continue;
		} else if (ret == ERR_LONG_TRUNC) {
			logger(-1, 0, "Warning: too large value for %s=%s"
				" was truncated", ltoken, rtoken);
		} else if (ret == ERR_DUP) {
			logger(-1, 0, "Warning: dup for %s=%s in line %d"
				" is ignored", ltoken, rtoken, line);
		} else if (ret == ERR_INVAL) {
			logger(-1, 0, "Invalid value for %s=%s, skipped",
				ltoken, rtoken);
		} else if (ret == ERR_UNK) {
			logger(-1, 0, "Unknown parameter %s, skipped", ltoken);
		} else if (ret == ERR_NOMEM) {
			logger(-1, 0, "Not enough memory");
			err = VZ_RESOURCE_ERROR;
			break;
		} else {
			logger(-1, 0, "Unknown exit code %d on parse %s",
				ret, ltoken);
		}
	}
	fclose(fp);
	if (len > 4096)
		free(str);
	return err;
}

/********************************************************/
/*	VE save config stuff				*/
/********************************************************/
static int read_conf(char *fname, list_head_t *conf_h)
{
	FILE *fp;
	char str[16384];

	if (!stat_file(fname))
		return 0;
	if (!(fp = fopen(fname, "r")))
		return -1;
	while (fgets(str, sizeof(str), fp)) {
		add_str_param(conf_h, str);
	}
	fclose(fp);

	return 0;
}

static int write_conf(char *fname, list_head_t *head)
{
	char buf[STR_SIZE];
	conf_struct *conf;
	int fd = 2;
	int len, ret;

	if (fname != NULL) {
		snprintf(buf, sizeof(buf), "%s.tmp", fname);
		if ((fd = open(buf, O_CREAT|O_WRONLY|O_TRUNC, 0644)) < 0) {
			logger(-1, errno, "Unable to create configuration"
				" file %s", buf);
			return 1;
		}
	}
	list_for_each(conf, head, list) {
		if (conf->val == NULL)
			continue;
		len = strlen(conf->val);
		ret = write(fd, conf->val, len);
		if (ret < 0) {
			logger(-1, errno, "Unable to write %d bytes to %s",
					len, buf);
			unlink(buf);
			close(fd);
			return 1;
		}
		if (strchr(conf->val, '\n') == NULL)
			write(fd, "\n", 1);
	}
	if (fname != NULL) {
		close(fd);
		if (rename(buf, fname)) {
			logger(-1, errno, "Unable to move %s -> %s",
				buf, fname);
			return 1;
		}
	}
	return 0;
}

static int vps_merge_conf(list_head_t *dst, list_head_t *src)
{
	int len, cnt = 0;
	conf_struct *conf, *line;
	char ltoken[STR_SIZE];
	char *p;

	if (list_empty(src))
		return 0;
	list_for_each(conf, src, list) {
		if ((p = strchr(conf->val, '=')) == NULL)
			 continue;
		len = p - conf->val + 1;
		snprintf(ltoken, len < sizeof(ltoken) ? len : sizeof(ltoken),
			 "%s", conf->val);
		line = find_conf_line(dst, ltoken, '=');
		if (line != NULL) {
			free(line->val);
			line->val = strdup(conf->val);
		} else
			add_str_param(dst, conf->val);
		cnt++;
	}
	return cnt;
}

int vps_save_config(envid_t veid, char *path, vps_param *new_p,
	vps_param *old_p, struct mod_action *action)
{
	vps_param *tmp_old_p = NULL;
	list_head_t conf, new_conf;
	int ret, n;

	list_head_init(&conf);
	list_head_init(&new_conf);
	if (old_p == NULL && stat_file(path)) {
		tmp_old_p = init_vps_param();
		vps_parse_config(veid, path, tmp_old_p, action);
		old_p = tmp_old_p;
	}
	if ((ret = read_conf(path, &conf)))
		return ret;
	n = store(old_p, new_p, &new_conf);
	if (action != NULL)
		mod_save_config(action, &new_conf);
	if ((ret = vps_merge_conf(&conf, &new_conf)) > 0)
		write_conf(path, &conf);
	free_str_param(&conf);
	free_str_param(&new_conf);
	free_vps_param(tmp_old_p);

	return 0;
}

int vps_remove_cfg_param(envid_t veid, char *path, char *name)
{
	list_head_t conf;
	conf_struct *line;
	int ret, found;

	list_head_init(&conf);
	if ((ret = read_conf(path, &conf)))
		return ret;
	if (list_empty(&conf))
		return 0;
	found = 0;
	while ((line = find_conf_line(&conf, name, '=')) != NULL) {
		free(line->val);
		list_del(&line->list);
		free(line);
		found++;
	}
	if (found)
		write_conf(path, &conf);
	free_str_param(&conf);

	return 0;

}

/********************************************************************/
int vps_parse_opt(envid_t veid, vps_param *param, int opt, char *rval,
	struct mod_action *action)
{
	int id, ret = 0;

	if (param == NULL)
		return -1;
	if ((id = opt_get_by_id(set_opt, opt)) != -1) {
		ret = parse(veid, param, rval, id);
	} else if (action != NULL) {
		ret = mod_parse(veid, action, NULL, opt, rval);
	}

	return ret;
}

/********************************************************************/
vps_param *init_vps_param()
{
	vps_param *param;

	param = (vps_param *) calloc(1, sizeof(vps_param));
	if (param == NULL)
		return NULL;
	list_head_init(&param->res.net.ip);
	list_head_init(&param->res.net.dev);
	list_head_init(&param->res.dev.dev);
	list_head_init(&param->res.misc.userpw);
	list_head_init(&param->res.misc.nameserver);
	list_head_init(&param->res.misc.searchdomain);
	list_head_init(&param->res.dev.dev);
	list_head_init(&param->res.veth.dev);

	list_head_init(&param->del_res.net.ip);
	list_head_init(&param->del_res.net.dev);
	list_head_init(&param->del_res.dev.dev);
	list_head_init(&param->del_res.misc.userpw);
	list_head_init(&param->del_res.misc.nameserver);
	list_head_init(&param->del_res.misc.searchdomain);
	list_head_init(&param->del_res.dev.dev);
	list_head_init(&param->del_res.veth.dev);
	param->res.meminfo.mode = -1;
	param->res.io.ioprio = -1;

	return param;
}

int get_veid_by_name(char *name)
{
	char buf[64];
	char content[256];
	char *p;
	int veid, ret;

	if (name == NULL)
		return -1;
	snprintf(buf, sizeof(buf), VENAME_DIR "/%s", name);
	if (stat_file(buf) != 1)
		return -1;
	ret = readlink(buf, content, sizeof(content) - 1);
	if (ret < 0)
		return -1;
	content[ret] = 0;
	if ((p = strrchr(content, '/')) == NULL)
		p = content;
	else
		p++;
	if (sscanf(p, "%d.conf", &veid) == 1)
		return veid;
	return -1;
}

static int check_name(char *name)
{
	char *p;

	for (p = name; *p != '\0'; p++) {
		if (!isdigit(*p) && !isalpha(*p) && *p != '-' && *p != '_')
			return -1;
	}
	return 0;
}

int set_name(int veid, char *new_name, char *old_name)
{
	int veid_old = -1;
	char buf[STR_SIZE];
	char conf[STR_SIZE];

	if (new_name == NULL)
		return 0;
	if (check_name(new_name)) {
		logger(-1, 0, "Error: invalid name %s", new_name);
		return VZ_SET_NAME_ERROR;
	}
	veid_old = get_veid_by_name(new_name);
	if (veid_old >= 0 && veid_old != veid) {
		logger(-1, 0, "Conflict: name %s already used by VE %d",
			new_name, veid_old);
		return VZ_SET_NAME_ERROR;
	}
	if (old_name != NULL &&
		!strcmp(old_name, new_name) &&
		veid == veid_old)
	{
		return 0;
	}
	if (new_name[0] != 0) {
		snprintf(buf, sizeof(buf), VENAME_DIR "/%s", new_name);
		get_vps_conf_path(veid, conf, sizeof(conf));
		unlink(buf);
		if (symlink(conf, buf)) {
			logger(-1, errno, "Unable to create link %s", buf);
			return VZ_SET_NAME_ERROR;
		}
	}
	veid_old = get_veid_by_name(old_name);
	/* Remove old alias */
	if (veid_old == veid &&
	    old_name != NULL && strcmp(old_name, new_name))
	{
		snprintf(buf, sizeof(buf), VENAME_DIR "/%s", old_name);
		unlink(buf);
	}
	logger(0, 0, "Name %s assigned", new_name);
	return 0;
}

#define FREE_P(x)	if ((x) != NULL) { free(x); x = NULL; }

static void free_opt(vps_opt *opt)
{
	FREE_P(opt->config)
	FREE_P(opt->origin_sample)
	FREE_P(opt->apply_cfg)
	FREE_P(opt->lockdir)
}

static void free_log_s(struct log_s *log)
{
	FREE_P(log->log_file)
	FREE_P(log->verbose)
}

static void free_fs(fs_param *fs)
{
	FREE_P(fs->root)
	FREE_P(fs->root_orig)
	FREE_P(fs->private)
	FREE_P(fs->private_orig)
	FREE_P(fs->tmpl)
}

static void free_tmpl(tmpl_param *tmpl)
{
	FREE_P(tmpl->def_ostmpl)
	FREE_P(tmpl->ostmpl)
	FREE_P(tmpl->pkgset)
	FREE_P(tmpl->pkgver)
	FREE_P(tmpl->dist)
}

static void free_ub(ub_param *ub)
{
	free_ub_param(ub);
}

static void free_misc(misc_param *misc)
{
	free_str_param(&misc->nameserver);
	free_str_param(&misc->searchdomain);
	free_str_param(&misc->userpw);
	FREE_P(misc->hostname)
}

static void free_net(net_param *net)
{
	free_str_param(&net->ip);
	free_str_param(&net->dev);
}

static void free_dev(dev_param *dev)
{
	free_dev_param(dev);
}

static void free_cpu(cpu_param *cpu)
{
	FREE_P(cpu->units)
	FREE_P(cpu->weight)
	FREE_P(cpu->limit)
	FREE_P(cpu->vcpus)
}

static void free_dq(dq_param *dq)
{
	FREE_P(dq->diskspace)
	FREE_P(dq->diskinodes)
	FREE_P(dq->exptime)
	FREE_P(dq->ugidlimit)
}

static void free_cpt(cpt_param *cpt)
{
	FREE_P(cpt->dumpdir)
	FREE_P(cpt->dumpfile)
}

static void free_name(name_param *name)
{
	FREE_P(name->name)
}
#undef FREE_P

static void free_vps_res(vps_res *res)
{
	free_fs(&res->fs);
	free_tmpl(&res->tmpl);
	free_ub(&res->ub);
	free_net(&res->net);
	free_misc(&res->misc);
	free_dev(&res->dev);
	free_cpu(&res->cpu);
	free_dq(&res->dq);
	free_cpt(&res->cpt);
	free_veth_param(&res->veth);
	free_name(&res->name);
}

void free_vps_param(vps_param *param)
{
	if (param == NULL)
		return;

	free_opt(&param->opt);
	free_log_s(&param->log);
	free_vps_res(&param->res);
	free_vps_res(&param->del_res);
	free(param);
}

#define MERGE_STR(x)						\
	if ((src->x) != NULL) {					\
		if ((dst->x) != NULL)				\
			free(dst->x);				\
		dst->x = strdup(src->x);			\
	}

#define MERGE_INT(x)	if (src->x) dst->x = src->x;

#define MERGE_P(x)							\
	if ((src->x) != NULL) { 					\
		if ((dst->x) == NULL) 					\
			dst->x = malloc(sizeof(*(dst->x)));	 	\
		*dst->x = *src->x;					\
	}

#define MERGE_P2(x)							\
	if ((src->x) != NULL) { 					\
		if ((dst->x) == NULL) 					\
			dst->x = malloc(sizeof(*(dst->x)) * 2);	 	\
		dst->x[0] = src->x[0];					\
		dst->x[1] = src->x[1];					\
	}

static void merge_opt(vps_opt *dst, vps_opt *src)
{
	MERGE_INT(save);
	MERGE_INT(fast_kill);
	MERGE_INT(skip_lock);
	MERGE_INT(skip_setup);
	MERGE_INT(start_disabled);
	MERGE_INT(start_force);
	MERGE_INT(onboot);
	MERGE_INT(setmode);
	MERGE_INT(apply_cfg_map);

	MERGE_STR(config)
	MERGE_STR(origin_sample)
	MERGE_STR(apply_cfg)
}

static void merge_fs(fs_param *dst, fs_param *src)
{
	MERGE_STR(root)
	MERGE_STR(root_orig)
	MERGE_STR(private)
	MERGE_STR(private_orig)
	MERGE_STR(tmpl)
	MERGE_INT(noatime);
}

static void merge_tmpl(tmpl_param *dst, tmpl_param *src)
{
	MERGE_STR(def_ostmpl)
	MERGE_STR(ostmpl)
	MERGE_STR(pkgset)
	MERGE_STR(pkgver)
	MERGE_STR(dist)
}

static void merge_cpu(cpu_param *dst, cpu_param *src)
{
	MERGE_P(units)
	MERGE_P(weight)
	MERGE_P(limit)
	MERGE_P(vcpus)
}

#define	MERGE_LIST(x)							\
	if (!list_empty(&(src->x))) {					\
		free_str_param(&(dst->x));				\
		copy_str_param(&(dst->x), &(src->x));			\
	}

static void merge_net(net_param *dst, net_param *src)
{
	MERGE_LIST(ip)
	MERGE_LIST(dev)
	MERGE_INT(delall)
	MERGE_INT(skip_arpdetect)
}

static void merge_misc(misc_param *dst, misc_param *src)
{
	MERGE_LIST(nameserver)
	MERGE_LIST(searchdomain)
	MERGE_LIST(userpw)
	MERGE_STR(hostname)
	MERGE_INT(wait);
}

static void merge_dq(dq_param *dst, dq_param *src)
{
	MERGE_INT(enable)
	MERGE_P2(diskspace)
	MERGE_P2(diskinodes)
	MERGE_P(exptime)
	MERGE_P(ugidlimit)

}

static void merge_dev(dev_param *dst, dev_param *src)
{
	dev_res *cur;

	if (list_empty(&src->dev))
		return;
	free_dev(dst);
	list_for_each(cur, &src->dev, list) {
		add_dev_param(dst, cur);
	}
}

static void merge_cap(cap_param *dst, cap_param *src)
{
	MERGE_INT(on);
	MERGE_INT(off);
}

static void merge_env(env_param *dst, env_param *src)
{
	MERGE_INT(veid);
	MERGE_INT(ipt_mask);
	MERGE_INT(features_mask);
	MERGE_INT(features_known);
}

static void merge_cpt(cpt_param *dst, cpt_param *src)
{
	MERGE_STR(dumpdir);
	MERGE_STR(dumpfile);
	MERGE_INT(ctx);
	MERGE_INT(cpu_flags);
	MERGE_INT(cmd);
}

static void merge_meminfo(meminfo_param *dst, meminfo_param *src)
{
	MERGE_INT(mode);
	MERGE_INT(val);
}

static void merge_veth(veth_param *dst, veth_param *src)
{
	if (!list_empty(&src->dev)) {
		free_veth_param(dst);
		copy_veth_param(dst, src);
	}
}

static void merge_name(name_param *dst, name_param *src)
{
	MERGE_STR(name);
	MERGE_INT(veid);
}

static void merge_io(io_param *dst, io_param *src) {
	MERGE_INT(ioprio);
}

static int merge_res(vps_res *dst, vps_res *src)
{
	merge_fs(&dst->fs, &src->fs);
	merge_tmpl(&dst->tmpl, &src->tmpl);
	merge_ub(&dst->ub, &src->ub);
	merge_net(&dst->net, &src->net);
	merge_misc(&dst->misc, &src->misc);
	merge_cpu(&dst->cpu, &src->cpu);
	merge_cap(&dst->cap, &src->cap);
	merge_dq(&dst->dq, &src->dq);
	merge_dev(&dst->dev, &src->dev);
	merge_env(&dst->env, &src->env);
	merge_cpt(&dst->cpt, &src->cpt);
	merge_meminfo(&dst->meminfo, &src->meminfo);
	merge_veth(&dst->veth, &src->veth);
	merge_name(&dst->name, &src->name);
	merge_io(&dst->io, &src->io);
	return 0;
}

int merge_vps_param(vps_param *dst, vps_param *src)
{
	merge_opt(&dst->opt, &src->opt);
	merge_res(&dst->res, &src->res);

	return 0;
}

int merge_global_param(vps_param *dst, vps_param *src)
{
	MERGE_INT(res.dq.enable);
	MERGE_INT(res.net.ipv6_net);
	MERGE_INT(del_res.net.ipv6_net);
	return 0;
}
