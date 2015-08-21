/*
 *  Copyright (C) 2011-2012, Parallels, Inc. All rights reserved.
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>

#include "image.h"
#include "logger.h"
#include "list.h"
#include "util.h"
#include "vzerror.h"
#include "vzctl.h"
#include "env.h"
#include "destroy.h"
#include "cleanup.h"
#include "vzconfig.h"

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#define DEFAULT_FSTYPE		"ext4"
#define SNAPSHOT_MOUNT_ID	"snap"

#ifdef HAVE_PLOOP
struct ploop_functions ploop;
#endif

/* We want get_ploop_type() to work even if vzctl is compiled
 * w/o ploop includes, thus a copy-paste from libploop.h:
 */
#ifndef HAVE_PLOOP
enum ploop_image_mode {
	PLOOP_EXPANDED_MODE = 0,
	PLOOP_EXPANDED_PREALLOCATED_MODE = 1,
	PLOOP_RAW_MODE = 2,
};
#endif

int get_ploop_type(const char *type)
{
	if (type == NULL)
		return -1;
	if (!strcmp(type, "expanded"))
		return PLOOP_EXPANDED_MODE;
	else if (!strcmp(type, "plain"))
		return PLOOP_EXPANDED_PREALLOCATED_MODE;
	else if (!strcmp(type, "raw"))
		return PLOOP_RAW_MODE;
	return -1;
}

static int lstat_file(const char *path, struct stat *st)
{
	if (lstat(path, st) == -1) {
		if (errno != ENOENT)
			logger(-1, errno, "Can not access %s", path);
		return 1;
	}
	return 0;
}

int guess_ve_private_is_ploop(const char *private)
{
	struct stat st;
	char dd[PATH_MAX], dir[PATH_MAX];

	/* first check that /root.hdd exists and is a directory */
	snprintf(dir, sizeof(dir), "%s/root.hdd", private);
	if (lstat_file(dir, &st))
		return 0;
	if (!S_ISDIR(st.st_mode)) {
		logger(-1, 0, "Warning: %s is not a directory.\n"
			"Please set the VE_LAYOUT parameter value in the VE config.", dir);
	}

	/* then look for /root.hdd/DiskDescriptor.xml */
	GET_DISK_DESCRIPTOR(dd, private);
	if (lstat_file(dd, &st))
		return 0;
	if (!S_ISREG(st.st_mode)) {
		logger(-1, 0, "Warning: %s is not a regular file.\n"
			"Please set the VE_LAYOUT parameter value in the VE config.", dd);
	}
	/* TODO: check whether base image from DD.xml is a regular file */
	return 1;
}

int ve_private_is_ploop(const struct fs_param *fs)
{
	if (fs->layout)
		return fs->layout == VE_LAYOUT_PLOOP;
	return guess_ve_private_is_ploop(fs->private);
}

#ifdef HAVE_PLOOP
/* This should only be called once */
static int load_ploop_lib(void)
{
	void *h;
	void (*resolve)(struct ploop_functions *);
	char *err;
	struct {
		struct ploop_functions ploop;
		/* Newer versions of ploop library might have struct ploop_functions
		 * bigger than the one we are compiling against. Reserve some space
		 * to prevent a buffer overflow.
		 */
		void *padding[32];
	} src = { {0}, {0} };

	h = dlopen("libploop.so.1", RTLD_LAZY);
	if (!h)
		/* ploop-lib < 1.9 had no .so version
		 * FIXME: remove this later
		 */
		h = dlopen("libploop.so", RTLD_LAZY);
	if (!h) {
		logger(-1, 0, "Can't load ploop library: %s", dlerror());
		logger(-1, 0, "Please install ploop packages!");
		return -1;
	}

	dlerror(); /* Clear any existing error */
	resolve = dlsym(h, "ploop_resolve_functions");
	if ((err = dlerror()) != NULL) {
		logger(-1, 0, "Can't init ploop library: %s", err);
		logger(-1, 0, "Please upgrade your ploop packages!");
		dlclose(h);
		return -1;
	}

	(*resolve)(&src.ploop);
	if (src.padding[0] != NULL)
		logger(1, 0, "Notice: ploop library is newer when expected");
	memcpy(&ploop, &src.ploop, sizeof(ploop));

	vzctl_init_ploop_log();
	logger(1, 0, "The ploop library has been loaded successfully");
	/* Don't dlclose(), we need lib for the rest of runtime */

	return 0;
}
#endif

