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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <getopt.h>
#include <fnmatch.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <limits.h>
#include <linux/vzcalluser.h>

#include "vzlist.h"
#include "vzconfig.h"
#include "fs.h"
#include "res.h"
#include "logger.h"
#include "util.h"
#include "types.h"
#include "image.h"
#include "vzfeatures.h"

static struct Cveinfo *veinfo = NULL;
static int n_veinfo = 0;

static char g_buf[4096] = "";
static char *p_buf = g_buf;
static char *e_buf = g_buf + sizeof(g_buf) - 1;
static char *host_pattern = NULL;
static char *name_pattern = NULL;
static char *desc_pattern = NULL;
static char *dumpdir = NULL;
static int vzctlfd;
static struct Cfield_order *g_field_order = NULL;
static int is_last_field = 1;
static char *default_field_order = "ctid,numproc,status,ip,hostname";
static char *default_nm_field_order = "smart_name,numproc,status,ip,hostname";
static int g_sort_field = 0;
static int *g_ve_list = NULL;
static int n_ve_list = 0;
static int sort_rev = 0;
static int show_hdr = 1;
static int trim = 1;
static int all_ve = 0;
static int only_stopped_ve = 0;
static long __clk_tck = -1;
static int fmt_json = 0;

char logbuf[32];
static int get_run_ve_proc(int);
#if HAVE_VZLIST_IOCTL
static int get_run_ve_ioctl(int);
static inline int get_run_ve(int update)
{
	int ret;
	ret = get_run_ve_ioctl(update);
	if (ret)
		ret = get_run_ve_proc(update);
	return ret;
}
#else
#define get_run_ve get_run_ve_proc
#endif

#define FOR_ALL_UBC(func)	\
	func(kmemsize)		\
	func(lockedpages)	\
	func(privvmpages)	\
	func(shmpages)		\
	func(numproc)		\
	func(physpages)		\
	func(vmguarpages)	\
	func(oomguarpages)	\
	func(numtcpsock)	\
	func(numflock)		\
	func(numpty)		\
	func(numsiginfo)	\
	func(tcpsndbuf)		\
	func(tcprcvbuf)		\
	func(othersockbuf)	\
	func(dgramrcvbuf)	\
	func(numothersock)	\
	func(dcachesize)	\
	func(numfile)		\
	func(numiptent)		\
	func(swappages)

/* Print functions */
static void print_json_str(const char *s)
{
	if (s)
		printf("\"%s\"", s);
	else
		printf("null");
}

#define PRINT_STR_FIELD_FNAME(funcname, fieldname, length)	\
static void print_ ## funcname(struct Cveinfo *p, int index)	\
{								\
	int r;							\
	char *str = "-";					\
								\
	if (fmt_json)						\
		return print_json_str(p->fieldname);		\
								\
	if (p->fieldname != NULL)				\
		str = p->fieldname;				\
	r = snprintf(p_buf, e_buf - p_buf,			\
		"%" #length "s", str);				\
	if (!is_last_field)					\
		r = abs(length);				\
	p_buf += r;						\
}

#define PRINT_STR_FIELD(name, length)				\
	PRINT_STR_FIELD_FNAME(name, name, length)

PRINT_STR_FIELD(private, -32)
PRINT_STR_FIELD(root, -32)
PRINT_STR_FIELD(mount_opts, -16)
PRINT_STR_FIELD(origin_sample, -32)
PRINT_STR_FIELD(hostname, -32)
PRINT_STR_FIELD(name, -32)
PRINT_STR_FIELD(description, -32)
PRINT_STR_FIELD(ostemplate, -32)
PRINT_STR_FIELD_FNAME(name_short, name, 10)

static void print_json_list(const char *list)
{
	static const char spc[] = " \t";
	int first_item = 1;
	const char *item;
	const char *endp;

	if (!list) {
		printf("[]");
		return;
	}

	printf("[");
	item = list;
	endp = list + strlen(list);
	while (item < endp) {
		int toklen;

		item += strspn(item, spc);
		if (item >= endp)
			break;

		toklen = strcspn(item, spc);
		printf("%s\"%.*s\"", first_item ? "" : ", ", toklen, item);
		first_item = 0;
		item += toklen;
	}
	printf("]");
}

static void print_strlist(char *list)
{
	int r;
	char *str = "-";
	char *ch;

	if (fmt_json)
		return print_json_list(list);

	if (list != NULL)
		str = list;
	if (!is_last_field)
	{
		/* Fixme: dont destroy original string */
		if ((ch = strchr(str, ' ')) != NULL)
			*ch = 0;
	}
	r = snprintf(p_buf, e_buf - p_buf, "%-15s", str);
	if (!is_last_field)
		r = 15;
	p_buf += r;
}

static void print_ip(struct Cveinfo *p, int index)
{
	print_strlist(p->ip);
}

static void print_nameserver(struct Cveinfo *p, int index)
{
	print_strlist(p->nameserver);
}

static void print_searchdomain(struct Cveinfo *p, int index)
{
	print_strlist(p->searchdomain);
}

static void print_veid(struct Cveinfo *p, int index)
{
	if (fmt_json)
		printf("%d", p->veid);
	else
		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%10d", p->veid);
}

static void print_smart_name(struct Cveinfo *p, int index)
{
	if (p->name != NULL)
		print_name_short(p, index);
	else
		print_veid(p, index);
}

static void print_status(struct Cveinfo *p, int index)
{
	if (fmt_json)
		print_json_str(ve_status[p->status]);
	else
		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%-9s", ve_status[p->status]);
}

static void print_laverage(struct Cveinfo *p, int index)
{
	if (p->cpustat == NULL) {
		if (fmt_json)
			printf("null");
		else
			p_buf += snprintf(p_buf, e_buf - p_buf,
					"%14s", "-");
	}
	else {
		if (fmt_json)
			printf("[%1.2f, %1.2f, %1.2f]",
					p->cpustat->la[0],
					p->cpustat->la[1],
					p->cpustat->la[2]);
		else
			p_buf += snprintf(p_buf, e_buf - p_buf,
					"%1.2f/%1.2f/%1.2f",
					p->cpustat->la[0],
					p->cpustat->la[1],
					p->cpustat->la[2]);
	}
}

static void print_uptime(struct Cveinfo *p, int index)
{

	if (fmt_json) {
		printf("%.3f", p->cpustat ? p->cpustat->uptime : 0.);
		return;
	}

	if (p->cpustat == NULL)
		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%15s", "-");
	else
	{
		unsigned int days, hours, min, secs;

		days  = (unsigned int)(p->cpustat->uptime / (60 * 60 * 24));
		min = (unsigned int)(p->cpustat->uptime / 60);
		hours = min / 60;
		hours = hours % 24;
		min = min % 60;
		secs = (unsigned int)(p->cpustat->uptime -
				(60ull * min + 60ull * 60 * hours +
				 60ull * 60 * 24 * days));

		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%.3dd%.2dh:%.2dm:%.2ds",
				days, hours, min, secs);
	}
}

#define PRINT_CPU(name)						\
static void print_cpu ## name(struct Cveinfo *p, int index)	\
{								\
	if (p->cpu == NULL) {					\
		if (fmt_json)					\
			printf("null");				\
		else						\
			p_buf += snprintf(p_buf, e_buf - p_buf,	\
					"%7s", "-");		\
	}							\
	else {							\
		if (fmt_json)					\
			printf("%lu", p->cpu->name[index]);	\
		else						\
			p_buf += snprintf(p_buf, e_buf - p_buf,	\
				"%7lu", p->cpu->name[index]);	\
	}							\
}

