/*
 *  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <linux/vzcalluser.h>
#include <wait.h>
#include <termios.h>
#include <pty.h>
#include <grp.h>
#include <pwd.h>

#include "vzerror.h"
#include "logger.h"
#include "env.h"
#include "util.h"

#define DEV_TTY		"/dev/tty"

static volatile sig_atomic_t child_term;
static volatile sig_atomic_t win_changed;
static struct termios s_tios;
extern char *_proc_title;
extern int _proc_title_len;

static int pty_alloc(int *master, int *slave, struct termios *tios,
	struct winsize *ws)
{
	char *name;

	if (openpty(master, slave, NULL, tios, ws) < 0) {
		logger(-1, errno, "Unable to open pty");
		return -1;
	}
	if ((name = ttyname(*slave)) == NULL) {
		logger(-1, errno, "Unable to get tty name");
	} else {
		logger(2, 0, "Open %s", name);
	}
	return 0;
}

static void set_ctty(int ttyfd)
{
	int fd;

	if ((fd = open(DEV_TTY, O_RDWR | O_NOCTTY)) >= 0) {
		ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
	if (setsid() < 0)
		logger(-1, errno, "setsid");
	if (ioctl(ttyfd, TIOCSCTTY, NULL) < 0)
		logger(-1, errno, "Failed to connect to controlling tty");
	setpgrp();
}

static void raw_off(void)
{
	if (tcsetattr(0, TCSADRAIN, &s_tios) == -1)
		logger(-1, errno, "Unable to restore term attr");
}

static void raw_on(void)
{
	struct termios tios;

	if (tcgetattr(0, &tios) == -1) {
		logger(-1, errno, "Unable to get term attr");
		return;
	}
	memcpy(&s_tios, &tios, sizeof(struct termios));
	cfmakeraw(&tios);
	if (tcsetattr(0, TCSADRAIN, &tios) == -1)
		logger(-1, errno, "Unable to set raw mode");
}

static int winchange(int info, int ptyfd)
{
	int ret;
	struct winsize ws;

	ret = read(info, &ws, sizeof(ws));
	if (ret < 0)
		return -1;
	else if (ret != sizeof(ws))
		return 0;
	ioctl(ptyfd, TIOCSWINSZ, &ws);
	return 0;
}

static void child_handler(int sig)
{
	child_term = 1;
}

static void winchange_handler(int sig)
{
	win_changed = 1;
}

static void set_proc_title(char *tty)
{
	char *p;

	p = tty;
	if (p != NULL && !strncmp(p, "/dev/", 5))
		p += 5;
	memset(_proc_title, 0, _proc_title_len);
	snprintf(_proc_title, _proc_title_len - 1, "vzctl: %s",
		p != NULL ? p : "");
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
			while ((lenw = write(wrfd, p, lenremain)) < 0)
				if (errno != EINTR && errno != EAGAIN)
					break;
			if (lenw < 0) {
				return -1;
			} else {
				lentotal += lenw;
				lenremain -= lenw;
				p += lenw;
			}
		}
	} else if (lenr == 0) {
		return -1;
	} else {
		if (errno == EAGAIN)
			return 1;
		else if (errno != EINTR)
			return -1;
	}
	return 0;
}

static void e_loop(int r_in, int w_in,  int r_out, int w_out, int info)
{
	int n, fl = 0;
	fd_set rd_set;

	set_not_blk(r_in);
	set_not_blk(r_out);
	while (!child_term) {
		/* Process SIGWINCH
		 * read winsize from stdin and send announce to the other end.
		 */
		if (win_changed) {
			struct winsize ws;

			if (!ioctl(r_in, TIOCGWINSZ, &ws))
				write(info, &ws, sizeof(ws));
			win_changed = 0;
		}
		FD_ZERO(&rd_set);
		if (!(fl & 1))
			FD_SET(r_in, &rd_set);
		if (!(fl & 2))
			FD_SET(r_out, &rd_set);
		if (!(fl & 4))
			FD_SET(info, &rd_set);

		n = select(FD_SETSIZE, &rd_set, NULL, NULL, NULL);
		if (n > 0) {
			if (FD_ISSET(r_in, &rd_set))
				if (stdredir(r_in, w_in) < 0) {
					close(w_in);
					fl |= 1;
				}
			if (FD_ISSET(r_out, &rd_set))
				if (stdredir(r_out, w_out) < 0) {
					close(r_out);
					fl |= 2;
					break;
				}
			if (FD_ISSET(info, &rd_set)) {
				if (winchange(info, w_in) < 0)
					fl |= 4;
			}
		} else if (n < 0 && errno != EINTR) {
			close(r_out);
			logger(-1, errno, "Error in select()");
			break;
		}
	}
	/* Flush fds */
	if (!(fl & 2))
		while (stdredir(r_out, w_out) == 0);
}

static void preload_lib()
{
	/* Preload libnss */
	(void)getpwnam("root");
	endpwent();
	(void)getgrnam("root");
	endgrent();
}

