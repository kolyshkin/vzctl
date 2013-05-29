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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "types.h"
#include "quota.h"
#include "vzerror.h"
#include "util.h"
#include "script.h"
#include "logger.h"

#define VZQUOTA		"/usr/sbin/vzquota"

int is_vzquota_available(void)
{
	return (access(VZQUOTA, X_OK) == 0);
}

void quota_inc(dq_param *param, int delta)
{
	if (param->enable != YES)
		return;
	if (param->diskspace != NULL) {
		param->diskspace[0] += delta;
		param->diskspace[1] += delta;
	}
	if (param->diskinodes != NULL) {
		param->diskinodes[0] += delta;
		param->diskinodes[1] += delta;
	}
}

int is_2nd_level_quota_on(const dq_param *param)
{
	return (param->ugidlimit != NULL && *param->ugidlimit != 0);
}

int quota_set(envid_t veid, char *private, dq_param *param)
{
	int i, ret;
	char buf[64];
	char *arg[24];

	if (param->diskspace == NULL &&
		param->diskinodes == NULL &&
		param->exptime == NULL &&
		param->ugidlimit == NULL &&
		private == NULL)
	{
		return 0;
	}
	i = 0;
	arg[i++] = strdup(VZQUOTA);
	arg[i++] = strdup("setlimit");
	snprintf(buf, sizeof(buf), "%d", veid);
	arg[i++] = strdup(buf);
	if (private != NULL) {
		arg[i++] = strdup("-p");
		arg[i++] = strdup(private);
	}
	if (param->diskspace != NULL) {
		arg[i++] = strdup("-b");
		snprintf(buf, sizeof(buf), "%lu", param->diskspace[0]);
		arg[i++] = strdup(buf);
		arg[i++] = strdup("-B");
		snprintf(buf, sizeof(buf), "%lu", param->diskspace[1]);
		arg[i++] = strdup(buf);
	}
	if (param->diskinodes != NULL) {
		arg[i++] = strdup("-i");
		snprintf(buf, sizeof(buf), "%lu", param->diskinodes[0]);
		arg[i++] = strdup(buf);
		arg[i++] = strdup("-I");
		snprintf(buf, sizeof(buf), "%lu", param->diskinodes[1]);
		arg[i++] = strdup(buf);
	}
	if (param->exptime != NULL) {
		arg[i++] = strdup("-e");
		snprintf(buf, sizeof(buf), "%lu", param->exptime[0]);
		arg[i++] = strdup(buf);
		arg[i++] = strdup("-n");
		snprintf(buf, sizeof(buf), "%lu", param->exptime[0]);
		arg[i++] = strdup(buf);
	}
	/* Set ugid limit */
	if (is_2nd_level_quota_on(param)) {
		arg[i++] = strdup("-u");
		snprintf(buf, sizeof(buf), "%lu", *param->ugidlimit);
		arg[i++] = strdup(buf);
	}
	arg[i] = NULL;
	if ((ret = run_script(VZQUOTA, arg, NULL, 0))) {
		logger(-1, 0, "vzquota setlimit failed [%d]", ret);
		ret = VZ_DQ_SET;
	}
	free_arg(arg);

	return ret;
}

int quota_init(envid_t veid, char *private, dq_param *param)
{
	int i, ret;
	char buf[64];
	char *arg[24];

	if (check_var(private,
		"Error: Not enough parameters, private not set"))
	{
		return VZ_FS_NOPRVT;
	}
	if (check_var(param->diskspace,
		"Error: Not enough parameters, diskspace quota not set"))
	{
		return VZ_DISKSPACE_NOT_SET;
	}
	if (check_var(param->diskinodes,
		"Error: Not enough parameters, diskinodes quota not set"))
	{
		return VZ_DISKINODES_NOT_SET;
	}
	i = 0;
	arg[i++] = strdup(VZQUOTA);
	arg[i++] = strdup("init");
	snprintf(buf, sizeof(buf), "%d", veid);
	arg[i++] = strdup(buf);
	/* Disk space */
	arg[i++] = strdup("-b");
	snprintf(buf, sizeof(buf), "%lu", param->diskspace[0]);
	arg[i++] = strdup(buf);
	arg[i++] = strdup("-B");
	snprintf(buf, sizeof(buf), "%lu", param->diskspace[1]);
	arg[i++] = strdup(buf);
	/* Disk inodes */
	arg[i++] = strdup("-i");
	snprintf(buf, sizeof(buf), "%lu", param->diskinodes[0]);
	arg[i++] = strdup(buf);
	arg[i++] = strdup("-I");
	snprintf(buf, sizeof(buf), "%lu", param->diskinodes[1]);
	arg[i++] = strdup(buf);
	/* CT private */
	arg[i++] = strdup("-p");
	arg[i++] = strdup(private);
	/* Expiration time */
	arg[i++] = strdup("-e");
	snprintf(buf, sizeof(buf), "%lu",
		param->exptime == NULL ? QUOTA_EXPTIME : param->exptime[0]);
	arg[i++] = strdup(buf);
	arg[i++] = strdup("-n");
	arg[i++] = strdup(buf);
	/* ugid limit */
	arg[i++] = strdup("-s");
	if (is_2nd_level_quota_on(param)) {
		arg[i++] = strdup("1");
		arg[i++] = strdup("-u");
		snprintf(buf, sizeof(buf), "%lu", *param->ugidlimit);
		arg[i++] = strdup(buf);
	} else
		arg[i++] = strdup("0");
	arg[i] = NULL;
	if ((ret = run_script(VZQUOTA, arg, NULL, 0))) {
		logger(-1, 0, "vzquota init failed [%d]", ret);
		ret = VZ_DQ_INIT;
	}
	free_arg(arg);

	return ret;
}

