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
#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "types.h"

/** Print message to log file & stdout.
 *
 * @param log_level		message severity.
 * @param err_num		errno
 * @param format		fprintf format.
 */
void logger(int log_level, int err_num, const char *format, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));

#define vzctl_err(ret, err_num, format, ...)	\
	({ logger(-1, err_num, format, ##__VA_ARGS__); ret; })

/** Change/set log file.
 *
 * @param file		file name.
 * @return		0 on success.
 */
int set_log_file(char *file);

/** Initialize logging.
 *
 * @param file		log file.
 * @param veid		CT ID.
 * @param enable	enable/disable logging.
 * @param level		maximum logging level.
 * @param quiet		skip logging to stdout/stderr.
 * @param progname	program name.
 */
int init_log(char *file, envid_t veid, int enable, int level, int quiet,
	const char *progname);

/** Close logging.
 */
void free_log();

void set_log_level(int level);
void set_log_verbose(int level);
void set_log_ctid(envid_t id);
void set_log_quiet(int quiet);
void vzctl_init_ploop_log(void);

#endif