int do_enter(vps_handler *h, envid_t veid, const char *root,
		int argc, char **argv)
{
	int pid, ret, status;
	int in[2], out[2], st[2], info[2];
	struct sigaction act;
	int i;

	if (pipe(in) < 0 || pipe(out) < 0 || pipe(st) < 0 || pipe(info) < 0) {
		logger(-1, errno, "Unable to create pipe");
		return VZ_RESOURCE_ERROR;
	}
	if ((ret = vz_setluid(veid)))
		return ret;
	preload_lib();
	child_term = 0;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	act.sa_handler = child_handler;
	sigaction(SIGCHLD, &act, NULL);

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);

	act.sa_handler = winchange_handler;
	sigaction(SIGWINCH, &act, NULL);

	if ((pid = fork()) < 0) {
		logger(-1, errno, "Unable to fork");
		ret = VZ_RESOURCE_ERROR;
		return ret;
	} else if (pid == 0) {
		int master, slave;
		struct termios tios;
		struct winsize ws;

		/* get terminal settings from 0 */
		ioctl(0, TIOCGWINSZ, &ws);
		tcgetattr(0, &tios);
		close(in[1]); close(out[0]); close(st[0]); close(info[1]);
		/* list of skipped fds -1 the end mark */
		close_fds(1, in[0], out[1], st[1], h->vzfd, info[0], -1);
		dup2(out[1], 1);
		dup2(out[1], 2);
		if ((ret = vz_chroot(root)))
			goto err;
		ret = vz_env_create_ioctl(h, veid, VE_ENTER);
		if (ret < 0) {
			if (errno == ESRCH)
				ret = VZ_VE_NOT_RUNNING;
			else
				ret = VZ_ENVCREATE_ERROR;
			goto err;
		}
		close(h->vzfd);
		if ((ret = pty_alloc(&master, &slave, &tios, &ws)))
			goto err;
		set_proc_title(ttyname(slave));
		child_term = 0;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_NOCLDSTOP;
		act.sa_handler = child_handler;
		sigaction(SIGCHLD, &act, NULL);
		if ((pid = fork()) == 0) {
			char buf[64];
			char *term;
			char *arg[] = {NULL, NULL};
			char *env[] = {ENV_PATH,
				"HISTFILE=/dev/null",
				"USER=root", "HOME=/root", "LOGNAME=root",
				NULL, /* for TERM */
				NULL};
			close(master);
			set_ctty(slave);
			dup2(slave, 0);
			dup2(slave, 1);
			dup2(slave, 2);
			/* Close the extra descriptor for the pseudo tty. */
			close(slave);
			/* Close other ends of our pipes */
			close(in[0]);
			close(out[1]);
			close(st[1]);
			close(info[0]);
			if ((term = getenv("TERM")) != NULL) {
				snprintf(buf, sizeof(buf), "TERM=%s", term);
				env[ARRAY_SIZE(env) - 2] = buf;
			}
			arg[0] = "-bash";
			execve("/bin/bash", arg, env);
			arg[0] = "-sh";
			execve("/bin/sh", arg, env);
			logger(-1, errno, "enter failed: unable to exec sh");
			exit(1);
		} else if (pid < 0) {
			logger(-1, errno, "Unable to fork");
			ret = VZ_RESOURCE_ERROR;
			write(st[1], &ret, sizeof(ret));
			exit(ret);
		}
		close(slave);
		close(st[1]);
		e_loop(in[0], master, master, out[1], info[0]);
		while ((ret = waitpid(pid, &status, 0)) == -1)
			if (errno != EINTR)
				break;
		close(master);
		exit(0);
err:
		write(st[1], &ret, sizeof(ret));
		exit(ret);
	}
	close(in[0]); close(out[1]); close(st[1]); close(info[0]);
	/* wait for pts allocation */
	ret = read(st[0], &status, sizeof(status));
	if (!ret) {
		fprintf(stdout, "entered into CT %d\n", veid);
		raw_on();
		if (argc) {
			/* pass command line arguments into CT */
			for (i = 0; i < argc; i++) {
				if (write(in[1], argv[i],
						strlen(argv[i])) < 0) {
					fprintf(stdout,
						"failed to pass "
						"command arguments into CT "
						"%d\n", veid);
					break;
				}
				/* separate every argument by space */
				if (write(in[1], " ", 1) < 0) {
					fprintf(stdout,
						"failed to pass "
						"command arguments into CT "
						"%d\n", veid);
					break;
				}
			}
			/* run command by 'Enter' key emulation */
			if (write(in[1], "\n", 1) < 0)
				fprintf(stdout,
					"failed to finish "
					"command arguments sequence "
					"while passing into CT %d\n", veid);
		}
		e_loop(fileno(stdin), in[1], out[0], fileno(stdout), info[1]);
	} else {
		fprintf(stdout, "enter into CT %d failed\n", veid);
		set_not_blk(out[0]);
		while (stdredir(out[0], fileno(stdout)) == 0)
			;
	}
	while ((waitpid(pid, &status, 0)) == -1)
		if (errno != EINTR)
			break;
	if (WIFSIGNALED(status))
		fprintf(stdout, "got signal %d\n", WTERMSIG(status));
	if (!ret) {
		raw_off();
		fprintf(stdout, "exited from CT %d\n", veid);
	}
	close(in[1]); close(out[0]);
	return ret ? status : 0;
}
