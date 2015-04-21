/*
 *  Copyright (C) 2013-2015, Parallels, Inc. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/cpt_ioctl.h>

static int usage(int rc)
{
	FILE *out = (rc == 0)? stdout : stderr;

	fprintf(out,
"Usage:	vzcptcheck <command> [argument ...]\n"
" [S]	vzcptcheck version		show cpt in-kernel version\n"
" [D]	vzcptcheck version <ver>	test if <ver> version is supported\n"
" [D]	vzcptcheck caps			show CPU capabilities\n"
" [S]	vzcptcheck caps <caps>		test if <caps> work\n"
" [S]	vzcptcheck caps <veid> <caps>	test if <caps> work for running <veid>\n"
"\n"
"[S] -- run this on source node\n"
"[D] -- run on destination node\n"
	);

	return rc;
}

static void ioctl_error(const char *str)
{
	fprintf(stderr, "Error in ioctl(%s): %m\n", str);
}

static int cpt_open(const char *f, int *error_fd)
{
	char file[256];
	int fd;
	int error_pipe[2];

	if (error_fd != NULL) {
		/* Prepare CPT error fd */
		if (pipe(error_pipe) < 0) {
			fprintf(stderr, "Can't create pipe: %m\n");
			return -1;
		}
		fcntl(error_pipe[0], F_SETFL, O_NONBLOCK);
		fcntl(error_pipe[1], F_SETFL, O_NONBLOCK);
	}

	/* Open the proc file */
	snprintf(file, sizeof(file), "/proc/%s", f);
	fd = open(file, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Can't open %s: %m\n", file);
		fprintf(stderr, "Kernel module vz%s not loaded\n", f);
		return -1;
	}

	if (error_fd != NULL) {
		/* Setup CPT error fd */
		if (ioctl(fd, CPT_SET_ERRORFD, error_pipe[1]) < 0) {
			ioctl_error("CPT_SET_ERRORFD");
			close(fd);
			return -1;
		}
		close(error_pipe[1]);
		*error_fd = error_pipe[0];
	}

	return fd;
}

static void cpt_read_error(int fd)
{
	int len, len1;
	char buf[PIPE_BUF];

	while ((len = read(fd, buf, PIPE_BUF)) > 0)
		do {
			len1 = write(STDERR_FILENO, buf, len);
			len -= len1;
		} while (len > 0 && len1 > 0);
}

static int cpt_version(int argc, char *argv[])
{
	int fd, version;

	if (argc == 1) { /* show version */
		fd = cpt_open("cpt", NULL);
		if (fd < 0)
			return 1;

		version = ioctl(fd, CPT_TEST_VERSION);
		close(fd);
		if (version <= 0) {
			ioctl_error("CPT_TEST_VERSION");
			return 1;
		}

		printf("%d\n", version);
		return 0;
	}
	else if (argc == 2) { /* test version */
		int ret;

		version = atoi(argv[1]);
		fd = cpt_open("rst", NULL);
		if (fd < 0)
			return 1;

		ret = ioctl(fd, CPT_TEST_VERSION, version);
		close(fd);
		if (ret < 0) {
			if (errno == EINVAL) /* no such ioctl -- can migrate */
				return 0;
			ioctl_error("CPT_TEST_VERSION");
			return 1;
		}
		else if (ret == 1) /* version accepted */
			return 0;

		/* else -- version rejected */
		fprintf(stderr, "CPT version %d rejected: can't migrate\n",
				version);

		return 2;

	}

	/* else */
	return usage(1);
}

static int cpt_caps(int argc, char *argv[])
{
	int fd, err_fd, caps;

	if (argc == 1) { /* show caps */
		fd = cpt_open("rst", &err_fd);
		if (fd < 0)
			return 1;

		caps = ioctl(fd, CPT_TEST_CAPS);
		cpt_read_error(err_fd);
		close(fd);
		if (caps <= 0) {
			ioctl_error("CPT_TEST_CAPS");
			return 1;
		}

		printf("%d\n", caps);
		return 0;
	}
	else if (argc == 2) { /* test caps, no VEID */
		int ret;

		caps = atoi(argv[1]);

		fd = cpt_open("cpt", &err_fd);
		if (fd < 0)
			return 1;

		ret = ioctl(fd, CPT_TEST_CAPS, caps);
		cpt_read_error(err_fd);
		close(fd);
		if (ret < 0) {
			if (errno == ENOSYS || errno == EAGAIN) {
				fprintf(stderr, "vzcptcheck WARNING: old "
						"kernel, please update to "
						"kernel >= 042stab107.x\n");
				/* As we can't test caps anyway, assume OK */
				return 0;
			}
			ioctl_error("CPT_TEST_CAPS");
			return 1;
		}

		if (ret > 0) {
			fprintf(stderr, "Insufficient CPU capabilities: "
					"can't migrate\n");
			return 2;
		}

		return 0;
	}
	else if (argc == 3) { /* test caps */
		int veid, ret;

		veid = atoi(argv[1]);
		caps = atoi(argv[2]);

		fd = cpt_open("cpt", &err_fd);
		if (fd < 0)
			return 1;

		ret = ioctl(fd, CPT_SET_VEID, veid);
		if (ret < 0) {
			ioctl_error("CPT_SET_VEID");
			close(fd);
			return 1;
		}

		ret = ioctl(fd, CPT_TEST_VECAPS, caps);
		cpt_read_error(err_fd);
		close(fd);
		if (ret < 0) {
			ioctl_error("CPT_TEST_VECAPS");
			return 1;
		}

		if (ret > 0) {
			fprintf(stderr, "Insufficient CPU capabilities: "
					"can't migrate\n");
			return 2;
		}

		return 0;
	}

	/* else */
	return usage(1);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		return usage(0);

	if (strcmp(argv[1], "version") == 0)
		return cpt_version(argc - 1, argv + 1);
	else if (strcmp(argv[1], "caps") == 0)
		return cpt_caps(argc - 1, argv + 1);
	else if (strcmp(argv[1], "help") == 0)
		return usage(0);

	return usage(1);
}
