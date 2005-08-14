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
#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>
#include "types.h"

#define	LOG_DATA	log_param g_log = {NULL, 0, 1, 0, "", 0};

/** Data structure for logging.
 */
typedef struct {
	FILE *fp;		/**< log file pointer. */
	int level;		/**< maximum logging level. */
	int enable;		/**< enable/disable logging. */
	int quiet;		/**< skip logging to stdout. */
	char prog[32];		/**< program name. */
	envid_t veid;
} log_param;

/** Print message to log file & stdout.
 *
 * @param log_level		message severity.
 * @param err_num		errno
 * @param format		fprintf format.
 */
void logger(int log_level, int err_num, const char *format, ...);

/** Change/set log file.
 *
 * @param file		file name.
 * @return		0 on success.
 */
int set_log_file(char *file);

/** Initialize logging.
 * 
 * @param file		log file.
 * @param veid		VPS id.
 * @param enable	enable/disable logging.
 * @param level		maximum logging level.
 * @param quiet		skip logging to stdout.
 * @param progname	program name.
 */
int init_log(char *file, envid_t veid, int enable, int level, int quiet,
	char *progname);

/** Close logging.
 */
void free_log();

void set_log_level(int level);

#endif
