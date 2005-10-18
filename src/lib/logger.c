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

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>

#include "types.h"
#include "logger.h"

extern log_param g_log;

static inline void get_date(char *buf, int len)
{
	struct tm *p_tm_time;
	time_t ptime;

	ptime = time(NULL);
	p_tm_time = localtime(&ptime);
	strftime(buf, len, "%Y-%m-%dT%T%z", p_tm_time);
}

void logger(int log_level, int err_no, const char *format, ...)
{
	va_list ap;
	FILE *out;
	char date[64];

	if (!log_level)
		out = stderr;
	else
		out = stdout;
	va_start(ap, format);
	if (!g_log.quiet && g_log.level >= log_level) {
		va_list ap_save;
		
		va_copy(ap_save, ap);
		vfprintf(out, format, ap_save);
		va_end(ap_save);
		if (err_no)
			fprintf(out, ": %s", strerror(err_no));
		fprintf(out, "\n");
		fflush(out);
	}
	if (g_log.fp != NULL && g_log.level >= log_level) {

		get_date(date, sizeof(date));

		fprintf(g_log.fp, "%s %s : ", date, g_log.prog);
		if (g_log.veid)
			fprintf(g_log.fp, "VPS %d : ", g_log.veid);
		vfprintf(g_log.fp, format, ap);
		if (err_no) 
			fprintf(g_log.fp, ": %s", strerror(err_no));
		fprintf(g_log.fp, "\n");
		fflush(g_log.fp);
	}
	va_end(ap);
}

int set_log_file(char *file)
{
	FILE *fp;
	
	if (g_log.fp != NULL) {
		fclose(g_log.fp);
		g_log.fp = NULL;
	}
	if (file != NULL) {
		if ((fp = fopen(file, "a")) == NULL) 
			return -1;
		g_log.fp = fp;
	}
	return 0;
}

void free_log()
{
	if (g_log.fp  != NULL)
		fclose(g_log.fp);
	memset(&g_log, 0, sizeof(g_log));
}

void set_log_level(int level)
{
	 g_log.level = level;
}

int init_log(char *file, envid_t veid, int enable, int level, int quiet,
	char *progname)
{
	int ret = 0;

	free_log();
	if ((ret = set_log_file(file)))
		return ret;
	g_log.enable = enable;
	set_log_level(level);
	g_log.veid = veid;
	g_log.quiet = quiet;
	if (progname != NULL)
		snprintf(g_log.prog, sizeof(g_log.prog), progname);
	else
		g_log.prog[0] = 0;

	return 0;
}