PRINT_CPU(limit)
PRINT_CPU(units)

static void print_ioprio(struct Cveinfo *p, int index)
{
	if (fmt_json) {
		printf("%d", (p->io.ioprio < 0) ? 0 : p->io.ioprio);
		return;
	}

	if (p->io.ioprio < 0)
		p_buf += snprintf(p_buf, e_buf - p_buf, "%3s", "-");
	else
		p_buf += snprintf(p_buf, e_buf - p_buf, "%3d",
			p->io.ioprio);
}

static void print_bool(const char *fmt, int val)
{
	if (fmt_json)
		printf(val ? "true" : "false");
	else
		p_buf += snprintf(p_buf, e_buf-p_buf,
				fmt, val ? "yes" : "no");
}

static void print_onboot(struct Cveinfo *p, int index)
{
	/* ONBOOT value is NONE (0), YES (1) or NO (2) */
	if (p->onboot)
		return print_bool("%6s", p->onboot == YES);

	if (fmt_json)
		printf("null");
	else
		p_buf += snprintf(p_buf, e_buf-p_buf, "%6s", "-");
}

static void print_bootorder(struct Cveinfo *p, int index)
{
	if (p->bootorder == NULL) {
		if (fmt_json)
			printf("null");
		else
			p_buf += snprintf(p_buf, e_buf-p_buf,
				"%10s", "-");
	}
	else {
		if (fmt_json)
			printf("%lu", p->bootorder[index]);
		else
			p_buf += snprintf(p_buf, e_buf-p_buf,
					"%10lu", p->bootorder[index]);
	}
}

static void print_cpunum(struct Cveinfo *p, int index)
{
	if (fmt_json) {
		printf("%d", p->cpunum < 0 ? 0 : p->cpunum);
		return;
	}

	if (p->cpunum <= 0)
		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%5s", "-");
	else
		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%5d", p->cpunum);
}

static const char *layout2str(int layout)
{
	switch (layout)
	{
		case VE_LAYOUT_SIMFS:
			return "simfs";
		case VE_LAYOUT_PLOOP:
			return "ploop";
	}
	return "???";
}

static void print_layout(struct Cveinfo *p, int index)
{
	if (fmt_json)
		print_json_str(layout2str(p->layout));
	else
		p_buf += snprintf(p_buf, e_buf - p_buf,
				"%-6s",	layout2str(p->layout));
}

static void print_features(struct Cveinfo *p, int index)
{
	int r;
	char str[64]="-";

	if (fmt_json)
		return print_json_features(p->features_mask,
				p->features_known);

	features_mask2str(p->features_mask, p->features_known,
		       ",", str, sizeof(str));
	r = snprintf(p_buf, e_buf - p_buf, "%-15s", str);
	if (!is_last_field)
		r = 15;
	p_buf += r;
}

static void print_ubc(struct Cveinfo *p, size_t res_off, int index)
{
	int running = p->status == VE_RUNNING;
	unsigned long *res = p->ubc ?
		(unsigned long *)(p->ubc) + res_off : NULL;

	if (fmt_json) {
		if (res) {
			printf("{\n"
				"      \"held\": %lu,\n"
				"      \"maxheld\": %lu,\n"
				"      \"barrier\": %lu,\n"
				"      \"limit\": %lu,\n"
				"      \"failcnt\": %lu\n"
				"    }",
				running ? res[0] : 0,
				running ? res[1] : 0,
				res[2], res[3],
				running ? res[4] : 0);
		} else
			printf("null");
		return;
	}

	if (!res || (!running && (index == 0 || index == 1 || index == 4)))
		p_buf += snprintf(p_buf, e_buf - p_buf, "%10s", "-");
	else
		p_buf += snprintf(p_buf, e_buf - p_buf, "%10lu",
				res[index]);
}

#ifndef offsetof
# define offsetof(TYPE, MEMBER)  __builtin_offsetof (TYPE, MEMBER)
#endif

#define PRINT_UBC(name)							\
static void print_ubc_ ## name(struct Cveinfo *p, int index)		\
{									\
	print_ubc(p, offsetof(struct Cubc, name) /			\
			sizeof(unsigned long), index);			\
}

FOR_ALL_UBC(PRINT_UBC)

static void print_vswap(struct Cveinfo *p, int index)
{
	/* Same conditions as used in is_vswap_config() */
	int vswap = (p->ubc != NULL) &&
		(p->ubc->physpages != NULL) &&
		(p->ubc->physpages[1] != LONG_MAX) &&
		(p->ubc->physpages[1] != INT_MAX);

	print_bool("%5s", vswap);
}

static void print_disabled(struct Cveinfo *p, int index)
{
	print_bool("%6s", p->disabled);
}

static void print_dq(struct Cveinfo *p, size_t res_off, int index)
{
	unsigned long *res = p->quota ?
		(unsigned long *)(p->quota) + res_off : NULL;

	if (fmt_json) {
		if (res) {
			printf("{\n"
				"      \"usage\": %lu,\n"
				"      \"softlimit\": %lu,\n"
				"      \"hardlimit\": %lu\n"
				"    }",
				res[0], res[1], res[2]);
		} else
			printf("null");
		return;
	}

	if (!res || (index == 0 && res[index] == 0))
		p_buf += snprintf(p_buf, e_buf - p_buf, "%10s", "-");
	else
		p_buf += snprintf(p_buf, e_buf - p_buf, "%10lu",
				res[index]);
}

#define PRINT_DQ(name)							\
static void print_ ## name(struct Cveinfo *p, int index)		\
{									\
	print_dq(p, offsetof(struct Cquota, name) /			\
			sizeof(unsigned long), index);			\
}

PRINT_DQ(diskspace)
PRINT_DQ(diskinodes)

/* Sort functions */

static inline int check_empty_param(const void *val1, const void *val2)
{
	if (val1 == NULL && val2 == NULL)
		return 0;
	else if (val1 == NULL)
		return -1;
	else if (val2 == NULL)
		return 1;
	return 2;
}

static int none_sort_fn(const void *val1, const void *val2)
{
	return 0;
}

static int laverage_sort_fn(const void *val1, const void *val2)
{
	const struct Ccpustat *st1 = ((const struct Cveinfo *)val1)->cpustat;
	const struct Ccpustat *st2 = ((const struct Cveinfo *)val2)->cpustat;
	int res;

	if ((res = check_empty_param(st1, st2)) != 2)
		return res;
	res = (st1->la[0] - st2->la[0]) * 100;
	if (res != 0)
		return res;
	res = (st1->la[1] - st2->la[1]) * 100;
	if (res != 0)
		return res;
	return (st1->la[2] - st2->la[2]) * 100;
}

static int uptime_sort_fn(const void *val1, const void *val2)
{
	struct Ccpustat *st1 = ((const struct Cveinfo *)val1)->cpustat;
	struct Ccpustat *st2 = ((const struct Cveinfo *)val2)->cpustat;
	int res;

	if ((res = check_empty_param(st1, st2)) != 2)
		return res;
	return (st2->uptime - st1->uptime);
}

static int id_sort_fn(const void *val1, const void *val2)
{
	int ret;
	ret = (((const struct Cveinfo*)val1)->veid >
		((const struct Cveinfo*)val2)->veid);
	return ret;
}

