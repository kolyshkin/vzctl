/*
 *  Copyright (C) 2014, Parallels, Inc. All rights reserved.
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

/* vzncc: VZ Nano NetCat, a trivial tool to either listen or connect to
 * a TCP port at localhost, and run a program with its input and output
 * redirected to the connected socket
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FAIL 220
#define SELF "vznnc"

void usage(void) {
	fprintf(stderr,
		"Usage: " SELF " {-l|-c} -p PORT [-f FD] CMD [arg ...]\n");

	exit(1);
}

/* Wait on socket before accept(), while checking that stdin/stdout
 * are not gone. This helps to solve a problem of stale vznnc
 * that was run by ssh if ssh is terminated.
 *
 * Return:
 *   0 - time to do accept()
 *   1 - stdin/stdout are gone, time to exit
 *  -1 - some unexpected error
 */
int pollwait(int fd) {
	struct pollfd pfd[] = {
		{.fd = fd, .events = POLLIN},
		{.fd = 0},
		{.fd = 1},
	};
	const int ev_err = POLLERR|POLLHUP;
	int ret;

	do {
		ret = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), 10 * 1000);
		if (ret < 0) {
			perror(SELF ": poll()");
			return -1;
		}
	} while (ret == 0);

	if (pfd[0].revents & POLLIN)
		/* ready for accept() */
		return 0;

	if (pfd[1].revents & ev_err || pfd[2].revents & ev_err) {
		/* stdin or stdout closed */
		/* most probably stderr is gone too, but let's try */
		fprintf(stderr, SELF ": broken pipe!");
		return 1;
	}

	/* some other unknown event, treat as error */
	return -1;
}

int main(int argc, char *argv[])
{
	int sockfd, connfd, port = -1;
	struct sockaddr_in srv_addr = {};
	int lis = 0, con = 0, fd = -1;
	const int yes = 1;

	while (1) {
		int c;

		c = getopt(argc, argv, "lcp:f:");
		if (c == -1)
			break;
		switch (c) {
			case 'l':
				lis++;
				break;
			case 'c':
				con++;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'f':
				fd = atoi(optarg);
				break;
			default:
				usage();
		}
	}

	if (lis && con) {
		fprintf(stderr, SELF ": options -l and -c are exclusive\n");
		usage();
	}

	if (!lis && !con) {
		fprintf(stderr, SELF ": either -l or -c is required\n");
		usage();
	}

	if (port < 0) {
		fprintf(stderr, SELF ": option -p is required\n");
		usage();
	}

	if (argc - optind < 1)
		usage();

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror(SELF ": socket()");
		return FAIL;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
		perror(SELF ": setsockopt()");
		return FAIL;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	srv_addr.sin_port = htons(port);

	if (lis) { /* listen */
		struct sockaddr_in cl_addr = {};
		socklen_t cl_len = sizeof(cl_addr);

		if (bind(sockfd, (struct sockaddr *) &srv_addr,
					sizeof(srv_addr)) < 0) {
			perror(SELF ": bind()");
			return FAIL;
		}

		if (listen(sockfd, 0) < 0) {
			perror(SELF ": listen()");
			return FAIL;
		}

		if (pollwait(sockfd))
			return FAIL;

		connfd = accept(sockfd, (struct sockaddr *)&cl_addr, &cl_len);
		if (connfd < 0) {
			perror(SELF ": accept()");
			return FAIL;
		}
		close(sockfd);
		sockfd = -1;
	}
	else /* if (con) */ { /* connect */
		if (connect(sockfd, (struct sockaddr *)&srv_addr,
					sizeof(srv_addr)) < 0) {
			perror(SELF ": connect()");
			return FAIL;
		}
		connfd = sockfd;
	}

	if (fd < 0) {
		/* redirect stdin/stdout */
		close(0);
		close(1);
		dup2(connfd, 0);
		dup2(connfd, 1);
		close(connfd);
	}
	else {
		if (fd != connfd) {
			close(fd); /* just in case */
			if (dup2(connfd, fd) != fd) {
				perror(SELF ": dup2()");
				return FAIL;
			}
			close(connfd);
		}
	}

	execvp(argv[optind], argv+optind);

	perror(SELF ": execvp()");
	return 127;
}
