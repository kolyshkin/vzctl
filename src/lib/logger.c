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

	if (log_level < 0)
		out = stderr;
	else
		out = stdout;
	va_start(ap, format);
	if (!g_log.quiet && g_log.verbose >= log_level) {
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
			fprintf(g_log.fp, "VE %d : ", g_log.veid);
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

void set_log_verbose(int level)
{
	g_log.verbose = level;
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
	set_log_verbose(level);
	g_log.veid = veid;
	g_log.quiet = quiet;
	if (progname != NULL)
		snprintf(g_log.prog, sizeof(g_log.prog), progname);
	else
		g_log.prog[0] = 0;

	return 0;
}
