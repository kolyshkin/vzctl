/*
 *  Copyright (C) 2011-2012, Parallels, Inc. All rights reserved.
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

#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <ploop/libploop.h>
#include "types.h"
#include "quota.h"
#include "fs.h"

#define GET_DISK_DESCRIPTOR(buf, ve_private) \
	snprintf(buf, sizeof(buf), \
		"%s/root.hdd/" DISKDESCRIPTOR_XML, ve_private);

struct vzctl_create_image_param {
	int mode;
	unsigned long size;
};

struct vzctl_mount_param {
	char device[64];
	int ro;
	char *guid;
	int mount_by_parent_guid;
	char *target;
	int quota;
	char *mount_data;
	char dummy[32];
};

int get_ploop_type(const char *type);
int vzctl_create_image(const char *ve_private, struct vzctl_create_image_param *param);
int vzctl_mount_image(const char *ve_private, struct vzctl_mount_param *param);
int vzctl_umount_image(const char *ve_private, const char *target);
int vzctl_convert_image(const char *ve_private, int mode);
int vzctl_resize_image(const char *ve_private, unsigned long long newsize);
int vzctl_get_ploop_dev(const char *mnt, char *out, int len);
int vzctl_create_snapshot(const char *ve_private, const char *guid);
int vzctl_merge_snapshot(const char *ve_private, const char *guid);
int vzctl_delete_snapshot(const char *ve_private, const char *guid);
int ve_private_is_ploop(const char *private);
int vzctl_env_convert_ploop(vps_handler *h, envid_t veid,
		fs_param *fs, dq_param *dq);

#endif /* _IMAGE_H_ */
