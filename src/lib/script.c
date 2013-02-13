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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>

#include "types.h"
#include "util.h"
#include "list.h"
#include "logger.h"
#include "vzerror.h"
#include "util.h"
#include "script.h"
#include "fs.h"
#include "dev.h"

volatile sig_atomic_t alarm_flag;
static char *envp_bash[] = {"HOME=/", "TERM=linux", ENV_PATH, NULL};

int read_script(const char *fname, char *include, char **buf)
{
	struct stat st;
	char *tmp, *p = NULL;
	int  fd, len = 0;
	char *inc;

	if (!fname) {
		logger(-1, 0, "read_script: file name not specified");
		return -1;
	}
	/* Read include file first */
	if (include != NULL) {
		inc = vz_malloc(strlen(fname) + strlen(include) + 1);
		if (!inc)
			return -1;
		if ((p = strrchr(fname, '/')) != NULL) {
			snprintf(inc, p - fname + 2, "%s", fname);
			strcat(inc, include);
		} else {
			snprintf(inc, sizeof(inc), "%s", include);
		}
		if (stat_file(inc))
			len = read_script(inc, NULL, buf);
		free(inc);
		if (len < 0)
			return -1;
	}
	if (stat(fname, &st)) {
		logger(-1, 0, "file %s not found", fname);
		return -1;
	}
	if ((fd = open(fname, O_RDONLY)) < 0) {
		logger(-1, errno, "Unable to open %s", fname);
		goto err;
	}
	if (*buf != NULL) {
		tmp = realloc(*buf, st.st_size + len + 2);
		if (tmp == NULL)
			goto err;
		*buf = tmp;
		p = *buf + len;
	} else {
		*buf = malloc(st.st_size + 2);
		if (*buf == NULL)
			goto err;
		p = *buf;
	}
	if ((len = read(fd, p, st.st_size)) < 0) {
		logger(-1, errno, "Error reading %s", fname);
		goto err;
	}
	p += len;
	p[0] = '\n';
	p[1] = 0;
	close(fd);

	return len;
err:
	if (fd > 0)
		close(fd);
	free(*buf);
	return -1;
}

#define ENV_SIZE	256
int run_script(const char *f, char *argv[], char *env[], int quiet)
{
	int child, pid, fd;
	int status;
	int ret, i, j;
	char *cmd;
	struct sigaction act, actold;
	int out[2];
	char *envp[ENV_SIZE];

	if (!stat_file(f)) {
		logger(-1, 0, "File %s not found", f);
		return VZ_NOSCRIPT;
	}
	sigaction(SIGCHLD, NULL, &actold);
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_DFL;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	cmd = arg2str(argv);
	if (cmd != NULL) {
		logger(2, 0, "Running: %s", cmd);
		free(cmd);
	}
	if (quiet && pipe(out) < 0) {
		logger(-1, errno, "run_script: unable to create pipe");
		ret = VZ_RESOURCE_ERROR;
		goto err;
	}
	i = 0;
	for (i = 0; i < ENV_SIZE - 1 && envp_bash[i] != NULL; i++)
			envp[i] = envp_bash[i];
	if (env != NULL)
		for (j = 0; i < ENV_SIZE - 1 && env[j] != NULL; i++, j++)
			envp[i] = env[j];
	envp[i] = NULL;
	if ((child = fork()) == 0) {
		fd = open("/dev/null", O_WRONLY);
		if (fd < 0)
			close(0);
		else
			dup2(fd, 0);

		if (quiet) {
			dup2(fd, 1);
			dup2(fd, 2);
		} else {
/*
			dup2(out[1], STDOUT_FILENO);
			dup2(out[1], STDERR_FILENO);
			close(out[0]);
			close(out[1]);
*/
		}
		execve(f, argv, envp);
		logger(-1, errno, "Error exec %s", f);
		exit(1);
	} else if (child == -1) {
		logger(-1, errno, "Unable to fork");
		ret = VZ_RESOURCE_ERROR;
		goto err;
	}
	while ((pid = waitpid(child, &status, 0)) == -1)
		if (errno != EINTR)
			break;
	ret = VZ_SYSTEM_ERROR;
	if (pid == child) {
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			logger(-1, 0, "Received signal:"
					"  %d in %s", WTERMSIG(status), f);
	} else {
		logger(-1, errno, "Error in waitpid");
	}
err:
	sigaction(SIGCHLD, &actold, NULL);

	return ret;
}

