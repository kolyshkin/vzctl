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
#ifndef _VZLIST_H
#define _VZLIST_H

/* #include <linux/vzlist.h> */
#define HAVE_VZLIST_IOCTL 0

#define PROCVESTAT	"/proc/vz/vestat"
#define PROCUBC		"/proc/user_beancounters"
#define PROCQUOTA	"/proc/vz/vzquota"
#define PROCFSHED	"/proc/fairsched"

#define VZQUOTA		"/usr/sbin/vzquota"

#define MAXCPUUNITS	500000

enum {
	VE_RUNNING,
	VE_STOPPED,
	VE_MOUNTED,
	VE_SUSPENDED
};

char *ve_status[]= {
	"running",
	"stopped",
	"mounted",
	"suspended"
};

struct Cubc {
	unsigned long kmemsize[5];
	unsigned long lockedpages[5];
	unsigned long privvmpages[5];
	unsigned long shmpages[5];
	unsigned long numproc[5];
	unsigned long physpages[5];
	unsigned long vmguarpages[5];
	unsigned long oomguarpages[5];
	unsigned long numtcpsock[5];
	unsigned long numflock[5];
	unsigned long numpty[5];
	unsigned long numsiginfo[5];
	unsigned long tcpsndbuf[5];
	unsigned long tcprcvbuf[5];
	unsigned long othersockbuf[5];
	unsigned long dgramrcvbuf[5];
	unsigned long numothersock[5];
	unsigned long dcachesize[5];
	unsigned long numfile[5];
	unsigned long numiptent[5];
	unsigned long swappages[5];
};

struct Cquota {
	unsigned long diskspace[3];	// 0-usage 1-softlimit 2-hardlimit
	unsigned long diskinodes[3];	// 0-usage 1-softlimit 2-hardlimit
};

struct Ccpustat {
	float la[3];			// load average
	float uptime;
};

struct Ccpu {
	unsigned long limit[1];
	unsigned long units[1];
};

struct Cio {
	int ioprio;
	int iolimit;
	int iopslimit;
};

struct Cveinfo {
	int veid;
	char *hostname;
	char *name;
	char *description;
	char *ostemplate;
	char *ip;
	char *nameserver;
	char *searchdomain;
	char *private;
	char *root;
	char *mount_opts;
	char *origin_sample;
	struct Cubc *ubc;
	struct Cquota *quota;
	struct Ccpustat *cpustat;
	struct Ccpu *cpu;
	struct Cio io;
	int status;
	int hide;
	int onboot;
	int cpunum;
	int disabled;
	unsigned long *bootorder;
	int layout; // VE_LAYOUT_*
	unsigned long long features_mask;
	unsigned long long features_known;
	float vm_overcommit;
};

#define RES_NONE	0
#define RES_HOSTNAME	1
#define RES_UBC		2
#define RES_QUOTA	3
#define RES_IP		4
#define RES_CPUSTAT	5
#define RES_CPU		6
#define RES_CPUNUM	7
#define RES_STATUS	8
#define RES_IO		9

struct Cfield {
	char *name;
	char *hdr;
	char *hdr_fmt;
	int index;
	int res_type;
	void (* const print_fn)(struct Cveinfo *p, int index);
	int (* const sort_fn)(const void* val1, const void* val2);
};

struct Cfield_order {
	int order;
	struct Cfield_order *next;
};

#ifndef NIPQUAD
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]
#endif

#endif //_VZLIST_H