static int status_sort_fn(const void *val1, const void *val2)
{
	int res;
	res = ((const struct Cveinfo*)val1)->status -
		((const struct Cveinfo*)val2)->status;
	if (!res)
		res = id_sort_fn(val1, val2);
	return res;
}

static int bootorder_sort_fn(const void *val1, const void *val2)
{
	int ret;
	unsigned long *r1 = ((const struct Cveinfo*)val1)->bootorder;
	unsigned long *r2 = ((const struct Cveinfo*)val2)->bootorder;

	ret = check_empty_param(r1, r2);
	switch (ret) {
		case 0: /* both NULL */
			return !id_sort_fn(val1, val2);
		case 2: /* both not NULL */
			break;
		default: /* one is NULL, other is not */
			return ret;
	}

	if (*r1 == *r2)
		return !id_sort_fn(val1, val2);

	return (*r1 > *r2);
}

static int ioprio_sort_fn(const void *val1, const void *val2)
{
	return ((const struct Cveinfo *)val1)->io.ioprio >
		((const struct Cveinfo *)val2)->io.ioprio;
}

static int cpunum_sort_fn(const void *val1, const void *val2)
{
	return ((const struct Cveinfo *)val1)->cpunum >
		((const struct Cveinfo *)val2)->cpunum;
}

static int layout_sort_fn(const void *val1, const void *val2)
{
	return ((const struct Cveinfo *)val1)->layout >
		((const struct Cveinfo *)val2)->layout;
}

#define SORT_STR_FN(name)						\
static int name ## _sort_fn(const void *val1, const void *val2)		\
{									\
	const char *h1 = ((const struct Cveinfo*)val1)->name;		\
	const char *h2 = ((const struct Cveinfo*)val2)->name;		\
	int ret;							\
	if ((ret = check_empty_param(h1, h2)) == 2)			\
		ret = strcmp(h1, h2);					\
	return ret;							\
}

SORT_STR_FN(private)
SORT_STR_FN(root)
SORT_STR_FN(mount_opts)
SORT_STR_FN(origin_sample)
SORT_STR_FN(hostname)
SORT_STR_FN(name)
SORT_STR_FN(description)
SORT_STR_FN(ostemplate)
SORT_STR_FN(ip)
SORT_STR_FN(nameserver)
SORT_STR_FN(searchdomain)

#define SORT_UL_RES(fn, res, name, index)				\
static int fn(const void *val1, const void *val2)			\
{									\
	const struct C ## res *r1 = ((const struct Cveinfo *)val1)->res;\
	const struct C ## res *r2 = ((const struct Cveinfo *)val2)->res;\
	int ret;							\
	if ((ret = check_empty_param(r1, r2)) == 2)			\
		ret = r1->name[index] > r2->name[index];		\
	return ret;							\
}

