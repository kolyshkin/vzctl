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

static char *envp_bash[] = {"HOME=/", "TERM=linux", "PATH=/bin:/sbin:/usr/bin:/usr/sbin:", NULL};

int read_script(const char *fname, char *include, char **buf)
{
	struct stat st;
	char *p = NULL;
	int  fd, len = 0;
	char *inc;

	if (!fname) {
		logger(0, 0, "read_script: file name not specified");
		return -1;
	}
	/* Read include file first */
	if (include != NULL) {
		inc = malloc(strlen(fname) + strlen(include) + 1);
		if ((p = strrchr(fname, '/')) != NULL) {
			snprintf(inc, p - fname + 2, "%s", fname);
			strcat(inc, include);
		} else {
			snprintf(inc, sizeof(inc), "%s", include);
		}
		if (stat_file(inc))
			len = read_script(inc, NULL, buf);
		if (inc != NULL) free(inc);
		if (len < 0)
			return -1;
	}
	if (stat(fname, &st)) {
		logger(0, 0, "file %s not found", fname);
		return -1;
	}
	if ((fd = open(fname, O_RDONLY)) < 0) {
		logger(0, errno, "Unable to open %s", fname);
		goto err;
	}
	if (*buf != NULL) {
		*buf = realloc(*buf, st.st_size + len + 2);
		if (*buf ==  NULL)
			goto err;
		p = *buf + len;
	} else {
		*buf = malloc(st.st_size + 2);
		if (*buf == NULL)
			goto err;
		p = *buf;
	}
	if ((len =  read(fd, p, st.st_size)) < 0) {
		logger(0, errno, "Error reading %s", fname);
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
	if (*buf != NULL)
		free(*buf);
	return -1;
}

int run_script(const char *f, char *argv[], char *envp[], int quiet)
{
	int child, pid, fd;
	int status;
	int ret;
	char *cmd;
	struct sigaction act, actold;
	int out[2];

	if (!stat_file(f)) {
		logger(0, 0, "File %s not found", f);
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
		logger(0, errno, "run_script: unable to create pipe");
		return -1;
	}
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
		execve(f, argv, envp != NULL ? envp : envp_bash);
		logger(0, errno, "Error exec %s", f);
		exit(1);
	} else if(child == -1) {
		logger(0, errno, "Unable to fork");
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
			logger(0, 0, "Received signal:"
					"  %d in %s", WTERMSIG(status), f);
	} else {
		logger(0, errno, "Error in waitpid");
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
	/* enviroment parameters*/
	snprintf(buf,  sizeof(buf), "VEID=%d", veid);
	env[0] = strdup(buf);
	snprintf(buf,  sizeof(buf), "VE_CONFFILE=%s%d.conf", VPS_CONF_DIR,
		veid);
	env[1] = strdup(buf);
	env[2] = strdup(ENV_PATH);
	env[3] = NULL;
/*
        if ((dist = get_ostmpl(gparam)) != NULL) {
                snprintf(buf,  sizeof(buf), "DIST=%s", dist);
                env = ListAddElem(env, buf, NULL, NULL);
        }
*/
        if ((ret = run_script(script, arg, env, 0)))
                ret = VZ_ACTIONSCRIPT_ERROR;
	free_arg(env);
        return ret;
}

int mk_reboot_script()
{
	char buf[STR_SIZE];
	char *rc;
	int fd;
#define REBOOT_MARK	"/reboot"
#define VZREBOOT	"S00vzreboot"
#define RC1		"/etc/rc.d/rc6.d"
#define RC2		"/etc/rc6.d"
#define REBOOT_SCRIPT	"#!/bin/bash\n>/" REBOOT_MARK

	/* remove reboot flag  */
	unlink(REBOOT_MARK);
	rc = NULL;
	if (stat_file(RC1))
		rc = RC1;
	if (stat_file(RC2))
		rc = RC2;
	if (rc == NULL)
		return 1;
	sprintf(buf, "%s/" VZREBOOT, rc);
	if ((fd = open(buf, O_CREAT|O_WRONLY|O_TRUNC, 0755)) < 0) {
		logger(0, errno, "Unable to create %s", buf);
		return 1;
	}
	write(fd, REBOOT_SCRIPT, sizeof(REBOOT_SCRIPT));
	close(fd);

	return 0;
}

#define PROC_QUOTA	"/proc/vz/vzaquota/"
#define QUOTA_U		"/aquota.user"
#define QUOTA_G		"/aquota.group"
int mk_quota_link()
{
	struct stat st;
	const char *fs;
	char buf[64];

	if (stat("/", &st)) {
		logger(0, errno, "Unable to stat /");
		return -1;
	}
	fs = vz_fs_get_name();
	/* make dev */
	snprintf(buf, sizeof(buf), "/dev/%s", fs);
	unlink(buf);
	logger(3, 0, "Setup quota dev %s", buf);
	if (mknod(buf, S_IFBLK | S_IXGRP, st.st_dev))
		logger(0, errno, "Unable to create %s", buf);
	snprintf(buf, sizeof(buf), PROC_QUOTA "%08lx" QUOTA_U,
		(unsigned long)st.st_dev);
	unlink(QUOTA_U);
	if (symlink(buf, QUOTA_U)) 
		logger(0, errno, "Unable to create symlink %s", buf); 
	snprintf(buf, sizeof(buf), PROC_QUOTA "%08lx" QUOTA_G,
		(unsigned long)st.st_dev);
	unlink(QUOTA_G);	
	if (symlink(buf, QUOTA_G))
		logger(0, errno, "Unable to create symlink %s", buf); 
	return 0;
}
