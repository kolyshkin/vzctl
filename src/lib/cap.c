/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

#include <linux/unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/capability.h>
#include <string.h>
#include <linux/vzcalluser.h>

#include "cap.h"
#include "res.h"
#include "vzerror.h"
#include "logger.h"
#include "util.h"

#ifndef	CAP_AUDIT_WRITE
#define	CAP_AUDIT_WRITE	29
#endif
#ifndef	CAP_VE_ADMIN
#define	CAP_VE_ADMIN	30
#endif
#ifndef	CAP_SETFCAP
#define	CAP_SETFCAP	31
#endif

#define CAPDEFAULTMASK_OLD				  \
	CAP_TO_MASK(CAP_CHOWN)				| \
	CAP_TO_MASK(CAP_DAC_OVERRIDE)			| \
	CAP_TO_MASK(CAP_DAC_READ_SEARCH)		| \
	CAP_TO_MASK(CAP_FOWNER)				| \
	CAP_TO_MASK(CAP_FSETID)				| \
	CAP_TO_MASK(CAP_KILL)				| \
	CAP_TO_MASK(CAP_SETGID)				| \
	CAP_TO_MASK(CAP_SETUID)				| \
	CAP_TO_MASK(CAP_LINUX_IMMUTABLE)		| \
	CAP_TO_MASK(CAP_NET_BIND_SERVICE)		| \
	CAP_TO_MASK(CAP_NET_BROADCAST)			| \
	CAP_TO_MASK(CAP_NET_RAW)			| \
	CAP_TO_MASK(CAP_IPC_LOCK)			| \
	CAP_TO_MASK(CAP_IPC_OWNER)			| \
	CAP_TO_MASK(CAP_SYS_CHROOT)			| \
	CAP_TO_MASK(CAP_SYS_PTRACE)			| \
	CAP_TO_MASK(CAP_SYS_BOOT)			| \
	CAP_TO_MASK(CAP_SYS_NICE)			| \
	CAP_TO_MASK(CAP_SYS_RESOURCE)			| \
	CAP_TO_MASK(CAP_SYS_TTY_CONFIG)			| \
	CAP_TO_MASK(CAP_MKNOD)				| \
	CAP_TO_MASK(CAP_LEASE)				| \
	CAP_TO_MASK(CAP_VE_ADMIN)			| \
	CAP_TO_MASK(CAP_AUDIT_WRITE)

#define CAPDEFAULTMASK					  \
	CAPDEFAULTMASK_OLD				| \
	CAP_TO_MASK(CAP_SETPCAP)			| \
	CAP_TO_MASK(CAP_SETFCAP)

static char *cap_names[] = {
"CHOWN",		/*	0	*/
"DAC_OVERRIDE",		/*	1	*/
"DAC_READ_SEARCH",	/*	2	*/
"FOWNER",		/*	3	*/
"FSETID",		/*	4	*/
"KILL",			/*	5	*/
"SETGID",		/*	6	*/
"SETUID",		/*	7	*/
"SETPCAP",		/*	8	*/
"LINUX_IMMUTABLE",	/*	9	*/
"NET_BIND_SERVICE",	/*	10	*/
"NET_BROADCAST",	/*	11	*/
"NET_ADMIN",		/*	12	*/
"NET_RAW",		/*	13	*/
"IPC_LOCK",		/*	14	*/
"IPC_OWNER",		/*	15	*/
"SYS_MODULE",		/*	16	*/
"SYS_RAWIO",		/*	17	*/
"SYS_CHROOT",		/*	18	*/
"SYS_PTRACE",		/*	19	*/
"SYS_PACCT",		/*	20	*/
"SYS_ADMIN",		/*	21	*/
"SYS_BOOT",		/*	22	*/
"SYS_NICE",		/*	23	*/
"SYS_RESOURCE",		/*	24	*/
"SYS_TIME",		/*	25	*/
"SYS_TTY_CONFIG",	/*	26	*/
"MKNOD",		/*	27	*/
"LEASE",		/*	28	*/
"AUDIT_WRITE",		/*	29	*/
"VE_ADMIN",		/*	30	*/
"SETFCAP",		/*	31	*/

"FS_MASK"		/*	0x1f	*/
};

/* We can't include sys/capability.h since it conflicts
 * with linux/capability.h, so we put this prototype here */
extern int capget(cap_user_header_t header, const cap_user_data_t data);

static inline int capset(cap_user_header_t header, cap_user_data_t data)
{
	return syscall(__NR_capset, header, data);
}

/** Add capability name to capability mask.
 *
 * @param name		capability name.
 * @param mask		capability mask.
 * @return		0 on success.
 */
int get_cap_mask(char *name, unsigned long *mask)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cap_names); i++) {
		if (!strcasecmp(name, cap_names[i])) {
			cap_raise(*mask, i);
			return 0;
		}
	}
	return -1;
}

/** merge capabilities and return in string format.
 *
 * @param new		new capability mask.
 * @param old		old capability mask.
 * @param buf		merged capabilities in string format.
 * @param len		max size of buf string.
 */
void build_cap_str(cap_param *new, cap_param *old, char *buf, int len)
{
	unsigned int i;
	int r;
	char *sp, *ep;

	sp = buf;
	ep = buf + len;
	sp += sprintf(sp, "\"");
	for (i = 0; i < ARRAY_SIZE(cap_names); i++) {
		int op = 0;

		if (CAP_TO_MASK(i) & new->on)
			op = 1;
		else if (CAP_TO_MASK(i) & new->off)
			op = 2;
		else if (old == NULL)
			continue;
		else if (CAP_TO_MASK(i) & old->on)
			op = 1;
		else if (CAP_TO_MASK(i) & old->off)
			op = 2;
		else
			continue;
		r = snprintf(sp, ep - sp,  "%s:%s ", cap_names[i],
			op == 1 ? "on" : "off");
		if (r < 0 || sp + r >= ep)
			break;
		sp += r;
	}
	sprintf(sp--, "\"");
}

static int set_cap(envid_t veid, cap_t mask, int pid)
{
	struct __user_cap_header_struct header;
	struct __user_cap_data_struct data;

	memset(&header, 0, sizeof(header));
	header.version = _LINUX_CAPABILITY_VERSION;
	capget(&header, NULL); /* Get linux capability version from kernel */
	header.pid = pid;

	memset(&data, 0, sizeof(data));
	data.effective = mask;
	data.permitted = mask;
	data.inheritable = mask;

	return capset(&header, &data);
}

static inline cap_t make_cap_mask(cap_t def, cap_t on, cap_t off)
{
	return (def | on) & ~off;
}

int vps_set_cap(envid_t veid, struct env_param *env, cap_param *cap)
{
	cap_t mask;

	if ((env->features_known & env->features_mask) & VE_FEATURE_BRIDGE)
		cap_raise(cap->on, CAP_NET_ADMIN);

	mask = make_cap_mask(CAPDEFAULTMASK, cap->on, cap->off);
	if (set_cap(veid, mask, 0) == 0)
		return 0;

	/* Probably old kernel -- retry with old default mask */
	mask = make_cap_mask(CAPDEFAULTMASK_OLD, cap->on, cap->off);
	if (set_cap(veid, mask, 0) == 0)
		return 0;

	logger(-1, errno, "Unable to set capability");
	return VZ_SET_CAP;
}
