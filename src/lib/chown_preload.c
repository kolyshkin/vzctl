/*
 *  Copyright (C) 2013, Parallels, Inc. All rights reserved.
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
 *
 *  Authors: Kir Kolyshkin and Glauber Costa
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <dlfcn.h>

static int (*real_chown)(const char *path, uid_t owner, gid_t group) = NULL;
static int (*real_fchown)(int fd, uid_t owner, gid_t group) = NULL;
static int (*real_lchown)(const char *path, uid_t owner, gid_t group) = NULL;
static int (*real_fchownat)(int dirfd, const char *pathname,
		uid_t owner, gid_t group, int flags) = NULL;

uid_t uid_offset = 0;
gid_t gid_offset = 0;

static void __init(void)
{
	char *uidstr, *gidstr;

	uidstr = getenv("UID_OFFSET");
	gidstr = getenv("GID_OFFSET");
	if (!uidstr || !gidstr) {
		fprintf(stderr, "Environment variables UID_OFFSET "
				"and GID_OFFSET are required -- aborting\n");
		exit(33);
	}
	uid_offset = strtol(uidstr, NULL, 10);
	gid_offset = strtol(gidstr, NULL, 10);

	real_chown = dlsym(RTLD_NEXT, "chown");
	real_fchown = dlsym(RTLD_NEXT, "fchown");
	real_lchown = dlsym(RTLD_NEXT, "lchown");
	real_fchownat = dlsym(RTLD_NEXT, "fchownat");

	if (!real_chown || !real_fchown || !real_lchown || !real_fchownat) {
		fprintf(stderr, "dlsym failed: %s\n", dlerror());
		exit(34);
	}
}

int chown(const char *path, uid_t owner, gid_t group)
{
	if (!real_chown)
		__init();

	return real_chown(path, owner + uid_offset, group + gid_offset);
}

int fchown(int fd, uid_t owner, gid_t group)
{
	if (!real_fchown)
		__init();

	return real_fchown(fd, owner + uid_offset, group + gid_offset);
}

int lchown(const char *path, uid_t owner, gid_t group)
{
	if (!real_lchown)
		__init();

	return real_lchown(path, owner + uid_offset, group + gid_offset);
}

int fchownat(int dirfd, const char *pathname,
		uid_t owner, gid_t group, int flags)
{
	if (!real_fchownat)
		__init();

	return real_fchownat(dirfd, pathname,
			owner + uid_offset, group + gid_offset, flags);
}