#define SORT_UBC(res)							\
SORT_UL_RES(res ## _h_sort_fn, ubc, res, 0)				\
SORT_UL_RES(res ## _m_sort_fn, ubc, res, 1)				\
SORT_UL_RES(res ## _l_sort_fn, ubc, res, 2)				\
SORT_UL_RES(res ## _b_sort_fn, ubc, res, 3)				\
SORT_UL_RES(res ## _f_sort_fn, ubc, res, 4)

FOR_ALL_UBC(SORT_UBC)

#define SORT_DQ(res)							\
SORT_UL_RES(res ## _u_sort_fn, quota, res, 0)				\
SORT_UL_RES(res ## _s_sort_fn, quota, res, 1)				\
SORT_UL_RES(res ## _h_sort_fn, quota, res, 2)

SORT_DQ(diskspace)
SORT_DQ(diskinodes)

SORT_UL_RES(cpulimit_sort_fn, cpu, limit, 0)
SORT_UL_RES(cpuunits_sort_fn, cpu, units, 0)

#define UBC_FIELD(name, header) \
{#name,      #header,      "%10s", 0, RES_UBC, print_ubc_ ## name, name ## _h_sort_fn},	\
{#name ".m", #header ".M", "%10s", 1, RES_UBC, print_ubc_ ## name, name ## _m_sort_fn},	\
{#name ".b", #header ".B", "%10s", 2, RES_UBC, print_ubc_ ## name, name ## _b_sort_fn},	\
{#name ".l", #header ".L", "%10s", 3, RES_UBC, print_ubc_ ## name, name ## _l_sort_fn},	\
{#name ".f", #header ".F", "%10s", 4, RES_UBC, print_ubc_ ## name, name ## _f_sort_fn}

static struct Cfield field_names[] =
{
/* ctid should have index 0 */
{"ctid", "CTID", "%10s", 0, RES_NONE, print_veid, id_sort_fn},
/* veid is for backward compatibility -- will be removed later */
{"veid", "CTID", "%10s", 0, RES_NONE, print_veid, id_sort_fn},
/* vpsid is for backward compatibility -- will be removed later */
{"vpsid", "CTID", "%10s", 0, RES_NONE, print_veid, id_sort_fn},

{"private", "PRIVATE", "%-32s", 0, RES_NONE, print_private, private_sort_fn},
{"root", "ROOT", "%-32s", 0, RES_NONE, print_root, root_sort_fn},
{"mount_opts", "MOUNT_OPTS", "%-16s", 0, RES_NONE, print_mount_opts, mount_opts_sort_fn},
{"origin_sample", "ORIGIN_SAMPLE", "%32s", 0, RES_NONE, print_origin_sample, origin_sample_sort_fn},
{"hostname", "HOSTNAME", "%-32s", 0, RES_HOSTNAME, print_hostname, hostname_sort_fn},
{"name", "NAME", "%-32s", 0, RES_NONE, print_name, name_sort_fn},
{"smart_name", "SMARTNAME", "%10s", 0, RES_NONE, print_smart_name, name_sort_fn},
{"description", "DESCRIPTION", "%-32s", 0, RES_NONE, print_description, description_sort_fn },
{"ostemplate", "OSTEMPLATE", "%-32s", 0, RES_NONE, print_ostemplate, ostemplate_sort_fn },
{"ip", "IP_ADDR", "%-15s", 0, RES_IP, print_ip, ip_sort_fn},
{"nameserver", "NAMESERVER", "%-15s", 0, RES_NONE, print_nameserver, nameserver_sort_fn},
{"searchdomain", "SEARCHDOMAIN", "%-15s", 0, RES_NONE, print_searchdomain, searchdomain_sort_fn},
{"status", "STATUS", "%-9s", 0, RES_NONE, print_status, status_sort_fn},
/*	UBC	*/
UBC_FIELD(kmemsize, KMEMSIZE),
UBC_FIELD(lockedpages, LOCKEDP),
UBC_FIELD(privvmpages, PRIVVMP),
UBC_FIELD(shmpages, SHMP),
UBC_FIELD(numproc, NPROC),
UBC_FIELD(physpages, PHYSP),
UBC_FIELD(vmguarpages, VMGUARP),
UBC_FIELD(oomguarpages, OOMGUARP),
UBC_FIELD(numtcpsock, NTCPSOCK),
UBC_FIELD(numflock, NFLOCK),
UBC_FIELD(numpty, NPTY),
UBC_FIELD(numsiginfo, NSIGINFO),
UBC_FIELD(tcpsndbuf, TCPSNDB),
UBC_FIELD(tcprcvbuf, TCPRCVB),
UBC_FIELD(othersockbuf, OTHSOCKB),
UBC_FIELD(dgramrcvbuf, DGRAMRB),
UBC_FIELD(numothersock, NOTHSOCK),
UBC_FIELD(dcachesize, DCACHESZ),
UBC_FIELD(numfile, NFILE),
UBC_FIELD(numiptent, NIPTENT),
UBC_FIELD(swappages, SWAPP),

{"diskspace", "DSPACE", "%10s", 0, RES_QUOTA, print_diskspace, diskspace_u_sort_fn},
{"diskspace.s", "DSPACE.S", "%10s", 1, RES_QUOTA, print_diskspace, diskspace_s_sort_fn},
{"diskspace.h", "DSPACE.H", "%10s", 2, RES_QUOTA, print_diskspace, diskspace_h_sort_fn},

{"diskinodes", "DINODES", "%10s", 0, RES_QUOTA, print_diskinodes, diskinodes_u_sort_fn},
{"diskinodes.s", "DINODES.S", "%10s", 1, RES_QUOTA, print_diskinodes, diskinodes_s_sort_fn},
{"diskinodes.h", "DINODES.H", "%10s", 2, RES_QUOTA, print_diskinodes, diskinodes_h_sort_fn},

{"laverage", "LAVERAGE", "%14s", 0, RES_CPUSTAT, print_laverage, laverage_sort_fn},
{"uptime", "UPTIME", "%15s", 0, RES_CPUSTAT, print_uptime, uptime_sort_fn},

{"cpulimit", "CPULIM", "%7s", 0, RES_CPU, print_cpulimit, cpulimit_sort_fn},
{"cpuunits", "CPUUNI", "%7s", 0, RES_CPU, print_cpuunits, cpuunits_sort_fn},
{"cpus", "CPUS", "%5s", 0, RES_CPUNUM, print_cpunum, cpunum_sort_fn},

{"ioprio", "IOP", "%3s", 0, RES_NONE, print_ioprio, ioprio_sort_fn},

{"onboot", "ONBOOT", "%6s", 0, RES_NONE, print_onboot, none_sort_fn},
{"bootorder", "BOOTORDER", "%10s", 0, RES_NONE,
	print_bootorder, bootorder_sort_fn},
{"layout", "LAYOUT", "%6s", 0, RES_NONE, print_layout, layout_sort_fn},
{"features", "FEATURES", "%-15s", 0, RES_NONE, print_features, none_sort_fn},
{"vswap", "VSWAP", "%5s", 0, RES_NONE, print_vswap, none_sort_fn},
{"disabled", "DISABL", "%6s", 0, RES_NONE, print_disabled, none_sort_fn},
};

static void *x_malloc(int size)
{
	void *p;
	if ((p = malloc(size)) == NULL) {
		fprintf(stderr, "Error: unable to allocate %d bytes\n", size);
		exit(1);
	}
	return p;
}

static void *x_realloc(void *ptr, int size)
{
	void *tmp;

	if ((tmp = realloc(ptr, size)) == NULL) {
		fprintf(stderr, "Error: unable to allocate %d bytes\n", size);
		exit(1);
	}
	return tmp;
}

static void usage()
{
	printf(
"Usage:	vzlist [-a | -S] [-n] [-H] [-o field[,field...] | -1] [-s [-]field]\n"
"	       [-h pattern] [-N pattern] [-d pattern] [CTID [CTID ...]]\n"
"	vzlist -L | --list\n"
"\n"
"Options:\n"
"	-a, --all		list all containers\n"
"	-S, --stopped		list stopped containers\n"
"	-n, --name		display containers' names\n"
"	-H, --no-header		suppress columns header\n"
"	-t, --no-trim		do not trim long values\n"
"	-j, --json		output in JSON format\n"
"	-o, --output		output only specified fields\n"
"	-1			synonym for -H -octid\n"
"	-s, --sort		sort by the specified field\n"
"				('-field' to reverse sort order)\n"
"	-h, --hostname		filter CTs by hostname pattern\n"
"	-N, --name_filter	filter CTs by name pattern\n"
"	-d, --description	filter CTs by description pattern\n"
"	-L, --list		get possible field names\n"
	);
}

static int id_search_fn(const void* val1, const void* val2)
{
	return (*(const int *)val1 - ((const struct Cveinfo*)val2)->veid);
}

static int veid_search_fn(const void* val1, const void* val2)
{
	return (*(const int *)val1 - *(const int *)val2);
}

static char* trim_eol_space(char *sp, char *ep)
{
/*	if (ep == NULL)
		ep = sp + strlen(sp); */

	ep--;
	while (isspace(*ep) && ep >= sp) *ep-- = '\0';

	return sp;
}

static void print_hdr()
{
	struct Cfield_order *p;
	int f;

	for (p = g_field_order; p != NULL; p = p->next) {
		f = p->order;
		p_buf += snprintf(p_buf, e_buf - p_buf,
				field_names[f].hdr_fmt, field_names[f].hdr);
		if (p_buf >= e_buf)
			break;
		if (p->next != NULL)
			*p_buf++ = ' ';
	}
	printf("%s\n", trim_eol_space(g_buf, p_buf));
	g_buf[0] = 0;
	p_buf = g_buf;
}

/*
	1 - match
	0 - do not match
*/
static inline int check_pattern(char *str, char *pat)
{
	if (pat == NULL)
		return 1;
	if (str == NULL)
		return 0;
	return !fnmatch(pat, str, 0);
}

static void filter_by_hostname()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (!check_pattern(veinfo[i].hostname, host_pattern))
			veinfo[i].hide = 1;
	}
}

static void filter_by_name()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (!check_pattern(veinfo[i].name, name_pattern))
			veinfo[i].hide = 1;
	}
}

static void filter_by_description()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (!check_pattern(veinfo[i].description, desc_pattern))
			veinfo[i].hide = 1;
	}
}

static void print_one_ve(struct Cveinfo *ve)
{
	struct Cfield_order *p;
	int f;

	if (trim)
		is_last_field = 0;

	for (p = g_field_order; p != NULL; p = p->next) {
		f = p->order;
		if (p->next == NULL)
			is_last_field = 1;
		field_names[f].print_fn(ve, field_names[f].index);
		if (p_buf >= e_buf)
			break;
		if (p->next != NULL)
			*p_buf++ = ' ';
	}

	printf("%s\n", trim_eol_space(g_buf, p_buf));
	g_buf[0] = 0;
	p_buf = g_buf;
}

static void print_field_json(struct Cveinfo *ve, int fi)
{
	static struct Cveinfo *prev_ve = NULL;

	printf("%s\n    \"%s\": ", prev_ve == ve ? "," : "",
			field_names[fi].name);
	field_names[fi].print_fn(ve, field_names[fi].index);
	prev_ve = ve;
}

static void print_one_ve_json(struct Cveinfo *ve)
{
	static int first_entry = 1;

	printf("%s\n  {", first_entry ? "" : ",");
	first_entry = 0;
	if (g_field_order) {
		struct Cfield_order *p;
		for (p = g_field_order; p != NULL; p = p->next)
			print_field_json(ve, p->order);
	} else {
		unsigned long i;
		for (i = 0; i < ARRAY_SIZE(field_names); i++)
			if (!field_names[i].index)
				print_field_json(ve, i);
	}
	printf("\n  }");
}
static void print_ve()
{
	int i, idx;

	/* If sort order != veid (already sorted by) */
	if (g_sort_field) {
		qsort(veinfo, n_veinfo, sizeof(struct Cveinfo),
			field_names[g_sort_field].sort_fn);
	}
	if (!(!show_hdr || fmt_json))
		print_hdr();
	if (fmt_json)
		printf("[");
	for (i = 0; i < n_veinfo; i++) {
		if (sort_rev)
			idx = n_veinfo - i - 1;
		else
			idx = i;
		if (veinfo[idx].hide)
			continue;
		if (only_stopped_ve && veinfo[idx].status == VE_RUNNING)
			continue;
		if (fmt_json)
			print_one_ve_json(&veinfo[idx]);
		else
			print_one_ve(&veinfo[idx]);
	}
	if (fmt_json)
		printf("\n]\n");
}

static void add_elem(struct Cveinfo *ve)
{
	veinfo = (struct Cveinfo *)x_realloc(veinfo,
				sizeof(struct Cveinfo) * ++n_veinfo);
	ve->cpunum = -1;
	memcpy(&veinfo[n_veinfo - 1], ve, sizeof(struct Cveinfo));
	return;
}

static struct Cveinfo *find_ve(int veid)
{
	return (struct Cveinfo *) bsearch(&veid, veinfo, n_veinfo,
			sizeof(struct Cveinfo), id_search_fn);
}

static void update_ve(int veid, char *ip, int status)
{
	struct Cveinfo *tmp, ve;

	tmp = find_ve(veid);
	if (tmp == NULL) {
		memset(&ve, 0, sizeof(struct Cveinfo));
		ve.veid = veid;
		ve.status = status;
		ve.ip = ip;
		add_elem(&ve);
		qsort(veinfo, n_veinfo, sizeof(struct Cveinfo), id_sort_fn);
		return;
	} else {
		if (tmp->ip == NULL)
			tmp->ip = ip;
		else
			free(ip);
		tmp->status = status;
	}
	return;
}

static void update_ubc(int veid, const struct Cubc *ubc)
{
	struct Cveinfo *tmp;

	if ((tmp = find_ve(veid)) != NULL) {
		if (tmp->status != VE_RUNNING)
			return;
		if (tmp->ubc == NULL)
			tmp->ubc = x_malloc(sizeof(*ubc));
		memcpy(tmp->ubc, ubc, sizeof(*ubc));
	}
	return ;
}

static void update_quota(int veid, struct Cquota *quota)
{
	struct Cveinfo *tmp;

	if ((tmp = find_ve(veid)) == NULL)
		return;
	tmp->quota = x_malloc(sizeof(*quota));
	memcpy(tmp->quota, quota, sizeof(*quota));
	return;
}

static void update_cpu(int veid, unsigned long limit, unsigned long units)
{
	struct Cveinfo *tmp;
	struct Ccpu *cpu;

	if ((tmp = find_ve(veid)) == NULL)
		return;
	cpu = x_malloc(sizeof(*cpu));
	cpu->limit[0] = limit;
	cpu->units[0] = units;
	tmp->cpu = cpu;
	return;
}

#define MERGE_QUOTA(name, quota, dq)				\
do {								\
	if (dq.name != NULL) {					\
		quota->name[1] = dq.name[0];			\
		quota->name[2] = dq.name[1];			\
	}							\
} while(0);

static void merge_conf(struct Cveinfo *ve, vps_res *res, vps_opt *opt)
{
	if (ve->ubc == NULL) {
		ve->ubc = x_malloc(sizeof(struct Cubc));
		memset(ve->ubc, 0, sizeof(struct Cubc));
#define MERGE_UBC(name)						\
	if (res != NULL && res->ub.name != NULL) {		\
		ve->ubc->name[2] = res->ub.name[0];		\
		ve->ubc->name[3] = res->ub.name[1];		\
	}

FOR_ALL_UBC(MERGE_UBC)

#undef MERGE_UBC
	}
	if (ve->ip == NULL && !list_empty(&res->net.ip)) {
		ve->ip = strdup(list2str(NULL, &res->net.ip));
	}
	if (ve->quota == NULL && (
		res->dq.diskspace != NULL ||
		res->dq.diskinodes != NULL))
	{
		ve->quota = x_malloc(sizeof(struct Cquota));
		memset(ve->quota, 0, sizeof(struct Cquota));

		if (res->dq.diskspace)
			MERGE_QUOTA(diskspace, ve->quota, res->dq);
		if (res->dq.diskinodes)
			MERGE_QUOTA(diskinodes, ve->quota, res->dq);

	}
	if (ve->cpu == NULL &&
		(res->cpu.units != NULL || res->cpu.limit != NULL))
	{
		ve->cpu = x_malloc(sizeof(struct Ccpu));
		memset(ve->cpu, 0, sizeof(struct Ccpu));
		if (res->cpu.limit != NULL)
			ve->cpu->limit[0] = *res->cpu.limit;
		if (res->cpu.units != NULL)
			ve->cpu->units[0] = *res->cpu.units;
	}
	if (res->misc.hostname != NULL)
		ve->hostname = strdup(res->misc.hostname);
	if (res->misc.description != NULL)
		ve->description = strdup(res->misc.description);
	if (res->tmpl.ostmpl != NULL)
		ve->ostemplate = strdup(res->tmpl.ostmpl);
	if (res->name.name != NULL) {
		int veid_nm = get_veid_by_name(res->name.name);
		if (veid_nm == ve->veid)
			ve->name = strdup(res->name.name);
	}
	if (!list_empty(&res->misc.nameserver))
		ve->nameserver = strdup(list2str(NULL, &res->misc.nameserver));
	if (!list_empty(&res->misc.searchdomain))
		ve->searchdomain = strdup(list2str(NULL, &res->misc.searchdomain));

	if (res->fs.root != NULL)
		ve->root = strdup(res->fs.root);
	if (res->fs.private != NULL)
		ve->private = strdup(res->fs.private);
	if (res->fs.mount_opts != NULL)
		ve->mount_opts = strdup(res->fs.mount_opts);
	ve->onboot = res->misc.onboot;
	if (res->misc.bootorder != NULL) {
		ve->bootorder = x_malloc(sizeof(*ve->bootorder));
		*ve->bootorder = *res->misc.bootorder;
	}
	ve->io.ioprio = res->io.ioprio;
	if (ve->cpunum == -1 && res->cpu.vcpus != NULL)
		ve->cpunum = *res->cpu.vcpus;
	ve->features_mask  = res->env.features_mask;
	ve->features_known = res->env.features_known;
	ve->disabled = opt->start_disabled;
	if (opt->origin_sample != NULL)
		ve->origin_sample = strdup(opt->origin_sample);
}

static int read_ves_param()
{
	int i;
	char buf[128];
	vps_param *param;
	char *ve_root = NULL;
	char *ve_private = NULL;
	int veid;

	param = init_vps_param();
	/* Parse global config file */
	vps_parse_config(0, GLOBAL_CFG, param, NULL);
	if (param->res.fs.root != NULL)
		ve_root = strdup(param->res.fs.root_orig);
	if (param->res.fs.private != NULL)
		ve_private = strdup(param->res.fs.private_orig);
	if (param->res.cpt.dumpdir != NULL)
		dumpdir = strdup(param->res.cpt.dumpdir);
	free_vps_param(param);
	for (i = 0; i < n_veinfo; i++) {
		veid = veinfo[i].veid;
		param = init_vps_param();
		snprintf(buf, sizeof(buf), VPS_CONF_DIR "%d.conf", veid);
		vps_parse_config(veid, buf, param, NULL);
		merge_conf(&veinfo[i], &param->res, &param->opt);
		if (veinfo[i].root == NULL)
			veinfo[i].root = subst_VEID(veinfo[i].veid, ve_root);
		if (veinfo[i].private == NULL)
			veinfo[i].private = subst_VEID(veinfo[i].veid,
								ve_private);
		free_vps_param(param);
	}
	free(ve_root);
	free(ve_private);

	return 0;
}


static int check_veid_restr(int veid)
{
	if (g_ve_list == NULL)
		return 1;
	return (bsearch(&veid, g_ve_list, n_ve_list,
			sizeof(*g_ve_list), veid_search_fn) != NULL);
}

#define UPDATE_UBC(param)			\
	if (!strcmp(name, #param)) {		\
		ubc.param[0] = held;		\
		ubc.param[1] = maxheld;		\
		ubc.param[2] = barrier;		\
		ubc.param[3] = limit;		\
		ubc.param[4] = failcnt;		\
	}

static int get_ub()
{
	char buf[256];
	int veid, prev_veid;
	unsigned long held, maxheld, barrier, limit, failcnt;
	char name[32];
	FILE *fp;
	char *s;
	struct Cubc ubc = {};

	if ((fp = fopen(PROC_BC_RES, "r")) == NULL) {
		if ((fp = fopen(PROCUBC, "r")) == NULL) {
			fprintf(stderr, "Unable to open %s: %s\n",
					PROCUBC, strerror(errno));
			return 1;
		}
	}

	veid = 0;
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if ((s = strchr(buf, ':')) != NULL) {
			prev_veid = veid;
			if (sscanf(buf, "%d:", &veid) != 1)
				continue;
			if (prev_veid && check_veid_restr(prev_veid)) {
				update_ubc(prev_veid, &ubc);
			}
			memset(&ubc, 0, sizeof(struct Cubc));
			s++;
		} else {
			s = buf;
		}
		if (sscanf(s, "%s %lu %lu %lu %lu %lu",
			name, &held, &maxheld, &barrier, &limit, &failcnt) != 6)
		{
			continue;
		}

		FOR_ALL_UBC(UPDATE_UBC)
	}
	if (veid && check_veid_restr(veid)) {
		update_ubc(veid, &ubc);
	}
	fclose(fp);
	return 0;
}

static char *invert_ip(char *ips)
{
	char *tmp, *p, *ep, *tp;
	size_t len;
	int rc;
	unsigned int ip[4];
	int family;
	char ip_str[INET6_ADDRSTRLEN];

	if (ips == NULL)
		return NULL;
	len = strlen(ips);
	tp = tmp = x_malloc(len + 2);
	tmp[0] = 0;
	p = ep = ips + len;
	/* Iterate in reverse order */
	while (p-- > ips) {
		/* Skip spaces */
		while (p > ips && isspace(*p)) {--p;}
		ep = p;
		/* find the string begin from */
		while (p > ips && !isspace(*(p - 1))) { --p;}
		len = ep - p + 1;
		if (len >= sizeof(ip_str))
			continue;
		strncpy(ip_str, p, len);
		ip_str[len] = 0;
		if ((family = get_netaddr(ip_str, ip)) == -1)
			continue;
		if ((inet_ntop(family, ip, ip_str, sizeof(ip_str) - 1)) == NULL)
			continue;
		rc = sprintf(tp, "%s ", ip_str);
		if (rc == -1)
			break;
		tp += rc;
	}
	return tmp;
}

static int get_run_ve_proc(int update)
{
	FILE *fp;
	struct Cveinfo ve;
	char buf[16384];
	char ips[16384];
	int res, veid, classid, nproc;

	if ((fp = fopen(PROCVEINFO, "r")) == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n",
				PROCVEINFO, strerror(errno));
		return 1;
	}
	memset(&ve, 0, sizeof(struct Cveinfo));
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		res = sscanf(buf, "%d %d %d %[^\n]",
			&veid, &classid, &nproc, ips);
		if (res < 3 || !veid)
			continue;
		if (!check_veid_restr(veid))
			continue;
		memset(&ve, 0, sizeof(struct Cveinfo));
		if (res == 4) {
			ve.ip = invert_ip(ips);

		}
		ve.veid = veid;
		ve.status = VE_RUNNING;
		if (update)
			update_ve(veid, ve.ip, ve.status);
		else
			add_elem(&ve);
	}
	if (!update)
		qsort(veinfo, n_veinfo, sizeof(struct Cveinfo), id_sort_fn);
	fclose(fp);
	return 0;
}

#if HAVE_VZLIST_IOCTL
static inline int get_ve_ips(unsigned int id, char **str)
{
	int ret = -1;
	struct vzlist_veipv4ctl veip;
	uint32_t *addr;

	veip.veid = id;
	veip.num = 256;
	addr = x_malloc(veip.num * sizeof(*veip.ip));
	for (;;) {
		veip.ip = addr;
		ret = ioctl(vzctlfd, VZCTL_GET_VEIPS, &veip);
		if (ret < 0)
			goto out;
		else if (ret <= veip.num)
			break;
		veip.num = ret;
		addr = x_realloc(addr, veip.num * sizeof(*veip.ip));
	}
	if (ret > 0) {
		char *buf, *cp;
		int i;

		buf = x_malloc(ret * (16 * sizeof(char)) + 1);
		cp = buf;
		for (i = ret - 1; i >= 0; i--)
			cp += sprintf(cp, "%d.%d.%d.%d ", NIPQUAD(addr[i]));
		*cp = '\0';
		*str = buf;
	} else
		*str = strdup("");
out:
	free(addr);
	return ret;
}

static int get_run_ve_ioctl(int update)
{
	int ret = -1;
	struct vzlist_veidctl veid;
	int nves;
	void *buf = NULL;
	int i;

	vzctlfd = open(VZCTLDEV, O_RDWR);
	if (vzctlfd < 0)
		goto error;
	veid.num = 256;
	buf = x_malloc(veid.num * sizeof(envid_t));
	while (1) {
		veid.id = buf;
		ret = ioctl(vzctlfd, VZCTL_GET_VEIDS, &veid);
		if (ret < 0)
			goto out;
		if (ret <= veid.num)
			break;
		veid.num = ret + 20;
		buf = x_realloc(buf, veid.num * sizeof(envid_t));
	}
	nves = ret;
	for (i = 0; i < nves; i++) {
		struct Cveinfo ve;
		envid_t id;

		id = veid.id[i];
		if (!id || !check_veid_restr(id))
			continue;
		memset(&ve, '\0', sizeof(ve));
		ve.veid = id;
		ve.status = VE_RUNNING;
		ret = get_ve_ips(id, &ve.ip);
		if (ret < 0) {
			if (errno != ESRCH)
				goto out;
			continue;
		}
		if (update)
			update_ve(id, ve.ip, ve.status);
		else
			add_elem(&ve);
	}
	if (!update)
		qsort(veinfo, n_veinfo, sizeof(struct Cveinfo), id_sort_fn);
	ret = 0;
out:
	free(buf);
	close(vzctlfd);
error:
	return ret;
}
#endif

static int get_run_quota_stat()
{
	unsigned long usage, softlimit, hardlimit, time, exp;
	int veid = 0, prev_veid = 0;
	struct Cquota quota;
	FILE *fp;
	char buf[128];
	char str[11];

	if ((fp = fopen(PROCQUOTA, "r")) == NULL) {
		return 1;
	}
	veid = 0;
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if (strchr(buf, ':') != NULL) {
			prev_veid = veid;
			if (sscanf(buf, "%d:", &veid) != 1)
				continue;
			if (prev_veid)
				update_quota(prev_veid, &quota);
			memset(&quota, 0, sizeof(quota));
			continue;

		}
		if (sscanf(buf, "%10s %lu %lu %lu %lu %lu", str, &usage,
			&softlimit, &hardlimit, &time, &exp) == 6)
		{
			if (!strcmp(str, "1k-blocks")) {
				quota.diskspace[0] = usage;
				quota.diskspace[1] = softlimit;
				quota.diskspace[2] = hardlimit;
			} else if (!strcmp(str, "inodes")) {
				quota.diskinodes[0] = usage;
				quota.diskinodes[1] = softlimit;
				quota.diskinodes[2] = hardlimit;
			}
		}
	}
	if (veid)
		update_quota(veid, &quota);
	fclose(fp);
	return 0;
}

static int get_ve_ploop_info(struct Cveinfo *ve)
{
#ifdef HAVE_PLOOP
	char descr[PATH_MAX];
	struct ploop_info i = {};

	GET_DISK_DESCRIPTOR(descr, ve->private);
	if (ploop.get_info_by_descr(descr, &i))
		return -1;

	if (ve->quota == NULL) {
		ve->quota = x_malloc(sizeof(struct Cquota));
		memset(ve->quota, 0, sizeof(struct Cquota));
	}

	// space avail
	ve->quota->diskspace[1] = ve->quota->diskspace[2] =
		(i.fs_blocks * i.fs_bsize) >> 10;
	// space used
	ve->quota->diskspace[0] =
		((i.fs_blocks - i.fs_bfree) * i.fs_bsize) >> 10;

	// inodes avail
	ve->quota->diskinodes[1] = ve->quota->diskinodes[2] =
		i.fs_inodes;
	// inodes used
	ve->quota->diskinodes[0] = i.fs_inodes - i.fs_ifree;
#else
	static int shown = 0;

	if (!shown) {
		fprintf(stderr, "Warning: ploop support is not compiled in\n");
		shown = 1;
	}
#endif /* HAVE_PLOOP */

	return 0;
}

static int get_ves_ploop_info()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].layout == VE_LAYOUT_PLOOP && !veinfo[i].hide
				&& is_ploop_supported())
			get_ve_ploop_info(&veinfo[i]);
	}
	return 0;
}

