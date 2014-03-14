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
#ifdef HAVE_PLOOP
#include <ploop/libploop.h>
#include "image.h"
#endif

#include "types.h"
#include "logger.h"

/** Data structure for logging.
 */
typedef struct {
	FILE *fp;		/**< log file pointer. */
	int level;		/**< maximum logging level. */
	int enable;		/**< enable/disable logging. */
	int quiet;		/**< skip logging to stdout/stderr. */
	int verbose;		/**< Console verbosity. */
	char prog[32];		/**< program name. */
	envid_t veid;		/**< Container ID (CTID). */
	char *file;		/**< log file name */
} log_param;

log_param g_log = {NULL, 0, 1, 0, 0, "", 0, NULL};

#ifdef HAVE_PLOOP
static int ploop_log = 0;
#endif

static inline void get_date(char *buf, int len)
{
	time_t ptime = time(NULL);
	struct tm *p_tm_time = localtime(&ptime);

	strftime(buf, len, "%Y-%m-%dT%T%z", p_tm_time);
}

void logger(int log_level, int err_no, const char *format, ...)
{
	va_list ap;
	int tmp_errno = errno;

	va_start(ap, format);
	if (!g_log.quiet && g_log.verbose >= log_level) {
		FILE *out = (log_level < 0) ? stderr : stdout;
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
		char date[64];

		get_date(date, sizeof(date));
		fprintf(g_log.fp, "%s %s : ", date, g_log.prog);
		if (g_log.veid)
			fprintf(g_log.fp, "CT %d : ", g_log.veid);
		vfprintf(g_log.fp, format, ap);
		if (err_no)
			fprintf(g_log.fp, ": %s", strerror(err_no));
		fprintf(g_log.fp, "\n");
		fflush(g_log.fp);
	}
	va_end(ap);
	errno = tmp_errno;
}

int set_log_file(char *file)
{
	FILE *fp;

	if (g_log.fp != NULL) {
		fclose(g_log.fp);
		g_log.fp = NULL;
	}
	if (g_log.file) {
		free(g_log.file);
		g_log.file = NULL;
	}
	if (file != NULL) {
		if ((fp = fopen(file, "a")) == NULL)
			return -1;
		g_log.fp = fp;
		g_log.file = strdup(file);
	}
#ifdef HAVE_PLOOP
	if (ploop_log)
		ploop.set_log_file(file);
#endif
	return 0;
}

void free_log()
{
	if (g_log.fp  != NULL)
		fclose(g_log.fp);
	free(g_log.file);
	memset(&g_log, 0, sizeof(g_log));
}

void set_log_level(int level)
{
	g_log.level = level;
#ifdef HAVE_PLOOP
	if (ploop_log)
		ploop.set_log_level(level);
#endif
}

void set_log_verbose(int level)
{
	g_log.verbose = level;
#ifdef HAVE_PLOOP
	if (ploop_log)
		ploop.set_verbose_level(g_log.quiet ?
				PLOOP_LOG_NOCONSOLE : level);
#endif
}

void set_log_ctid(envid_t id) {
	g_log.veid = id;
}

void set_log_quiet(int quiet) {
	g_log.quiet = quiet;
#ifdef HAVE_PLOOP
	if (ploop_log)
		ploop.set_verbose_level(g_log.quiet ?
				PLOOP_LOG_NOCONSOLE : g_log.verbose);
#endif
}

int init_log(char *file, envid_t veid, int enable, int level, int quiet,
	const char *progname)
{
	int ret = 0;

	free_log();
	if ((ret = set_log_file(file)))
		return ret;
	g_log.enable = enable;
	g_log.veid = veid;
	g_log.quiet = quiet;
	set_log_level(level);
	set_log_verbose(level);
	if (progname != NULL)
		snprintf(g_log.prog, sizeof(g_log.prog), "%s", progname);
	else
		g_log.prog[0] = 0;

	return 0;
}

#ifdef HAVE_PLOOP
void vzctl_init_ploop_log(void)
{
	ploop.set_log_file(g_log.file);
	ploop.set_log_level(g_log.level);
	ploop.set_verbose_level(g_log.quiet ?
			PLOOP_LOG_NOCONSOLE : g_log.verbose);
	ploop_log = 1;
}
#endif
