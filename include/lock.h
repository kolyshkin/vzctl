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
#ifndef _LOCK_H_
#define _LOCK_H_

/** Lock VPS.
 * Create lock file $dir/$veid.lck.
 * @param veid		VPD id.
 * @param dir		lock directory.
 * @param status	transition status.
 * @return		0 - success
 *			1 - locked
 *			-1- error.
 */
int vps_lock(envid_t veid, char *dir, char *status);

/** Unlock VPS.
 *
 * @param veid		VPS id.
 * @param dir		lock directory.
 */
void vps_unlock(envid_t veid, char *dir);
int _lock(char *lockfile, int blk);
void _unlock(int fd, char *lockfile);

#endif