/** Turn disk quota on.
 *
 * @param veid		CT ID.
 * @param private	CT private area path.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int quota_on(envid_t veid, char *private, dq_param *param)
{
	int i, ret, ret2 = 0, tried = 0;
	char buf[64];
	char *arg[24];

	if (check_var(param->diskspace,
		"Error: Not enough parameters, diskspace quota not set"))
	{
		return VZ_DISKSPACE_NOT_SET;
	}
	if (check_var(param->diskinodes,
		"Error: Not enough parameters, diskinodes quota not set"))
	{
		return VZ_DISKINODES_NOT_SET;
	}
	i = 0;
	arg[i++] = strdup(VZQUOTA);
	arg[i++] = strdup("on");
	snprintf(buf, sizeof(buf), "%d", veid);
	arg[i++] = strdup(buf);
	/* Disk space */
	arg[i++] = strdup("-b");
	snprintf(buf, sizeof(buf), "%lu", param->diskspace[0]);
	arg[i++] = strdup(buf);
	arg[i++] = strdup("-B");
	snprintf(buf, sizeof(buf), "%lu", param->diskspace[1]);
	arg[i++] = strdup(buf);
	/* Disk inodes */
	arg[i++] = strdup("-i");
	snprintf(buf, sizeof(buf), "%lu", param->diskinodes[0]);
	arg[i++] = strdup(buf);
	arg[i++] = strdup("-I");
	snprintf(buf, sizeof(buf), "%lu", param->diskinodes[1]);
	arg[i++] = strdup(buf);
	/* Expiration time */
	arg[i++] = strdup("-e");
	snprintf(buf, sizeof(buf), "%lu",
		param->exptime == NULL ? QUOTA_EXPTIME : param->exptime[0]);
	arg[i++] = strdup(buf);
	arg[i++] = strdup("-n");
	arg[i++] = strdup(buf);
	/* ugid limit */
	arg[i++] = strdup("-s");
	if (is_2nd_level_quota_on(param)) {
		arg[i++] = strdup("1");
		arg[i++] = strdup("-u");
		snprintf(buf, sizeof(buf), "%lu", *param->ugidlimit);
		arg[i++] = strdup(buf);
	} else
		arg[i++] = strdup("0");
	arg[i] = 0;

retry:
	if ((ret = run_script(VZQUOTA, arg, NULL, 0))) {
		switch (ret) {
		case EXITCODE_QUOTNOTEXIST:
			if (!tried) {
				tried++;
				/* Initialize the quota and retry */
				ret2 = quota_init(veid, private, param);
				if (!ret2)
					goto retry;
			}
			break;
		case EXITCODE_QUOTARUN:
			/* Ignore the error, run quota set instead */
			ret = 0;
			ret2 = quota_set(veid, private, param);
			break;
		}
	}
	free_arg(arg);

	if (ret2)
		return ret2;

	if (ret) {
		logger(-1, 0, "vzquota on failed [%d]", ret);
		return VZ_DQ_ON;
	}

	return 0;
}

/** Turn disk quota off.
 *
 * @param veid		CT ID.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int quota_off(envid_t veid, int force)
{
	int i, ret;
	char buf[64];
	char *arg[5];

	i = 0;
	arg[i++] = strdup(VZQUOTA);
	arg[i++] = strdup("off");
	snprintf(buf, sizeof(buf), "%d", veid);
	arg[i++] = strdup(buf);
	if (force)
		arg[i++] = strdup("-f");
	arg[i] = NULL;
	if ((ret = run_script(VZQUOTA, arg, NULL, 0))) {
		if (ret != EXITCODE_QUOTANOTRUN) {
			logger(-1, 0, "vzquota off failed [%d]", ret);
			ret = VZ_DQ_OFF;
		}
		else
			ret = 0;
	}
	free_arg(arg);

	return ret;
}

/** Disk quota management wrapper.
 *
 * @param veid		CT ID.
 * @param cmd		quota commands (QUOTA_MARKDIRTY QUOTA_DROP QUOTA_STAT)
 * @return		0 on success.
 */
