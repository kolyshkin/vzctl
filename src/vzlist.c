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

#include <getopt.h>
#include <fnmatch.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/vzcalluser.h>

#include "vzlist.h"
#include "config.h"
#include "fs.h"
#include "res.h"
#include "logger.h"
#include "util.h"
#include "types.h"

LOG_DATA
static struct Cveinfo *veinfo = NULL; 
static int n_veinfo = 0;

static char g_outbuffer[4096] = "";
static char *p_outbuffer = g_outbuffer;
static char *host_pattern = NULL;
static char *name_pattern = NULL;
static int vzctlfd;
static struct Cfield_order *g_field_order = NULL;
struct Cfield_order *last_field = NULL;
static char *default_field_order = "vpsid,numproc,status,ip,hostname";
static char *default_nm_field_order = "vpsid,numproc,status,ip,name";
static int g_sort_field = 0;
static int *g_ve_list = NULL;
static int n_ve_list = 0;
static int veid_only = 0;
static int sort_rev = 0;
static int show_hdr = 1;
static int all_ve = 0;
static int only_stopped_ve = 0;
static int with_names = 0;

char logbuf[32];
char *plogbuf = logbuf;
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


static void print_hostname(struct Cveinfo *p, int index);
static void print_name(struct Cveinfo *p, int index);
static void print_ip(struct Cveinfo *p, int index);

/* Print functions */
static void print_veid(struct Cveinfo *p, int index)
{
	p_outbuffer += sprintf(p_outbuffer, "%10d", p->veid);
}

static void print_status(struct Cveinfo *p, int index)
{
	p_outbuffer += sprintf(p_outbuffer, "%-7s", ve_status[p->status]);
}

static void print_laverage(struct Cveinfo *p, int index)
{
	if (p->la == NULL)
		p_outbuffer += sprintf(p_outbuffer, "%14s", "-");
	else
		p_outbuffer += sprintf(p_outbuffer, "%1.2f/%1.2f/%1.2f",
			p->la->la[0], p->la->la[1], p->la->la[2]);
}

static void print_cpulimit(struct Cveinfo *p, int index)
{
	if (p->cpu == NULL)
		p_outbuffer += sprintf(p_outbuffer, "%7s", "-");
	else
		p_outbuffer += sprintf(p_outbuffer, "%7lu",
			p->cpu->limit[index]);
}

#define PRINT_UL_RES(fn, res, name)					\
static void fn(struct Cveinfo *p, int index)		  		\
{									\
	if (p->res == NULL ||						\
		(p->status != VE_RUNNING &&				\
			(index == 0 || index == 1 || index == 4)))	\
		p_outbuffer += sprintf(p_outbuffer, "%10s", "-");	\
	else								\
		p_outbuffer += sprintf(p_outbuffer, "%10lu",		\
					p->res->name[index]);		\
}									\

PRINT_UL_RES(print_ubc_kmemsize, ubc, kmemsize)
PRINT_UL_RES(print_ubc_lockedpages, ubc, lockedpages)
PRINT_UL_RES(print_ubc_privvmpages, ubc, privvmpages)
PRINT_UL_RES(print_ubc_shmpages, ubc, shmpages)
PRINT_UL_RES(print_ubc_numproc, ubc, numproc)
PRINT_UL_RES(print_ubc_physpages, ubc, physpages)
PRINT_UL_RES(print_ubc_vmguarpages, ubc, vmguarpages)
PRINT_UL_RES(print_ubc_oomguarpages, ubc, oomguarpages)
PRINT_UL_RES(print_ubc_numtcpsock, ubc, numtcpsock)
PRINT_UL_RES(print_ubc_numflock, ubc, numflock)
PRINT_UL_RES(print_ubc_numpty, ubc, numpty)
PRINT_UL_RES(print_ubc_numsiginfo, ubc, numsiginfo)
PRINT_UL_RES(print_ubc_tcpsndbuf, ubc, tcpsndbuf)
PRINT_UL_RES(print_ubc_tcprcvbuf, ubc, tcprcvbuf)
PRINT_UL_RES(print_ubc_othersockbuf, ubc, othersockbuf)
PRINT_UL_RES(print_ubc_dgramrcvbuf, ubc, dgramrcvbuf)
PRINT_UL_RES(print_ubc_numothersock, ubc, numothersock)
PRINT_UL_RES(print_ubc_dcachesize, ubc, dcachesize)
PRINT_UL_RES(print_ubc_numfile, ubc, numfile)
PRINT_UL_RES(print_ubc_numiptent, ubc, numiptent)

#define PRINT_DQ_RES(fn, res, name)					\
static void fn(struct Cveinfo *p, int index)		  		\
{									\
	if (p->res == NULL ||						\
		(p->status != VE_RUNNING && (index == 0)))		\
		p_outbuffer += sprintf(p_outbuffer, "%10s", "-");	\
	else								\
		p_outbuffer += sprintf(p_outbuffer, "%10lu",		\
					p->res->name[index]);		\
}									\

PRINT_DQ_RES(print_dq_blocks, quota, diskspace)
PRINT_DQ_RES(print_dq_inodes, quota, diskinodes)

/* Sort functions */

inline int check_empty_param(const void *val1, const void *val2)
{
	if (val1 == NULL && val2 == NULL)
		return 0;
	else if (val1 == NULL)
		return -1;
	else if (val2 == NULL)
		return 1;
	return 2;
}

int none_sort_fn(const void *val1, const void *val2)
{
	return 0;
}

int laverage_sort_fn(const void *val1, const void *val2)
{
	struct Cla *la1 = ((struct Cveinfo *)val1)->la;
	struct Cla *la2 = ((struct Cveinfo *)val2)->la;
	int res;

	if ((res = check_empty_param(la1, la2)) == 2)
		return res;
	res = (la1->la[0] - la2->la[0]) * 100;
	if (res != 0)
		return res;
	res = (la1->la[1] - la2->la[1]) * 100;
	if (res != 0)
		return res;
	return (la1->la[2] - la2->la[2]) * 100;
}

int id_sort_fn(const void *val1, const void *val2)
{
	int ret;
	ret = (((struct Cveinfo*)val1)->veid > ((struct Cveinfo*)val2)->veid);
	return ret;
}

int status_sort_fn(const void *val1, const void *val2)
{
	int res;
	res = ((struct Cveinfo*)val1)->status - ((struct Cveinfo*)val2)->status;	if (!res)
		res = id_sort_fn(val1, val2);
	return res;
}

#define SORT_STR_FN(fn, name)	 					\
int fn(const void *val1, const void *val2)				\
{									\
	const char *h1 = ((struct Cveinfo*)val1)->name;			\
	const char *h2 = ((struct Cveinfo*)val2)->name;			\
	int ret;							\
	if ((ret = check_empty_param(h1, h2)) == 2)			\
		ret = strcmp(h1, h2);					\
	return ret;					\
}									

SORT_STR_FN(hostnm_sort_fn, hostname)
SORT_STR_FN(name_sort_fn, name)
SORT_STR_FN(ip_sort_fn, ip)

#define SORT_UL_RES(fn, type, res, name, index)				\
int fn(const void *val1, const void *val2)				\
{									\
	struct type *r1 = ((struct Cveinfo *)val1)->res;		\
	struct type *r2 = ((struct Cveinfo *)val2)->res;		\
	int ret;							\
	if ((ret = check_empty_param(r1, r2)) == 2)			\
		ret = r1->name[index] > r2->name[index];		\
	return ret;					\
}									

