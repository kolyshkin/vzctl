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
#ifndef	_EXEC_H_
#define	_EXEC_H_


#include "types.h"

#define SCRIPT_EXEC_TIMEOUT	5 * 60

/** Exec modes.
 */
enum {
	MODE_EXEC,	/**< use exec syscall. */
	MODE_BASH,	/**< exec bash, and put command on stdin. */
};

int execvep(const char *path, char *const argv[], char *const envp[]);

/** Execute command inside CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param root		CT root.
 * @param exec_mode	execution mode (MODE_EXEC, MODE_BASH).
 * @param arg		argv array.
 * @param envp		command environment array.
 * @param std_i		read command from buffer stdin point to.
 * @param timeout	execution timeout, 0 - unlimited.
 * @return		0 on success.
 */
int vps_exec(vps_handler *h, envid_t veid, const char *root, int exec_mode,
	char *argv[], char *const envp[], char *std_in, int timeout);

/** Read script and execute it in CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param root		CT root.
 * @param arg		argv array.
 * @param envp		command environment array.
 * @param fname		script file name
 * @paran func		function file name
 * @param timeout	execution timeout, 0 - unlimited.
 * @return		0 on success.
 */
int vps_exec_script(vps_handler *h, envid_t veid, const char *root,
	char *argv[], char *const envp[], const char *fname, char *func,
	int timeout);

int vps_execFn(vps_handler *h, envid_t veid, const char *root,
		execFn fn, void *data, int flags);

int env_wait(int pid);

struct vps_param;
int vps_run_script(vps_handler *h, envid_t veid, char *script,
	struct vps_param *vps_p);
#endif