int quota_ctl(envid_t veid, int cmd)
{
	int i, ret;
	char buf[64];
	char *arg[6];
	int quiet = 0;

	i = 0;
	arg[i++] = strdup(VZQUOTA);
	switch (cmd) {
	case QUOTA_MARKDIRTY:
		arg[i++] =strdup( "setlimit");
		snprintf(buf, sizeof(buf), "%d", veid);
		arg[i++] = strdup(buf);
		arg[i++] = strdup("-f");
		quiet = 1;
		break;
	case QUOTA_DROP	:
		arg[i++] = strdup("drop");
		snprintf(buf, sizeof(buf), "%d", veid);
		arg[i++] = strdup(buf);
		quiet = 1;
		break;
	case QUOTA_STAT :
		arg[i++] = strdup("stat");
		snprintf(buf, sizeof(buf), "%d", veid);
		arg[i++] = strdup(buf);
		arg[i++] = strdup("-f");
		quiet = 1;
		break;
	case QUOTA_STAT2:
		arg[i++] = strdup("stat");
		snprintf(buf, sizeof(buf), "%d", veid);
		arg[i++] = strdup(buf);
		arg[i++] = strdup("-f");
		arg[i++] = strdup("-t");
		quiet = 1;
		break;
	case QUOTA_SHOW:
		arg[i++] = strdup("show");
		snprintf(buf, sizeof(buf), "%d", veid);
		arg[i++] = strdup(buf);
		quiet = 1;
		break;
	default	:
		logger(-1, 0, "quota_ctl: Unknown action %d", cmd);
		return VZ_SYSTEM_ERROR;
	}
	arg[i] = NULL;
	ret = run_script(VZQUOTA, arg, NULL, quiet);
	free_arg(arg);

	return ret;
}

/** Setup disk quota limits.
 *
 * @param veid		CT ID.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int vps_set_quota(envid_t veid, dq_param *dq)
{
	int ret;
	unsigned long *tmp_ugidlimit = NULL;

	if (dq->enable == NO)
		return 0;
	if (dq->diskspace == NULL &&
		dq->diskinodes == NULL &&
		dq->exptime == NULL &&
		dq->ugidlimit == NULL)
	{
		return 0;
	}
	if (quota_ctl(veid, QUOTA_STAT)) {
		logger(-1, 0, "Error: Unable to apply new quota values:"
			" quota not running");
		return VZ_DQ_SET;
	}
	if (dq->ugidlimit != NULL) {
		ret = quota_ctl(veid, QUOTA_STAT2);
		if (ret == EXITCODE_UGID_NOTRUN && *dq->ugidlimit) {
			logger(-1, 0, "Unable to apply new quota values:"
				" ugid quota not initialized");
			return VZ_DQ_UGID_NOTINITIALIZED;
		} else if (!ret && !*dq->ugidlimit) {
			logger(-1, 0, "WARNING: Unable to turn ugid quota"
				" off. New parameters will be applied"
				" during the next start");
			tmp_ugidlimit = dq->ugidlimit;
			dq->ugidlimit = NULL;
		}
	}
	ret = quota_set(veid, NULL, dq);
	if (tmp_ugidlimit != NULL)
		dq->ugidlimit = tmp_ugidlimit;

	return ret;
}

/** Turn disk quota on.
 *
 * @param veid		CT ID.
 * @param private	CT private area path.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int vps_quotaon(envid_t veid, char *private, dq_param *dq)
{
	int ret;

	if (dq == NULL)
		return 0;
	if (dq->enable == NO) {
//		quota_ctl(veid, QUOTA_MARKDIRTY);
		return 0;
	} else {
		if (quota_ctl(veid, QUOTA_SHOW) == EXITCODE_QUOTNOTEXIST) {
			logger(0, 0, "Initializing quota ...");
			if ((ret = quota_init(veid, private, dq)))
				return ret;
		}
	}
	if ((ret = quota_on(veid, private, dq)))
		return ret;
	return 0;
}

/** Turn disk quota off.
 *
 * @param veid		CT ID.
 * @param dq		disk quota parameters.
 * @return		0 on success.
 */
int vps_quotaoff(envid_t veid, dq_param *dq)
{
	if (dq == NULL)
		return 0;
	if (dq->enable == NO)
		return 0;
	return quota_off(veid, 0);
}
