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

struct iptables_s {
	char *name;
	unsigned long long id;
	unsigned long long mask;
};

#define IPTABLES_GEN(name, mask) { name, mask##_MOD, mask }

static struct iptables_s _g_iptables[] = {
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
	{ NULL }
};

static struct iptables_s *find_ipt_by_name(struct iptables_s *ipt,
		const char *name)
{
	struct iptables_s *p;

	for (p = ipt; p->name != NULL; p++)
		if (!strcmp(name, p->name))
			return p;

	return NULL;
}

void ipt_mask2str(unsigned long long mask, char *buf, int size)
{
	int r;
	char *sp, *ep;
	struct iptables_s *p;

	*buf = '\0';
	sp = buf;
	ep = buf + size;
	for (p = _g_iptables; p->name != NULL; p++) {
		if (!(mask & p->id))
			continue;
		r = snprintf(sp, ep - sp, "%s ", p->name);
		if (r < 0 || sp + r >= ep)
			break;
		sp += r;
	}
}

static unsigned long long get_iptables_mask(unsigned long long ids)
{
	unsigned long long mask = 0;
	struct iptables_s *p;

	for (p = _g_iptables; p->name != NULL; p++) {
		if (p->id & ids)
			mask |= p->mask;
	}

	return mask;
}

int parse_iptables(env_param_t *env, char *val)
{
	char *token;
	struct iptables_s *ipt;
	int ret = 0;

	for_each_strtok(token, val, "\t ,") {
		ipt = find_ipt_by_name(_g_iptables, token);
		if (!ipt) {
			logger(-1, 0, "Warning: Unknown iptable module: %s,"
				" skipped", token);
			ret = ERR_INVAL_SKIP;
			continue;
		}
		env->ipt_mask |= ipt->mask;
	}

	return ret;
}

/* Netfilter functionality, replacing iptables */

#define VE_NF_STATELESS	\
	(VE_IP_FILTER | VE_IP_MANGLE)
#define VE_NF_STATELESS6 \
	(VE_IP_FILTER6 | VE_IP_MANGLE6)
#define VE_NF_STATEFUL \
	(VE_NF_STATELESS | VE_NF_CONNTRACK | VE_IP_CONNTRACK | \
	 VE_IP_CONNTRACK_FTP | VE_IP_CONNTRACK_IRC)
#define VE_NF_STATEFUL6 \
	(VE_NF_STATELESS6 | VE_NF_CONNTRACK | VE_IP_CONNTRACK)

enum {
	VZCTL_NF_DISABLED = 1,
	VZCTL_NF_STATELESS,
	VZCTL_NF_STATEFUL,
	VZCTL_NF_FULL,
};

static struct iptables_s _g_netfilter[] = {
	{"disabled",	VZCTL_NF_DISABLED,	VE_IP_NONE},
	{"stateless",	VZCTL_NF_STATELESS,	VE_NF_STATELESS | VE_NF_STATELESS6},
	{"stateful",	VZCTL_NF_STATEFUL,	VE_NF_STATEFUL | VE_NF_STATEFUL6},
	{"full",	VZCTL_NF_FULL,		VE_IP_ALL},
	{ NULL }
};

const char* netfilter_mask2str(unsigned long id)
{
	struct iptables_s *p;

	for (p = _g_netfilter; p->name != NULL; p++)
		if (p->id == id)
			return p->name;

	return NULL;
}

int parse_netfilter(env_param_t *env, const char *val)
{
	struct iptables_s *p;

	p = find_ipt_by_name(_g_netfilter, val);
	if (p == NULL) {
		logger(-1, 0, "Incorrect netfilter value: %s", val);
		return ERR_INVAL;
	}

	env->nf_mask = p->id;

	return 0;
}

static unsigned long long get_netfilter_mask(unsigned long id)
{
	struct iptables_s *p;

	for (p = _g_netfilter; p->name != NULL; p++)
		if (p->id == id)
			return p->mask;

	return 0;
}

unsigned long long get_ipt_mask(env_param_t *p)
{
	if (p->nf_mask) {
#if 0	/* TODO: enable some time later */
		if (p->ipt_mask)
			logger(0, 0, "Warning: IPTABLES obsoleted by "
					"NETFILTER and is ignored, please "
					"remove it from CT config");
#endif
		return get_netfilter_mask(p->nf_mask);
	} else if (p->ipt_mask)
		return get_iptables_mask(p->ipt_mask);

	return VE_IP_DEFAULT;
}
