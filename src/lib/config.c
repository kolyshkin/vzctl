/*
 *  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
#include <limits.h>

#include "logger.h"
#include "list.h"
#include "bitmap.h"
#include "vzconfig.h"
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
#include "net.h"
#include "image.h"

static int _page_size;
static int check_name(const char *name);

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
{"NETFILTER",	NULL, PARAM_NETFILTER},
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
{"SWAPPAGES",	NULL, PARAM_SWAPPAGES},
/*	Capability */
{"CAPABILITY",	NULL, PARAM_CAP},
/*	Network	*/
{"IP_ADDRESS",	NULL, PARAM_IP_ADD},
{"",		NULL, PARAM_IP_DEL},
{"NETDEV",	NULL, PARAM_NETDEV_ADD},
{"",		NULL, PARAM_NETDEV_DEL},
{"HOSTNAME",	NULL, PARAM_HOSTNAME},
{"DESCRIPTION",	NULL, PARAM_DESCRIPTION},
{"NAMESERVER",	NULL, PARAM_NAMESERVER},
{"IPV6",	NULL, PARAM_IPV6NET},
{"SEARCHDOMAIN",NULL, PARAM_SEARCHDOMAIN},
{"",		NULL, PARAM_USERPW},
/*	Devices	*/
{"DEVICES",	NULL, PARAM_DEVICES},
{"DEVNODES",	NULL, PARAM_DEVNODES},
{"PCI",		NULL, PARAM_PCI_ADD},
{"",		NULL, PARAM_PCI_DEL},
/*	fs param */
{"VE_ROOT",	NULL, PARAM_ROOT},
{"VE_PRIVATE",	NULL, PARAM_PRIVATE},
{"TEMPLATE",	NULL, PARAM_TEMPLATE},
{"MOUNT_OPTS",	NULL, PARAM_MOUNT_OPTS},
{"VE_LAYOUT",	NULL, PARAM_VE_LAYOUT},
/*	template     */
{"OSTEMPLATE",	NULL, PARAM_OSTEMPLATE},
{"DEF_OSTEMPLATE", NULL, PARAM_DEF_OSTEMPLATE},
/*	CPU	*/
{"CPUUNITS",	NULL, PARAM_CPUUNITS},
{"CPUUWEIGHT",	NULL, PARAM_CPUWEIGHT},
{"CPULIMIT",	NULL, PARAM_CPULIMIT},
{"CPUS",	NULL, PARAM_VCPUS},
{"CPUMASK",	NULL, PARAM_CPUMASK},
{"NODEMASK",	NULL, PARAM_NODEMASK},
/* create param	*/
{"ONBOOT",	NULL, PARAM_ONBOOT},
{"CONFIGFILE",	NULL, PARAM_CONFIG},
{"ORIGIN_SAMPLE",NULL,PARAM_CONFIG_SAMPLE},
{"DISABLED",	NULL, PARAM_DISABLED},
#ifdef HAVE_UPSTREAM
{"LOCAL_UID",	NULL, PARAM_LOCAL_UID},
{"LOCAL_GID",	NULL, PARAM_LOCAL_GID},
#endif
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
{"IOLIMIT",	NULL, PARAM_IOLIMIT},
{"IOPSLIMIT",	NULL, PARAM_IOPSLIMIT},
{"BOOTORDER",	NULL, PARAM_BOOTORDER},
{"STOP_TIMEOUT", NULL, PARAM_STOP_TIMEOUT},
{"VM_OVERCOMMIT", NULL, PARAM_VM_OVERCOMMIT},

/* These ones are either known parameters for global config file,
 * or some obsoleted parameters used in the past. In both cases
 * vzctl is not interested in those, but it doesn't make sense to
 * report those as being unknown.
 */
{"VIRTUOZZO",		NULL, PARAM_IGNORED},
{"VE0CPUUNITS",		NULL, PARAM_IGNORED},
{"VZFASTBOOT",		NULL, PARAM_IGNORED},
{"NEIGHBOUR_DEVS",	NULL, PARAM_IGNORED},
{"ERROR_ON_ARPFAIL",	NULL, PARAM_IGNORED},
{"VZWDOG",		NULL, PARAM_IGNORED},
{"IPTABLES_MODULES",	NULL, PARAM_IGNORED},
{"IP6TABLES",		NULL, PARAM_IGNORED},
{"CONFIG_CUSTOMIZED",	NULL, PARAM_IGNORED},
{"VE_STOP_MODE",	NULL, PARAM_IGNORED},
{"SKIP_SYSCTL_SETUP",	NULL, PARAM_IGNORED},
{"SKIP_ARPDETECT",	NULL, PARAM_IGNORED},
{"VE_PARALLEL",		NULL, PARAM_IGNORED},

{NULL,		NULL, -1}
};

static const vps_config *conf_get_by_name(const vps_config *conf,
		const char *name)
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

static const vps_config *conf_get_by_id(const vps_config *conf, int id)
{
	const vps_config *p;

	if (conf == NULL)
		return NULL;
	for (p = conf; p->name != NULL; p++)
		if (p->id == id)
			return p;
	return NULL;
}

static int opt_get_by_id(struct option *opt, int id)
{
	struct option *p;

	for (p = opt; p->name != NULL; p++)
		if (p->val == id)
			return p->val;
	return -1;
}

