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

#ifdef HAVE_PLOOP
#include <ploop/libploop.h>
#include <ploop/dynload.h>
#endif

#include "types.h"
#include "quota.h"
#include "fs.h"
#include "res.h"

#define VZCTL_VE_ROOTHDD_DIR	"root.hdd"

/* We want ve_private_is_ploop() to work even when vzctl is compiled
 * without ploop lib headers, thus a define copied from libploop.h:
 */
#ifndef DISKDESCRIPTOR_XML
#define DISKDESCRIPTOR_XML	"DiskDescriptor.xml"
#endif

#define GET_DISK_DESCRIPTOR(buf, ve_private) \
	snprintf(buf, sizeof(buf), \
		"%s/" VZCTL_VE_ROOTHDD_DIR "/" DISKDESCRIPTOR_XML, ve_private)

struct vzctl_create_image_param {
	int mode;
	unsigned long size;
	unsigned long inodes;	/* number of inodes */
};

struct vzctl_mount_param {
	char device[64];
	int ro;
	char *guid;
	int mount_by_parent_guid;
	char *target;
	int quota;
	char *mount_data;
	int fsck;
	char dummy[32];
};

int is_ploop_supported(void);
int get_ploop_type(const char *type);
int guess_ve_private_is_ploop(const char *private);
int ve_private_is_ploop(const fs_param *fs);

#ifdef HAVE_PLOOP
extern struct ploop_functions ploop;

#define PLOOP_CLEANUP(code)					\
do {								\
	struct vzctl_cleanup_handler *__cl_h;			\
	__cl_h = add_cleanup_handler(cancel_ploop_op, NULL);	\
	code;							\
	del_cleanup_handler(__cl_h);				\
} while(0)

int is_image_mounted(const char *ve_private);
int vzctl_create_image(const char *ve_private, struct vzctl_create_image_param *param);
int vzctl_mount_image(const char *ve_private, struct vzctl_mount_param *param);
int vzctl_umount_image(const char *ve_private);
int vzctl_convert_image(const char *ve_private, int mode);
int vzctl_resize_image(const char *ve_private, unsigned long long newsize, int offline);
int vzctl_get_ploop_dev(const char *mnt, char *out, int len);
int vzctl_create_snapshot(const char *ve_private, const char *guid);
int vzctl_merge_snapshot(const char *ve_private, const char *guid);
int vzctl_delete_snapshot(const char *ve_private, const char *guid);
int vzctl_mount_snapshot(unsigned envid, const char *ve_private, struct vzctl_mount_param *param);
int vzctl_umount_snapshot(unsigned envid, const char *ve_private, char *guid);
const char *generate_snapshot_component_name(unsigned int envid,
		const char *data, char *buf, int len);
int vzctl_env_convert_ploop(vps_handler *h, envid_t veid, vps_param *vps_p,
		fs_param *fs, dq_param *dq, int mode);
#endif /* HAVE_PLOOP */

#endif /* _IMAGE_H_ */
