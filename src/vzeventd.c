/*
 *  Copyright (C) 2010-2015, Parallels, Inc. All rights reserved.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <wait.h>

#include "types.h"
#include "logger.h"
#include "vzconfig.h"
#include "vzerror.h"
#include "script.h"

#define NETLINK_UEVENT	31	/* from kernel/ve/vzevent.c */

static void child_handler(int signo)
{
	int pid, status;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if (WIFEXITED(status))
			if (WEXITSTATUS(status) != 0)
				logger(-1, 0, "Child %d failed "
						"with exit code %d",
						pid, WEXITSTATUS(status));
			else
				logger(1, 0, "Child %d exited with success",
						pid);
		else if (WIFSIGNALED(status))
			logger(-1, 0, "Child %d killed by signal %d",
					pid, WTERMSIG(status));
	}
}

static int run_event_script(envid_t ctid, const char *event)
{
	char script[sizeof(SCRIPTDIR)*2];
	int pid;

	snprintf(script, sizeof(script), "%s/vzevent-%s",
			SCRIPTDIR, event);
	if (access(script, X_OK) != 0) {
		if (errno == ENOENT)
			return 0;
		logger(-1, errno, "Can't execute %s", script);
	}

	logger(1, 0, "Running %s event script", event);

	pid = fork();
	switch (pid) {
		case -1:
			logger(-1, errno, "Failed to fork()");
			return 1;
		case 0:
			exit(run_pre_script(ctid, script));
		default:
			logger(1, 0, "Forked child %d for %s event",
					pid, event);
	}
	return 0;
}

static int parse_event(char *buf)
{
	char *ctidp, *endp, *name;
	long int t;
	int len;
	envid_t ctid;
	const int min_event_len = 7; /* ve-stop */

	/* Parse CTID */
	if ((ctidp = strchr(buf, '@')) == NULL) {
		logger(-1, 0, "Bad message: can't find CTID");
		return -1;
	}

	/* Bail out of definitely bad/unknown events */
	len = ctidp - buf;
	if (len < min_event_len)
		goto ev_unknown;
	if (! (buf[0] == 'v') && (buf[1] == 'e') && (buf[2] == '-') )
		goto ev_unknown;
	name = buf + 3; /* Omit common "ve-" prefix */

	*ctidp++ = '\0';
	t = strtol(ctidp, &endp, 10);
	if (*endp != '\0') {
		logger(-1, 0, "Garbage in CTID in message: %s (endp=%s)",
				ctidp, endp);
		return -1;
	}
	if ((t <= 0) || (t > INT_MAX)) {
		logger(-1, 0, "Bad CTID in message: %s", ctidp);
		return -1;
	}
	ctid = (envid_t)t;
	set_log_ctid(ctid);
	logger(2, 0, "CTID = %d, event = %s (len=%d)", ctid, buf, len);

	/* Continue parsing event name */

	switch (len - 3) {
		case 4:
			if (strncmp(name, "stop", 4) == 0)
				break;
			goto ev_unknown;
		case 5:
			if (strncmp(name, "start", 5) == 0)
				break;
			else if (strncmp(name, "mount", 5) == 0)
				break;
			goto ev_unknown;
		case 6:
			if (strncmp(name, "reboot", 6) == 0)
				break;
			else if (strncmp(name, "umount", 6) == 0)
				break;
			goto ev_unknown;
		default:
			goto ev_unknown;
	}

	return run_event_script(ctid, name);

ev_unknown:
	logger(-1, 0, "Unknown event: %s", buf);
	return -1;
}

static int read_events(int fd, struct sockaddr_nl *sa)
{
	int len;
	char buf[32];
	struct iovec iov = { buf, sizeof(buf) };
	struct msghdr msg;
	int ret;

	logger(0, 0, "Started");

	while (1) {
		memset(&msg, 0, sizeof(msg));
		msg.msg_name = (void *)&sa;
		msg.msg_namelen = sizeof(sa);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;

		len = recvmsg(fd, &msg, 0);
		if (len > 0) {
			buf[len] = '\0';
			parse_event(buf);
			set_log_ctid(0);
		} else if (len < 0) {
			if (errno != EINTR) {
				logger(-1, errno, "Error in recvmsg() "
						" (ret=%d, errno=%d)",
						len, errno);
				ret = 1;
				break;
			}
		} else /* len==0 */ {
			logger(0, 0, "Connection closed");
			ret = 0;
			break;
		}
	}
	close(fd);
	logger(0, 0, "Exiting...");
	return ret;
}

static int daemon_read_events(int fd, struct sockaddr_nl *sa)
{
	if (daemon(0, 0) < 0) {
		logger(-1, errno, "Error in daemon()");
		return 1;
	}
	/* Now make logger stop printing to stdout/stderr */
	set_log_quiet(1);
	return read_events(fd, sa);
}

static int prepare_read_events(int daemonize)
{
	int fd;
	struct sockaddr_nl sa;
	struct sigaction act;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_UEVENT);
	if (fd < 0) {
		logger(-1, errno, "Error in socket()");
		return 1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = 1;
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		if (errno == ENOENT)
			logger(-1, 0, "Looks like vzevent kernel module "
					"is not loaded; exiting.");
		else
			logger(-1, errno, "Error in bind()");
		close(fd);
		return 1;
	}

	sigemptyset(&act.sa_mask);
	act.sa_handler = child_handler;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	if (daemonize != 0)
		return daemon_read_events(fd, &sa);
	else
		return read_events(fd, &sa);
}

static void usage()
{
	printf(
"Usage: vzeventd [options]\n"
"	-v	increase verbosity (can be used multiple times)\n"
"	-d	debug (do not daemonize, run in foreground)\n"
"	-h	print this help message\n"
	);
}

int main(int argc, char **argv)
{
	struct vps_param *param = NULL;
	int daemonize = 1;
	int opt, verbose = 0;

	while ((opt = getopt(argc, argv, "dvh")) != -1) {
		switch (opt) {
		case 'd':
			daemonize = 0;
			verbose++;
			break;
		case 'v':
			verbose++;
			break;
		case 'h':
			usage();
			return 0;
		default:
			usage();
			return 1;
		}
	}

	param = init_vps_param();
	/* Read global config file */
	if (vps_parse_config(0, GLOBAL_CFG, param, NULL)) {
		fprintf(stderr, "Global configuration file %s not found\n",
				GLOBAL_CFG);
		return VZ_NOCONFIG;
	}

	init_log(param->log.log_file, 0, param->log.enable != NO,
			param->log.level + verbose,
			0, "vzeventd");

	return prepare_read_events(daemonize);
}