static long get_clk_tck()
{
	if (__clk_tck != -1)
		return __clk_tck;
	__clk_tck = sysconf(_SC_CLK_TCK);
	return __clk_tck;
}

static int get_ve_cpustat(struct Cveinfo *ve)
{
	struct vz_cpu_stat stat;
	struct vzctl_cpustatctl statctl;
	struct Ccpustat st;

	statctl.veid = ve->veid;
	statctl.cpustat = &stat;
	if (ioctl(vzctlfd, VZCTL_GET_CPU_STAT, &statctl) != 0)
		return 1;
	st.la[0] = stat.avenrun[0].val_int + (stat.avenrun[0].val_frac * 0.01);
	st.la[1] = stat.avenrun[1].val_int + (stat.avenrun[1].val_frac * 0.01);
	st.la[2] = stat.avenrun[2].val_int + (stat.avenrun[2].val_frac * 0.01);

	st.uptime = (float) stat.uptime_jif / get_clk_tck();

	ve->cpustat = x_malloc(sizeof(st));
	memcpy(ve->cpustat, &st, sizeof(st));
	return 0;
}

static int get_ves_cpustat()
{
	int i;

	if ((vzctlfd = open(VZCTLDEV, O_RDWR)) < 0)
		return 1;
	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].hide)
			continue;
		get_ve_cpustat(&veinfo[i]);
	}
	close(vzctlfd);
	return 0;
}

