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
#ifndef	_FS_H_
#define _FS_H_

#include "types.h"
#include "quota.h"

#define MNT_DETACH              0x00000002
/** Action script prefixes.
 */
#define MOUNT_PREFIX            "mount"
#define UMOUNT_PREFIX           "umount"
#define START_PREFIX            "start"
#define STOP_PREFIX             "stop"

#define DESTR_PREFIX            "destroyed"

/**  Data structure for file system parameter.
 */
typedef struct {
	char *private;		/**< VPS private path. */
	char *private_orig;	/**< original not expanded private path. */
	char *root;		/**< VPS root path. */
	char *root_orig;	/**< original not expanded root path. */
	char *tmpl;		/**< TEMPLATE path. */
	int noatime;
} fs_param;

/** Get VPS mount status.
 *
 * @param root		VPS root.
 * @return		1 - VPS mounted
 *			0 - VPS unmounted.
 *			-1- error
 */
int vps_is_mounted(char *root);

/** Mount VPS.
 *
 * @param veid		VPS id.
 * @param fs		file system parameters.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int fsmount(envid_t veid, fs_param *fs, dq_param *dq);

/** Mount VPS and run mount action script if exists.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param fs		file system parameters.
 * @param dq		disk quota parameters.
 * @param skip		skip mount action scrips
 * @return		0 on success.
 */
int vps_mount(vps_handler *h, envid_t veid, fs_param *fs, dq_param *dq,
	skipFlags skip);

/** Unmount VPS.
 *
 * @param veid		VPS id.
 * @param root		VPS root.
 * @return		0 on success.
 */
int fsumount(envid_t veid, char *root);

/** Unmount VPS and run unmount action script if exists.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param root		VPS root.
 * @param skip		skip unmount action scrips
 * @return		0 on success.
 */
int vps_umount(vps_handler *h, envid_t veid, char *root, skipFlags skip);

int vps_set_fs(fs_param *g_fs, fs_param *fs);

#endif