int run_pre_script(int veid, char *script)
{
	char *arg[2];
	char *env[4];
	char buf[STR_SIZE];
	int ret;

	if (!stat_file(script))
		return 0;
	/* cmd parameters */
	arg[0] = script;
	arg[1] = NULL;
	/* environment parameters*/
	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	env[0] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_CONFFILE=%s%d.conf", VPS_CONF_DIR, veid);
	env[1] = strdup(buf);
	env[2] = strdup(ENV_PATH);
	env[3] = NULL;

	if ((ret = run_script(script, arg, env, 0)))
		ret = VZ_ACTIONSCRIPT_ERROR;
	free_arg(env);
	return ret;
}

#define PROC_QUOTA	"/proc/vz/vzaquota/"
#define QUOTA_U		"/aquota.user"
#define QUOTA_G		"/aquota.group"
static int mk_vzquota_link()
{
	struct stat st;
	const char *fs;
	char buf[64];

	if (stat("/", &st)) {
		logger(-1, errno, "Unable to stat /");
		return -1;
	}
	fs = vz_fs_get_name();
	/* make dev */
	create_static_dev(NULL, fs, "root", S_IFBLK|S_IXGRP, st.st_dev);

	snprintf(buf, sizeof(buf), PROC_QUOTA "%08lx" QUOTA_U,
		(unsigned long)st.st_dev);
	unlink(QUOTA_U);
	if (symlink(buf, QUOTA_U))
		logger(-1, errno, "Unable to create symlink %s", buf);
	snprintf(buf, sizeof(buf), PROC_QUOTA "%08lx" QUOTA_G,
		(unsigned long)st.st_dev);
	unlink(QUOTA_G);
	if (symlink(buf, QUOTA_G))
		logger(-1, errno, "Unable to create symlink %s", buf);
	return 0;
}

int setup_env_quota(const struct setup_env_quota_param *param)
{
	if (param == NULL)
		return -1;
	if (param->dev_name[0] == '\0') /* simfs/vzquota */
		return mk_vzquota_link();
	/* ploop */
	if (create_static_dev(NULL, param->dev_name, "root",
				S_IFBLK|S_IXGRP, param->dev))
		return -1;
	return system("quotaon -a");
}

#define INITTAB_FILE		"/etc/inittab"
#define INITTAB_VZID		"vz:"
#define INITTAB_ACTION		INITTAB_VZID "2345:once:touch " VZFIFO_FILE "\n"

#define EVENTS_DIR		"/etc/event.d/"
#define EVENTS_FILE		EVENTS_DIR "vz_init_done"
#define EVENTS_SCRIPT	\
	"# This task runs if default runlevel is reached\n"	\
	"# Added by OpenVZ vzctl\n"				\
	"start on stopped rc2\n"				\
	"start on stopped rc3\n"				\
	"start on stopped rc4\n"				\
	"start on stopped rc5\n"				\
	"exec touch " VZFIFO_FILE "\n"

/* Ubuntu 9.10 needs different machinery */
#define EVENTS_DIR_UBUNTU	"/etc/init/"
#define EVENTS_FILE_UBUNTU	EVENTS_DIR_UBUNTU "vz_init_done.conf"
#define EVENTS_SCRIPT_UBUNTU	\
	"# tell vzctl that start was successfull\n"			\
	"#\n"								\
	"# This task is to tell vzctl that start was successfull\n"	\
	"\n"								\
	"description	\"tell vzctl that start was successfull\"\n"	\
	"\n"								\
	"start on stopped rc RUNLEVEL=[2345]\n"				\
	"\n"								\
	"task\n"							\
	"\n"								\
	"exec touch " VZFIFO_FILE "\n"