int is_ploop_supported()
{
	static int ret = -1;

	if (ret >= 0)
		return ret;

#ifndef HAVE_PLOOP
	ret = 0;
	logger(-1, 0, "Warning: ploop support is not compiled in");
#else
	if (stat_file("/proc/vz/ploop_minor") != 1) {
		logger(-1, 0, "No ploop support in the kernel, or kernel is way too old.\n"
				"Make sure you have OpenVZ kernel 042stab058.7 or later running,\n"
				"and kernel ploop modules loaded.");
		ret = 0;
		return ret;
	}

	if (load_ploop_lib() != 0) {
		/* Error is printed by load_ploop_lib() */
		ret = 0;
		return ret;
	}

	ret = 1;
#endif
	return ret;
}

#ifdef HAVE_PLOOP

static void cancel_ploop_op(void *data)
{
	ploop.cancel_operation();
}

/* Note: caller should call is_ploop_supported() before.
 * Currently the only caller is vps_start_custom() which does.
 */
int is_image_mounted(const char *ve_private)
{
	int ret;
	char dev[64];
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;

	GET_DISK_DESCRIPTOR(fname, ve_private);
	ret = ploop.read_disk_descr(&di, fname);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		return -1;
	}
	ret = ploop.get_dev(di, dev, sizeof(dev));
	ploop.free_diskdescriptor(di);

	return (ret == 0);
}

int vzctl_mount_image(const char *ve_private, struct vzctl_mount_param *param)
{
	int ret;
	struct ploop_mount_param mount_param = {};
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	GET_DISK_DESCRIPTOR(fname, ve_private);
	ret = ploop.read_disk_descr(&di, fname);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_MOUNT_IMAGE;
	}

	mount_param.fstype = DEFAULT_FSTYPE;
	mount_param.target = param->target;
	mount_param.quota = param->quota;
	mount_param.mount_data = param->mount_data;
	mount_param.fsck = param->fsck;

	PLOOP_CLEANUP(ret = ploop.mount_image(di, &mount_param));
	if (ret) {
		logger(-1, 0, "Failed to mount image: %s [%d]",
				ploop.get_last_error(), ret);
		ret = VZCTL_E_MOUNT_IMAGE;
	}
	ploop.free_diskdescriptor(di);
	return ret;
}

int vzctl_umount_image(const char *ve_private)
{
	int ret;
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	GET_DISK_DESCRIPTOR(fname, ve_private);
	ret = ploop.read_disk_descr(&di, fname);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_UMOUNT_IMAGE;
	}

	PLOOP_CLEANUP(ret = ploop.umount_image(di));
	if (ret) {
		logger(-1, 0, "Failed to umount image: %s [%d]",
				ploop.get_last_error(), ret);
		ret = VZCTL_E_UMOUNT_IMAGE;
	}
	ploop.free_diskdescriptor(di);
	return ret;
}

int vzctl_create_image(const char *ve_private,
		struct vzctl_create_image_param *param)
{
	int ret = 0;
	struct ploop_create_param create_param = {};
	char dir[PATH_MAX];
	char image[PATH_MAX];
	unsigned long size_by_inodes;

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	snprintf(dir, sizeof(dir), "%s/" VZCTL_VE_ROOTHDD_DIR, ve_private);
	ret = make_dir_mode(dir, 1, 0700);
	if (ret)
		return ret;

	snprintf(image, sizeof(image), "%s/root.hdd", dir);