SORT_UL_RES(kmemsize_h_sort_fn, Cubc, ubc, kmemsize, 0)
SORT_UL_RES(kmemsize_m_sort_fn, Cubc, ubc, kmemsize, 1)
SORT_UL_RES(kmemsize_l_sort_fn, Cubc, ubc, kmemsize, 2)
SORT_UL_RES(kmemsize_b_sort_fn, Cubc, ubc, kmemsize, 3)
SORT_UL_RES(kmemsize_f_sort_fn, Cubc, ubc, kmemsize, 4)

SORT_UL_RES(lockedpages_h_sort_fn, Cubc, ubc, lockedpages, 0)
SORT_UL_RES(lockedpages_m_sort_fn, Cubc, ubc, lockedpages, 1)
SORT_UL_RES(lockedpages_l_sort_fn, Cubc, ubc, lockedpages, 2)
SORT_UL_RES(lockedpages_b_sort_fn, Cubc, ubc, lockedpages, 3)
SORT_UL_RES(lockedpages_f_sort_fn, Cubc, ubc, lockedpages, 4)

SORT_UL_RES(privvmpages_h_sort_fn, Cubc, ubc, privvmpages, 0)
SORT_UL_RES(privvmpages_m_sort_fn, Cubc, ubc, privvmpages, 1)
SORT_UL_RES(privvmpages_l_sort_fn, Cubc, ubc, privvmpages, 2)
SORT_UL_RES(privvmpages_b_sort_fn, Cubc, ubc, privvmpages, 3)
SORT_UL_RES(privvmpages_f_sort_fn, Cubc, ubc, privvmpages, 4)

SORT_UL_RES(shmpages_h_sort_fn, Cubc, ubc, shmpages, 0)
SORT_UL_RES(shmpages_m_sort_fn, Cubc, ubc, shmpages, 1)
SORT_UL_RES(shmpages_l_sort_fn, Cubc, ubc, shmpages, 2)
SORT_UL_RES(shmpages_b_sort_fn, Cubc, ubc, shmpages, 3)
SORT_UL_RES(shmpages_f_sort_fn, Cubc, ubc, shmpages, 4)

SORT_UL_RES(numproc_h_sort_fn, Cubc, ubc, numproc, 0)
SORT_UL_RES(numproc_m_sort_fn, Cubc, ubc, numproc, 1)
SORT_UL_RES(numproc_l_sort_fn, Cubc, ubc, numproc, 2)
SORT_UL_RES(numproc_b_sort_fn, Cubc, ubc, numproc, 3)
SORT_UL_RES(numproc_f_sort_fn, Cubc, ubc, numproc, 4)

SORT_UL_RES(physpages_h_sort_fn, Cubc, ubc, physpages, 0)
SORT_UL_RES(physpages_m_sort_fn, Cubc, ubc, physpages, 1)
SORT_UL_RES(physpages_l_sort_fn, Cubc, ubc, physpages, 2)
SORT_UL_RES(physpages_b_sort_fn, Cubc, ubc, physpages, 3)
SORT_UL_RES(physpages_f_sort_fn, Cubc, ubc, physpages, 4)

SORT_UL_RES(vmguarpages_h_sort_fn, Cubc, ubc, vmguarpages, 0)
SORT_UL_RES(vmguarpages_m_sort_fn, Cubc, ubc, vmguarpages, 1)
SORT_UL_RES(vmguarpages_l_sort_fn, Cubc, ubc, vmguarpages, 2)
SORT_UL_RES(vmguarpages_b_sort_fn, Cubc, ubc, vmguarpages, 3)
SORT_UL_RES(vmguarpages_f_sort_fn, Cubc, ubc, vmguarpages, 4)

SORT_UL_RES(oomguarpages_h_sort_fn, Cubc, ubc, oomguarpages, 0)
SORT_UL_RES(oomguarpages_m_sort_fn, Cubc, ubc, oomguarpages, 1)
SORT_UL_RES(oomguarpages_l_sort_fn, Cubc, ubc, oomguarpages, 2)
SORT_UL_RES(oomguarpages_b_sort_fn, Cubc, ubc, oomguarpages, 3)
SORT_UL_RES(oomguarpages_f_sort_fn, Cubc, ubc, oomguarpages, 4)

SORT_UL_RES(numtcpsock_h_sort_fn, Cubc, ubc, numtcpsock, 0)
SORT_UL_RES(numtcpsock_m_sort_fn, Cubc, ubc, numtcpsock, 1)
SORT_UL_RES(numtcpsock_l_sort_fn, Cubc, ubc, numtcpsock, 2)
SORT_UL_RES(numtcpsock_b_sort_fn, Cubc, ubc, numtcpsock, 3)
SORT_UL_RES(numtcpsock_f_sort_fn, Cubc, ubc, numtcpsock, 4)

SORT_UL_RES(numflock_h_sort_fn, Cubc, ubc, numflock, 0)
SORT_UL_RES(numflock_m_sort_fn, Cubc, ubc, numflock, 1)
SORT_UL_RES(numflock_l_sort_fn, Cubc, ubc, numflock, 2)
SORT_UL_RES(numflock_b_sort_fn, Cubc, ubc, numflock, 3)
SORT_UL_RES(numflock_f_sort_fn, Cubc, ubc, numflock, 4)

SORT_UL_RES(numpty_h_sort_fn, Cubc, ubc, numpty, 0)
SORT_UL_RES(numpty_m_sort_fn, Cubc, ubc, numpty, 1)
SORT_UL_RES(numpty_l_sort_fn, Cubc, ubc, numpty, 2)
SORT_UL_RES(numpty_b_sort_fn, Cubc, ubc, numpty, 3)
SORT_UL_RES(numpty_f_sort_fn, Cubc, ubc, numpty, 4)

SORT_UL_RES(numsiginfo_h_sort_fn, Cubc, ubc, numsiginfo, 0)
SORT_UL_RES(numsiginfo_m_sort_fn, Cubc, ubc, numsiginfo, 1)
SORT_UL_RES(numsiginfo_l_sort_fn, Cubc, ubc, numsiginfo, 2)
SORT_UL_RES(numsiginfo_b_sort_fn, Cubc, ubc, numsiginfo, 3)
SORT_UL_RES(numsiginfo_f_sort_fn, Cubc, ubc, numsiginfo, 4)

SORT_UL_RES(tcpsndbuf_h_sort_fn, Cubc, ubc, tcpsndbuf, 0)
SORT_UL_RES(tcpsndbuf_m_sort_fn, Cubc, ubc, tcpsndbuf, 1)
SORT_UL_RES(tcpsndbuf_l_sort_fn, Cubc, ubc, tcpsndbuf, 2)
SORT_UL_RES(tcpsndbuf_b_sort_fn, Cubc, ubc, tcpsndbuf, 3)
SORT_UL_RES(tcpsndbuf_f_sort_fn, Cubc, ubc, tcpsndbuf, 4)

SORT_UL_RES(tcprcvbuf_h_sort_fn, Cubc, ubc, tcprcvbuf, 0)
SORT_UL_RES(tcprcvbuf_m_sort_fn, Cubc, ubc, tcprcvbuf, 1)
SORT_UL_RES(tcprcvbuf_l_sort_fn, Cubc, ubc, tcprcvbuf, 2)
SORT_UL_RES(tcprcvbuf_b_sort_fn, Cubc, ubc, tcprcvbuf, 3)
SORT_UL_RES(tcprcvbuf_f_sort_fn, Cubc, ubc, tcprcvbuf, 4)

