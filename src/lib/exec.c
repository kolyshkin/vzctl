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

#include <sys/ioctl.h>
#include <linux/vzcalluser.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "vzerror.h"
#include "exec.h"
#include "env.h"
#include "util.h"
#include "logger.h"
#include "script.h"

static volatile sig_atomic_t alarm_flag, child_exited;
static char *envp_bash[] = {"HOME=/", "TERM=linux", ENV_PATH, NULL};

int execvep(const char *path, char *const argv[], char *const envp[])
{
	const char *p, *p2;

	if (strchr(path, '/'))
		return execve(path, argv, envp);

	for (p = DEF_PATH; p && *p; p = p2) {
		char partial[FILENAME_MAX];
		size_t res;

		p2 = strchr(p, ':');
		if (p2) {
			size_t len = p2 - p;

			strncpy(partial, p, len);
			partial[len] = 0;
			p2++;
		} else {
			strcpy(partial, p);
		}
		if (strlen(partial))
			vz_strlcat(partial, "/", sizeof(partial));

		res = vz_strlcat(partial, path, sizeof(partial));
		if (res >= sizeof(partial)) {
			errno = ENAMETOOLONG;
			break;
		}

		execve(partial, argv, envp ? envp : envp_bash);

		if (errno != ENOENT)
			break;
	}
	return -1;
}

static int stdredir(int rdfd, int wrfd)
{
	int lenr, lenw, lentotal, lenremain;
	char buf[10240];
	char *p;

	if ((lenr = read(rdfd, buf, sizeof(buf)-1)) > 0) {
		lentotal = 0;
		lenremain = lenr;
		p = buf;
		while (lentotal < lenr) {
			if ((lenw = write(wrfd, p, lenremain)) < 0) {
				if (errno != EINTR)
					return -1;
			} else {
				lentotal += lenw;
				lenremain -= lenw;
				p += lenw;
			}
		}
	} else if (lenr == 0){
		return -1;
	} else {
		if (errno == EAGAIN)
			return 1;
		else if (errno != EINTR)
			return -1;
	}
	return 0;
}

static void exec_handler(int sig)
{
	child_exited = 1;
	return;
}

static void alarm_handler(int sig)
{
	alarm_flag = 1;
	return;
}

int env_wait(int pid)
{
	int ret, status;

	do {
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			break;
		}
	} while (WIFSTOPPED(status) || WIFCONTINUED(status));

	if (ret == pid) {
		ret = VZ_SYSTEM_ERROR;
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
		else if (WIFSIGNALED(status)) {
			logger(-1, 0, "Got signal %d", WTERMSIG(status));
		}
	} else {
		ret = VZ_SYSTEM_ERROR;
		logger(-1, errno, "Error in waitpid()");
	}
	return ret;
}