	logger(0, 0, "Creating image: %s size=%luK", image, param->size);
	/* size by inodes is calculated based on ext4 bytes per inode ratio
	 * (mke2fs -i or mke2fs.conf's inode_ratio) which is assumed to be
	 * set to 16384, i.e. one inode per 16K disk space.
	 */
	size_by_inodes = param->inodes * 16;
	if (ploop.get_max_size) { /* available since ploop 1.12 */
		unsigned long long max;

		if (ploop.get_max_size(0, &max)) {
			logger(-1, 0, "Error in ploop_get_max_size()");
			return VZ_SYSTEM_ERROR;
		}
		max /= 2; /* sectors to KB */

		if (size_by_inodes > max) {
			logger(-1, 0, "Error: diskinodes value specified "
					"(%lu) is too high.\n"
					"Maximum allowed diskinodes value "
					"is %llu.",
					param->inodes,
					max / 16);
			return VZ_INVALID_PARAMETER_VALUE;

		}
	}
	create_param.mode = param->mode;
	create_param.fstype = DEFAULT_FSTYPE;
	/* KB to 512b sector conversion */
	create_param.size = 2 * MAX(param->size, size_by_inodes);
	create_param.image = image;
	PLOOP_CLEANUP(ret = ploop.create_image(&create_param));
	if (ret) {
		rmdir(dir);
		logger(-1, 0, "Failed to create image: %s [%d]",
				ploop.get_last_error(), ret);
		return VZCTL_E_CREATE_IMAGE;
	}
	if (param->size < size_by_inodes) {
		/* A larger size image was created due to inodes requirement,
		 * so let's resize it down to requested size (by ballooning)
		 */
		ret = vzctl_resize_image(ve_private, param->size, NO);
		if (ret) {
			rmdir(dir);
			/* Error is printed by vzctl_resize_image */
			return ret;
		}
	}
	return 0;
}

int vzctl_convert_image(const char *ve_private, int mode)
{
	int ret;
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	GET_DISK_DESCRIPTOR(fname, ve_private);
	ret = ploop.read_disk_descr(&di, fname);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_CONVERT_IMAGE;
	}
	PLOOP_CLEANUP(ret = ploop.convert_image(di, mode, 0));
	if (ret) {
		logger(-1, 0, "Failed to convert image: %s [%d]",
				ploop.get_last_error(), ret);
		ret = VZCTL_E_CONVERT_IMAGE;
	}
	ploop.free_diskdescriptor(di);
	return ret;
}

int vzctl_resize_image(const char *ve_private,
		unsigned long long newsize, int offline)
{
	int ret;
	struct ploop_disk_images_data *di;
	struct ploop_resize_param param = {};
	char fname[PATH_MAX];

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	if (ve_private == NULL) {
		logger(-1, 0, "Failed to resize image: "
				"CT private is not specified");
		return VZ_VE_PRIVATE_NOTSET;
	}

	GET_DISK_DESCRIPTOR(fname, ve_private);
	ret = ploop.read_disk_descr(&di, fname);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_RESIZE_IMAGE;
	}
	param.size = newsize * 2; /* Kb to 512b sectors */
	param.offline_resize = (offline == YES);
	PLOOP_CLEANUP(ret = ploop.resize_image(di, &param));
	if (ret) {
		logger(-1, 0, "Failed to resize image: %s [%d]",
				ploop.get_last_error(), ret);
		ret = VZCTL_E_RESIZE_IMAGE;
	}
	ploop.free_diskdescriptor(di);
	return ret;
}

/* Note: caller should call is_ploop_supported() before.
 * Currently the only caller is fill_2quota_param() which does.
 */
int vzctl_get_ploop_dev(const char *mnt, char *out, int len)
{
	return ploop.get_partition_by_mnt(mnt, out, len);
}

int vzctl_create_snapshot(const char *ve_private, const char *guid)
{
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;
	int ret;
	struct ploop_snapshot_param param = {};

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	if (ve_private == NULL) {
		logger(-1, 0, "Failed to create snapshot: "
				"CT private is not specified");
		return VZ_VE_PRIVATE_NOTSET;
	}
	GET_DISK_DESCRIPTOR(fname, ve_private);
	if (ploop.read_disk_descr(&di, fname)) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_CREATE_SNAPSHOT;
	}
	param.guid = strdup(guid);
	PLOOP_CLEANUP(ret = ploop.create_snapshot(di, &param));
	free(param.guid);
	if (ret) {
		logger(-1, 0, "Failed to create snapshot: %s [%d]",
				ploop.get_last_error(), ret);
		ret = VZCTL_E_CREATE_SNAPSHOT;
	}
	ploop.free_diskdescriptor(di);

	return ret;
}