SORT_UL_RES(othersockbuf_h_sort_fn, Cubc, ubc, othersockbuf, 0)
SORT_UL_RES(othersockbuf_m_sort_fn, Cubc, ubc, othersockbuf, 1)
SORT_UL_RES(othersockbuf_l_sort_fn, Cubc, ubc, othersockbuf, 2)
SORT_UL_RES(othersockbuf_b_sort_fn, Cubc, ubc, othersockbuf, 3)
SORT_UL_RES(othersockbuf_f_sort_fn, Cubc, ubc, othersockbuf, 4)

SORT_UL_RES(dgramrcvbuf_h_sort_fn, Cubc, ubc, dgramrcvbuf, 0)
SORT_UL_RES(dgramrcvbuf_m_sort_fn, Cubc, ubc, dgramrcvbuf, 1)
SORT_UL_RES(dgramrcvbuf_l_sort_fn, Cubc, ubc, dgramrcvbuf, 2)
SORT_UL_RES(dgramrcvbuf_b_sort_fn, Cubc, ubc, dgramrcvbuf, 3)
SORT_UL_RES(dgramrcvbuf_f_sort_fn, Cubc, ubc, dgramrcvbuf, 4)

SORT_UL_RES(numothersock_h_sort_fn, Cubc, ubc, numothersock, 0)
SORT_UL_RES(numothersock_m_sort_fn, Cubc, ubc, numothersock, 1)
SORT_UL_RES(numothersock_l_sort_fn, Cubc, ubc, numothersock, 2)
SORT_UL_RES(numothersock_b_sort_fn, Cubc, ubc, numothersock, 3)
SORT_UL_RES(numothersock_f_sort_fn, Cubc, ubc, numothersock, 4)

SORT_UL_RES(dcachesize_h_sort_fn, Cubc, ubc, dcachesize, 0)
SORT_UL_RES(dcachesize_m_sort_fn, Cubc, ubc, dcachesize, 1)
SORT_UL_RES(dcachesize_l_sort_fn, Cubc, ubc, dcachesize, 2)
SORT_UL_RES(dcachesize_b_sort_fn, Cubc, ubc, dcachesize, 3)
SORT_UL_RES(dcachesize_f_sort_fn, Cubc, ubc, dcachesize, 4)

SORT_UL_RES(numfile_h_sort_fn, Cubc, ubc, numfile, 0)
SORT_UL_RES(numfile_m_sort_fn, Cubc, ubc, numfile, 1)
SORT_UL_RES(numfile_l_sort_fn, Cubc, ubc, numfile, 2)
SORT_UL_RES(numfile_b_sort_fn, Cubc, ubc, numfile, 3)
SORT_UL_RES(numfile_f_sort_fn, Cubc, ubc, numfile, 4)

SORT_UL_RES(numiptent_h_sort_fn, Cubc, ubc, numiptent, 0)
SORT_UL_RES(numiptent_m_sort_fn, Cubc, ubc, numiptent, 1)
SORT_UL_RES(numiptent_l_sort_fn, Cubc, ubc, numiptent, 2)
SORT_UL_RES(numiptent_b_sort_fn, Cubc, ubc, numiptent, 3)
SORT_UL_RES(numiptent_f_sort_fn, Cubc, ubc, numiptent, 4)

SORT_UL_RES(dqblocks_u_sort_fn, Cquota, quota, diskspace, 0)
SORT_UL_RES(dqblocks_s_sort_fn, Cquota, quota, diskspace, 1)
SORT_UL_RES(dqblocks_h_sort_fn, Cquota, quota, diskspace, 2)

SORT_UL_RES(dqinodes_u_sort_fn, Cquota, quota, diskinodes, 0)
SORT_UL_RES(dqinodes_s_sort_fn, Cquota, quota, diskinodes, 1)
SORT_UL_RES(dqinodes_h_sort_fn, Cquota, quota, diskinodes, 2)

SORT_UL_RES(cpulimit_sort_fn, Ccpu, cpu, limit, 0)
SORT_UL_RES(cpuunits_sort_fn, Ccpu, cpu, limit, 1)