static conf_struct *find_conf_line(list_head_t *head, const char *name,
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

static long long get_mul(char c)
{
	switch (c) {
	case 'T':
	case 't':
		return 1024ll * 1024 * 1024 * 1024;
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

static int conf_parse_strlist(list_head_t *list, const char *val)
{
	if (add_str2list(list, val))
		return ERR_NOMEM;
	return 0;
}

static int conf_parse_str(char **dst, const char *val)
{
	if (*dst)
		return ERR_DUP;
	*dst = strdup(val);
	if (*dst == NULL)
		return ERR_NOMEM;
	return 0;
}

static int conf_parse_yesno(int *dst, const char *val)
{
	int ret;

	if (*dst)
		return ERR_DUP;
	if ((ret = yesno2id(val)) < 0)
		return ERR_INVAL;
	*dst = ret;
	return 0;
}

static int conf_parse_ulong(unsigned long **dst, const char *valstr)
{
	unsigned long val;
	if (*dst != NULL)
		return ERR_DUP;
	if (parse_ul(valstr, &val) != 0)
		return ERR_INVAL;
	*dst = malloc(sizeof(unsigned long));
	if (*dst == NULL)
		return ERR_NOMEM;
	**dst = val;
	return 0;
}

static int conf_parse_float(float **dst, const char *valstr)
{
	float val;
	if (*dst != NULL)
		return ERR_DUP;
	if (sscanf(valstr, "%f", &val) != 1)
		return ERR_INVAL;
	*dst = malloc(sizeof(float));
	if (*dst == NULL)
		return ERR_NOMEM;
	**dst = val;

	return 0;
}
static int conf_parse_bitmap(unsigned long **dst, int nmaskbits,
			     const char *valstr)
{
	if (*dst != NULL)
		return ERR_DUP;

	*dst = alloc_bitmap(nmaskbits);
	if (*dst == NULL)
		return ERR_NOMEM;

	if (!strcmp(valstr, "all")) {
		bitmap_set_all(*dst, nmaskbits);
		return 0;
	}
	if (bitmap_parse(valstr, *dst, nmaskbits) != 0) {
		free(*dst);
		*dst = NULL;
		return ERR_INVAL;
	}
	return 0;
}

static int conf_parse_guid(char **dst, const char *val)
{
	char guid[64];

	if (vzctl_get_normalized_guid(val, guid, sizeof(guid)))
		return ERR_INVAL;

	return conf_parse_str(dst, guid);
}

static int conf_store_strlist(list_head_t *conf, const char *name,
		list_head_t *val, int allow_empty)
{
	char *str;

	if (list_empty(val) && !allow_empty)
		return 0;
	if ((str = list2str_c(name, '"', val)) == NULL)
		return ERR_NOMEM;
	if (add_str_param2(conf, str)) {
		free(str);
		return ERR_NOMEM;
	}
	return 0;
}

static int conf_store_str(list_head_t *conf, const char *name, const char *val)
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

static int conf_store_yesno(list_head_t *conf, const char *name, int val)
{
	if (!val)
		return 0;
	return conf_store_str(conf, name, val == YES ? "yes" : "no");
}

static int conf_store_ulong(list_head_t *conf, const char *name,
		unsigned long *val)
{
	char buf[] = "18446744073709551615"; /* ULONG_MAX on 64 bit */

	if (val == NULL)
		return 0;

	snprintf(buf, sizeof(buf), "%lu", *val);
	return conf_store_str(conf, name, buf);
}

static int conf_store_int(list_head_t *conf, const char *name, int val)
{
	char buf[] = "2147483647"; /* INT_MAX */

	snprintf(buf, sizeof(buf), "%d", val);
	return conf_store_str(conf, name, buf);
}

static int conf_store_float(list_head_t *conf, const char *name, float val)
{
	char buf[] = "1.34567e+12";

	snprintf(buf, sizeof(buf), "%g", val);
	return conf_store_str(conf, name, buf);
}

static int conf_store_bitmap(list_head_t *conf, const char *name,
			     unsigned long *val, int nmaskbits)
{
	int ret;
	char *buf;
	unsigned int buflen = nmaskbits * 2;

	if (val == NULL)
		return 0;

	if (bitmap_find_first_zero_bit(val, nmaskbits) == nmaskbits) {
		conf_store_str(conf, name, "all");
		return 0;
	}

	buf = malloc(buflen);
	if (buf == NULL)
		return ERR_NOMEM;

	bitmap_snprintf(buf, buflen, val, nmaskbits);
	ret = conf_store_str(conf, name, buf);

	free(buf);
	return ret;
}

/******************** Features *************************/
static int parse_features(env_param_t *env, char *val)
{
	int ret = 0;
	char *token;
	struct feature_s *feat;

	for_each_strtok(token, val, "\t ,") {
		feat = find_feature(token);
		if (!feat) {
			logger(0, 0, "Warning: Unknown feature: %s", token);
			ret = ERR_INVAL_SKIP;
			continue;
		}
		if (feat->on)
			env->features_mask |= feat->mask;
		env->features_known |= feat->mask;
	}

	return ret;
}

static void store_features(unsigned long long mask, unsigned long long known,
		vps_config *conf, list_head_t *conf_h)
{
	char buf[2 * STR_SIZE];
	int r;

	r = snprintf(buf, sizeof(buf) - 1, "%s=\"", conf->name);
	features_mask2str(mask, known, " ", buf + r, sizeof(buf) - r - 2);
	strcat(buf, "\"");
	add_str_param(conf_h, buf);
}

static int parse_ioprio(io_param *io, char *val)
{
	if (parse_int(val, &io->ioprio))
		return ERR_INVAL;
	if (io->ioprio < VE_IOPRIO_MIN || io->ioprio > VE_IOPRIO_MAX) {
		io->ioprio = -1;
		return ERR_INVAL;
	}
	return 0;
}

static int parse_iolimit(io_param *io, char *val, int def_mul)
{
	char *tail;
	unsigned long long tmp;
	long long n;
	int ret = 0;

	if (!strcmp(val, "unlimited")) {
		io->iolimit = 0;
		return 0;
	}

	errno = 0;
	tmp = strtoull(val, &tail, 10);
	if (errno == ERANGE)
		goto inval;

	if (tmp > INT_MAX) {
		tmp = INT_MAX;
		ret = ERR_LONG_TRUNC;
	}

	if (*tail != '\0') {
		n = get_mul(*tail++);

		if ((n < 0) || (*tail != '\0'))
			goto inval;

		tmp = tmp * n;
	} else {
		tmp *= def_mul;
	}

	/* After multiplication, truncate again */
	if (tmp > INT_MAX) {
		tmp = INT_MAX;
		ret = ERR_LONG_TRUNC;
	}

	io->iolimit = tmp;

	return ret;

inval:
	io->iolimit = -1;

	return ERR_INVAL;
}

static int parse_iopslimit(io_param *io, char *val)
{
	int tmp;

	if (parse_int(val, &tmp))
		goto inval;

	if (tmp < 0)
		goto inval;

	io->iopslimit = tmp;

	return 0;

inval:
	io->iopslimit = -1;

	return ERR_INVAL;
}

static int store_io(vps_param *old_p, vps_param *vps_p,
				 vps_config *conf, list_head_t *conf_h)
{
	io_param *io_param = &vps_p->res.io;
	int val = -1;

	switch (conf->id) {
		case PARAM_IOPRIO:
			val = io_param->ioprio;
			break;
		case PARAM_IOLIMIT:
			val = io_param->iolimit;
			break;
		case PARAM_IOPSLIMIT:
			val = io_param->iopslimit;
			break;
	}

	if (val < 0)
		return 0;

	return conf_store_int(conf_h, conf->name, val);
}

/******************** Iptables *************************/
static void store_iptables(unsigned long long ipt_mask, vps_config *conf,
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
	env_param_t *env = &vps_p->res.env;

	switch (conf->id) {
	case PARAM_IPTABLES:
		if (!env->ipt_mask)
			break;
		store_iptables(env->ipt_mask, conf, conf_h);
		break;
	case PARAM_NETFILTER:
		if (!env->nf_mask)
			break;
		conf_store_str(conf_h, conf->name,
				netfilter_mask2str(env->nf_mask));
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

/* This function parses string in form xxx[GMKPB]
 */
static const char *parse_ul_sfx(const char *str, unsigned long long *val,
		int divisor, int allow_unlimited)
{
	long long n;
	char *tail;
	long double v = 0;

	if (!str || !val)
		return NULL;
	if (allow_unlimited && !strncmp(str, "unlimited", 9)) {
		*val = LONG_MAX;
		return str + 9;
	}
	errno = 0;
	*val = strtoull(str, &tail, 10);
	if (errno == ERANGE)
		return NULL;
	v = *val;
	if (*tail == '.') { /* Floating point */
		errno = 0;
		v = strtold(str, &tail);
		if (errno == ERANGE)
			return NULL;
		*val = (unsigned long long) v;
	}
	if (*tail != ':' && *tail != '\0') {
		if (!divisor)
			return NULL;
		if ((n = get_mul(*tail)) < 0)
			return NULL;
		v = v * n / divisor;
		if (v > (long double) LONG_MAX)
			*val = LONG_MAX + 1UL; /* Overflow */
		else
			*val = (unsigned long long) v;
		++tail;
	}
	return tail;
}

/* This function parses string in form xxx[GMKPB]:yyy[GMKPB]
 * If :yyy is omitted, it is set to xxx.
 */
static int parse_twoul_sfx(const char *str, unsigned long *val,
		int divisor, int allow_unlimited)
{
	unsigned long long tmp;
	int ret = 0;

	if (!(str = parse_ul_sfx(str, &tmp, divisor, allow_unlimited)))
		return ERR_INVAL;
	if (tmp > LONG_MAX) {
		tmp = LONG_MAX;
		ret = ERR_LONG_TRUNC;
	}
	val[0] = tmp;
	if (*str == ':') {
		str = parse_ul_sfx(++str, &tmp, divisor, allow_unlimited);
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
static int parse_meminfo(meminfo_param *param, const char *val)
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
	if ((mode != VE_MEMINFO_NONE) && meminfo_val == 0)
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

static int parse_ub(vps_param *vps_p, const char *val, int id, int divisor)
{
	int ret;
	ub_res res;

	if (conf_get_by_id(config, id) == NULL)
		return ERR_OTHER;
	ret = parse_twoul_sfx(val, res.limit, divisor, 1);
	if (ret && ret != ERR_LONG_TRUNC)
		return ret;
	res.res_id = id;
	if (add_ub_param(&vps_p->res.ub, &res))
		return ERR_NOMEM;
	return ret;
}

static int parse_vswap(ub_param *ub, const char *val, int id, int force)
{
	int ret=0;
	ub_res res;
	const char *tail;
	unsigned long long tmp;

	/* If current kernel is not vswap-capable, cry out loud */
	if (!force && !is_vswap_mode()) {
		logger(-1, 0, "Error: kernel does not support vswap, "
				"unable to use --ram/--swap parameters");
		return ERR_OTHER;
	}

	/* Translate from VSwap ('easy') to UBC parameter */
	switch(id) {
		case PARAM_RAM:
			id = PARAM_PHYSPAGES;
			break;
		case PARAM_SWAP:
			id = PARAM_SWAPPAGES;
			break;
		default:
			return ERR_OTHER;
	}

	if (conf_get_by_id(config, id) == NULL)
		return ERR_OTHER;

	tail = parse_ul_sfx(val, &tmp, 1, 1);
	if (tail == NULL)
		return ERR_INVAL;
	if (*tail != '\0')
		return ERR_INVAL;
	tmp = (tmp + _page_size - 1) / _page_size;
	if (tmp > LONG_MAX) {
		tmp = LONG_MAX;
		ret = ERR_LONG_TRUNC;
	}

	res.res_id = id;
	res.limit[0] = 0;
	res.limit[1] = tmp;
	if (add_ub_param(ub, &res))
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
	snprintf(buf, sizeof(buf), "%s=\"%s\"", conf->name,		\
		ubcstr(ub->res[0], ub->res[1]));			\
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
	ADD_UB_PARAM(swappages, PARAM_SWAPPAGES)
#undef ADD_UB_PARAM

	if (ub->vm_overcommit && ub->vm_overcommit != 0) {
		conf = conf_get_by_id(config, PARAM_VM_OVERCOMMIT);
		conf_store_float(conf_h, conf->name, *ub->vm_overcommit);
	}

	return 0;
}

/********************** Capability ********************/

static int parse_cap(char *str, cap_param *cap)
{
	unsigned int len;
	char *p, *token;
	char cap_nm[128];
	unsigned long *mask;

	for_each_strtok(token, str, "\t ,") {
		if ((p = strrchr(token, ':')) == NULL)
			goto err;
		if (!strcmp(p + 1, "off"))
			mask = &cap->off;
		else if (!strcmp(p + 1, "on"))
			mask = &cap->on;
		else
			goto err;
		len = p - token;
		if (len + 1 > sizeof(cap_nm))
			return ERR_INVAL;
		strncpy(cap_nm, token, len);
		cap_nm[len] = 0;
		if (get_cap_mask(cap_nm, mask)) {
			logger(-1, 0, "Capability %s is unknown", cap_nm);
			return ERR_INVAL;
		}
	}

	return 0;
err:
	logger(-1, 0, "Can't parse capability %s (expecting capname:on|off)",
			token);
	return ERR_INVAL;
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
	p += sprintf(buf, "%s=\"", conf->name);
	len = buf + sizeof(buf) - p - 1;
	build_cap_str(cap, &old_p->res.cap, " ", p, len);
	strcat(p, "\"");
	add_str_param(conf_h, buf);

	return 0;
}
/********************** Network ************************/

static int parse_ip(vps_param *vps_p, char *val, int id)
{
	char *token;
	char *dst;
	net_param *net;

	if (id == PARAM_IP_ADD)
		net = &vps_p->res.net;
	else if (id == PARAM_IP_DEL)
		net = &vps_p->del_res.net;
	else
		return 0;

	for_each_strtok(token, val, "\t ") {
		if (id == PARAM_IP_DEL && !strcmp(token, "all")) {
			vps_p->res.net.delall = YES;
			vps_p->del_res.net.delall = YES;
			continue;
		}
		if (!strcmp(token, "0.0.0.0"))
			continue;
		if (!strcmp(token, "::"))
			continue;
		if (!strcmp(token, "::0"))
			continue;
		dst = canon_ip(token);
		if (dst == NULL)
			return ERR_INVAL;
		if (!find_ip(&net->ip, dst))
			add_str_param(&net->ip, dst);
	}

	return 0;
}

static int store_ip(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	list_head_t ip;
	int ret;

	if (conf->id != PARAM_IP_ADD)
		return 0;
	if (!vps_p->res.net.delall &&
		list_empty(&vps_p->res.net.ip) &&
		list_empty(&vps_p->del_res.net.ip))
	{
		return 0;
	}
	list_head_init(&ip);
	merge_ip_list(vps_p->res.net.delall, &old_p->res.net.ip,
		&vps_p->res.net.ip, &vps_p->del_res.net.ip, &ip);
	ret = conf_store_strlist(conf_h, conf->name, &ip, 1);
	free_str_param(&ip);

	return ret;
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

static int add_netdev(net_param *net, char *val)
{
	char *token;

	for_each_strtok(token, val, "\t ") {
		if (check_netdev(token))
			return ERR_INVAL;
		add_str_param(&net->dev, token);
	}

	return 0;
}

static int store_netdev(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	list_head_t dev;
	int ret;

	if (conf->id != PARAM_NETDEV_ADD)
		return 0;
	if (list_empty(&vps_p->res.net.dev) &&
		list_empty(&vps_p->del_res.net.dev))
	{
		return 0;
	}
	list_head_init(&dev);
	merge_str_list(0, &old_p->res.net.dev, &vps_p->res.net.dev,
		 &vps_p->del_res.net.dev, &dev);
	ret = conf_store_strlist(conf_h, conf->name, &dev, 1);
	free_str_param(&dev);

	return ret;
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
	case PARAM_MOUNT_OPTS:
		ret = conf_store_str(conf_h, conf->name, fs->mount_opts);
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
static int parse_dq(unsigned long **param, const char *val, int divisor)
{
	int ret;
	unsigned long *tmp;

	tmp = malloc(sizeof(unsigned long) * 2);
	if (tmp == NULL)
		return ERR_NOMEM;
	ret = parse_twoul_sfx(val, tmp, divisor, 0);
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
	case PARAM_DISK_QUOTA:
		conf_store_yesno(conf_h, conf->name, param->enable);
		break;
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
		conf_store_ulong(conf_h, conf->name, param->exptime);
		break;
	case PARAM_QUOTAUGIDLIMIT:
		conf_store_ulong(conf_h, conf->name, param->ugidlimit);
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

static const char* devperm2str(unsigned long perms)
{
	static char mask[4];
	int i=0;
	if (perms & S_IROTH)
		mask[i++] = 'r';
	if (perms & S_IWOTH)
		mask[i++] = 'w';
	if (perms & S_IXGRP)
		mask[i++] = 'q';
	mask[i] = 0;
	return mask;
}

/* Do not allow devices with specific majors */
static int dev_major_denied(int major)
{
	/* loop device */
	if (major == 7)
		return 1;

	return 0;
}

static int parse_devices_str(const char *str, dev_res *dev)
{
	int minor, major;
	char type;
	char minor_str[16];
	char mode[6];

	if (sscanf(str, "%c:%d:%15[^:]:%5s",
			&type, &major, minor_str, mode) != 4)
		return -1;
	if (dev_major_denied(major))
		return -1;
	memset(dev, 0, sizeof(*dev));
	if (type == 'b')
		dev->type = S_IFBLK;
	else if (type == 'c')
		dev->type = S_IFCHR;
	else
		return -1;
	if (!strcmp(minor_str, "all")) {
		dev->use_major = VE_USE_MAJOR;
		dev->type |= VE_USE_MAJOR;
		dev->dev = makedev(major, 0);
	} else {
		dev->type |= VE_USE_MINOR;
		if (parse_int(minor_str, &minor))
			return -1;
		dev->dev = makedev(major, minor);
	}
	if (parse_dev_perm(mode, &dev->mask) != 0)
		return -1;

	return 0;
}

static int parse_dev(vps_param *vps_p, char *val)
{
	char *token;
	dev_res dev;

	for_each_strtok(token, val, " ") {
		if (parse_devices_str(token, &dev))
			return ERR_INVAL;
		if (add_dev_param(&vps_p->res.dev, &dev))
			return ERR_NOMEM;
	}

	return 0;
}

static int store_dev(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	dev_param *odev = &old_p->res.dev; /* from ve.conf */
	dev_param *ndev = &vps_p->res.dev; /* from cmdline (set --devices) */
	dev_res *res;
	int r;
	char *sp, *ep;

	if (conf->id != PARAM_DEVICES)
		return 0;
	if (list_empty(&odev->dev) && list_empty(&ndev->dev))
		return 0;

	sp = buf;
	ep = buf + sizeof(buf) - 1;

#define PRINT_DEV \
do {									\
	int major, minor;						\
									\
	if (sp == buf)							\
		sp += snprintf(buf, sizeof(buf), "%s=\"", conf->name);	\
	major = major(res->dev);					\
	minor = minor(res->dev);					\
	if (res->use_major) {						\
		r = snprintf(sp, ep - sp, "%c:%d:all:%s ",		\
				S_ISBLK(res->type) ? 'b' : 'c', major,	\
				devperm2str(res->mask));		\
	} else {							\
		r = snprintf(sp, ep - sp, "%c:%d:%d:%s ",		\
				S_ISBLK(res->type) ? 'b' : 'c',		\
				major, minor, devperm2str(res->mask));	\
	}								\
	sp += r;							\
	if ((r < 0) || (sp >= ep))					\
		break;							\
} while (0)

	/* First, go through odev, adding all entries
	 * unless they also present in ndev.
	 */
	list_for_each(res, &odev->dev, list) {
		int in_ndev = 0;

		/* Devices with names (--devnodes) are handled by
		 * store_devnodes(), so skip those here */
		if (res->name)
			continue;
		/* Skip devices with 'none' permissions */
		if (!res->mask)
			continue;
		/* Skip devices present in ndev */
		if (! list_empty (&ndev->dev)) {
			dev_res *nres;

			list_for_each(nres, &ndev->dev, list)
				if (res->type == nres->type &&
						res->dev == nres->dev) {
					in_ndev = 1;
					continue;
				}
		}
		if (in_ndev)
			continue;

		PRINT_DEV;
	}

	list_for_each(res, &ndev->dev, list) {
		/* Devices with names (--devnodes) are handled by
		 * store_devnodes(), so skip those here */
		if (res->name)
			continue;
		/* Skip devices with 'none' permissions */
		if (!res->mask)
			continue;

		PRINT_DEV;
	}
	if (sp != buf)
		strcat(buf, "\"");
	else {
		/* Special case: removing all devices
		 * If both odev and ndev lists are non-empty
		 * but the resulting string is, it means all devices
		 * were deleted. In order for vps_merge_conf() to report
		 * configuration has changed we need to print something.
		 */
		if (!list_empty(&odev->dev) && !list_empty (&ndev->dev))
			snprintf(buf, sizeof(buf),
					"%s=\"\"", conf->name);
	}
	add_str_param(conf_h, buf);

	return 0;

#undef PRINT_DEV
}

static int parse_devnodes_str(const char *str, dev_res *dev)
{
	char *ch;
	unsigned int len, buf_len;
	char *buf;
	struct stat st;
	int ret = ERR_INVAL;

	if ((ch = strrchr(str, ':')) == NULL)
		return ERR_INVAL;
	ch++;
	len = ch - str;
	memset(dev, 0, sizeof(*dev));

	dev->name = malloc(len);
	if (dev->name == NULL)
		return ERR_NOMEM;
	snprintf(dev->name, len, "%s", str);

	buf_len = 5 + len;
	buf = alloca(buf_len);
	if (buf == NULL) {
		ret = ERR_NOMEM;
		goto err;
	}
	snprintf(buf, buf_len, "/dev/%s", dev->name);
	if (stat(buf, &st)) {
		logger(-1, errno, "Incorrect device name %s", buf);
		goto err;
	}

	if (S_ISCHR(st.st_mode))
		dev->type = S_IFCHR;
	else if (S_ISBLK(st.st_mode))
		dev->type = S_IFBLK;
	else {
		logger(-1, 0, "The %s is not block or character device", buf);
		goto err;
	}
	if (dev_major_denied(major(st.st_rdev))) {
		logger(-1, 0, "Device %s is not allowed "
				"to be assigned to a CT",
				dev->name);
		goto err;
	}
	dev->dev = st.st_rdev;
	dev->type |= VE_USE_MINOR;
	if (parse_dev_perm(ch, &dev->mask))
		goto err;
	return 0;
err:
	free(dev->name);
	dev->name = NULL;
	return ret;
}

static int parse_devnodes(vps_param *vps_p, char *val)
{
	char *token;
	dev_res dev;

	for_each_strtok(token, val, " ") {
		int ret;
		if (parse_devnodes_str(token, &dev))
			return ERR_INVAL;
		ret = add_dev_param(&vps_p->res.dev, &dev);
		free(dev.name);
		if (ret)
			return ERR_NOMEM;
	}

	return 0;
}

static int store_devnodes(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	char buf[STR_SIZE];
	dev_param *odev = &old_p->res.dev; /* from ve.conf */
	dev_param *ndev = &vps_p->res.dev; /* from cmdline (set --devnodes) */
	dev_res *res;
	int r;
	char *sp, *ep;

	if (conf->id != PARAM_DEVNODES)
		return 0;
	if (list_empty(&odev->dev) && list_empty (&ndev->dev))
		return 0;

	sp = buf;
	*sp = 0;
	ep = buf + sizeof(buf) - 1;

	/* First, go through odev, adding all entries
	 * unless they also present in ndev.
	 */
	list_for_each (res, &odev->dev, list) {
		int in_ndev = 0;
		/* Devices with no names (--devices) are handled by
		 * store_dev(), so skip those here */
		if (!res->name)
			continue;
		/* Skip devices with 'none' permissions */
		if (!res->mask)
			continue;
		/* Skip devices present in ndev */
		if (! list_empty (&ndev->dev)) {
			dev_res *nres;

			list_for_each(nres, &ndev->dev, list)
				if (nres->name &&
						!strcmp(res->name,
							nres->name)) {
					in_ndev = 1;
					continue;
				}
		}
		if (in_ndev)
			continue;
		if (sp == buf)
			sp += snprintf(buf, sizeof(buf), "%s=\"", conf->name);
		r = snprintf(sp, ep - sp, "%s:%s ", res->name,
			devperm2str(res->mask));
		sp += r;
		if ((r < 0) || (sp >= ep))
			break;

	}

	list_for_each (res, &ndev->dev, list) {
		/* Devices with no names (--devices) are handled by
		 * store_dev(), so skip those here */
		if (!res->name)
			continue;
		/* Skip devices with 'none' permissions */
		if (!res->mask)
			continue;
		if (sp == buf)
			sp += snprintf(buf, sizeof(buf), "%s=\"", conf->name);
		r = snprintf(sp, ep - sp, "%s:%s ", res->name,
			devperm2str(res->mask));
		sp += r;
		if ((r < 0) || (sp >= ep))
			break;
	}
	if (sp != buf)
		strcat(buf, "\"");
	else {
		/* Special case: removing all devices.
		 * If both odev and ndev lists are non-empty
		 * but the resulting string is, it means all devices
		 * were deleted. In order for vps_merge_conf() to report
		 * configuration has changed we need to print something.
		 */
		 if (!list_empty(&odev->dev) && !list_empty (&ndev->dev))
			 snprintf(buf, sizeof(buf),
					 "%s=\"\"", conf->name);
	}
	add_str_param(conf_h, buf);
	return 0;
}

static int parse_pci(vps_param *vps_p, char *val, int id)
{
	int domain;
	unsigned int bus;
	unsigned int slot;
	unsigned int func;
	char *token;

	int ret;
	pci_param *pci;
	char buf[64];

	if (id == PARAM_PCI_ADD)
		pci = &vps_p->res.pci;
	else if (id == PARAM_PCI_DEL)
		pci = &vps_p->del_res.pci;
	else
		return 0;

	for_each_strtok(token, val, " ") {
		ret = sscanf(token, "%x:%x:%x.%d", &domain, &bus, &slot, &func);
		if (ret != 4) {
			domain = 0;
			ret = sscanf(token, "%x:%x.%d", &bus, &slot, &func);
			if (ret != 3)
				return ERR_INVAL;
		}
		snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%d",
						domain, bus, slot, func);
		if (!find_str(&pci->list, buf))
			add_str_param(&pci->list, buf);
	}

	return 0;
}

static int store_pci(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	list_head_t pci;
	int ret;

	if (conf->id != PARAM_PCI_ADD)
		return 0;

	if (list_empty(&vps_p->res.pci.list) &&
			list_empty(&vps_p->del_res.pci.list))
		return 0;

	list_head_init(&pci);
	merge_str_list(0, &old_p->res.pci.list, &vps_p->res.pci.list,
					&vps_p->del_res.pci.list, &pci);

	ret = conf_store_strlist(conf_h, conf->name, &pci, 1);
	free_str_param(&pci);

	return ret;
}

static int store_misc(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	misc_param *misc = &vps_p->res.misc;
	int ret;

	ret = 0;
	switch (conf->id) {
	case PARAM_ONBOOT:
		ret = conf_store_yesno(conf_h, conf->name, misc->onboot);
		break;
	case PARAM_BOOTORDER:
		ret = conf_store_ulong(conf_h, conf->name,
				misc->bootorder);
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
		ret = conf_store_strlist(conf_h, conf->name,
				&misc->nameserver, 0);
		break;
	case PARAM_DESCRIPTION:
		ret = conf_store_str(conf_h, conf->name, misc->description);
		break;
	case PARAM_SEARCHDOMAIN:
		ret = conf_store_strlist(conf_h, conf->name,
			&misc->searchdomain, 0);
		break;
	case PARAM_LOCAL_UID:
		ret = conf_store_ulong(conf_h, conf->name, misc->local_uid);
		break;
	case PARAM_LOCAL_GID:
		ret = conf_store_ulong(conf_h, conf->name, misc->local_gid);
		break;
	case PARAM_STOP_TIMEOUT:
		if (misc->stop_timeout >= 0)
			ret = conf_store_int(conf_h, conf->name,
					misc->stop_timeout);
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
	if (!*param)
		return ERR_NOMEM;
	**param = val;
	return 0;
}

static int store_cpu(vps_param *old_p, vps_param *vps_p, vps_config *conf,
	list_head_t *conf_h)
{
	cpu_param *cpu = &vps_p->res.cpu;

	switch (conf->id) {
	case PARAM_CPUUNITS:
		conf_store_ulong(conf_h, conf->name, cpu->units);
		break;
	case PARAM_CPUWEIGHT:
		conf_store_ulong(conf_h, conf->name, cpu->weight);
		break;
	case PARAM_CPULIMIT:
		conf_store_ulong(conf_h, conf->name, cpu->limit);
		break;
	case PARAM_VCPUS:
		conf_store_ulong(conf_h, conf->name, cpu->vcpus);
		break;
	case PARAM_CPUMASK:
		if (cpu->cpumask_auto)
			conf_store_str(conf_h, conf->name, "auto");
		else
			conf_store_bitmap(conf_h, conf->name,
					cpu->mask->bits, CPUMASK_NBITS);
		break;
	case PARAM_NODEMASK:
		conf_store_bitmap(conf_h, conf->name,
				cpu->nodemask->bits, NODEMASK_NBITS);
		break;
	}
	return 0;
}

/********************* NETIF **********************************/

static int parse_mac_filter_cmd (veth_dev *dev, char *str)
{

	if (dev == NULL)
		return ERR_INVAL;

	if (!strcmp(str, "on"))
		dev->mac_filter = YES;
	else if (!strcmp(str, "off"))
		dev->mac_filter = NO;
	else
		return ERR_INVAL;
	return 0;
}

static void generate_veth_name(int veid, char *dev_name_ve,
		char *dev_name, int len)
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
	int ret;

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
			logger(-1, 0, "Multiple use of"
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
	case PARAM_NETIF_MAC_FILTER:
		if ((ret = parse_mac_filter_cmd(dev, str)))
			return ret;
		break;
	case PARAM_NETIF_BRIDGE:
		if (dev->dev_bridge[0] != 0) {
			logger(-1, 0, "Multiple use of"
				" --bridge option not allowed");
			return ERR_INVAL;
		}
		if (len > IFNAMSIZE)
			return ERR_INVAL;
		strcpy(dev->dev_bridge, str);
		break;
	}
	return 0;
}

static int parse_netif_str(envid_t veid, const char *str, veth_dev *dev)
{
	const char *p, *next, *ep;
	unsigned int len;
	int err;
	char tmp[256];

	memset(dev, 0, sizeof(*dev));
	next = p = str;
	ep = p + strlen(str);
	do {
		while (*next != '\0' && *next != ',') next++;
		if (!strncmp("ifname=", p, 7)) {
			p += 7;
			len = next - p;
			if (len + 1 > IFNAMSIZE)
				return ERR_INVAL;
			if (dev->dev_name_ve[0] == '\0')
				snprintf(dev->dev_name_ve, len + 1, "%s", p);
		} else if (!strncmp("host_ifname=", p, 12)) {
			p += 12;
			len = next - p;
			if (len + 1 > IFNAMSIZE)
				return ERR_INVAL;
			if (dev->dev_name[0] == '\0')
				snprintf(dev->dev_name, len + 1, "%s", p);
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
		} else if (!strncmp("mac_filter=", p, 11)) {
			p += 11;
			len = next - p;
			if (len == 0)
				continue;
			if (len >= sizeof(tmp))
				return ERR_INVAL;
			strncpy(tmp, p, len);
			tmp[len] = 0;
			err = parse_mac_filter_cmd(dev, tmp);
			if (err)
				return err;
		} else if (!strncmp("bridge=", p, 7)) {
			p += 7;
			len = next - p;
			if (len + 1 > IFNAMSIZE)
				return ERR_INVAL;
			if (dev->dev_bridge[0] == '\0')
				snprintf(dev->dev_bridge, len + 1, "%s", p);
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
	int ret = 0;
	char *token;
	veth_dev dev;

	for_each_strtok(token, val, ";") {
		ret = parse_netif_str(veid, token, &dev);
		if (ret)
			goto out;
		if (!find_veth_by_ifname_ve(&veth->dev, dev.dev_name_ve)) {
			ret = add_veth_param(veth, &dev);
			if (ret)
				goto out;
		}
		free_veth_dev(&dev);
	}
out:
	free_veth_dev(&dev);
	return ret;
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
		if (dev->dev_bridge[0] != 0) {
			sp += snprintf(sp, ep - sp, "bridge=%s,",
				dev->dev_bridge);
			if (sp >= ep)
				break;
		}
		if (dev->addrlen_ve != 0) {
			sp += snprintf(sp, ep - sp,
				"mac=" MAC2STR_FMT ",",
				MAC2STR(dev->dev_addr_ve));
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
				"host_mac=" MAC2STR_FMT ",",
				MAC2STR(dev->dev_addr));
			if (sp >= ep)
				break;
		}
		if (dev->mac_filter) {
			sp += snprintf(sp, ep - sp, "mac_filter=%s,",
				dev->mac_filter == YES ? "on" : "off");
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
}

static int parse_netif_str_cmd(envid_t veid, const char *str, veth_dev *dev)
{
	const char *ch, *tmp, *ep;
	int len, err;

	memset(dev, 0, sizeof(*dev));
	ep = str + strlen(str);
	/* Creating veth device */
	/* Parsing veth device name in CT */
	if ((ch = strchr(str, ',')) == NULL) {
		ch = ep;
		len = ep - str;
	} else {
		len = ch - str;
		ch++;
	}
	if (len + 1 > IFNAMSIZE)
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
	/* Parsing veth MAC address in CT */
	if ((ch = strchr(tmp, ',')) == NULL) {
		ch = ep;
		len = ch - tmp;
	} else {
		len = ch - tmp;
		ch++;
	}

	if (len) {
		if (len != MAC_SIZE) {
			logger(-1, 0, "Invalid container MAC address length");
			return ERR_INVAL;
		}
		err = parse_hwaddr(tmp, dev->dev_addr_ve);
		if (err) {
			logger(-1, 0, "Invalid container MAC address format");
			return ERR_INVAL;
		}
	} else {
		generate_mac(veid, dev->dev_name_ve, dev->dev_addr_ve);
	}
	dev->addrlen_ve = ETH_ALEN;

	tmp = ch;

	if (ch == ep) {
		generate_veth_name(veid, dev->dev_name_ve, dev->dev_name,
			sizeof(dev->dev_name));
		memcpy(dev->dev_addr, dev->dev_addr_ve, ETH_ALEN);
		dev->addrlen = ETH_ALEN;
		return 0;
	}
	/* Parsing veth name in CT0 */
	if ((ch = strchr(tmp, ',')) == NULL) {
		ch = ep;
		len = ch - tmp;
	} else {
		len = ch - tmp;
		ch++;
	}
	if (len) {
		if (len + 1 > IFNAMSIZE)
			return ERR_INVAL;
		snprintf(dev->dev_name, len + 1, "%s", tmp);
		if (ch == ep) {
			memcpy(dev->dev_addr, dev->dev_addr_ve, ETH_ALEN);
			dev->addrlen = ETH_ALEN;
			return 0;
		}
	} else {
		generate_veth_name(veid, dev->dev_name_ve, dev->dev_name,
				sizeof(dev->dev_name));
	}

	tmp = ch;

	/* Parsing veth MAC address in CT */
	if ((ch = strchr(tmp, ',')) == NULL) {
		ch = ep;
		len = ch - tmp;
	} else {
		len = ch - tmp;
		ch++;
	}

	if (len) {
		if (len != MAC_SIZE) {
			logger(-1, 0, "Invalid host MAC address");
			return ERR_INVAL;
		}
		err = parse_hwaddr(tmp, dev->dev_addr);
		if (err) {
			logger(-1, 0, "Invalid host MAC address");
			return ERR_INVAL;
		}
		dev->addrlen = ETH_ALEN;
		if (ch == ep) {
			return 0;
		}
	} else {
		generate_mac(veid, dev->dev_name, dev->dev_addr);
		dev->addrlen = ETH_ALEN;
	}

	/* Parsing bridge name */
	len = strlen (ch);

	if (len + 1 > IFNAMSIZE)
		return ERR_INVAL;

	snprintf(dev->dev_bridge, len + 1, "%s", ch);

	return 0;
}

static int parse_netif_cmd(envid_t veid, veth_param *veth, char *val)
{
	int ret;
	char *token;
	veth_dev dev;

	for_each_strtok(token, val, " ") {
		if (parse_netif_str_cmd(veid, token, &dev))
			return ERR_INVAL;
		ret = add_veth_param(veth, &dev);
		if (ret)
			return ret;
	}

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

static int parse_ve_layout(int *layout, int *mode, const char *val)
{
	if (!strcmp(val, "simfs")) {
		*layout = VE_LAYOUT_SIMFS;
		return 0;
	}
	else if (!strncmp(val, "ploop", 5)) {
		if (val[5] == '\0') {
			*layout = VE_LAYOUT_PLOOP;
			*mode = -1; /* unset */
			return 0;
		}
		else if (val[5] == ':') {
			int ret;
			ret = get_ploop_type(val + 6);
			if (ret < 0)
				return ERR_INVAL;
			*layout = VE_LAYOUT_PLOOP;
			*mode = ret;
			return 0;
		}
	}
	return ERR_INVAL;
}

static int store_ve_layout(vps_param *old_p, vps_param *vps_p, vps_config *conf,
		list_head_t *conf_h)
{
	int ret = 0;
	int layout;

	if (conf->id != PARAM_VE_LAYOUT)
		return 0;

	layout = vps_p->res.fs.layout ?: old_p->res.fs.layout;
	if (layout) {
		ret = conf_store_str(conf_h, conf->name,
			layout == VE_LAYOUT_PLOOP ? "ploop" : "simfs");
	}
	return ret;
}

int save_ve_layout(int veid, vps_param *param, int layout)
{
	int ret;
	struct vps_param *fixed;
	char path[PATH_MAX];

	fixed = init_vps_param();
	fixed->res.fs.layout = layout;
	get_vps_conf_path(veid, path, sizeof(path));
	ret = vps_save_config(0, path, fixed, param, NULL);
	free_vps_param(fixed);
	return ret;
}

static int parse(envid_t veid, vps_param *vps_p, char *val, int id)
{
	int ret;
	int int_id;

	ret = 0;
	if (!_page_size) {
		_page_size = get_pagesize();
		if (_page_size < 0)
			return ERR_OTHER;
	}
	switch (id) {
	case PARAM_CONFIG:
		ret = conf_parse_str(&vps_p->opt.config, val);
		break;
	case PARAM_CONFIG_SAMPLE:
		ret = conf_parse_str(&vps_p->opt.origin_sample, val);
		break;
	case PARAM_SAVE:
		vps_p->opt.save = YES;
		break;
	case PARAM_FORCE:
		vps_p->opt.save_force = YES;
		break;
	case PARAM_RESET_UB:
		vps_p->opt.reset_ub = YES;
		break;
	case PARAM_ONBOOT:
		ret = conf_parse_yesno(&vps_p->res.misc.onboot, val);
		break;
	case PARAM_DISABLED:
		ret = conf_parse_yesno(&vps_p->opt.start_disabled, val);
		break;
	case PARAM_HOSTNAME:
		ret = conf_parse_str(&vps_p->res.misc.hostname, val);
		break;
	case PARAM_DESCRIPTION:
		ret = conf_parse_str(&vps_p->res.misc.description, val);
		break;
	case PARAM_NAMESERVER:
		ret = conf_parse_strlist(&vps_p->res.misc.nameserver, val);
		break;
	case PARAM_SEARCHDOMAIN:
		ret = conf_parse_strlist(&vps_p->res.misc.searchdomain, val);
		break;
	case PARAM_USERPW:
		ret = conf_parse_strlist(&vps_p->res.misc.userpw, val);
		break;
	case PARAM_APPLYCONFIG_MAP:
		if (!strcmp(val, "name"))
			vps_p->opt.apply_cfg_map = APPCONF_MAP_NAME;
		else
			ret = ERR_INVAL;
		break;
	case PARAM_APPLYCONFIG:
		ret = conf_parse_str(&vps_p->opt.apply_cfg, val);
		break;
	case PARAM_SETMODE:
		ret = parse_setmode(vps_p, val);
		break;
	case PARAM_LOGFILE:
		ret = conf_parse_str(&vps_p->log.log_file, val);
		break;
	case PARAM_LOCKDIR:
		ret = conf_parse_str(&vps_p->opt.lockdir, val);
		break;
	case PARAM_DUMPDIR:
		ret = conf_parse_str(&vps_p->res.cpt.dumpdir, val);
		break;
	case PARAM_LOGGING:
		ret = conf_parse_yesno(&vps_p->log.enable, val);
		break;
	case PARAM_LOGLEVEL:
		if (parse_int(val, &int_id))
			return ERR_INVAL;
		vps_p->log.level = int_id;
		break;
	case PARAM_VERBOSE:
		if (vps_p->log.verbose != NULL)
			return ERR_DUP;
		if (parse_int(val, &int_id))
			return ERR_INVAL;
		vps_p->log.verbose = malloc(sizeof(*vps_p->log.verbose));
		if (vps_p->log.verbose == NULL)
			return ERR_NOMEM;
		*vps_p->log.verbose = int_id;
		break;
	case PARAM_IPTABLES:
		/* TODO:
		 * vzctl  4.8: warn the option is obsoleted by netfilter
		 * vzctl  4.9: ignore from command line with a warning
		 * vzctl 4.10: ignore from configuration file?
		 * vzctl 4.11: remove?
		 */
		ret = parse_iptables(&vps_p->res.env, val);
		break;
	case PARAM_NETFILTER:
		ret = parse_netfilter(&vps_p->res.env, val);
		break;
	case PARAM_LOCAL_UID:
		conf_parse_ulong(&vps_p->res.misc.local_uid, val);
		break;
	case PARAM_LOCAL_GID:
		conf_parse_ulong(&vps_p->res.misc.local_gid, val);
		break;
	case PARAM_LOCKEDPAGES:
	case PARAM_PRIVVMPAGES:
	case PARAM_SHMPAGES:
	case PARAM_PHYSPAGES:
	case PARAM_VMGUARPAGES:
	case PARAM_OOMGUARPAGES:
	case PARAM_SWAPPAGES:
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
	case PARAM_RAM:
	case PARAM_SWAP:
		ret = parse_vswap(&vps_p->res.ub, val, id,
				vps_p->opt.save_force);
		break;
	case PARAM_VM_OVERCOMMIT:
		ret = conf_parse_float(&vps_p->res.ub.vm_overcommit, val);
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
		ret = conf_parse_yesno(&vps_p->res.net.ipv6_net, val);
		vps_p->del_res.net.ipv6_net = vps_p->res.net.ipv6_net;
		break;
	case PARAM_NETDEV_ADD:
		ret = add_netdev(&vps_p->res.net, val);
		break;
	case PARAM_NETDEV_DEL:
		ret = add_netdev(&vps_p->del_res.net, val);
		break;
	case PARAM_ROOT:
		if (!(ret = conf_parse_str(&vps_p->res.fs.root_orig, val)))
			vps_p->res.fs.root = subst_VEID(veid, val);
		break;
	case PARAM_PRIVATE:
		if (!(ret = conf_parse_str(&vps_p->res.fs.private_orig, val)))
			vps_p->res.fs.private = subst_VEID(veid, val);
		break;
	case PARAM_TEMPLATE:
		ret = conf_parse_str(&vps_p->res.fs.tmpl, val);
		break;
	case PARAM_VE_LAYOUT:
		ret = parse_ve_layout(&vps_p->res.fs.layout,
				&vps_p->res.fs.mode, val);
		break;
	case PARAM_DEF_OSTEMPLATE:
		ret = conf_parse_str(&vps_p->res.tmpl.def_ostmpl, val);
		break;
	case PARAM_OSTEMPLATE:
		ret = conf_parse_str(&vps_p->res.tmpl.ostmpl, val);
		break;
	case PARAM_DISK_QUOTA:
		ret = conf_parse_yesno(&vps_p->res.dq.enable, val);
		break;
	case PARAM_DISKSPACE:
		if (vps_p->res.dq.diskspace != NULL)
			return ERR_DUP;
		if (parse_dq(&vps_p->res.dq.diskspace, val, 1024))
			return ERR_INVAL;
		break;
	case PARAM_DISKINODES:
		if (vps_p->res.dq.diskinodes != NULL)
			return ERR_DUP;
		if (parse_dq(&vps_p->res.dq.diskinodes, val, 1))
			return ERR_INVAL;
		break;
	case PARAM_QUOTATIME:
		ret = conf_parse_ulong(&vps_p->res.dq.exptime, val);
		break;
	case PARAM_QUOTAUGIDLIMIT:
		ret = conf_parse_ulong(&vps_p->res.dq.ugidlimit, val);
		break;
	case PARAM_OFFLINE_RESIZE:
		vps_p->res.dq.offline_resize = YES;
		break;
	case PARAM_DEVICES:
		ret = parse_dev(vps_p, val);
		break;
	case PARAM_PCI_ADD:
	case PARAM_PCI_DEL:
		ret = parse_pci(vps_p, val, id);
		break;
	case PARAM_DEVNODES:
		ret = parse_devnodes(vps_p, val);
		break;
	case PARAM_CPUUNITS:
		ret = conf_parse_ulong(&vps_p->res.cpu.units, val);
		if (ret != 0)
			break;

		if ((*vps_p->res.cpu.units < MINCPUUNITS ||
				*vps_p->res.cpu.units > MAXCPUUNITS))
		{
			free(vps_p->res.cpu.units);
			vps_p->res.cpu.units = NULL;
			ret = ERR_INVAL;
		}
		break;
	case PARAM_CPUWEIGHT:
		ret = conf_parse_ulong(&vps_p->res.cpu.weight, val);
		break;
	case PARAM_CPULIMIT:
		if (vps_p->res.cpu.limit != NULL)
			return ERR_DUP;
		ret = parse_cpulimit(&vps_p->res.cpu.limit, val);
		break;
	case PARAM_VCPUS:
		ret = conf_parse_ulong(&vps_p->res.cpu.vcpus, val);
		break;
	case PARAM_CPUMASK:
		if (strcmp(val, "auto") == 0)
			vps_p->res.cpu.cpumask_auto = 1;
		else
			ret = conf_parse_bitmap(
				(unsigned long **)&vps_p->res.cpu.mask,
				CPUMASK_NBITS, val);
		break;
	case PARAM_NODEMASK:
		ret = conf_parse_bitmap(
				(unsigned long **)&vps_p->res.cpu.nodemask,
				NODEMASK_NBITS, val);
		break;
	case PARAM_MEMINFO:
		ret = parse_meminfo(&vps_p->res.meminfo, val);
		break;
	case PARAM_VETH_ADD:
		logger(0, 0, "Warning: VETH parameter is deprecated, remove "
				"it and use --netif_add instead");
		break;
	case PARAM_NETIF_ADD:
		if (!list_empty(&vps_p->res.veth.dev))
			return ERR_DUP;
		ret = parse_netif(veid, &vps_p->res.veth, val);
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
	case PARAM_NETIF_MAC_FILTER:
	case PARAM_NETIF_BRIDGE:
		ret = add_netif_param(&vps_p->res.veth, id, val);
		break;
	case PARAM_NAME:
		if (vps_p->res.name.name != NULL)
			return ERR_DUP;
		if (check_name(val))
			return ERR_INVAL;
		ret = conf_parse_str(&vps_p->res.name.name, val);
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
		ret = parse_ioprio(&vps_p->res.io, val);
		break;
	case PARAM_IOLIMIT:
	case PARAM_IOLIMIT_MB:
		ret = parse_iolimit(&vps_p->res.io, val,
				/* value from cmdline is in MB/s
				 * unless a suffix is provided;
				 * value from config is in B/s.
				 */
				(id == PARAM_IOLIMIT_MB) ? 1024*1024 : 1);
		break;
	case PARAM_IOPSLIMIT:
		ret = parse_iopslimit(&vps_p->res.io, val);
		break;
	case PARAM_BOOTORDER:
		ret = conf_parse_ulong(&vps_p->res.misc.bootorder, val);
		break;
	case PARAM_SNAPSHOT_GUID:
		ret = conf_parse_guid(&vps_p->snap.guid, val);
		break;
	case PARAM_SNAPSHOT_NAME:
		ret = conf_parse_str(&vps_p->snap.name, val);
		break;
	case PARAM_SNAPSHOT_DESC:
		ret = conf_parse_str(&vps_p->snap.desc, val);
		break;
	case PARAM_SNAPSHOT_SKIP_SUSPEND:
		vps_p->snap.flags |= SNAPSHOT_SKIP_SUSPEND;
		break;
	case PARAM_SNAPSHOT_SKIP_CONFIG:
		vps_p->snap.flags |= SNAPSHOT_SKIP_CONFIG;
		break;
	case PARAM_SNAPSHOT_SKIP_RESUME:
		vps_p->snap.flags |= SNAPSHOT_SKIP_RESUME;
		break;
	case PARAM_SNAPSHOT_MUST_RESUME:
		vps_p->snap.flags |= SNAPSHOT_MUST_RESUME;
		break;
	case PARAM_SNAPSHOT_MOUNT_TARGET:
		ret = conf_parse_str(&vps_p->snap.target, val);
		break;
	case PARAM_MOUNT_OPTS:
		ret = conf_parse_str(&vps_p->res.fs.mount_opts, val);
		break;
	case PARAM_STOP_TIMEOUT:
		if (vps_p->res.misc.stop_timeout != -1)
			return ERR_DUP;
		if (parse_int(val, &int_id))
			return ERR_INVAL;
		if (int_id < 0)
			return ERR_INVAL;
		vps_p->res.misc.stop_timeout = int_id;
		break;
	case PARAM_IGNORED:
		/* Well known but ignored parameter */
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
		store_pci(old_p, vps_p, conf, conf_h);
		store_misc(old_p, vps_p, conf, conf_h);
		store_cpu(old_p, vps_p, conf, conf_h);
		store_meminfo(old_p, vps_p, conf, conf_h);
		store_netif(old_p, vps_p, conf, conf_h);
		store_name(old_p, vps_p, conf, conf_h);
		store_io(old_p, vps_p, conf, conf_h);
		store_ve_layout(old_p, vps_p, conf, conf_h);
	}
	return 0;
}

/********************************************************/
/*	CT parse config stuff				*/
/********************************************************/

int vps_parse_config(envid_t veid, const char *path, vps_param *vps_p,
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
	char *parse_err;
	const vps_config *conf;

	if ((fp = fopen(path, "r")) == NULL) {
		logger(-1, errno, "Unable to open %s", path);
		return VZ_SYSTEM_ERROR;
	}
	if (!stat(path, &st))
		len = st.st_size;
	if (len > 4096)
		str = malloc(len);
	else
		str = alloca(len);
	if (str == NULL) {
		fclose(fp);
		logger(-1, ENOMEM, "Error parsing %s", path);
		return VZ_RESOURCE_ERROR;
	}
	while (fgets(str, len, fp)) {
		line++;
		rtoken = parse_line(str, ltoken, sizeof(ltoken), &parse_err);
		if (rtoken == NULL) {
			if (parse_err != NULL)
				logger(-1, 0, "Warning: can't parse "
						"%s:%d (%s), skipping",
						path, line, parse_err);
			continue;
		}
		if ((conf = conf_get_by_name(config, ltoken)) != NULL) {
			ret = parse(veid, vps_p, rtoken, conf->id);
		} else if (action != NULL)
			ret = mod_parse(veid, action, ltoken, -1, rtoken);
		else {
			logger(1, 0, "Warning at %s:%d: unknown parameter "
				"%s (\"%s\"), ignored",
				path, line, ltoken, rtoken);
			continue;
		}
		if (!ret) {
			continue;
		} else if (ret == ERR_INVAL_SKIP) {
			/* Warning is printed by parse() */
			continue;
		} else if (ret == ERR_LONG_TRUNC) {
			logger(-1, 0, "Warning at %s:%d: too large value "
				"for %s (\"%s\"), truncated",
				path, line, ltoken, rtoken);
		} else if (ret == ERR_DUP) {
			logger(-1, 0, "Warning at %s:%d: duplicate "
				"for %s (\"%s\"), ignored",
				path, line, ltoken, rtoken);
		} else if (ret == ERR_INVAL) {
			logger(-1, 0, "Warning at %s:%d: invalid value "
				"for %s (\"%s\"), skipped",
				path, line, ltoken, rtoken);
		} else if (ret == ERR_UNK) {
			logger(1, 0, "Warning at %s:%d: unknown parameter "
				"%s (\"%s\"), ignored",
				path, line, ltoken, rtoken);
		} else if (ret == ERR_NOMEM) {
			logger(-1, ENOMEM, "Error while parsing %s:%d",
				path, line);
			err = VZ_RESOURCE_ERROR;
			break;
		} else if (ret == ERR_OTHER) {
			logger(-1, 0, "System error while parsing %s:%d",
				path, line);
			err = VZ_SYSTEM_ERROR;
			break;
		} else {
			logger(-1, 0, "Internal error at %s:%d: "
				"bad return value %d from parse(), "
				"parameter %s (\"%s\")", path, line,
				ret, ltoken, rtoken);
		}
	}
	fclose(fp);
	if (len > 4096)
		free(str);
	return err;
}

vps_param *reread_vps_config(envid_t veid)
{
	vps_param *gparam = NULL;
	vps_param *vps_p = NULL;
	char fname[PATH_MAX];

	get_vps_conf_path(veid, fname, sizeof(fname));
	if (stat_file(fname) != 1)
		goto err;

	gparam = init_vps_param();
	if (vps_parse_config(veid, GLOBAL_CFG, gparam, NULL))
		goto err;

	vps_p = init_vps_param();
	if (vps_parse_config(veid, fname, vps_p, NULL))
		goto err;

	merge_vps_param(gparam, vps_p);
	free_vps_param(vps_p);

	return gparam;

err:
	free_vps_param(gparam);
	free_vps_param(vps_p);

	return NULL;
}

/********************************************************/
/*	CT save config stuff				*/
/********************************************************/
static int read_conf(const char *fname, list_head_t *conf_h)
{
	FILE *fp;
	char str[16384];

	if (stat_file(fname) != 1)
		return 0;
	if (!(fp = fopen(fname, "r")))
		return -1;
	while (fgets(str, sizeof(str), fp)) {
		add_str_param(conf_h, str);
	}
	fclose(fp);

	return 0;
}

static int write_conf(const char *fname, list_head_t *head)
{
	char *tmpfile, *file;
	conf_struct *conf;
	FILE * fp;
	int ret = VZ_CONFIG_SAVE_ERROR;
	const char *suffix = ".tmp";
	char *fmt;

	file = canonicalize_file_name(fname);
	if (file == NULL) {
		if (errno != ENOENT) {
			logger(-1, errno, "Unable to resolve path %s", fname);
			return ret;
		}
		file = vz_strdup(fname);
		if (!file)
			return ret;
	}
	tmpfile = vz_malloc(strlen(file) + strlen(suffix) + 1);
	if (!tmpfile)
		goto out_file;
	sprintf(tmpfile, "%s%s", file, suffix);
	if ((fp = fopen(tmpfile, "w")) == NULL) {
		logger(-1, errno, "Unable to create configuration"
			" file %s", tmpfile);
		goto out;
	}
	list_for_each(conf, head, list) {
		if (conf->val == NULL)
			continue;
		if (strchr(conf->val, '\n') == NULL)
			fmt="%s\n";
		else
			fmt="%s";
		if (fprintf(fp, fmt, conf->val) < 0) {
			logger(-1, errno, "Error writing to %s",
					tmpfile);
			fclose(fp);
			goto out2;
		}
	}
	fclose(fp);
	if (rename(tmpfile, file)) {
		logger(-1, errno, "Unable to move %s -> %s",
			tmpfile, file );
		goto out2;
	}
	ret = 0;
out2:
	unlink(tmpfile);
out:
	free(tmpfile);
out_file:
	free(file);
	return ret;
}

static int vps_merge_conf(list_head_t *dst, list_head_t *src)
{
	unsigned int len;
	int cnt = 0;
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

int vps_save_config(envid_t veid, const char *path, vps_param *new_p,
	vps_param *old_p, struct mod_action *action)
{
	vps_param *tmp_old_p = NULL;
	list_head_t conf, new_conf;
	int ret = VZ_CONFIG_SAVE_ERROR;

	list_head_init(&conf);
	list_head_init(&new_conf);
	if (old_p == NULL) {
		tmp_old_p = init_vps_param();
		if (stat_file(path) == 1)
			vps_parse_config(veid, path, tmp_old_p, action);
		old_p = tmp_old_p;
	}
	if (read_conf(path, &conf) != 0)
		goto err_read;
	store(old_p, new_p, &new_conf);
	if (action != NULL)
		mod_save_config(action, &new_conf);
	if (vps_merge_conf(&conf, &new_conf) == 0)
	{
		/* Nothing to save */
		logger(0, 0, "No changes in CT configuration, not saving");
		ret = 0;
		goto out;
	}

	ret = write_conf(path, &conf);
	if (ret == 0)
		logger(0, 0, "CT configuration saved to %s", path);
out:
	free_str_param(&conf);
	free_str_param(&new_conf);
err_read:
	free_vps_param(tmp_old_p);

	return ret;
}

/********************************************************************/
int vps_parse_opt(envid_t veid, struct option *opts, vps_param *param,
	int opt, char *rval, struct mod_action *action)
{
	int id, ret = 0;

	if (param == NULL)
		return -1;
	if ((id = opt_get_by_id(opts, opt)) != -1) {
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
	list_head_init(&param->res.pci.list);
	list_head_init(&param->res.veth.dev);

	list_head_init(&param->del_res.net.ip);
	list_head_init(&param->del_res.net.dev);
	list_head_init(&param->del_res.dev.dev);
	list_head_init(&param->del_res.misc.userpw);
	list_head_init(&param->del_res.misc.nameserver);
	list_head_init(&param->del_res.misc.searchdomain);
	list_head_init(&param->del_res.dev.dev);
	list_head_init(&param->del_res.pci.list);
	list_head_init(&param->del_res.veth.dev);
	param->res.meminfo.mode = -1;
	param->res.io.ioprio = -1;
	param->res.io.iolimit = -1;
	param->res.io.iopslimit = -1;
	param->res.fs.mode = -1;
	param->res.misc.stop_timeout = -1;

	return param;
}

int get_veid_by_name(const char *name)
{
	char buf[STR_SIZE];
	char content[STR_SIZE];
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

static int check_name(const char *name)
{
	const char *p;
	const char *validchars = " -+_.";

	for (p = name; *p != '\0'; p++) {
		if (!isdigit(*p) && !isalpha(*p) && !strchr(validchars, *p))
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
		logger(-1, 0, "Conflict: name %s already used by container %d",
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

#define FREE_P(x)	free(x); x = NULL;

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
	FREE_P(fs->mount_opts)
}

static void free_tmpl(tmpl_param *tmpl)
{
	FREE_P(tmpl->def_ostmpl)
	FREE_P(tmpl->ostmpl)
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
	FREE_P(misc->description)
	FREE_P(misc->bootorder)
	FREE_P(misc->local_uid)
	FREE_P(misc->local_gid)
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

static void free_pci(pci_param *pci)
{
	free_str_param(&pci->list);
}

static void free_cpu(cpu_param *cpu)
{
	FREE_P(cpu->units)
	FREE_P(cpu->weight)
	FREE_P(cpu->limit)
	FREE_P(cpu->vcpus)
	FREE_P(cpu->mask);
	FREE_P(cpu->nodemask);
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
	free_pci(&res->pci);
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
		free(dst->x);					\
		dst->x = strdup(src->x);			\
	}

#define MERGE_INT(x)	if (src->x) dst->x = src->x;

#define MERGE_INT2(x)	if (src->x >= 0) dst->x = src->x;

#define MERGE_P(x)							\
	if ((src->x) != NULL) {						\
		if ((dst->x) == NULL)					\
			dst->x = malloc(sizeof(*(dst->x)));		\
		*dst->x = *src->x;					\
	}

#define MERGE_P2(x)							\
	if ((src->x) != NULL) {						\
		if ((dst->x) == NULL)					\
			dst->x = malloc(sizeof(*(dst->x)) * 2);		\
		dst->x[0] = src->x[0];					\
		dst->x[1] = src->x[1];					\
	}

static void merge_opt(vps_opt *dst, vps_opt *src)
{
	MERGE_INT(save)
	MERGE_INT(fast_kill)
	MERGE_INT(skip_lock)
	MERGE_INT(skip_setup)
	MERGE_INT(skip_umount)
	MERGE_INT(start_disabled)
	MERGE_INT(start_force)
	MERGE_INT(setmode)
	MERGE_INT(apply_cfg_map)

	MERGE_STR(config)
	MERGE_STR(origin_sample)
	MERGE_STR(apply_cfg)
}

void merge_ub(ub_param *dst, ub_param *src)
{
	FOR_ALL_UBC(MERGE_P2)
	MERGE_P(vm_overcommit)
}

static void merge_fs(fs_param *dst, fs_param *src)
{
	MERGE_STR(root)
	MERGE_STR(root_orig)
	MERGE_STR(private)
	MERGE_STR(private_orig)
	MERGE_STR(tmpl)
	MERGE_STR(mount_opts)
	MERGE_INT(flags)
	MERGE_INT(layout)
	MERGE_INT2(mode)
}

static void merge_tmpl(tmpl_param *dst, tmpl_param *src)
{
	MERGE_STR(def_ostmpl)
	MERGE_STR(ostmpl)
	MERGE_STR(dist)
}

static void merge_cpu(cpu_param *dst, cpu_param *src)
{
	MERGE_P(units)
	MERGE_P(weight)
	MERGE_P(limit)
	MERGE_P(vcpus)
	MERGE_P(mask);
	MERGE_P(nodemask);
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
	MERGE_STR(description)
	MERGE_INT(onboot)
	MERGE_P(local_uid)
	MERGE_P(local_gid)
	MERGE_P(bootorder)
	MERGE_INT(wait)
	MERGE_INT2(stop_timeout)
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

static void merge_pci(pci_param *dst, pci_param *src)
{
	MERGE_LIST(list)
}

static void merge_cap(cap_param *dst, cap_param *src)
{
	MERGE_INT(on)
	MERGE_INT(off)
}

static void merge_env(env_param_t *dst, env_param_t *src)
{
	MERGE_INT(veid)
	MERGE_INT(ipt_mask)
	MERGE_INT(nf_mask)
	MERGE_INT(features_mask)
	MERGE_INT(features_known)
}

static void merge_cpt(cpt_param *dst, cpt_param *src)
{
	MERGE_STR(dumpdir)
	MERGE_STR(dumpfile)
	MERGE_INT(ctx)
	MERGE_INT(cpu_flags)
	MERGE_INT(cmd)
}

static void merge_meminfo(meminfo_param *dst, meminfo_param *src)
{
	MERGE_INT2(mode)
	MERGE_INT(val)
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
	MERGE_STR(name)
	MERGE_INT(veid)
}

static void merge_io(io_param *dst, io_param *src) {
	MERGE_INT2(ioprio)
	MERGE_INT2(iolimit)
	MERGE_INT2(iopslimit)
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
	merge_pci(&dst->pci, &src->pci);
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
	MERGE_INT(res.net.ipv6_net)
	MERGE_INT(del_res.net.ipv6_net)
	return 0;
}