int vzctl_delete_snapshot(const char *ve_private, const char *guid)
{
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;
	int ret;

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	if (ve_private == NULL) {
		logger(-1, 0, "Failed to delete snapshot: "
				"CT private is not specified");
		return VZ_VE_PRIVATE_NOTSET;
	}
	GET_DISK_DESCRIPTOR(fname, ve_private);
	if (ploop.read_disk_descr(&di, fname)) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_DELETE_SNAPSHOT;
	}
	PLOOP_CLEANUP(ret = ploop.delete_snapshot(di, guid));
	if (ret == SYSEXIT_NOSNAP)
		/* ignore: snapshot already deleted */
		ret = 0;
	if (ret) {
		logger(-1, 0, "Failed to delete snapshot: %s [%d]",
				ploop.get_last_error(), ret);
		ret = VZCTL_E_DELETE_SNAPSHOT;
	}
	ploop.free_diskdescriptor(di);

	return ret;
}

int vzctl_merge_snapshot(const char *ve_private, const char *guid)
{
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;
	int ret;
	struct ploop_merge_param param = {};

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	if (guid == NULL)
		return VZCTL_E_MERGE_SNAPSHOT;
	if (ve_private == NULL) {
		logger(-1, 0, "Failed to merge snapshot: "
				"CT private is not specified");
		return VZ_VE_PRIVATE_NOTSET;
	}
	GET_DISK_DESCRIPTOR(fname, ve_private);
	if (ploop.read_disk_descr(&di, fname)) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_MERGE_SNAPSHOT;
	}
	param.guid = guid;
	PLOOP_CLEANUP(ret = ploop.merge_snapshot(di, &param));
	if (ret) {
		logger(-1, 0, "Failed to merge snapshot %s: %s [%d]",
				guid, ploop.get_last_error(), ret);
		ret = VZCTL_E_MERGE_SNAPSHOT;
	}

	ploop.free_diskdescriptor(di);

	return ret;
}


const char *generate_snapshot_component_name(unsigned int envid,
		const char *data, char *buf, int len)
{
	snprintf(buf, len, "%d-%s-%s", envid, SNAPSHOT_MOUNT_ID, data);
	return buf;
}

int vzctl_mount_snapshot(unsigned envid, const char *ve_private,
		struct vzctl_mount_param *param)
{
	int ret = VZCTL_E_MOUNT_SNAPSHOT;
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;
	struct ploop_mount_param mount_param = {};

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	GET_DISK_DESCRIPTOR(fname, ve_private);
	if (ploop.read_disk_descr(&di, fname)) {
		logger(-1, 0, "Failed to read %s", fname);
		return ret;
	}

	mount_param.ro = param->ro;
	if (param->mount_by_parent_guid) {
		mount_param.guid = ploop.find_parent_by_guid(di, param->guid);
		if (mount_param.guid == NULL) {
			logger(-1, 0, "Unable to find parent guid by %s",
					param->guid);
			goto err;
		}
	} else {
		mount_param.guid = param->guid;
	}
	mount_param.target = param->target;
	ploop.set_component_name(di,
			generate_snapshot_component_name(envid, param->guid,
				fname, sizeof(fname)));

	PLOOP_CLEANUP(ret = ploop.mount_image(di, &mount_param));
	if (ret) {
		logger(-1, 0, "Failed to mount snapshot %s: %s [%d]",
				param->guid, ploop.get_last_error(), ret);
		ret = VZCTL_E_MOUNT_SNAPSHOT;
		goto err;
	}

	/* provide device to caller */
	strncpy(param->device, mount_param.device, sizeof(param->device)-1);

err:
	ploop.free_diskdescriptor(di);

	return ret;
}