struct Cfield field_names[] = 
{
/* veid should have index 0 */
{"vpsid" , "VPSID", "%10s",  0, RES_NONE, print_veid, id_sort_fn},
{"hostname", "HOSTNAME", "%-32s", 0, RES_HOSTNAME, print_hostname, hostnm_sort_fn},
{"name", "NAME", "%-32s", 0, RES_NAME, print_name, name_sort_fn},
{"ip", "IP_ADDR", "%-15s", 0, RES_IP, print_ip, ip_sort_fn},
{"status", "STATUS", "%-7s", 0, RES_NONE, print_status, status_sort_fn},
/*	UBC	*/
{"kmemsize", "KMEMSIZE", "%10s", 0, RES_UBC, print_ubc_kmemsize, kmemsize_h_sort_fn},
{"kmemsize.m", "KMEMSIZE.M", "%10s", 1, RES_UBC, print_ubc_kmemsize, kmemsize_m_sort_fn},
{"kmemsize.b", "KMEMSIZE.B", "%10s", 2, RES_UBC, print_ubc_kmemsize, kmemsize_b_sort_fn},
{"kmemsize.l", "KMEMSIZE.L", "%10s", 3, RES_UBC, print_ubc_kmemsize, kmemsize_l_sort_fn},
{"kmemsize.f", "KMEMSIZE.F", "%10s", 4, RES_UBC, print_ubc_kmemsize, kmemsize_f_sort_fn},

{"lockedpages", "LOCKEDP", "%10s", 0, RES_UBC, print_ubc_lockedpages, lockedpages_h_sort_fn},
{"lockedpages.m", "LOCKEDP.M", "%10s", 1, RES_UBC, print_ubc_lockedpages, lockedpages_m_sort_fn},
{"lockedpages.b", "LOCKEDP.B", "%10s", 2, RES_UBC, print_ubc_lockedpages, lockedpages_b_sort_fn},
{"lockedpages.l", "LOCKEDP.L", "%10s", 3, RES_UBC, print_ubc_lockedpages, lockedpages_l_sort_fn},
{"lockedpages.f", "LOCKEDP.F", "%10s", 4, RES_UBC, print_ubc_lockedpages, lockedpages_f_sort_fn},

{"privvmpages", "PRIVVMP", "%10s", 0, RES_UBC, print_ubc_privvmpages, privvmpages_h_sort_fn},
{"privvmpages.m", "PRIVVMP.M", "%10s", 1, RES_UBC, print_ubc_privvmpages, privvmpages_m_sort_fn},
{"privvmpages.b", "PRIVVMP.B", "%10s", 2, RES_UBC, print_ubc_privvmpages, privvmpages_b_sort_fn},
{"privvmpages.l", "PRIVVMP.L", "%10s", 3, RES_UBC, print_ubc_privvmpages, privvmpages_l_sort_fn},
{"privvmpages.f", "PRIVVMP.F", "%10s", 4, RES_UBC, print_ubc_privvmpages, privvmpages_f_sort_fn},

{"shmpages", "SHMP", "%10s", 0, RES_UBC, print_ubc_shmpages, shmpages_h_sort_fn},
{"shmpages.m", "SHMP.M", "%10s", 1, RES_UBC, print_ubc_shmpages, shmpages_m_sort_fn},
{"shmpages.b", "SHMP.B", "%10s", 2, RES_UBC, print_ubc_shmpages, shmpages_b_sort_fn},
{"shmpages.l", "SHMP.L", "%10s", 3, RES_UBC, print_ubc_shmpages, shmpages_l_sort_fn},
{"shmpages.f", "SHMP.F", "%10s", 4, RES_UBC, print_ubc_shmpages, shmpages_f_sort_fn},
{"numproc", "NPROC", "%10s", 0, RES_UBC, print_ubc_numproc, numproc_h_sort_fn},
{"numproc.m", "NPROC.M", "%10s", 1, RES_UBC, print_ubc_numproc, numproc_m_sort_fn},
{"numproc.b", "NPROC.B", "%10s", 2, RES_UBC, print_ubc_numproc, numproc_b_sort_fn},
{"numproc.l", "NPROC.L", "%10s", 3, RES_UBC, print_ubc_numproc, numproc_l_sort_fn},
{"numproc.f", "NPROC.F", "%10s", 4, RES_UBC, print_ubc_numproc, numproc_f_sort_fn},

{"physpages", "PHYSP", "%10s", 0, RES_UBC, print_ubc_physpages, physpages_h_sort_fn},
{"physpages.m", "PHYSP.M", "%10s", 1, RES_UBC, print_ubc_physpages, physpages_m_sort_fn},
{"physpages.b", "PHYSP.B", "%10s", 2, RES_UBC, print_ubc_physpages, physpages_b_sort_fn},
{"physpages.l", "PHYSP.L", "%10s", 3, RES_UBC, print_ubc_physpages, physpages_l_sort_fn},
{"physpages.f", "PHYSP.F", "%10s", 4, RES_UBC, print_ubc_physpages, physpages_f_sort_fn},

{"vmguarpages", "VMGUARP", "%10s", 0, RES_UBC, print_ubc_vmguarpages, vmguarpages_h_sort_fn},
{"vmguarpages.m", "VMGUARP.M", "%10s", 1, RES_UBC,print_ubc_vmguarpages, vmguarpages_m_sort_fn},
{"vmguarpages.b", "VMGUARP.B", "%10s", 2, RES_UBC,print_ubc_vmguarpages, vmguarpages_b_sort_fn},
{"vmguarpages.l", "VMGUARP.L", "%10s", 3, RES_UBC,print_ubc_vmguarpages, vmguarpages_l_sort_fn},
{"vmguarpages.f", "VMGUARP.F", "%10s", 4, RES_UBC,print_ubc_vmguarpages, vmguarpages_f_sort_fn},

{"oomguarpages", "OOMGUARP", "%10s", 0, RES_UBC,print_ubc_oomguarpages, oomguarpages_h_sort_fn},
{"oomguarpages.m", "OOMGUARP.M", "%10s", 1, RES_UBC,print_ubc_oomguarpages, oomguarpages_m_sort_fn},
{"oomguarpages.b", "OOMGUARP.B", "%10s", 2, RES_UBC,print_ubc_oomguarpages, oomguarpages_b_sort_fn},
{"oomguarpages.l", "OOMGUARP.L", "%10s", 3, RES_UBC,print_ubc_oomguarpages, oomguarpages_l_sort_fn},
{"oomguarpages.f", "OOMGUARP.F", "%10s", 4, RES_UBC,print_ubc_oomguarpages, oomguarpages_f_sort_fn},

{"numtcpsock", "NTCPSOCK", "%10s", 0, RES_UBC,print_ubc_numtcpsock, numtcpsock_h_sort_fn},
{"numtcpsock.m", "NTCPSOCK.M", "%10s", 1, RES_UBC,print_ubc_numtcpsock, numtcpsock_m_sort_fn},
{"numtcpsock.b", "NTCPSOCK.B", "%10s", 2, RES_UBC,print_ubc_numtcpsock, numtcpsock_b_sort_fn},
{"numtcpsock.l", "NTCPSOCK.L", "%10s", 3, RES_UBC,print_ubc_numtcpsock, numtcpsock_l_sort_fn},
{"numtcpsock.f", "NTCPSOCK.F", "%10s", 4, RES_UBC,print_ubc_numtcpsock, numtcpsock_f_sort_fn},

{"numflock", "NFLOCK", "%10s", 0, RES_UBC,print_ubc_numflock, numflock_h_sort_fn},
{"numflock.m", "NFLOCK.M", "%10s", 1, RES_UBC,print_ubc_numflock, numflock_m_sort_fn},
{"numflock.b", "NFLOCK.B", "%10s", 2, RES_UBC,print_ubc_numflock, numflock_b_sort_fn},
{"numflock.l", "NFLOCK.L", "%10s", 3, RES_UBC,print_ubc_numflock, numflock_l_sort_fn},
{"numflock.f", "NFLOCK.F", "%10s", 4, RES_UBC,print_ubc_numflock, numflock_f_sort_fn},

{"numpty", "NPTY", "%10s", 0, RES_UBC,print_ubc_numpty, numpty_h_sort_fn},
{"numpty.m", "NPTY.M", "%10s", 1, RES_UBC,print_ubc_numpty, numpty_m_sort_fn},
{"numpty.b", "NPTY.B", "%10s", 2, RES_UBC,print_ubc_numpty, numpty_b_sort_fn},
{"numpty.l", "NPTY.L", "%10s", 3, RES_UBC,print_ubc_numpty, numpty_l_sort_fn},
{"numpty.f", "NPTY.F", "%10s", 4, RES_UBC,print_ubc_numpty, numpty_f_sort_fn},

{"numsiginfo", "NSIGINFO", "%10s", 0, RES_UBC,print_ubc_numsiginfo, numsiginfo_h_sort_fn},
{"numsiginfo.m", "NSIGINFO.M", "%10s", 1, RES_UBC,print_ubc_numsiginfo, numsiginfo_m_sort_fn},
{"numsiginfo.b", "NSIGINFO.B", "%10s", 2, RES_UBC,print_ubc_numsiginfo, numsiginfo_b_sort_fn},
{"numsiginfo.l", "NSIGINFO.L", "%10s", 3, RES_UBC,print_ubc_numsiginfo, numsiginfo_l_sort_fn},
{"numsiginfo.f", "NSIGINFO.F", "%10s", 4, RES_UBC,print_ubc_numsiginfo, numsiginfo_f_sort_fn},

{"tcpsndbuf", "TCPSNDB", "%10s", 0, RES_UBC,print_ubc_tcpsndbuf, tcpsndbuf_h_sort_fn},
{"tcpsndbuf.m", "TCPSNDB.M", "%10s", 1, RES_UBC,print_ubc_tcpsndbuf, tcpsndbuf_m_sort_fn},
{"tcpsndbuf.b", "TCPSNDB.B", "%10s", 2, RES_UBC,print_ubc_tcpsndbuf, tcpsndbuf_b_sort_fn},
{"tcpsndbuf.l", "TCPSNDB.L", "%10s", 3, RES_UBC,print_ubc_tcpsndbuf, tcpsndbuf_l_sort_fn},
{"tcpsndbuf.f", "TCPSNDB.F", "%10s", 4, RES_UBC,print_ubc_tcpsndbuf, tcpsndbuf_f_sort_fn},

{"tcprcvbuf", "TCPRCVB", "%10s", 0, RES_UBC,print_ubc_tcprcvbuf, tcprcvbuf_h_sort_fn},
{"tcprcvbuf.m", "TCPRCVB.M", "%10s", 1, RES_UBC,print_ubc_tcprcvbuf, tcprcvbuf_m_sort_fn},
{"tcprcvbuf.b", "TCPRCVB.B", "%10s", 2, RES_UBC,print_ubc_tcprcvbuf, tcprcvbuf_b_sort_fn},
{"tcprcvbuf.l", "TCPRCVB.L", "%10s", 3, RES_UBC,print_ubc_tcprcvbuf, tcprcvbuf_l_sort_fn},
{"tcprcvbuf.f", "TCPRCVB.F", "%10s", 4, RES_UBC,print_ubc_tcprcvbuf, tcprcvbuf_f_sort_fn},

{"othersockbuf", "OTHSOCKB", "%10s", 0, RES_UBC,print_ubc_othersockbuf, othersockbuf_h_sort_fn},
{"othersockbuf.m", "OTHSOCKB.M", "%10s", 1, RES_UBC,print_ubc_othersockbuf, othersockbuf_m_sort_fn},
{"othersockbuf.b", "OTHSOCKB.B", "%10s", 2, RES_UBC,print_ubc_othersockbuf, othersockbuf_b_sort_fn},
{"othersockbuf.l", "OTHSOCKB.L", "%10s", 3, RES_UBC,print_ubc_othersockbuf, othersockbuf_l_sort_fn},
{"othersockbuf.f", "OTHSOCKB.F", "%10s", 4, RES_UBC,print_ubc_othersockbuf, othersockbuf_f_sort_fn},

{"dgramrcvbuf", "DGRAMRB", "%10s", 0, RES_UBC,print_ubc_dgramrcvbuf, dgramrcvbuf_h_sort_fn},
{"dgramrcvbuf.m", "DGRAMRB.M", "%10s", 1, RES_UBC,print_ubc_dgramrcvbuf, dgramrcvbuf_m_sort_fn},
{"dgramrcvbuf.b", "DGRAMRB.B", "%10s", 2, RES_UBC,print_ubc_dgramrcvbuf, dgramrcvbuf_b_sort_fn},
{"dgramrcvbuf.l", "DGRAMRB.L", "%10s", 3, RES_UBC,print_ubc_dgramrcvbuf, dgramrcvbuf_l_sort_fn},
{"dgramrcvbuf.f", "DGRAMRB.F", "%10s", 4, RES_UBC,print_ubc_dgramrcvbuf, dgramrcvbuf_f_sort_fn},

{"numothersock", "NOTHSOCK", "%10s", 0, RES_UBC,print_ubc_numothersock, numothersock_h_sort_fn},
{"numothersock.m", "NOTHSOCK.M", "%10s", 1, RES_UBC,print_ubc_numothersock, numothersock_m_sort_fn},
{"numothersock.b", "NOTHSOCK.B", "%10s", 2, RES_UBC,print_ubc_numothersock, numothersock_b_sort_fn},
{"numothersock.l", "NOTHSOCK.L", "%10s", 3, RES_UBC,print_ubc_numothersock, numothersock_l_sort_fn},
{"numothersock.f", "NOTHSOCK.F", "%10s", 4, RES_UBC,print_ubc_numothersock, numothersock_f_sort_fn},

{"dcachesize", "DCACHESZ", "%10s", 0, RES_UBC,print_ubc_dcachesize, dcachesize_h_sort_fn},
{"dcachesize.m", "DCACHESZ.M", "%10s", 1, RES_UBC,print_ubc_dcachesize, dcachesize_h_sort_fn},
{"dcachesize.b", "DCACHESZ.B", "%10s", 2, RES_UBC,print_ubc_dcachesize, dcachesize_h_sort_fn},
{"dcachesize.l", "DCACHESZ.L", "%10s", 3, RES_UBC,print_ubc_dcachesize, dcachesize_h_sort_fn},
{"dcachesize.f", "DCACHESZ.F", "%10s", 4, RES_UBC,print_ubc_dcachesize, dcachesize_h_sort_fn},

{"numfile", "NFILE", "%10s", 0, RES_UBC,print_ubc_numfile, numfile_h_sort_fn},
{"numfile.m", "NFILE.M", "%10s", 1, RES_UBC,print_ubc_numfile, numfile_m_sort_fn},
{"numfile.b", "NFILE.B", "%10s", 2, RES_UBC,print_ubc_numfile, numfile_b_sort_fn},
{"numfile.l", "NFILE.L", "%10s", 3, RES_UBC,print_ubc_numfile, numfile_l_sort_fn},
{"numfile.f", "NFILE.F", "%10s", 4, RES_UBC,print_ubc_numfile, numfile_f_sort_fn},

{"numiptent", "NIPTENT", "%10s", 0, RES_UBC,print_ubc_numiptent, numiptent_h_sort_fn},
{"numiptent.m", "NIPTENT.M", "%10s", 1, RES_UBC,print_ubc_numiptent, numiptent_m_sort_fn},
{"numiptent.b", "NIPTENT.B", "%10s", 2, RES_UBC,print_ubc_numiptent, numiptent_b_sort_fn},
{"numiptent.l", "NIPTENT.L", "%10s", 3, RES_UBC,print_ubc_numiptent, numiptent_l_sort_fn},
{"numiptent.f", "NIPTENT.F", "%10s", 4, RES_UBC,print_ubc_numiptent, numiptent_f_sort_fn},

{"diskspace", "DQBLOCKS", "%10s", 0, RES_QUOTA, print_dq_blocks, dqblocks_u_sort_fn},
{"diskspace.s", "DQBLOCKS.S", "%10s", 1, RES_QUOTA, print_dq_blocks, dqblocks_s_sort_fn},
{"diskspace.h", "DQBLOCKS.H", "%10s", 2, RES_QUOTA, print_dq_blocks, dqblocks_h_sort_fn},

{"diskinodes", "DQINODES", "%10s", 0, RES_QUOTA, print_dq_inodes, dqinodes_u_sort_fn},
{"diskinodes.s", "DQINODES.S", "%10s", 1, RES_QUOTA, print_dq_inodes, dqinodes_u_sort_fn},
{"diskinodes.h", "DQINODES.H", "%10s", 2, RES_QUOTA, print_dq_inodes, dqinodes_u_sort_fn},

{"laverage", "LAVERAGE", "%14s", 0, RES_LA, print_laverage, laverage_sort_fn},

{"cpulimit", "CPULIM", "%7s", 0, RES_CPU, print_cpulimit, cpulimit_sort_fn},
{"cpuunits", "CPUUNI", "%7s", 1, RES_CPU, print_cpulimit, cpuunits_sort_fn}
};

