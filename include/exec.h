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

/** Execute command inside VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param root		VPS root.
 * @param exec_mode	execution mode (MODE_EXEC, MODE_BASH).
 * @param arg		argv array.
 * @param envp		command environment array.
 * @param std_i		read command from buffer stdin point to.
 * @param timeout	execution timeout, 0 - unlimited.
 * @return		0 on success.
 */
int vps_exec(vps_handler *h, envid_t veid, char *root, int exec_mode,
        char *const argv[], char *const envp[], char *std_in, int timeout);

/** Read script and execute it in VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param root		VPS root.
 * @param arg		argv array.
 * @param envp		command environment array.
 * @param fname		script file name
 * @paran func		function file name
 * @param timeout	execution timeout, 0 - unlimited.
 * @return		0 on success.
 */
int vps_exec_script(vps_handler *h, envid_t veid, char *root,
	char *const argv[], char *const envp[], const char *fname, char *func,
	int timeout);
#endif