static int get_mounted_status()
{
	int i;
	char buf[512];

	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].status == VE_RUNNING)
			continue;
		if (veinfo[i].private == NULL ||
			!stat_file(veinfo[i].private))
		{
			veinfo[i].hide = 1;
			continue;
		}
		get_dump_file(veinfo[i].veid, dumpdir, buf, sizeof(buf));
		if (stat_file(buf))
			veinfo[i].status = VE_SUSPENDED;
		if (veinfo[i].root == NULL)
			continue;
		if (vps_is_mounted(veinfo[i].root, veinfo[i].private))
			veinfo[i].status = VE_MOUNTED;
	}
	return 0;
}

static int get_ves_layout()
{
	int i, ploop;

	for (i = 0; i < n_veinfo; i++) {
		ploop = ve_private_is_ploop(veinfo[i].private);
		veinfo[i].layout = ploop ? VE_LAYOUT_PLOOP : VE_LAYOUT_SIMFS;
	}

	return 0;
}

static int get_ve_cpunum(struct Cveinfo *ve) {
	char path[] = "/proc/vz/fairsched/2147483647/cpu.nr_cpus";
	int veid = ve->veid;
	FILE *fp;
	int ret = -1;

	snprintf(path, sizeof(path),
			"/proc/vz/fairsched/%d/cpu.nr_cpus", veid);
	fp = fopen(path, "r");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n",
				path, strerror(errno));
		return -1;
	}
	if (fscanf(fp, "%d", &ve->cpunum) != 1)
		goto out;

	ret = 0;
