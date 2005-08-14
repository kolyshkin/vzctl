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
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <asm/timex.h>
#include <linux/vzcalluser.h>
#include <wait.h>
#include <termios.h>
#include <pty.h>

#include "vzerror.h"
#include "logger.h"
#include "env.h"


#define DEV_TTY		"/dev/tty"

static volatile sig_atomic_t child_term;
static struct termios s_tios;

static int pty_alloc(int *master, int *slave)
{
	char *name;
	struct termios tios;
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);
	tcgetattr(0, &tios);
	if (openpty(master, slave, NULL, &tios, &ws) < 0) {
		logger(0, errno, "Unable to open pty");
		return -1;
	}
	if ((name = ttyname(*slave)) == NULL) {
		logger(0, errno, "Unable to get tty name");
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
		logger(0, errno, "setsid");
	if (ioctl(ttyfd, TIOCSCTTY, NULL) < 0)
		logger(0, errno, "Failed to connect to controlling tty");
	setpgrp();
}

static void raw_off(void)
{
        if (tcsetattr(0, TCSADRAIN, &s_tios) == -1)
		logger(0, errno, "Unable to restore term attr");
}

static void raw_on(void)
{
	struct termios tios;

	if (tcgetattr(0, &tios) == -1) {
		logger(0, errno, "Unable to get term attr");
		return;
	}
	memcpy(&s_tios, &tios, sizeof(struct termios));
	cfmakeraw(&tios);
	if (tcsetattr(0, TCSADRAIN, &tios) == -1) 
		logger(0, errno, "Unable to set raw mode");
}

static void child_handler(int sig)
{
	child_term = 1;
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

static void e_loop(int r_in, int w_in,  int r_out, int w_out)
{
	int n, fl = 0;
	fd_set rd_set;

	set_not_blk(r_in);
	set_not_blk(r_out);
	while(!child_term) {
		FD_ZERO(&rd_set);
		if (!(fl & 1))
			FD_SET(r_in, &rd_set);
		if (!(fl & 2))
			FD_SET(r_out, &rd_set);

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
		} else if (n < 0 && errno != EINTR) {
			close(r_out);
			logger(0, errno, "Error in select()");
			break;
		}
	}
	/* Flush fds */
	if (!(fl & 2)) 
		while (stdredir(r_out, w_out) == 0);
}

int do_enter(vps_handler *h, envid_t veid, char *root)
{
	int pid, ret, status;
	int i, in[2], out[2], st[2];
	struct sigaction act;

	if (pipe(in) < 0 || pipe(out) < 0 || pipe(st) < 0) {
		logger(0, errno, "Unable to create pipe");
		return VZ_RESOURCE_ERROR;
	}
	
	if ((ret = vz_setluid(veid)))
		return ret;
	child_term = 0;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_NOCLDSTOP;
       	act.sa_handler = child_handler;
        sigaction(SIGCHLD, &act, NULL);

	if ((pid = fork()) < 0) {
		logger(0, errno, "Unable to fork");
		ret = VZ_RESOURCE_ERROR;
		return ret;
	} else if (pid == 0) {
		int master, slave;

		close(in[1]); close(out[0]); close(st[0]);
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
		if ((ret = pty_alloc(&master, &slave)))
			goto err;
		for (i = 0; i < FOPEN_MAX; i++) {
			if (i != in[0] && i != out[1] &&
				i != master && i != slave)
			{
				close(i);
			}
		}
		child_term = 0;
		if ((pid = fork()) == 0) {
			char buf[64];
			char *term;
		        char *arg[] = {"bash",  "-i", NULL};
		        char *env[] = {"HOME=/", "HISTFILE=/dev/null",
				"PATH=/bin:/sbin:/usr/bin:/usr/sbin:", NULL, NULL};
			close(master);
			set_ctty(slave);
			dup2(slave, 0);
			dup2(slave, 1);
			dup2(slave, 2);
			/* Close the extra descriptor for the pseudo tty. */
			close(slave);
			for (i = 3; i < FOPEN_MAX; i++)
				close(i);
			if ((term = getenv("TERM")) != NULL) {
				snprintf(buf, sizeof(buf), "TERM=%s", term);
				env[2] = buf;
			}
			execve("/bin/bash", arg, env);
			execve("/bin/sh", arg, env);
			logger(0, errno, "enter failed: unable to exec bash");
			exit(1);
		} else if (pid < 0) {
			logger(0, errno, "Unable to fork");
			ret = VZ_RESOURCE_ERROR;
			exit(ret);
		}
		close(slave);
		e_loop(in[0], master, master, out[1]);
		while ((ret = waitpid(pid, &status, 0)) == -1)
			if (errno != EINTR)
				break;
		close(master);
		exit(0);
err:
		write(st[1], &ret, sizeof(ret));
		exit(ret);
	}
	close(in[0]); close(out[1]); close(st[1]);
	/* wait for pts allocation */
	ret = read(st[0], &status, sizeof(status));
	if (!ret) {
		fprintf(stdout, "entered into VPS %d\n", veid);
		raw_on();
		e_loop(fileno(stdin), in[1], out[0], fileno(stdout));
	} else {
		fprintf(stdout, "enter failed\n");
	}
	while ((waitpid(pid, &status, 0)) == -1)
		if (errno != EINTR)
			break;
	if (!ret)
		raw_off();
	close(in[1]); close(out[0]);
	return 0;
}