static void print_hostname(struct Cveinfo *p, int index)
{
	int r;
	char *str = "-";

	if (p->hostname != NULL)
		str = p->hostname;
	r = sprintf(p_outbuffer, "%-32s", str);
	if (last_field != NULL && 
		field_names[last_field->order].res_type != RES_HOSTNAME)
	{
		r = 32;
	}
	p_outbuffer +=  r;
}

static void print_name(struct Cveinfo *p, int index)
{
	int r;
	char *str = "-";

	if (p->name != NULL)
		str = p->name;
	r = sprintf(p_outbuffer, "%-32s", str);
	if (last_field != NULL &&
		field_names[last_field->order].res_type != RES_NAME)
	{
		r = 32;
	}
	p_outbuffer +=  r;
}

static void print_ip(struct Cveinfo *p, int index)
{
	int r;
	char *str = "-";
	char *ch;

	if (p->ip != NULL)
		str = p->ip;
	if (last_field != NULL && 
		field_names[last_field->order].res_type != RES_IP)
	{
		/* Fixme: dont destroy original string */
		if ((ch = strchr(str, ' ')) != NULL)
			*ch = 0;
	}
	r = sprintf(p_outbuffer, "%-15s", str);
	if (last_field != NULL && 
		field_names[last_field->order].res_type != RES_IP)
	{
		r = 15;
	}
	p_outbuffer +=  r;
}