out:
	fclose(fp);

	return ret;
}

static int get_ves_cpunum()
{
	int i;

	if (access("/proc/vz/fairsched/", F_OK))
		/* pre-RHEL6 kernel */
		return -1;

	for (i = 0; i < n_veinfo; i++) {
		if ((veinfo[i].hide) || (veinfo[i].status != VE_RUNNING))
			continue;
		get_ve_cpunum(&veinfo[i]);
	}
	return 0;
}

static int get_ves_cpu()
{
	unsigned long tmp;
	int veid, id, weight, rate;
	FILE *fp;
	char buf[128];

	if ((fp = fopen(PROCFSHED, "r")) == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n",
				PROCFSHED, strerror(errno));
		return 1;
	}
	veid = 0;
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if (sscanf(buf, "%d %d %lu %d %d",
			&veid, &id, &tmp, &weight, &rate) != 5)
		{
			continue;
		}
		if (id && !veid) {
			rate = (rate * 100) / 1024;
			weight = MAXCPUUNITS / weight;
			update_cpu(id, rate, weight);
		}
	}
	fclose(fp);
	return 0;
}

static int get_ve_list()
{
	DIR *dp;
	struct dirent *ep;
	int veid, res;
	struct Cveinfo ve;
	char str[6];

	dp = opendir(VPS_CONF_DIR);
	if (dp == NULL) {
		return -1;
	}
	memset(&ve, 0, sizeof(struct Cveinfo));
	ve.status = VE_STOPPED;
	while ((ep = readdir (dp))) {
		res = sscanf(ep->d_name, "%d.%5s", &veid, str);
		if (!(res == 2 && !strcmp(str, "conf")))
			continue;
		if (veid < 0 || veid > VEID_MAX) {
			fprintf(stderr, "Warning: invalid CTID in config file "
					"name: %s, skipping\n", ep->d_name);
			continue;
		}
		if (!check_veid_restr(veid))
			continue;
		ve.veid = veid;
		add_elem(&ve);
	}
	closedir(dp);
	if (veinfo != NULL)
		qsort(veinfo, n_veinfo, sizeof(struct Cveinfo), id_sort_fn);
	return 0;
}

