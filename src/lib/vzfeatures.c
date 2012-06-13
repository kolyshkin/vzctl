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

#include "util.h"
#include "vzfeatures.h"
#include <linux/vzcalluser.h>

static struct feature_s features[] = {
	{ "sysfs",	0,	VE_FEATURE_SYSFS },
	{ "nfs",	0,	VE_FEATURE_NFS },
	{ "sit",	0,	VE_FEATURE_SIT},
	{ "ipip",	0,	VE_FEATURE_IPIP},
	{ "ppp",	0,	VE_FEATURE_PPP},
	{ "ipgre",	0,	VE_FEATURE_IPGRE },
	{ "bridge",	0,	VE_FEATURE_BRIDGE },
	{ "nfsd",	0,	VE_FEATURE_NFSD },
};

struct feature_s *find_feature(const char *name)
{
	struct feature_s *feat;
	int len = 0; /* make gcc happy */
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(features); i++) {
		feat = &features[i];
		len = strlen(feat->name);
		if (strncmp(name, feat->name, len) == 0 && name[len] == ':')
			break;
	}

	if (feat->name == NULL)
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
		const char *delim, char *buf, int len)
{
	struct feature_s *feat;
	int ret;
	unsigned int i, j = 0;

	for (i = 0; i < ARRAY_SIZE(features); i++) {
		feat = &features[i];
		if (!(known & feat->mask))
			continue;

		ret = snprintf(buf, len, "%s%s:%s",
				j++ == 0 ? "" : delim,
				feat->name,
				mask & feat->mask ? "on" : "off");

		buf += ret;
		len -= ret;
		if (len <= 0)
			break;
	}
}

void print_json_features(unsigned long long mask, unsigned long long known)
{
	struct feature_s *feat;
	unsigned int i, j = 0;

	for (i = 0; i < ARRAY_SIZE(features); i++) {
		feat = &features[i];
		if (!(known & feat->mask))
			continue;

		printf("%s      \"%s\": %s",
				j++ == 0 ? "{\n" : ",\n",
				feat->name,
				mask & feat->mask ? "true" : "false");

	}
	if (j)
		printf("\n    }");
	else
		printf("null");
}