static int vps_real_exec(vps_handler *h, envid_t veid, const char *root,
	int exec_mode, char *argv[], char *const envp[], char *std_in,
	int timeout)
{
	int ret, pid;
	int in[2], out[2], err[2], st[2];
	int fl = 0;
	struct sigaction act;
	char *def_argv[] = { NULL, NULL };

	if (pipe(in) < 0 ||
		pipe(out) < 0 ||
		pipe(err) < 0 ||
		pipe(st) < 0)
	{
		logger(-1, errno, "Unable to create pipe");
		return VZ_RESOURCE_ERROR;
	}
	/* Default for envp if not set */
	if (envp == NULL)
		envp = envp_bash;
	/* Set non block mode */
	set_not_blk(out[0]);
	set_not_blk(err[0]);
	/* Setup alarm handler */
	if (timeout) {
		alarm_flag = 0;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		act.sa_handler = alarm_handler;
		sigaction(SIGALRM, &act, NULL);
		alarm(timeout);
	}
	child_exited = 0;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	act.sa_handler = exec_handler;
	sigaction(SIGCHLD, &act, NULL);

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);

	if ((ret = h->setcontext(veid)))
		return ret;
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Unable to fork");
		ret = VZ_RESOURCE_ERROR;
		goto err;
	} else if (pid == 0) {
		close(0); close(1); close(2);
		dup2(in[0], STDIN_FILENO);
		dup2(out[1], STDOUT_FILENO);
		dup2(err[1], STDERR_FILENO);

		close(in[0]); close(out[1]); close(err[1]);
		close(in[1]); close(out[0]); close(err[0]);
		close(st[0]);
		fcntl(st[1], F_SETFD, FD_CLOEXEC);
		close_fds(0, st[1], h->vzfd, -1);
		if ((ret = h->enter(h, veid, root, 0)))
			goto env_err;
		if (exec_mode == MODE_EXEC && argv != NULL) {
			execvep(argv[0], argv, envp);
		} else {
			if (argv == NULL)
				argv = def_argv;
			argv[0] = "/bin/bash";
			execve(argv[0], argv, envp);
			argv[0] = "/bin/sh";
			execve(argv[0], argv, envp);
		}
		ret = VZ_FS_BAD_TMPL;
env_err:
		write(st[1], &ret, sizeof(ret));
		exit(ret);
	}
	close(st[1]);
	close(out[1]); close(err[1]); close(in[0]);
	while ((ret = read(st[0], &ret, sizeof(ret))) == -1)
		if (errno != EINTR)
			break;
	if (ret)
		goto err;
	if (std_in != NULL) {
		if (write(in[1], std_in, strlen(std_in)) < 0) {
			ret = VZ_COMMAND_EXECUTION_ERROR;
			/* Flush fd */
			while (stdredir(out[0], STDOUT_FILENO) == 0);
			while (stdredir(err[0], STDERR_FILENO) == 0);
			goto err;
		}
		close(in[1]);
		/* do not set STDIN_FILENO in select() */
		fl = 4;
	}
	while(!child_exited) {
		int n;
		fd_set rd_set;

		if (timeout && alarm_flag) {
			logger(-1, 0, "Execution timeout expired");
			kill(pid, SIGTERM);
			alarm(0);
			break;
		}
		if ((fl & 3) == 3) {
			/* all fd are closed */
			close(in[1]);
			break;
		}
		FD_ZERO(&rd_set);
		if (!(fl & 4))
			FD_SET(STDIN_FILENO, &rd_set);
		if (!(fl & 1))
			FD_SET(out[0], &rd_set);
		if (!(fl & 2))
			FD_SET(err[0], &rd_set);
		n = select(FD_SETSIZE, &rd_set, NULL, NULL, NULL);
		if (n > 0) {
			if (FD_ISSET(out[0], &rd_set))
				if (stdredir(out[0], STDOUT_FILENO) < 0) {
					fl |= 1;
					close(out[0]);
				}
			if (FD_ISSET(err[0], &rd_set))
				if (stdredir(err[0], STDERR_FILENO) < 0) {
					fl |= 2;
					close(err[0]);
				}
			if (FD_ISSET(STDIN_FILENO, &rd_set))
				if (stdredir(STDIN_FILENO, in[1]) < 0) {
					fl |= 4;
					close(in[1]);
				}
		} else if (n < 0 && errno != EINTR) {
			logger(-1, errno, "Error in select()");
			close(out[0]);
			close(err[0]);
			break;
		}
	}
	/* Flush fds */
	if (!(fl & 1)) {
		while (stdredir(out[0], STDOUT_FILENO) == 0);
	}
	if (!(fl & 2)) {
		while (stdredir(err[0], STDERR_FILENO) == 0);
	}
	ret = env_wait(pid);
	if (ret && timeout && alarm_flag)
		ret = VZ_EXEC_TIMEOUT;

err:
	close(st[0]); close(st[1]);
	close(out[0]); close(out[1]);
	close(err[0]); close(err[1]);
	close(in[0]); close(in[1]);

	return ret;
}

/** Execute command inside CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param root		CT root.
 * @param exec_mode	execution mode (MODE_EXEC, MODE_BASH).
 * @param arg		argv array.
 * @param envp		command environment array.
 * @param std_in	read command from buffer stdin point to.
 * @param timeout	execution timeout, 0 - unlimited.
 * @return		0 on success.
 */
