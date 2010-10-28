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

#include <string.h>
#include <stdio.h>
#include <linux/vzcalluser.h>

#include "util.h"
#include "iptables.h"

#define IPTABLES_GEN(name, mask) { name, mask##_MOD, mask }

static struct iptables_s iptables[] = {
#ifdef VZCTL_ENV_CREATE_DATA
	IPTABLES_GEN("ip_tables",	VE_IP_IPTABLES),
	IPTABLES_GEN("iptable_filter",	VE_IP_FILTER),
	IPTABLES_GEN("iptable_mangle",	VE_IP_MANGLE),
	IPTABLES_GEN("ipt_limit",	VE_IP_MATCH_LIMIT),
	IPTABLES_GEN("ipt_multiport",	VE_IP_MATCH_MULTIPORT),
	IPTABLES_GEN("ipt_tos",		VE_IP_MATCH_TOS),
	IPTABLES_GEN("ipt_TOS",		VE_IP_TARGET_TOS),
	IPTABLES_GEN("ipt_REJECT",	VE_IP_TARGET_REJECT),
	IPTABLES_GEN("ipt_TCPMSS",	VE_IP_TARGET_TCPMSS),
	IPTABLES_GEN("ipt_tcpmss",	VE_IP_MATCH_TCPMSS),
	IPTABLES_GEN("ipt_ttl",		VE_IP_MATCH_TTL),
	IPTABLES_GEN("ipt_LOG",		VE_IP_TARGET_LOG),
	IPTABLES_GEN("ipt_length",	VE_IP_MATCH_LENGTH),
	IPTABLES_GEN("ip_conntrack",	VE_IP_CONNTRACK),
	IPTABLES_GEN("ip_conntrack_ftp",VE_IP_CONNTRACK_FTP),
	IPTABLES_GEN("ip_conntrack_irc",VE_IP_CONNTRACK_IRC),
	IPTABLES_GEN("ipt_conntrack",	VE_IP_MATCH_CONNTRACK),
	IPTABLES_GEN("ipt_state",	VE_IP_MATCH_STATE),
	IPTABLES_GEN("ipt_helper",	VE_IP_MATCH_HELPER),
	IPTABLES_GEN("iptable_nat",	VE_IP_NAT),
	IPTABLES_GEN("ip_nat_ftp",	VE_IP_NAT_FTP),
	IPTABLES_GEN("ip_nat_irc",	VE_IP_NAT_IRC),
#ifdef VE_IP_TARGET_REDIRECT
	IPTABLES_GEN("ipt_REDIRECT",	VE_IP_TARGET_REDIRECT),
#endif
#ifdef VE_IP_MATCH_MAC
	IPTABLES_GEN("xt_mac",		VE_IP_MATCH_MAC),
#endif
#ifdef VE_IP_MATCH_RECENT
	IPTABLES_GEN("ipt_recent",	VE_IP_MATCH_RECENT),
#endif
#endif /* VZCTL_ENV_CREATE_DATA */
	IPTABLES_GEN("ipt_owner",	VE_IP_MATCH_OWNER),
};

struct iptables_s *find_ipt(const char *name)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(iptables); i++)
		if (!strcmp(name, iptables[i].name))
			return &iptables[i];
	return NULL;
}

void ipt_mask2str(unsigned long long mask, char *buf, int size)
{
	int i, r;
	char *sp, *ep;

	sp = buf;
	ep = buf + size;
	for (i = 0;  i < (int)ARRAY_SIZE(iptables); i++) {
		if (!(mask & iptables[i].id))
			continue;
		r = snprintf(sp, ep - sp, "%s ", iptables[i].name);
		if (r < 0 || sp + r >= ep)
			break;
		sp += r;
	}
}

unsigned long long get_ipt_mask(unsigned long long ids)
{
	unsigned long long mask;
	int i;

	if (!ids)
		return VE_IP_DEFAULT;
	mask = 0;
	for (i = 0;  i < (int)ARRAY_SIZE(iptables); i++) {
		if (iptables[i].id & ids)
			mask |= iptables[i].mask;
	}
	return mask;
}