void *x_malloc(int size)
{
	void *p;
	if ((p = malloc(size)) == NULL) {
		printf("Error: unable to allocate %d bytes\n", size);
		exit(1);
	}
	return p;
}

void *x_realloc(void *ptr, int size)
{
	if ((ptr = realloc(ptr, size)) == NULL) {
		printf("Error: unable to allocate %d bytes\n", size);
		exit(1);
	}
	return ptr;
}

void usage()
{
	fprintf(stderr, "Usage: vzlist [-a] [-o name[,name...]] [-s {name|-name}] [-h <pattern>] [-N <pattern>]\n");
	fprintf(stderr, "\t\t[-H] [-S] [vpsid [vpsid ...]|-1]\n");
	fprintf(stderr, "       vzlist -L\n\n");
	fprintf(stderr, "\t--all -a\t list of all VPS\n");
	fprintf(stderr, "\t--output -o\t output only specified parameters\n");
	fprintf(stderr, "\t--hostname -h\t hostname search pattern\n");
	fprintf(stderr, "\t--name -n\t display VE name\n");
	fprintf(stderr, "\t--name_filter -N\t name search patter\n");
	fprintf(stderr, "\t--sort -s\t sort by specified parameter, - sign before parametr\n");
	fprintf(stderr, "\t\t\t mean sort in reverse order\n");
	fprintf(stderr, "\t--no-header -H\t supress display header\n");
	fprintf(stderr, "\t--stopped -S\t list of stopped Ve\n");
	fprintf(stderr, "\t--list -L\t list of allowed parameters\n");
	return;
}

int id_search_fn(const void* val1, const void* val2)
{
	return ((int)val1 - ((struct Cveinfo*)val2)->veid);
}

int veid_search_fn(const void* val1, const void* val2)
{
	return (*(int *)val1 - *(int *)val2);
}

void print_hdr()
{
	struct Cfield_order *p;
	int f;

	for (p = g_field_order; p != NULL; p = p->next) {
		f = p->order;
		printf(field_names[f].hdr_fmt, field_names[f].hdr);
		if (p->next != NULL)
			printf(" ");
	}
	printf("\n");
}

/*
   1 - match
   0 - do not match
*/
inline int check_pattern(char *str, char *pat)
{
	if (pat == NULL)
		return 1;
	if (str == NULL)
		return 0;
	return !fnmatch(pat, str, 0);
}

int filter_by_hostname()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (!check_pattern(veinfo[i].hostname, host_pattern))
			veinfo[i].hide = 1;
	}
	return 0;
}

int filter_by_name()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (!check_pattern(veinfo[i].name, name_pattern))
			veinfo[i].hide = 1;
	}
	return 0;
}

void print_ve()
{
	struct Cfield_order *p;
	int i, f, idx;

	/* If sort order != veid (already sorted by) */
	if (g_sort_field) {
		qsort(veinfo, n_veinfo, sizeof(struct Cveinfo),
			field_names[g_sort_field].sort_fn);
	}
	if (!(veid_only || !show_hdr))
		print_hdr();
	for (i = 0; i < n_veinfo; i++) {
		if (sort_rev)
			idx = n_veinfo - i - 1;
		else
			idx = i;
		if (veinfo[idx].hide)
			continue;
		if (only_stopped_ve && veinfo[idx].status == VE_RUNNING)
			continue;
		for (p = g_field_order; p != NULL; p = p->next) {
			f = p->order;
			field_names[f].print_fn(&veinfo[idx],
						field_names[f].index);
			if (p->next != NULL)
				*p_outbuffer++ = ' ';
		}
		printf("%s\n", g_outbuffer);
		g_outbuffer[0] = 0;
		p_outbuffer = g_outbuffer;
	}
}

void add_elem(struct Cveinfo *ve)
{
	veinfo = (struct Cveinfo *)x_realloc(veinfo,
				sizeof(struct Cveinfo) * ++n_veinfo);
	memcpy(&veinfo[n_veinfo - 1], ve, sizeof(struct Cveinfo));
	return;
}

inline struct Cveinfo *find_ve(int veid)
{
	return (struct Cveinfo *) bsearch((void*)veid, veinfo, n_veinfo,
			sizeof(struct Cveinfo), id_search_fn);
}

void update_ve(int veid, char *ip, int status)
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
		else if (ip != NULL)
			free(ip);
		tmp->status = status;
	}
	return;
}

void update_ubc(int veid, struct Cubc *ubc)
{
	struct Cveinfo *tmp;

	if ((tmp = find_ve(veid)) != NULL)
		tmp->ubc = ubc;
	return ;
}

void update_quota(int veid, struct Cquota *quota)
{
	struct Cveinfo *tmp;

	if ((tmp = find_ve(veid)) == NULL)
		return;
	tmp->quota = x_malloc(sizeof(*quota));
	memcpy(tmp->quota, quota, sizeof(*quota));
	return; 
}

void update_cpu(int veid, unsigned long limit, unsigned long units)
{
	struct Cveinfo *tmp;
	struct Ccpu *cpu;

	if ((tmp = find_ve(veid)) == NULL)
		return;
	cpu = x_malloc(sizeof(*cpu));
	cpu->limit[0] = limit;
	cpu->limit[1] = units;
	tmp->cpu = cpu;
	return; 
}

void update_la(int veid, struct Cla *la)
{
	struct Cveinfo *tmp;

	if ((tmp = find_ve(veid)) == NULL)
		return;
	tmp->la = x_malloc(sizeof(*la));
	memcpy(tmp->la, la, sizeof(*la));
	return; 
}

char *parse_var(char *var)
{
	char *sp, *ep;

	if (var == NULL)
		return NULL;
	sp = var;
	while (*sp && (*sp == '"' || isspace(*sp))) sp++;
	ep = var + strlen(var) - 1;
	while (ep > sp && (*ep == '"' || isspace(*ep))) *ep-- = 0;

	return strdup(sp);
}

#define MERGE_QUOTA(name, quota, dq)				\
do {								\
	if (dq.name != NULL) {					\
		quota->name[1] = dq.name[0];     		\
		quota->name[2] = dq.name[1];			\
	}							\
} while(0);							

