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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

static int usage(int rc)
{
	FILE *out = (rc == 0)? stdout : stderr;

	fprintf(out,
"Usage: vzfsync {-s|-d} [-n] file [file ...]\n"
"       -s, --sync		do fsync()\n"
"       -d, --datasync		do fdatasync()\n"
"       -n, --dontneed		do fadvise(DONTNEED)\n"
	);

	return rc;
}

int main(int argc, char *argv[])
{
	int i;
	int fails = 0;
	int sync = 0, datasync = 0, dontneed = 0;
	struct option longopt[] = {
		{ "help",	no_argument, 0, 'h' },
		{ "sync",	no_argument, 0, 's' },
		{ "datasync",	no_argument, 0, 'd' },
		{ "dontneed",	no_argument, 0, 'n' },
		{}
	};

	while ((i = getopt_long(argc, argv, "hsdn", longopt, NULL)) != EOF) {
		switch (i) {
		   case 'h':
			return usage(0);
			break;
		   case 's':
			sync = 1;
			break;
		   case 'd':
			datasync = 1;
			break;
		   case 'n':
			dontneed = 1;
			break;
		   default:
			return usage(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		return usage(0);

	if (sync == 1 && datasync == 1) {
		fprintf(stderr, "Options -s and -d can't be used together\n");
		return usage(1);
	}
	if (sync == 0 && datasync == 0 && dontneed == 0) {
		fprintf(stderr, "No options given\n");
		return usage(1);
	}

	/* Process files one by one */
	for (; argc > 0; argc--, argv++) {
		const char *name = argv[0];
		int fd;

		fd = open(name, O_RDONLY|O_DIRECT);
		if (fd < 0) {
			fprintf(stderr, "Can't open %s: %m\n", name);
			fails++;
			continue;
		}

		if (sync && fsync(fd) < 0) {
			fprintf(stderr, "Error fsync(%s): %m\n", name);
			fails++;
		}
		if (datasync && fdatasync(fd) < 0) {
			fprintf(stderr, "Error fdatasync(%s): %m\n", name);
			fails++;
		}
		if (dontneed && posix_fadvise(fd, 0, 0,
					POSIX_FADV_DONTNEED) < 0) {
			fprintf(stderr, "Error fadvise(%s): %m\n", name);
			fails++;
		}

		if (close(fd) < 0) {
			fprintf(stderr, "Error close(%s): %m\n", name);
			fails++;
		}
	}

	return (fails == 0) ? 0 : 2;
}
