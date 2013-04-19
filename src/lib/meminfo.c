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

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/vzcalluser.h>
#include <string.h>
#include <limits.h>

#include "vzerror.h"
#include "res.h"
#include "logger.h"
#include "meminfo.h"
#include "util.h"
#include "ub.h"

static struct {
	char *mode_nm;
	int mode_id;
} mode_tbl_[] = {
	{"none", VE_MEMINFO_NONE},
	{"pages", VE_MEMINFO_PAGES},
	{"privvmpages",	VE_MEMINFO_PRIVVMPAGES},
};

int vps_meminfo_set(vps_handler *h, envid_t veid, meminfo_param *gparam,
	vps_param *vps_p, int state)
{
	int ret;
	unsigned long *privvmpages = vps_p->res.ub.privvmpages;
	struct vzctl_ve_meminfo meminfo;
	meminfo_param *param = gparam;
	meminfo_param default_param = {
		VE_MEMINFO_PRIVVMPAGES, 1
	};

	if (!is_vz_kernel(h))
		return 0;

	if (is_vswap_config(&vps_p->res.ub))
			return 0;

	if (state != STATE_STARTING) {
		/* update meminfo on --privvmpages, --meminfo */
		if (param->mode < 0 && privvmpages == NULL)
			return 0;
		if (vps_p->g_param != NULL) {
			if (privvmpages == NULL)
				/* use privvmpages from VE.conf on --meminfo */
				privvmpages = vps_p->g_param->res.ub.privvmpages;

			if (param->mode < 0) {
				/* use meminfo from VE.conf on --privvmpages */
				param = &vps_p->g_param->res.meminfo;
				if (param->mode < 0)
					param = &default_param;
				if (param->mode != VE_MEMINFO_PRIVVMPAGES)
					return 0;
			}
		}
	}
	if (param->mode < 0)
		param = &default_param;

	meminfo.veid = veid;
	switch (param->mode) {
	case VE_MEMINFO_NONE:
		meminfo.val = 0;
		break;
	case VE_MEMINFO_PAGES:
		meminfo.val = param->val;
		break;
	case VE_MEMINFO_PRIVVMPAGES:
		if (privvmpages == NULL) {
			logger(0, 0, "Warning: privvmpages not set, "
				"skipping meminfo configuration");
			return 0;
		}
		meminfo.val = (((ULONG_MAX - 1) / param->val + 1)
			< privvmpages[0]) ?
			 ULONG_MAX : privvmpages[0] * param->val;
		break;
	default:
		logger(0, 0, "Warning: unrecognized mode"
			" to set meminfo parameter");
		return 0;
	}
	logger(1, 0, "Configuring meminfo: %lu", meminfo.val);
	ret = ioctl(h->vzfd, VZCTL_VE_MEMINFO, &meminfo);
	if (ret < 0) {
		if (errno == ENOTTY) {
			logger(0, 0, "Warning: meminfo feature is not supported"
				" by kernel, skipped meminfo configure");
		} else {
			logger(-1, errno, "Unable to set meminfo");
			return VZ_SET_MEMINFO_ERROR;
		}
	}
	return 0;
}

int get_meminfo_mode(char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mode_tbl_); i++)
		if (!strcmp(mode_tbl_[i].mode_nm, name))
			return mode_tbl_[i].mode_id;

	return -1;
}

const char *get_meminfo_mode_nm(int id)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mode_tbl_); i++)
		if (mode_tbl_[i].mode_id == id)
			return mode_tbl_[i].mode_nm;
	return NULL;
}