static int search_field(char *name)
{
	unsigned int i;

	if (name == NULL)
		return -1;
	for (i = 0; i < ARRAY_SIZE(field_names); i++) {
		if (!strcmp(name, field_names[i].name))
			return i;
	}
	return -1;
}

static int build_field_order(char *fields)
{
	struct Cfield_order *tmp, *prev = NULL;
	char *sp, *ep, *p;
	char name[32];
	int order;
	size_t nm_len;

	sp = fields;
	if (fields == NULL) {
		if (fmt_json)
			return 0;
		sp = default_field_order;
	}
	ep = sp + strlen(sp);
	do {
		if ((p = strchr(sp, ',')) == NULL)
			p = ep;
		nm_len = p - sp + 1;
		if (nm_len > sizeof(name) - 1) {
			fprintf(stderr, "Invalid field: %s\n", sp);
			return 1;
		}
		snprintf(name, nm_len, "%s", sp);
		sp = p + 1;
		if ((order = search_field(name)) < 0) {
			fprintf(stderr, "Unknown field: %s\n", name);
			return 1;
		}
		if (fmt_json && field_names[order].index) {
			fprintf(stderr, "Field `%s' is not available "
					"in JSON output\n", name);
			return 1;
		}
		tmp = x_malloc(sizeof(struct Cfield_order));
		tmp->order = order;
		tmp->next = NULL;
		if (prev == NULL)
			g_field_order = tmp;
		else
			prev->next = tmp;
		prev = tmp;
	} while (sp < ep);
	return 0;
}

static int check_param(int res_type)
{
	struct Cfield_order *p;

	if (fmt_json && !g_field_order)
		return 1;

	for (p = g_field_order; p != NULL; p = p->next) {
		if (field_names[p->order].res_type == res_type)
			return 1;
	}
	return 0;
}

static int collect()
{
	int update = 0;
	int ret;

	if (all_ve || g_ve_list != NULL || only_stopped_ve) {
		get_ve_list();
		update = 1;
	}
	get_run_ve(update);
	if (!only_stopped_ve && (ret = get_ub()))
		return ret;
	/* No CT found, exit with error */
	if (!n_veinfo) {
		fprintf(stderr, "Container(s) not found\n");
		return 1;
	}
	if (check_param(RES_CPUSTAT))
		get_ves_cpustat();
	if (check_param(RES_CPU))
		if (!only_stopped_ve && (ret = get_ves_cpu()))
			return ret;
	if (check_param(RES_CPUNUM))
		if (!only_stopped_ve && (ret = get_ves_cpunum()))
			return ret;
	read_ves_param();
	get_mounted_status();
	get_ves_layout();
	if (check_param(RES_QUOTA)) {
		get_run_quota_stat();
		get_ves_ploop_info();
	}
	if (host_pattern != NULL)
		filter_by_hostname();
	if (name_pattern != NULL)
		filter_by_name();
	if (desc_pattern != NULL)
		filter_by_description();
	return 0;
}

static void print_names()
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(field_names); i++)
		printf("%-15s %-15s\n", field_names[i].name,
					field_names[i].hdr);
	return;
}

static void free_veinfo()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		free(veinfo[i].ip);
		free(veinfo[i].nameserver);
		free(veinfo[i].searchdomain);
		free(veinfo[i].hostname);
		free(veinfo[i].name);
		free(veinfo[i].description);
		free(veinfo[i].ostemplate);
		free(veinfo[i].ubc);
		free(veinfo[i].quota);
		free(veinfo[i].cpustat);
		free(veinfo[i].cpu);
		free(veinfo[i].root);
		free(veinfo[i].private);
		free(veinfo[i].mount_opts);
		free(veinfo[i].origin_sample);
		free(veinfo[i].bootorder);
	}
}

static struct option list_options[] =
{
	{"no-header",	no_argument, NULL, 'H'},
	{"no-trim",	no_argument, NULL, 't'},
	{"stopped",	no_argument, NULL, 'S'},
	{"all",		no_argument, NULL, 'a'},
	{"name",	no_argument, NULL, 'n'},
	{"json",	no_argument, NULL, 'j'},
	{"name_filter", required_argument, NULL, 'N'},
	{"hostname",	required_argument, NULL, 'h'},
	{"description", required_argument, NULL, 'd'},
	{"output",	required_argument, NULL, 'o'},
	{"sort",	required_argument, NULL, 's'},
	{"list",	no_argument, NULL, 'L'},
	{"help",	no_argument, NULL, 'e'},
	{ NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
	int ret;
	char *f_order = NULL;
	char *ep, *p;
	int veid, c;

	while (1) {
		int option_index = -1;
		c = getopt_long(argc, argv, "HtSanjN:h:d:o:s:Le1",
				list_options, &option_index);
		if (c == -1)
			break;

		switch(c) {
		case 'S'	:
			only_stopped_ve = 1;
			break;
		case 't'	:
			trim = 0;
			break;
		case 'H'	:
			show_hdr = 0;
			break;
		case 'L'	:
			print_names();
			return 0;
		case 'a'	:
			all_ve = 1;
			break;
		case 'h'	:
			host_pattern = strdup(optarg);
			break;
		case 'd'	:
			desc_pattern = strdup(optarg);
			break;
		case 'o'	:
			f_order = strdup(optarg);
			break;
		case 's'	:
			p = optarg;
			if (p[0] == '-') {
				p++;
				sort_rev = 1;
			}
			if ((g_sort_field = search_field(p)) < 0) {
				fprintf(stderr, "Invalid sort field name: "
						"%s\n", optarg);
				return 1;
			}
			break;
		case '1'	:
			f_order = strdup("veid");
			show_hdr = 0;
			break;
		case 'n'	:
			f_order = strdup(default_nm_field_order);
			break;
		case 'N'	:
			name_pattern = strdup(optarg);
			break;
		case 'j'	:
			fmt_json = 1;
			p_buf = e_buf = NULL;
			break;
		default		:
			/* "Unknown option" error msg is printed by getopt */
			usage();
			return 1;
		}
	}
	if (optind < argc) {
		while (optind < argc) {
			veid = strtol(argv[optind], &ep, 10);
			if (*ep != 0 || !veid) {
				veid = get_veid_by_name(argv[optind]);
			}
			if (veid < 0 || veid > VEID_MAX) {
				fprintf(stderr, "CT ID %s is invalid.\n",
						argv[optind]);
				return 1;
			}
			optind++;
			g_ve_list = x_realloc(g_ve_list,
				sizeof(*g_ve_list) * ++n_ve_list);
			g_ve_list[n_ve_list - 1] = veid;
		}
		qsort(g_ve_list, n_ve_list, sizeof(*g_ve_list), id_sort_fn);
	}
	init_log(NULL, 0, 0, 0, 0, NULL);
	if (build_field_order(f_order))
		return 1;
	if (getuid()) {
		fprintf(stderr, "This program can only be run under root.\n");
		return 1;
	}
	if ((ret = collect())) {
		/* If no specific CTIDs are specified in arguments,
		 * 'no containers found' is not an error (bug #2149)
		 */
		if (g_ve_list == NULL)
			return 0;
		else
			return ret;
	}
	print_ve();
	free_veinfo();
	free(host_pattern);
	free(name_pattern);
	free(desc_pattern);
	free(f_order);

	return 0;
}
