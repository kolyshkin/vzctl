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

#include <errno.h>
#include <sys/ioctl.h>
#include <linux/vzcalluser.h>
#include <string.h>
#include <limits.h>

#include "vzerror.h"
#include "res.h"
#include "logger.h"
#include "meminfo.h"

static struct {
	char *mode_nm;
	int mode_id;
} mode_tbl_[] = {
	{"none", VE_MEMINFO_NONE},
	{"pages", VE_MEMINFO_PAGES},
	{"privvmpages",	VE_MEMINFO_PRIVVMPAGES},
};

int vps_meminfo_set(vps_handler *h, envid_t veid, meminfo_param *param,
	vps_param *vps_p)
{
	int ret;
	unsigned long *privvmpages;
	struct vzctl_ve_meminfo meminfo;

	if (param->mode < 0)
		return 0;

	meminfo.veid = veid;
	switch (param->mode) {
	case VE_MEMINFO_NONE:
		meminfo.val = 0;
	case VE_MEMINFO_PAGES:
		meminfo.val = param->val;
		break;
	case VE_MEMINFO_PRIVVMPAGES:
		if ((privvmpages = vps_p->res.ub.privvmpages) == NULL) {
			privvmpages = vps_p->g_param != NULL ?
				vps_p->g_param->res.ub.privvmpages : NULL; 
		}
		if (privvmpages == NULL) {
			logger(0, 0, "Warning: privvmpages is not set"
				" configure meminfo skipped");
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
	if (param->mode == VE_MEMINFO_NONE)
		logger(0, 0, "Configure meminfo: none");
	else
		logger(0, 0, "Configure meminfo: %lu", meminfo.val);
	ret = ioctl(h->vzfd, VZCTL_VE_MEMINFO, &meminfo);
	if (ret < 0) {
		if (errno == ENOTTY) {
			logger(0, 0, "Warning: meminfo feature is not supported"
				" by kernel. skipped meminfo configure");
		} else {
			logger(-1, errno, "Unable to set meminfo");	
			return VZ_SET_MEMINFO_ERROR;
		}
	}
	return 0;
}

int get_meminfo_mode(char *name)
{
	int i;

	for (i = 0; i < sizeof(mode_tbl_) / sizeof(mode_tbl_[0]); i++)
		if (!strcmp(mode_tbl_[i].mode_nm, name))
			return mode_tbl_[i].mode_id;

	return -1;
}

const char *get_meminfo_mode_nm(int id)
{
	int i;

	for (i = 0; i < sizeof(mode_tbl_) / sizeof(mode_tbl_[0]); i++)
		if (mode_tbl_[i].mode_id == id)
			return mode_tbl_[i].mode_nm;
	return NULL;
}
