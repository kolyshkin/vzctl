/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <asm/timex.h>
#include <linux/vzcalluser.h>
#include <string.h>
#include <stdio.h>

#include "iptables.h"

static struct iptables_s iptables[] = {
#ifdef VZCTL_ENV_CREATE_DATA
	{"ip_tables", VE_IP_IPTABLES_MOD, VE_IP_IPTABLES},
	{"iptable_filter", VE_IP_FILTER_MOD, VE_IP_FILTER},
	{"iptable_mangle", VE_IP_MANGLE_MOD, VE_IP_MANGLE},
	{"ipt_limit", VE_IP_MATCH_LIMIT_MOD, VE_IP_MATCH_LIMIT},
	{"ipt_multiport", VE_IP_MATCH_MULTIPORT_MOD, VE_IP_MATCH_MULTIPORT},
	{"ipt_tos", VE_IP_MATCH_TOS_MOD, VE_IP_MATCH_TOS},
	{"ipt_TOS", VE_IP_TARGET_TOS_MOD, VE_IP_TARGET_TOS},
	{"ipt_REJECT", VE_IP_TARGET_REJECT_MOD, VE_IP_TARGET_REJECT},
	{"ipt_TCPMSS", VE_IP_TARGET_TCPMSS_MOD, VE_IP_TARGET_TCPMSS},
	{"ipt_tcpmss", VE_IP_MATCH_TCPMSS_MOD, VE_IP_MATCH_TCPMSS},
	{"ipt_ttl", VE_IP_MATCH_TTL_MOD, VE_IP_MATCH_TTL},
	{"ipt_LOG", VE_IP_TARGET_LOG_MOD, VE_IP_TARGET_LOG},
	{"ipt_length", VE_IP_MATCH_LENGTH_MOD, VE_IP_MATCH_LENGTH},
	{"ip_conntrack", VE_IP_CONNTRACK_MOD, VE_IP_CONNTRACK},
	{"ip_conntrack_ftp", VE_IP_CONNTRACK_FTP_MOD, VE_IP_CONNTRACK_FTP},
	{"ip_conntrack_irc", VE_IP_CONNTRACK_IRC_MOD, VE_IP_CONNTRACK_IRC},
	{"ipt_conntrack", VE_IP_MATCH_CONNTRACK_MOD, VE_IP_MATCH_CONNTRACK},
	{"ipt_state", VE_IP_MATCH_STATE_MOD, VE_IP_MATCH_STATE},
	{"ipt_helper", VE_IP_MATCH_HELPER_MOD, VE_IP_MATCH_HELPER},
	{"iptable_nat", VE_IP_NAT_MOD, VE_IP_NAT},
	{"ip_nat_ftp", VE_IP_NAT_FTP_MOD, VE_IP_NAT_FTP},
	{"ip_nat_irc", VE_IP_NAT_IRC_MOD, VE_IP_NAT_IRC},
#ifdef VE_IP_TARGET_REDIRECT
	{"ipt_REDIRECT", VE_IP_TARGET_REDIRECT_MOD, VE_IP_TARGET_REDIRECT},
#endif
#endif /* VZCTL_ENV_CREATE_DATA */
	{NULL, 0}
};

struct iptables_s *find_ipt(const char *name)
{
	int i;

	for (i = 0; iptables[i].name != NULL; i++) 
		if (!strcmp(name, iptables[i].name))
			return &iptables[i];
	return NULL;
}

void ipt_mask2str(unsigned long mask, char *buf, int size)
{
	int i, r;
	char *sp, *ep;

	sp = buf;
	ep = buf + size;
	for (i = 0; iptables[i].name != NULL; i++) {
		if (!(mask & iptables[i].id))
			continue;
		r = snprintf(sp, ep - sp, "%s ", iptables[i].name);
		if (r < 0 || sp + r >= ep)
			break;
		sp += r;
	}
}

unsigned long long get_ipt_mask(unsigned long mask)
{
	if (!mask)
		return VE_IP_DEFAULT;
	return mask; 
}
