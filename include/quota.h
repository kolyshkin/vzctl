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
#ifndef	_QUOTA_H_
#define	_QUOTA_H_

/** Disk quota commands.
 */
#define QUOTA_DROP		1
#define QUOTA_STAT		2
#define QUOTA_STAT2		3
#define QUOTA_MARKDIRTY		9
#define QUOTA_SHOW		5

#define QUOTA_EXPTIME		259200

/** /usr/sbin/vzquota exit codes.
 */
#define EXITCODE_QUOTARUN	5
#define EXITCODE_QUOTANOTRUN	6
#define EXITCODE_UGID_NOTRUN	9
#define EXITCODE_QUOTNOTEXIST	11

/** Data structure for disk quota parameters.
 */
typedef struct {
	int enable;			/**< is quota enabled (YES or NO). */
	unsigned long *diskspace;	/**< disk blocks limit. */
	unsigned long *diskinodes;	/**< disk inodes limit. */
	unsigned long *exptime;		/**< quota expiration time. */
	unsigned long *ugidlimit;	/**< user/group quota limit. */
	int offline_resize;		/**< ploop offline resize. */
} dq_param;

int is_vzquota_available(void);
int is_2nd_level_quota_on(const dq_param *param);
int vps_set_quota(envid_t veid, dq_param *dq);
int vps_quotaon(envid_t veid, char *private, dq_param *dq);
int vps_quotaoff(envid_t veid, dq_param *dq);

int quota_ctl(envid_t veid, int cmd);
int quota_off(envid_t veid, int force);
int quota_on(envid_t veid, char *private, dq_param *param);
int quota_set(envid_t veid, char *private, dq_param *param);
int quota_init(envid_t veid, char *private, dq_param *param);
void quota_inc(dq_param *param, int delta);
#endif