void merge_conf(struct Cveinfo *ve, vps_res *res)
{
	if (ve->ubc == NULL) {
		ve->ubc = x_malloc(sizeof(struct Cubc));
		memset(ve->ubc, 0, sizeof(struct Cubc));
#define MERGE_UBC(name, ubc, res)				\
do {								\
	if (res == NULL || res->ub.name == NULL) 		\
		break;						\
	ubc->name[2] = res->ub.name[0];     			\
	ubc->name[3] = res->ub.name[1];				\
} while(0);							

		MERGE_UBC(kmemsize, ve->ubc, res);
		MERGE_UBC(lockedpages, ve->ubc, res);
		MERGE_UBC(privvmpages, ve->ubc, res);
		MERGE_UBC(shmpages, ve->ubc, res);
		MERGE_UBC(numproc, ve->ubc, res);
		MERGE_UBC(physpages, ve->ubc, res);
		MERGE_UBC(vmguarpages, ve->ubc, res);
		MERGE_UBC(oomguarpages, ve->ubc, res);
		MERGE_UBC(numtcpsock, ve->ubc, res);
		MERGE_UBC(numflock, ve->ubc, res);
		MERGE_UBC(numpty, ve->ubc, res);
		MERGE_UBC(numsiginfo, ve->ubc, res);
		MERGE_UBC(tcpsndbuf, ve->ubc, res);
		MERGE_UBC(tcprcvbuf, ve->ubc, res);
		MERGE_UBC(othersockbuf, ve->ubc, res);
		MERGE_UBC(dgramrcvbuf, ve->ubc, res);
		MERGE_UBC(numothersock, ve->ubc, res);
		MERGE_UBC(dcachesize, ve->ubc, res);
		MERGE_UBC(numfile, ve->ubc, res);
		MERGE_UBC(numiptent, ve->ubc, res);
#undef MERGE_UBC
	}
	if (ve->ip == NULL && !list_empty(&res->net.ip)) {
		ve->ip = strdup(list2str(NULL, &res->net.ip));
	}
	if (ve->quota == NULL &&
		res->dq.diskspace != NULL &&
		res->dq.diskinodes != NULL)
	{
		ve->quota = x_malloc(sizeof(struct Cquota));
		memset(ve->quota, 0, sizeof(struct Cquota));

		MERGE_QUOTA(diskspace, ve->quota, res->dq);
		MERGE_QUOTA(diskinodes, ve->quota, res->dq);

	}
	if (ve->cpu == NULL &&
		(res->cpu.units != NULL || res->cpu.limit != NULL))
	{
		ve->cpu = x_malloc(sizeof(struct Ccpu));
		memset(ve->cpu, 0, sizeof(struct Ccpu));
		if (res->cpu.units != NULL)
			ve->cpu->limit[0] = *res->cpu.units;
		if (res->cpu.limit != NULL)
			ve->cpu->limit[1] = *res->cpu.limit;
	}
	if (res->misc.hostname != NULL)
		ve->hostname = strdup(res->misc.hostname);
	if (res->name.name != NULL) {
		int veid_nm = get_veid_by_name(res->name.name);
		if (veid_nm == ve->veid)
			ve->name = strdup(res->name.name);
	}
	if (res->fs.root != NULL)
		ve->ve_root = strdup(res->fs.root);
	if (res->fs.private != NULL)
		ve->ve_private = strdup(res->fs.private);
}

int read_ves_param()
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
	free_vps_param(param);
	for (i = 0; i < n_veinfo; i++) { 
		veid = veinfo[i].veid;
		param = init_vps_param();
		snprintf(buf, sizeof(buf), VPS_CONF_DIR "%d.conf", veid);
		vps_parse_config(veid, buf, param, NULL);
		merge_conf(&veinfo[i], &param->res);
		if (veinfo[i].ve_root == NULL)
			veinfo[i].ve_root = subst_VEID(veinfo[i].veid, ve_root);
		if (veinfo[i].ve_private == NULL)
			veinfo[i].ve_private = subst_VEID(veinfo[i].veid,
								ve_private);
		free_vps_param(param);
	}
	if (ve_root != NULL)
		free(ve_root);
	if (ve_private != NULL)
		free(ve_private);

	return 0;
}


int check_veid_restr(int veid)
{
	if (g_ve_list == NULL)
		return 1;
	return (bsearch(&veid, g_ve_list, n_ve_list,
			sizeof(*g_ve_list), veid_search_fn) != NULL);
}

#define UPDATE_UBC(str,name,ubc,held,maxheld,barrier,limit,failcnt)	\
do {						\
	if (!strcmp(str, #name)) {		\
		ubc->name[0] = held;		\
		ubc->name[1] = maxheld;     	\
		ubc->name[2] = barrier;     	\
		ubc->name[3] = limit;     	\
		ubc->name[4] = failcnt;     	\
	}					\
} while(0);					\

int get_ub()
{
	char buf[256];
	int veid, prev_veid;
	unsigned long held, maxheld, barrier, limit, failcnt;
	char name[32];
	FILE *fp;
	char *s;
	struct Cveinfo ve;

	if ((fp = fopen(PROCUBC, "r")) == NULL) {
		fprintf(stderr, "Unable to open %s\n", PROCUBC);
		return 1;
	}
	veid = 0;
	memset(&ve, 0, sizeof(struct Cveinfo));
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if ((s = strchr(buf, ':')) != NULL) {
			prev_veid = veid;
			if (sscanf(buf, "%d:", &veid) != 1)
				continue;
			if (prev_veid && check_veid_restr(prev_veid)) {
				update_ubc(prev_veid, ve.ubc);
			}
			ve.ubc = x_malloc(sizeof(struct Cubc));
			memset(ve.ubc, 0, sizeof(struct Cubc));
			s++;
		} else {
			s = buf;
		}
		if (sscanf(s, "%s %lu %lu %lu %lu %lu",
			name, &held, &maxheld, &barrier, &limit, &failcnt) != 6)
		{
			continue;
		}
		UPDATE_UBC(name, kmemsize, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, lockedpages, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, privvmpages, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, shmpages, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numproc, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, physpages, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, vmguarpages, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, oomguarpages, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numtcpsock, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numflock, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numpty, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numsiginfo, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, tcpsndbuf, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, tcprcvbuf, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, othersockbuf, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, dgramrcvbuf, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numothersock, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, dcachesize, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numfile, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
		UPDATE_UBC(name, numiptent, ve.ubc, held, maxheld, barrier, \
			limit, failcnt)
	}
	if (veid && check_veid_restr(veid)) {
		update_ubc(veid, ve.ubc);
	}
	fclose(fp);
	return 0;
}

char *remove_sp(char *str)
{
	char *sp, *ep, *tp;
	int skip;

	if (str == NULL)
		return NULL;
	tp = str;
	sp = tp;
	ep = str + strlen(str);
	skip = 0;
	while (sp < ep) {
		*tp = *sp;
		if (isspace(*sp)) {
			*tp =  ' ';
			skip++;
		} else
			skip = 0;
		if (skip <= 1)
			tp++;
		sp++;
	}
	*tp = 0;
	return strdup(str);
}

char *invert_ip(char *ips)
{
	char *tmp, *p, *ep, *tp;
	int len;
	if (ips == NULL)
		return NULL;
	len = strlen(ips);
	tp = tmp = x_malloc(len + 2);
	tmp[0] = 0;
	p = ep = ips + len - 1;
	while (p > ips) {
		/* Skip spaces */
		while (p > ips && isspace(*p)) {--p;}
		ep = p;
		/* find the string begin from */
		while (p > ips && !isspace(*(p - 1))) { --p;}
		snprintf(tp, ep - p + 3, "%s ", p);
		tp += ep - p + 2;
		--p;
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
		fprintf(stderr, "Unable to open %s\n", PROCVEINFO);
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
			
		} else if (res == 3)
			ve.ip = strdup("");
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

	vzctlfd = open("/dev/vzctl", O_RDWR);
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

int get_run_quota_stat()
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

int get_stop_quota_stat(int veid)
{
	char buf[255];
	char res[16];
	FILE *fp;
	unsigned long usage, softlimit, hardlimit;
	int status;
	struct Cquota quota;

	snprintf(buf, sizeof(buf), VZQUOTA " show %d 2>/dev/null", veid);
	if ((fp = popen(buf, "r")) == NULL)
		return 1;
	memset(&quota, 0, sizeof(quota));
	while(!feof(fp)) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if (sscanf(buf, "%15s %lu %lu %lu", res,  &usage,
				&softlimit, &hardlimit) != 4)
		{
			continue;
		}
		if (!strcmp(res, "1k-blocks")) {
			quota.diskspace[0] = usage;
			quota.diskspace[1] = softlimit;
			quota.diskspace[2] = hardlimit;
		} else if (!strcmp(res, "inodes")) {
			quota.diskinodes[0] = usage;
			quota.diskinodes[1] = softlimit;
			quota.diskinodes[2] = hardlimit;
		}
	}
	status = pclose(fp);
	if (WEXITSTATUS(status))
		return 1;
	update_quota(veid, &quota);
	return 0;
}

int get_stop_quota_stats()
{
	int i;
	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].status == VE_STOPPED && !veinfo[i].hide)
			get_stop_quota_stat(veinfo[i].veid);
	}
	return 0;
}

