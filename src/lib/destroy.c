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
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

#include "list.h"
#include "logger.h"
#include "vzconfig.h"
#include "vzerror.h"
#include "util.h"
#include "script.h"
#include "lock.h"
#include "modules.h"
#include "create.h"
#include "env.h"
#include "image.h"

#define BACKUP		0
#define DESTR		1

const char destroy_dir_magic[]="vzctl-rm-me.";

int destroydir(char *dir);

int del_dir(char *dir)
{
	int ret;
	char *argv[4];

	argv[0] = "/bin/rm";
	argv[1] = "-rf";
	argv[2] = dir;
	argv[3] = NULL;
	ret = run_script("/bin/rm", argv, NULL, 0);

	return ret;
}

int vps_destroy_dir(envid_t veid, char *dir, int layout)
{
	int ret, ploop;

	logger(0, 0, "Destroying container private area: %s", dir);

	ploop = layout ? (layout == VE_LAYOUT_PLOOP) : guess_ve_private_is_ploop(dir);
	if (!ploop) {
		if (!quota_ctl(veid, QUOTA_STAT))
			if ((ret = quota_off(veid, 0)))
				if ((ret = quota_off(veid, 1)))
					return ret;
		quota_ctl(veid, QUOTA_DROP);
	}

	if ((ret = destroydir(dir)))
		return ret;

	return 0;
}

/* Removes all the directories under 'root'
 * those names start with 'destroy_dir_magic'
 */
static void _destroydir(const char *root)
{
	char buf[STR_SIZE];
	struct stat st;
	struct dirent *ep;
	DIR *dp;
	int del, ret;

	do {
		if (!(dp = opendir(root)))
			return;
		del = 0;
		while ((ep = readdir(dp))) {
			if (strncmp(ep->d_name, destroy_dir_magic,
					sizeof(destroy_dir_magic) - 1))
			{
				continue;
			}
			snprintf(buf, sizeof(buf), "%s/%s", root, ep->d_name);
			if (stat(buf, &st))
				continue;
			if (!S_ISDIR(st.st_mode))
				continue;
			snprintf(buf, sizeof(buf), "rm -rf %s/%s",
				root, ep->d_name);
			ret = system(buf);
			if (ret == -1 || WEXITSTATUS(ret))
				sleep(10);
			del = 1;
		}
		closedir(dp);
	} while(del);
}

static int _unlink(const char *s)
{
	if (unlink(s)) {
		logger(-1, errno, "Unable to unlink %s", s);
		return VZ_FS_DEL_PRVT;
	}
	return 0;

}

int destroydir(char *dir)
{
	char buf[STR_SIZE];
	char tmp[STR_SIZE];
	char *root;
	int fd_lock, pid;
	struct sigaction act, actold;
	int ret = 0;
	struct stat st;

	if (lstat(dir, &st)) {
		if (errno != ENOENT) {
			logger(-1, errno, "Unable to lstat %s", dir);
			return VZ_FS_DEL_PRVT;
		}
		return 0;
	}

	if (S_ISLNK(st.st_mode)) {
		int i;
		i = readlink(dir, tmp, sizeof(tmp) - 1);
		if (i == -1) {
			logger(-1, errno, "Unable to readlink %s", dir);
			return VZ_FS_DEL_PRVT;
		}
		tmp[i] = '\0';
		logger(-1, 0, "Warning: container private area %s "
				"is a symlink to %s.\n"
				"Not removing link destination, "
				"you have to do it manually.",
				dir, tmp);
		return _unlink(dir);
	}

	if (!S_ISDIR(st.st_mode)) {
		logger(-1, 0, "Warning: container private area %s "
				"is not a directory", dir);
		return _unlink(dir);
	}

	root = get_fs_root(dir);
	if (root == NULL) {
		logger(-1, 0, "Unable to get root for %s", dir);
		return VZ_FS_DEL_PRVT;
	}
	snprintf(tmp, sizeof(tmp), "%s/vztmp", root);
	free(root);
	if (stat_file(tmp) != 1) {
		if (mkdir(tmp, 0755)) {
			logger(-1, errno, "Can't create tmp dir %s", tmp);
			return VZ_FS_DEL_PRVT;
		}
	}
	/* Fast/async removal -- first move to tmp dir */
	snprintf(buf, sizeof(buf), "%s/%sXXXXXX", tmp, destroy_dir_magic);
	if (mkdtemp(buf) == NULL) {
		logger(-1, errno, "Unable to create temporary directory, "
				"mkdtemp(%s) failed", buf);
		return VZ_FS_DEL_PRVT;
	}
	if (rename(dir, buf)) {
		rmdir(buf);
		if (errno == EXDEV) {
			/* See http://bugzilla.openvz.org/457 */
			logger(0, 0, "Warning: directory %s is not on the same"
					" filesystem as %s - doing slow/sync"
					" removal", dir, tmp);
			if (del_dir(dir))
				return VZ_FS_DEL_PRVT;
			else
				return 0;
		} else {
			logger(-1, errno, "Can't move %s -> %s", dir, buf);
			return VZ_FS_DEL_PRVT;
		}
	}
	snprintf(buf, sizeof(buf), "%s/rm.lck", tmp);
	if ((fd_lock = _lock(buf, 0)) == -2) {
		/* Already locked */
		return 0;
	} else if (fd_lock == -1)
		return VZ_FS_DEL_PRVT;

	sigaction(SIGCHLD, NULL, &actold);
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	if (!(pid = fork())) {
		setsid();
		close_fds(1, fd_lock, -1);
		_destroydir(tmp);
		_unlock(fd_lock, buf);
		exit(0);
	} else if (pid < 0)  {
		logger(-1, errno, "destroydir: Unable to fork");
		ret = VZ_RESOURCE_ERROR;
	}
	sleep(1);
	sigaction(SIGCHLD, &actold, NULL);

	return ret;
}

int destroy_dump(envid_t veid, const char *dumpdir)
{
	char buf[128];

	get_dump_file(veid, dumpdir, buf, sizeof(buf));
	if (stat_file(buf) == 0)
		return 0;

	logger(1, 0, "Removing CT dump %s", buf);
	if (unlink(buf) == 0)
		return 0;
	else if (errno == ENOENT)
		return 0;
	else if (errno == EISDIR)
		return del_dir(buf);
	else return -1;
}

int vps_destroy(vps_handler *h, envid_t veid, fs_param *fs, cpt_param *cpt)
{
	int ret;

	if (check_var(fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (check_var(fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (vps_is_run(h, veid)) {
		logger(0, 0, "Container is currently running."
			" Stop it first.");
		return VZ_VE_RUNNING;
	}
	if (vps_is_mounted(fs->root, fs->private) == 1) {
		logger(0, 0, "Container is currently mounted (umount first)");
		return VZ_FS_MOUNTED;
	}
	ret = vps_destroy_dir(veid, fs->private, fs->layout);
	move_config(veid, BACKUP);
	if (destroy_dump(veid, cpt != NULL ? cpt->dumpdir : NULL) < 0)
		logger(-1, errno, "Warning: failed to remove dump file");
	if (rmdir(fs->root) < 0)
		logger(-1, errno, "Warning: failed to remove %s", fs->root);
	if (!ret)
		logger(0, 0, "Container private area was destroyed");

	return ret;
}
