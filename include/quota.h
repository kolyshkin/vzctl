/*
 * Copyright (C) 2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef	_QUOTA_H_
#define	_QUOTA_H_

/** Disk quota commands.
 */
#define QUOTA_DROP              1
#define QUOTA_STAT              2
#define QUOTA_STAT2             3
#define QUOTA_MARKDURTY         9
#define QUOTA_SHOW              5

#define QUOTA_EXPTIME		259200

/** /usr/sbin/vzquota exit codes.
 */
#define EXITCODE_QUOTARUN	5
#define EXITCODE_QUOTANOTRUN	6
#define EXITCODE_QUOTNOTEXIST	11

/** Data structure for disk quota parameters.
 */
typedef struct {
	int enable;			/**< quota enable yes/no. */
	unsigned long *diskspace;	/**< disk block limit. */
	unsigned long *diskinodes;	/**< disk inodes limit. */
	unsigned long *exptime;		/**< quot aexpiration time. */
	unsigned long *ugidlimit;	/**< userqroup quota limit. */
} dq_param;

/** Setup disk quota limits.
 *
 * @param veid		VPS id.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int vps_set_quota(envid_t veid, dq_param *dq);

/** Turn disk quota on.
 *
 * @param veid		VPS id.
 * @param private	VPS private area path.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int vps_quotaon(envid_t veid, char *private, dq_param *dq);

/** Turn disk quota off.
 *
 * @param veid		VPS id.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int vps_quotaoff(envid_t veid, dq_param *dq);

/** Disk quota managment wraper.
 *
 * @param veid		VPS id.
 * @param cmd		quota commands (QUOTA_MARKDURTY QUOTA_DROP QUOTA_STAT)
 * @return		0 on success.
 */
int quota_ctl(envid_t veid, int cmd);

/** Turn quota off.
 *
 * @param veid		VPS id.
 * @param force		forcebly turn quota off.
 * @return		0 on success.
 */
int quota_off(envid_t veid, int force);
int quota_on(envid_t veid, char *private, dq_param *param);
int quota_set(envid_t veid, char *private, dq_param *param);
int quota_init(envid_t veid, char *private, dq_param *param);
void quouta_inc(dq_param *param, int delta);
#endif