int vps_exec(vps_handler *h, envid_t veid, const char *root, int exec_mode,
	char *argv[], char *const envp[], char *std_in, int timeout)
{
	int pid, ret;

	if (check_var(root, "Container root (VE_ROOT) is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	fflush(stderr);
	fflush(stdout);
	/* Extra fork to skip UBC limit applying to the current process */
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can not fork");
		return VZ_RESOURCE_ERROR;
	} else if (pid == 0) {
		ret = vps_real_exec(h, veid, root, exec_mode, argv, envp,
			std_in, timeout);
		exit(ret);
	}
	ret = env_wait(pid);

	return ret;
}

static int _real_execFn(vps_handler *h, envid_t veid, const char *root,
		execFn fn, void *data, int flags)
{
	int ret, pid;

	if ((ret = h->setcontext(veid)))
		return ret;
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Unable to fork");
		return VZ_RESOURCE_ERROR;
	} else if (pid == 0) {
		close_fds(1, h->vzfd, -1);
		if ((ret = h->enter(h, veid, root, flags)))
			goto env_err;
		ret = fn(data);
env_err:
		exit(ret);
	}
	ret = env_wait(pid);
	return ret;
}

int vps_execFn(vps_handler *h, envid_t veid, const char *root,
		execFn fn, void *data, int flags)
{
	int pid, ret;

	if (check_var(root, "Container root (VE_ROOT) is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	fflush(stderr);
	fflush(stdout);
	/* Extra fork to skip UBC limit applying to the current process */
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can not fork");
		return VZ_RESOURCE_ERROR;
	} else if (pid == 0) {
		ret = _real_execFn(h, veid, root, fn, data, flags);
		exit(ret);
	}
	ret = env_wait(pid);
	return ret;
}

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
	int timeout)
{
	int len, ret;
	char *script = NULL;

	if ((len = read_script(fname, func, &script)) < 0)
		return -1;
	logger(1, 0, "Running container script: %s", fname);
	ret = vps_exec(h, veid, root, MODE_BASH, argv, envp, script, timeout);
	free(script);
	return ret;
}


int vps_run_script(vps_handler *h, envid_t veid, char *script, vps_param *vps_p)
{
	int is_run, is_mounted;
	int rd_p[2], wr_p[2];
	int ret, retry;
	char *argv[2];
	const char *root = vps_p->res.fs.root;
	const char *private = vps_p->res.fs.private;

	if (stat_file(script) != 1) {
		logger(-1, 0, "Script not found: %s", script);
		return VZ_NOSCRIPT;
	}
	if (pipe(rd_p) || pipe(wr_p)) {
		logger(-1, errno, "Unable to create pipe");
		return VZ_RESOURCE_ERROR;
	}
	if (check_var(root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (check_var(vps_p->res.fs.private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (stat_file(vps_p->res.fs.private) != 1) {
		logger(-1, 0, "Container private area %s does not exist",
			vps_p->res.fs.private);
		return VZ_FS_NOPRVT;
	}
	if (!(is_run = vps_is_run(h, veid))) {
		if ((ret = check_ub(h, &vps_p->res.ub)))
			return ret;
		is_mounted = vps_is_mounted(root, private);
		if (is_mounted == 0) {
			if ((ret = fsmount(veid, &vps_p->res.fs,
				&vps_p->res.dq, 0)))
			{
				return ret;
			}
		}
		if ((ret = vz_env_create(h, veid, &vps_p->res, rd_p, NULL,
					wr_p, NULL, NULL)))
		{
			return ret;
		}
	}
	argv[0] = script;
	argv[1] = NULL;
	ret = vps_exec_script(h, veid, root, argv, NULL, script,
		NULL, 0);
	if (!is_run) {
		/* Close w/o writing to signal we don't want to start init */
		close(rd_p[1]);
		retry = 0;
		while (retry++ < 10 && vps_is_run(h, veid))
			usleep(500000);
		if (is_mounted == 0)
			fsumount(veid, &vps_p->res.fs);
	}
	close(rd_p[0]);
	close(rd_p[1]);
	close(wr_p[0]);
	close(wr_p[1]);
	return ret;
}
