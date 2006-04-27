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
#ifndef	_ENV_H_
#define _ENV_H_

#include "types.h"
#include "res.h"
#include "modules.h"

/** Shutdown timeout.
 */
#define MAX_SHTD_TM		120
#define MIN_NUMIPTENT		16
/* default cpu units values */
#define LHTCPUUNITS		250
#define UNLCPUUNITS		1000
#define HNCPUUNITS		1000



typedef int (*env_create_FN)(vps_handler *h, envid_t veid, int wait_p,
	int err_p, void *data);

/** Stop modes.
 */
enum {
	M_HALT,		/**< stop by halt. */
	M_REBOOT,	/**< stop by reboot. */
	M_KILL,		/**< stop by SIGTERM. */
};

/** Allocate and inittialize VPS handler.
 *
 * @param veid		VPS id.
 * @return		handler or NULL on error.
 */
vps_handler *vz_open(envid_t veid);

/** Close VPS handler.
 *
 * @param h		VPS handler.
 */
void vz_close(vps_handler *h);

/** Get VPS status.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @return		1 - VPS is running
 *			0 - VPS is stopped.
 */
int vps_is_run(vps_handler *h, envid_t veid);

/** Sart VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param param		VPS resourses.
 * @param skip		flags to skip VPS setup (SKIP_SETUP|SKIP_ACTION_SCRIPT)
 * @param action	modules list, used to call setup() callback
 * @return		0 on success.
 */
int vps_start(vps_handler *h, envid_t veid, vps_param *param,
	skipFlags skip, struct mod_action *action);
	
int vps_start_custom(vps_handler *h, envid_t veid, vps_param *param,
	skipFlags skip, struct mod_action *mod,
	env_create_FN fn, void *data);

/** Stop VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param param		VPS resourses.
 * @param stop_mode	stop mode one of (M_REBOOT M_HALT M_KILL).
 * @param skip		flags to skip run action script (SKIP_ACTION_SCRIPT)
 * @param action	modules list, used to call cleanup() callback.
 * @return		0 on success.
 */
int vps_stop(vps_handler *h, envid_t veid, vps_param *param, int stop_mode,
	skipFlags skip, struct mod_action *action);

/** Change root to specified directory
 *
 * @param		VPS root
 * @return		0 on success
 */
int vz_chroot(char *root);
int vz_env_create(vps_handler *h, envid_t veid, vps_res *res,
	int wait_p[2], int err_p[2], env_create_FN fn, void *data);
int vz_setluid(envid_t veid);
int vz_env_create_ioctl(vps_handler *h, envid_t veid, int flags);
int vps_restart(vps_handler *h, envid_t veid, vps_param *param);
#endif