#define MAX_WAIT_TIMEOUT	60 * 60

int add_reach_runlevel_mark()
{
	int fd, found, ret;
	ssize_t len, w;
	char buf[4096];
	struct stat st;

	unlink(VZFIFO_FILE);
	if (mkfifo(VZFIFO_FILE, 0644)) {
		fprintf(stderr, "Unable to create " VZFIFO_FILE " %s\n",
			strerror(errno));
		return -1;
	}
	/* Create upstart specific script */
	if (!stat(EVENTS_DIR_UBUNTU, &st)) {
		fd = open(EVENTS_FILE_UBUNTU, O_WRONLY|O_TRUNC|O_CREAT, 0644);
		if (fd == -1) {
			fprintf(stderr, "Unable to create "
				EVENTS_FILE_UBUNTU ": %s\n",
				strerror(errno));
			return -1;
		}
		len = sizeof(EVENTS_SCRIPT_UBUNTU) - 1;
		w = write(fd, EVENTS_SCRIPT_UBUNTU, len);
		close(fd);
		if (len != w) {
			fprintf(stderr, "Error writing "
					EVENTS_FILE_UBUNTU ": %s\n",
					strerror(errno));
			return -1;
		}
		return 0;
	} else if (!stat(EVENTS_DIR, &st)) {
		fd = open(EVENTS_FILE, O_WRONLY|O_TRUNC|O_CREAT, 0644);
		if (fd == -1) {
			fprintf(stderr, "Unable to create " EVENTS_FILE
				": %s\n", strerror(errno));
			return -1;
		}
		len = sizeof(EVENTS_SCRIPT) - 1;
		w = write(fd, EVENTS_SCRIPT, len);
		close(fd);
		if (len != w) {
			fprintf(stderr, "Error writing " EVENTS_FILE ": %s\n",
					strerror(errno));
			return -1;
		}
		return 0;
	}
	/* Add a line to /etc/inittab */
	if ((fd = open(INITTAB_FILE, O_RDWR | O_APPEND)) == -1) {
		fprintf(stderr, "Unable to open " INITTAB_FILE " %s\n",
			strerror(errno));
		return -1;
	}
	ret = 0;
	found = 0;
	while (1) {
		len = read(fd, buf, sizeof(buf));
		if (len == 0)
			break;
		if (len < 0) {
			fprintf(stderr, "Unable to read from " INITTAB_FILE
				" %s\n",
				strerror(errno));
			ret = -1;
			break;
		}
		buf[len] = 0;
		if (strstr(buf, "\n" INITTAB_VZID) != NULL) {
			found = 1;
			break;
		}
	}
	if (!found) {
		if (write(fd, INITTAB_ACTION, sizeof(INITTAB_ACTION) - 1) == -1)
		{
			fprintf(stderr, "Unable to write to " INITTAB_FILE
				" %s\n",
				strerror(errno));
			ret = -1;
		}
	}
	close(fd);
	return ret;
}

static void alarm_handler(int sig)
{
	alarm_flag = 1;
}

int wait_on_fifo(void *data)
{
	int fd, buf, ret;
	struct sigaction act, actold;

	ret = 0;
	alarm_flag = 0;
	act.sa_flags = 0;
	act.sa_handler = alarm_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGALRM, &act, &actold);

	alarm(MAX_WAIT_TIMEOUT);
	fd = open(VZFIFO_FILE, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Unable to open " VZFIFO_FILE " %s\n",
			strerror(errno));
		ret = -1;
		goto err;
	}
	if (read(fd, &buf, sizeof(buf)) == -1)
		ret = -1;
err:
	if (alarm_flag)
		ret = VZ_EXEC_TIMEOUT;
	alarm(0);
	sigaction(SIGALRM, &actold, NULL);
	unlink(VZFIFO_FILE);
	return ret;
}
