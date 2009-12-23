/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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

#include <linux/vzcalluser.h>
#include <string.h>
#include <stdio.h>

#include "vzfeatures.h"

static struct feature_s features[] = {
	{ "sysfs",	0,	VE_FEATURE_SYSFS },
	{ "nfs",	0,	VE_FEATURE_NFS },
	{ "sit",	0,	VE_FEATURE_SIT},
	{ "ipip",	0,	VE_FEATURE_IPIP},
	{ "ppp",	0,	VE_FEATURE_PPP},
	{ "ipgre",	0,	VE_FEATURE_IPGRE },
	{ "bridge",	0,	VE_FEATURE_BRIDGE },
	{ NULL,		0,	0 }
};

struct feature_s *find_feature(const char *name)
{
	struct feature_s *feat;
	int len = 0; /* make gcc happy */

	for (feat = features; feat->name != NULL; feat++) {
		len = strlen(feat->name);
		if (strncmp(name, feat->name, len) == 0)
			break;
	}

	if (feat->name == NULL)
		return NULL;

	if (name[len] != ':')
		return NULL;

	if (strcmp(name + len + 1, "on") == 0) {
		feat->on = 1;
		return feat;
	}
	if (strcmp(name + len + 1, "off") == 0) {
		feat->on = 0;
		return feat;
	}

	return NULL;
}

void features_mask2str(unsigned long long mask, unsigned long long known,
		char *buf, int len)
{
	struct feature_s *feat;
	int ret;

	for (feat = features; feat->name != NULL; feat++) {
		if (!(known & feat->mask))
			continue;

		ret = snprintf(buf, len, "%s:%s ", feat->name,
				mask & feat->mask ? "on" : "off");

		buf += ret;
		len -= ret;
		if (len <= 0)
			break;
	}
}