int get_ve_la(int veid)
{
	struct vz_cpu_stat stat;
	struct vzctl_cpustatctl statctl;
	struct Cla la;

	statctl.veid = veid;
	statctl.cpustat = &stat;
	if (ioctl(vzctlfd, VZCTL_GET_CPU_STAT, &statctl) != 0)
		return 1;
        la.la[0] = stat.avenrun[0].val_int + (stat.avenrun[0].val_frac * 0.01);
        la.la[1] = stat.avenrun[1].val_int + (stat.avenrun[1].val_frac * 0.01);
        la.la[2] = stat.avenrun[2].val_int + (stat.avenrun[2].val_frac * 0.01);

	update_la(veid, &la);
	return 0;
}

int get_ves_la()
{
	int i;
	
	if ((vzctlfd = open("/dev/vzctl", O_RDWR)) < 0)
		return 1;
	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].hide)
			continue;
		get_ve_la(veinfo[i].veid);
	}
	close(vzctlfd);
	return 0;
}

int get_mounted_status()
{
	int i;
	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].ve_root == NULL)
			continue;
		if (veinfo[i].status == VE_RUNNING)
			continue;
		if (veinfo[i].ve_private == NULL ||
			!stat_file(veinfo[i].ve_private))
		{
			veinfo[i].hide = 1;
			continue;
		}
		if (vps_is_mounted(veinfo[i].ve_root))
			veinfo[i].status = VE_MOUNTED;
	}
	return 0;
}

int get_ves_cpu()
{
	unsigned long tmp;
	int veid, id, weight, rate; 
	FILE *fp;
	char buf[128];

	if ((fp = fopen(PROCFSHED, "r")) == NULL) {
		fprintf(stderr, "Unable to open %s\n", PROCFSHED);
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

int get_ve_list()
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

int search_field(char *name)
{
	int i;

	if (name == NULL)
		return -1;
	for (i = 0; i < sizeof(field_names) / sizeof(*field_names); i++) {
		if (!strcmp(name, field_names[i].name))
			return i;
	}
	return -1;
}

int build_field_order(char *fields)
{
	struct Cfield_order *tmp, *prev = NULL;
	char *sp, *ep, *p;
	char name[32];
	int order, nm_len;

	sp = fields;
	if (fields == NULL)
		sp = default_field_order;
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
		tmp = x_malloc(sizeof(struct Cfield_order));
		tmp->order = order;
		tmp->next = NULL;	
		if (prev == NULL)
			g_field_order = tmp;
		else
			prev->next = tmp;
		prev = tmp;
	} while (sp < ep);
	last_field = prev;
	return 0;
}

static inline int check_param(int res_type)
{
	struct Cfield_order *p;

	for (p = g_field_order; p != NULL; p = p->next) {
		if (field_names[p->order].res_type == res_type)
			return 1;
	}
	return 0;
}

int collect()
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
	/* No VE found, exit with error */
	if (!n_veinfo) {
		fprintf(stderr, "VPS not found\n");
		return 1;
	}
	if (check_param(RES_QUOTA)) 
		get_run_quota_stat();
	if (check_param(RES_LA))
		get_ves_la();
	if (check_param(RES_CPU))
		if ((ret = get_ves_cpu()))
			return 1;
	read_ves_param();
	get_mounted_status();
	if (host_pattern != NULL)
		filter_by_hostname();
	if (name_pattern != NULL)
		filter_by_name();
	return 0;
}	

void print_names()
{
	int i;

	for (i = 0; i < sizeof(field_names) / sizeof(*field_names); i++) 
		printf("%-15s %-15s\n", field_names[i].name,
					field_names[i].hdr);
	return;
}

void free_veinfo()
{
	int i;

	for (i = 0; i < n_veinfo; i++) {
		if (veinfo[i].ip != NULL)
			free(veinfo[i].ip);
		if (veinfo[i].hostname != NULL)
			free(veinfo[i].hostname);
		if (veinfo[i].name != NULL)
			free(veinfo[i].name);
		if (veinfo[i].ubc != NULL)
			free(veinfo[i].ubc);
		if (veinfo[i].quota != NULL)
			free(veinfo[i].quota);
		if (veinfo[i].la != NULL)
			free(veinfo[i].la);
		if (veinfo[i].cpu != NULL)
			free(veinfo[i].cpu);
		if (veinfo[i].ve_root != NULL)
			free(veinfo[i].ve_root);

	}
}

static struct option list_options[] =
{
	{"no-header",	no_argument, NULL, 'H'},
	{"stopped",	no_argument, NULL, 'S'},
	{"all",		no_argument, NULL, 'a'},
	{"name",        no_argument, NULL, 'n'},
	{"name_filter", required_argument, NULL, 'N'},
	{"hostname",	required_argument, NULL, 'h'},
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
		c = getopt_long(argc, argv, "HSanN:h:o:s:Le1", list_options,
				&option_index);
		if (c == -1)
			break;
		
		switch(c) {
		case 'S'        :
			only_stopped_ve = 1;
			break;
		case 'H'	:
			show_hdr = 0;
			break;
		case 'L'	:
			print_names();
			return 0;
		case 'a'        :
			all_ve = 1;
			break;
		case 'h'	:
			host_pattern = strdup(optarg);
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
				printf("Invalid sort field name: %s\n", optarg);
				return 1;
			}
			break;
		case '1'	:
			f_order = strdup("veid");
			veid_only = 1;
			break;
		case 'n'	:
			f_order = strdup(default_nm_field_order);	
			with_names = 1;
			break;
		case 'N'	:
			name_pattern = strdup(optarg);
			break;
		default		:
			usage();
			return 1;
                }
	}
	if (optind < argc) {
		while (optind < argc) {
			veid =  strtol(argv[optind++], &ep, 10);
			if (*ep != 0 || !veid) {
				fprintf(stderr, "Invalid veid: %s\n",
				argv[optind - 1]);
				return 1;
			}
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
	if ((ret = collect()))
		return ret;
	print_ve();
	free_veinfo();
	return 0;
}