int vzctl_umount_snapshot(unsigned envid, const char *ve_private, char *guid)
{
	int ret;
	char fname[PATH_MAX];
	struct ploop_disk_images_data *di;

	if (!is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	GET_DISK_DESCRIPTOR(fname, ve_private);
	if (ploop.read_disk_descr(&di, fname)) {
		logger(-1, 0, "Failed to read %s", fname);
		return VZCTL_E_UMOUNT_SNAPSHOT;
	}

	ploop.set_component_name(di,
			generate_snapshot_component_name(envid, guid,
				fname, sizeof(fname)));

	PLOOP_CLEANUP(ret = ploop.umount_image(di));
	ploop.free_diskdescriptor(di);
	if (ret)
		return vzctl_err(VZCTL_E_UMOUNT_SNAPSHOT, 0,
				"Failed to umount snapshot %s: %s [%d]",
				guid, ploop.get_last_error(), ret);

	return 0;
}

/* Convert a CT to ploop layout */
int vzctl_env_convert_ploop(vps_handler *h, envid_t veid, vps_param *vps_p,
		fs_param *fs, dq_param *dq, int mode)
{
	struct vzctl_create_image_param param = {};
	struct vzctl_mount_param mount_param = {};
	int ret, ret2;
	char cmd[STR_SIZE];
	char new_private[STR_SIZE];
	char tmp_private[STR_SIZE];

	if (ve_private_is_ploop(fs)) {
		logger(0, 0, "CT is already on ploop");
		return 0;
	}
	if (!is_ploop_supported()) {
		return VZ_PLOOP_UNSUP;
	}
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "CT is running (stop it first)");
		return VZ_VE_RUNNING;
	}
	if (vps_is_mounted(fs->root, fs->private) == 1) {
		logger(-1, 0, "CT is mounted (umount it first)");
		return VZ_FS_MOUNTED;
	}
	if (!dq->diskspace || dq->diskspace[1] <= 0) {
		logger(-1, 0, "Error: diskspace not set");
		return VZ_DISKSPACE_NOT_SET;
	}

	snprintf(new_private, sizeof(new_private), "%s.ploop", fs->private);
	if (make_dir_mode(new_private, 1, 0700) != 0)
		return VZ_CANT_CREATE_DIR;

	param.mode = mode;
	param.size = dq->diskspace[1]; // limit
	if (dq->diskinodes)
		param.inodes = dq->diskinodes[1];
	ret = vzctl_create_image(new_private, &param);
	if (ret)
		goto err;

	mount_param.target = fs->root;

	ret = vzctl_mount_image(new_private, &mount_param);
	if (ret)
		goto err;

	logger(0, 0, "Copying content to ploop...");
	snprintf(cmd, sizeof(cmd), "/bin/cp -ax %s/. %s",
			fs->private, fs->root);
	logger(1, 0, "Executing %s", cmd);
	ret = system(cmd);
	ret2 = vzctl_umount_image(new_private);
	if (ret) {
		ret = VZ_SYSTEM_ERROR;
		goto err;
	}
	if (ret2) {
		/* Error message already printed by vzctl_umount_image() */
		ret = ret2;
		goto err;
	}
	/* Move current private to tmp_private */
	snprintf(tmp_private, sizeof(tmp_private), "%s.XXXXXX", fs->private);
	if (mkdtemp(tmp_private) == NULL) {
		logger(-1, errno, "Can't create directory %s", tmp_private);
		ret = VZ_CANT_CREATE_DIR;
		goto err;
	}
	if (rename(fs->private, tmp_private)) {
		logger(-1, errno, "Can't rename %s to %s",
				fs->private, tmp_private);
		unlink(tmp_private);
		ret = VZ_SYSTEM_ERROR;
		goto err;
	}
	/* Move new private to its place */
	if (rename(new_private, fs->private))
		ret = errno;
	else
		ret2 = save_ve_layout(veid, vps_p, VE_LAYOUT_PLOOP);
	if (ret || ret2) {
		if (ret) {
			logger(-1, ret, "Can't rename %s to %s",
				new_private, fs->private);
		}

		/* try to restore old private back */
		if (rename(tmp_private, fs->private)) {
			logger(-1, errno, "Can't restore old private area, "
					"mv %s %s failed, please fix!",
					tmp_private, fs->private);
		}
		ret = VZ_SYSTEM_ERROR;
		goto err;
	}
	/* Fix its mode just in case */
	if (chmod(fs->private, 0700))
		/* Too late to rollback, so just issue a warning */
		logger(-1, errno, "Warning: chmod(%s, 0700) failed",
				fs->private);
	/* Finally, del the old private */
	destroydir(tmp_private);
	logger(0, 0, "Container was successfully converted "
			"to the ploop layout");
err:
	if (ret != 0)
		destroydir(new_private);
	return ret;
}

#endif /* HAVE_PLOOP */
