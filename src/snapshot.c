#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <linux/vzcalluser.h>
#include <linux/cpt_ioctl.h>
#include <ploop/libploop.h>

#include "cpt.h"
#include "util.h"
#include "cpt.h"
#include "env.h"
#include "res.h"
#include "fs.h"
#include "image.h"
#include "snapshot.h"
#include "vzerror.h"
#include "vzconfig.h"
#include "cleanup.h"

#define VZCTL_VE_DUMP_DIR	"/dump"
#define VZCTL_VE_CONF		"ve.conf"

static void cancel_ploop_op(void *data)
{
	ploop.cancel_operation();
}

static void vzctl_get_snapshot_dumpfile(const char *private, const char *guid,
		char *buf, int len)
{
	snprintf(buf, len, "%s" VZCTL_VE_DUMP_DIR "/%s", private, guid);
}

static void vzctl_get_snapshot_ve_conf(const char *private, const char *guid,
		char *buf, int len)
{
	snprintf(buf, len, "%s" VZCTL_VE_DUMP_DIR "/%s." VZCTL_VE_CONF,
			private, guid);
}

int is_snapshot_supported(const fs_param *fs)
{
	if (! ve_private_is_ploop(fs)) {
		logger(-1, 0, "Snapshot feature is only available "
				"for ploop-based CTs");
		return 0;
	}

	return is_ploop_supported();
}

int vzctl_env_create_snapshot(vps_handler *h, envid_t veid,
		const fs_param *fs,
		const struct vzctl_snapshot_param *param)
{
	int ret, run = 0;
	cpt_param cpt = {};
	char guid[39];
	char fname[PATH_MAX];
	char tmp[PATH_MAX];
	char snap_ve_conf[PATH_MAX] = "";
	struct ploop_snapshot_param image_param = {};
	struct ploop_merge_param merge_param = {};
	struct ploop_disk_images_data *di = NULL;
	struct vzctl_snapshot_tree *tree = NULL;

	if (!is_snapshot_supported(fs))
		return VZCTL_E_CREATE_SNAPSHOT;
	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		return VZ_RESOURCE_ERROR;
	if (param->guid == NULL) {
		if (ploop.uuid_generate(guid, sizeof(guid)))
			goto err;
	} else
		snprintf(guid, sizeof(guid), "%s", param->guid);
	GET_SNAPSHOT_XML(fname, fs->private);
	if (stat_file(fname) == 1) {
		ret = vzctl_read_snapshot_tree(fname, tree);
		if (ret) {
			logger(-1, 0, "Failed to read %s", fname);
			goto err;
		}
	}
	logger(0, 0, "Creating snapshot %s", guid);
	GET_DISK_DESCRIPTOR(fname, fs->private);
	if (ploop.read_disk_descr(&di, fname)) {
		logger(-1, 0, "Failed to read %s", fname);
		goto err;
	}
	// Store snapshot.xml
	ret = vzctl_add_snapshot(tree, guid, param);
	if (ret)
		goto err;
	GET_SNAPSHOT_XML_TMP(tmp, fs->private);
	ret = vzctl_store_snapshot_tree(tmp, tree);
	if (ret) {
		logger(-1, 0, "Failed to store %s", tmp);
		goto err;
	}
	if (!(param->flags & SNAPSHOT_SKIP_CONFIG)) {
		// Store ve.conf
		get_vps_conf_path(veid, fname, sizeof(fname));
		vzctl_get_snapshot_ve_conf(fs->private, guid,
				snap_ve_conf, sizeof(snap_ve_conf));
		make_dir(snap_ve_conf, 0);
		if (cp_file(snap_ve_conf, fname))
			goto err1;
	}

	if (!(param->flags & SNAPSHOT_SKIP_SUSPEND))
		run = vps_is_run(h, veid);
	/* 1 freeze */
	if (run) {
		ret = vps_chkpnt(h, veid, fs, CMD_SUSPEND, &cpt);
		if (ret)
			goto err1;
	}
	/* 2 create snapshot with specified guid */
	image_param.guid = guid;
	PLOOP_CLEANUP(ret = ploop.create_snapshot(di, &image_param));
	if (ret) {
		logger(-1, 0, "Failed to create snapshot: %s [%d]",
				ploop.get_last_error(), ret);
		goto err1;
	}
	/* 3 store dump & continue */
	if (run) {
		vzctl_get_snapshot_dumpfile(fs->private, guid, fname, sizeof(fname));
		cpt.dumpfile = fname;
		ret = vps_chkpnt(h, veid, fs, CMD_DUMP, &cpt);
		if (ret)
			goto err2;
		if (cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0))
			logger(-1, 0, "Failed to resume Container");
	}
	// move snapshot.xml to its place
	GET_SNAPSHOT_XML(fname, fs->private);
	if (rename(tmp, fname))
		logger(-1, errno, "Failed to rename %s -> %s", tmp, fname);
	logger(0, 0, "Snapshot %s has been successfully created",
			guid);
	ret = 0;
	goto out;

err2:
	// merge top_delta
	merge_param.guid = guid;
	PLOOP_CLEANUP(ret = ploop.merge_snapshot(di, &merge_param));
	if (ret)
		logger(-1, 0, "Rollback failed, ploop_merge_snapshot %s: %s [%d]",
				guid, ploop.get_last_error(), ret);

err1:
	if (run)
		cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0);
	unlink(tmp);
	unlink(snap_ve_conf);

err:
	ret = VZCTL_E_CREATE_SNAPSHOT;
	logger(-1, 0, "Failed to create snapshot");

out:
	if (di != NULL)
		ploop.free_diskdescriptor(di);
	if (tree != NULL)
		vzctl_free_snapshot_tree(tree);

	return ret;
}

int vzctl_env_switch_snapshot(vps_handler *h, envid_t veid,
		vps_param *g_p, const struct vzctl_snapshot_param *param)
{
	int ret, run;
	int flags = 0;
	cpt_param cpt = {};
	const char *guid = param->guid;
	fs_param *fs = &g_p->res.fs;
	char fname[PATH_MAX];
	char snap_xml_tmp[PATH_MAX];
	char ve_conf_tmp[PATH_MAX] = "";
	char ve_conf_old[PATH_MAX] = "";
	char dumpfile[PATH_MAX];
	char guid_buf[39];
	const char *guid_tmp = NULL;
	struct vzctl_snapshot_tree *tree = NULL;
	struct ploop_disk_images_data *di = NULL;
	struct ploop_snapshot_switch_param switch_param = {};

	if (!is_snapshot_supported(fs))
		return VZCTL_E_SWITCH_SNAPSHOT;

	GET_SNAPSHOT_XML(fname, fs->private);
	if (stat_file(fname) != 1)
		return vzctl_err(VZCTL_E_SWITCH_SNAPSHOT, 0,
				"Unable to find snapshot by uuid %s", guid);
	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		goto err;

	ret = vzctl_read_snapshot_tree(fname, tree);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		goto err;
	}
	if (vzctl_find_snapshot_by_guid(tree, guid) == -1) {
		logger(-1, 0, "Unable to find snapshot by uuid %s", guid);
		goto err;
	}
	GET_DISK_DESCRIPTOR(fname, fs->private);
	ret = ploop.read_disk_descr(&di, fname);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		goto err;
	}

	vzctl_get_snapshot_dumpfile(fs->private, guid,
			dumpfile, sizeof(dumpfile));

	if ((param->flags & SNAPSHOT_MUST_RESUME) && stat_file(dumpfile) != 1) {
		logger(-1, 0, "Error: no dump in snapshot, unable to resume");
		goto err;
	}

	logger(0, 0, "Switching to snapshot %s", guid);
	vzctl_snapshot_tree_set_current(tree, guid);
	GET_SNAPSHOT_XML_TMP(snap_xml_tmp, fs->private);
	ret = vzctl_store_snapshot_tree(snap_xml_tmp, tree);
	if (ret) {
		logger(-1, 0, "Failed to store %s", snap_xml_tmp);
		goto err;
	}

	run = vps_is_run(h, veid);
	if (run == -1)
		goto err1;
	/* freeze */
	if (run) {
		/* preserve current top delta with 'guid_tmp' for rollback */
		if (ploop.uuid_generate(guid_buf, sizeof(guid_buf))) {
			logger(-1, 0, "Can't generate ploop uuid: %s",
					ploop.get_last_error());
			goto err1;
		}
		guid_tmp = guid_buf;
		flags = PLOOP_SNAP_SKIP_TOPDELTA_DESTROY;

		ret = vps_chkpnt(h, veid, fs, CMD_SUSPEND, &cpt);
		if (ret)
			goto err1;
	} else if (vps_is_mounted(fs->root, fs->private) == 1) {
		if (vps_umount(h, veid, fs, 0))
			goto err1;
	}

	/* switch snapshot */
	switch_param.guid = guid;
	switch_param.guid_old = guid_tmp;
	switch_param.flags = flags;
	PLOOP_CLEANUP(ret = ploop.switch_snapshot_ex(di, &switch_param));
	if (ret)
		goto err2;

	/* restore ve.conf */
	vzctl_get_snapshot_ve_conf(fs->private, guid, fname, sizeof(fname));
	if (stat_file(fname) == 1 && !(param->flags & SNAPSHOT_SKIP_CONFIG)) {
		get_vps_conf_path(veid, ve_conf_tmp, sizeof(ve_conf_tmp) - 4);
		strcat(ve_conf_tmp, ".tmp");
		if (cp_file(ve_conf_tmp, fname))
			goto err2;
	}

	/* stop CT */
	if (run) {
		ret = cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_KILL, 0);
		if (ret)
			goto err3;
		if (vps_umount(h, veid, fs, 0))
			goto err3;
	}
	if (ve_conf_tmp[0] != '\0') {
		get_vps_conf_path(veid, fname, sizeof(fname));
		strncpy(ve_conf_old, fname, sizeof(ve_conf_old) - 4);
		strcat(ve_conf_old, ".old");
		cp_file(ve_conf_old, fname);
		if (rename(ve_conf_tmp, fname))
			logger(-1, errno, "Failed to rename %s -> %s",
					ve_conf_tmp, fname);
	}
	/* resume CT in case dump file exists */
	if (stat_file(dumpfile) == 1 &&	!(param->flags & SNAPSHOT_SKIP_RESUME)) {
		vps_param *vps_p;

		vps_p = reread_vps_config(veid);
		if (vps_p && g_p->res.net.skip_arpdetect)
			vps_p->res.net.skip_arpdetect =
				g_p->res.net.skip_arpdetect;
		cpt.dumpfile = dumpfile;
		if (vps_restore(h, veid, (vps_p) ? vps_p : g_p,
					CMD_RESTORE, &cpt, SKIP_DUMPFILE_UNLINK)) {
			/*
			 * We have switched to a new snapshot, but
			 * the restore from a dump file failed.
			 * Treat it either as a warning or an error,
			 * depending on the --must-resume flag being set.
			 */
			if (param->flags & SNAPSHOT_MUST_RESUME) {
				logger(-1, 0, "Error: failed to resume CT");
				free_vps_param(vps_p);
				goto err4;
			}
			else {
				/* no rollback, ignore restore error */
				logger(0, 0, "Warning: failed to resume CT, "
					"ignoring (use --must-resume "
					"to treat as error)");
			}
		}
		free_vps_param(vps_p);
	}
	GET_SNAPSHOT_XML(fname, fs->private);
	if (rename(snap_xml_tmp, fname))
		logger(-1, errno, "Failed to rename %s %s", snap_xml_tmp, fname);

	/* remove temporary snapshot */
	if (guid_tmp != NULL)
		vzctl_delete_snapshot(fs->private, guid_tmp);

	logger(0, 0, "Container has been successfully switched "
			"to another snapshot");
	ret = 0;
	goto out;

err4:
	/* Restore config */
	if (ve_conf_old[0] != '\0') {
		get_vps_conf_path(veid, fname, sizeof(fname));
		if (rename(ve_conf_old, fname))
			logger(-1, errno, "Failed to rename %s to %s",
					ve_conf_old, fname);
	}

err3:
	if (guid_tmp != NULL) {
		switch_param.guid = guid_tmp;
		switch_param.guid_old = NULL;
		switch_param.flags = PLOOP_SNAP_SKIP_TOPDELTA_CREATE;
		PLOOP_CLEANUP(ploop.switch_snapshot_ex(di, &switch_param));
	}

err2:
	if (run && cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0))
		logger(-1, 0, "Failed to resume container during rollback");

err1:
	unlink(snap_xml_tmp);

err:
	ret = VZCTL_E_SWITCH_SNAPSHOT;
	logger(-1, 0, "Failed to switch to snapshot %s", guid);

out:
	if (tree != NULL)
		vzctl_free_snapshot_tree(tree);
	if (di != NULL)
		ploop.free_diskdescriptor(di);

	return ret;
}

int vzctl_env_delete_snapshot(vps_handler *h, envid_t veid,
		const fs_param *fs, const char *guid)
{
	int ret;
	char fname[PATH_MAX];
	char tmp[PATH_MAX];
	struct vzctl_snapshot_tree *tree = NULL;

	if (!is_snapshot_supported(fs))
		return VZCTL_E_DELETE_SNAPSHOT;

	GET_SNAPSHOT_XML(fname, fs->private);
	if (stat_file(fname) != 1)
		return vzctl_err(VZCTL_E_DELETE_SNAPSHOT, 0,
				"Unable to find snapshot by uuid %s", guid);
	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		return VZCTL_E_DELETE_SNAPSHOT;

	ret = vzctl_read_snapshot_tree(fname, tree);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		goto err;
	}
	if (vzctl_find_snapshot_by_guid(tree, guid) == -1) {
		logger(-1, 0, "Unable to find snapshot by uuid %s", guid);
		goto err;
	}
	logger(0, 0, "Deleting snapshot %s", guid);
	vzctl_del_snapshot_tree_entry(tree, guid);
	GET_SNAPSHOT_XML_TMP(tmp, fs->private);
	ret = vzctl_store_snapshot_tree(tmp, tree);
	if (ret) {
		logger(-1, 0, "Failed to store %s", tmp);
		goto err;
	}

	ret = vzctl_delete_snapshot(fs->private, guid);
	if (ret)
		goto err1;

	vzctl_get_snapshot_dumpfile(fs->private, guid, fname, sizeof(fname));
	if (stat_file(fname) == 1) {
		logger(1, 0, "Deleting CT dump %s", fname);
		if (unlink(fname))
			logger(-1, errno, "Failed to delete dump %s",
					fname);
	}

	// delete ve.conf
	vzctl_get_snapshot_ve_conf(fs->private, guid, fname, sizeof(fname));
	if (stat_file(fname) == 1 && unlink(fname))
		logger(-1, errno, "Failed to delete %s", fname);

	// move snapshot.xml to its place
	GET_SNAPSHOT_XML(fname, fs->private);
	if (rename(tmp, fname))
		logger(-1, errno, "Failed to rename %s %s", tmp, fname);

	logger(0, 0, "Snapshot %s has been successfully deleted", guid);
	vzctl_free_snapshot_tree(tree);
	return 0;
err1:
	unlink(tmp);
err:
	logger(-1, 0, "Failed to delete snapshot %s", guid);
	vzctl_free_snapshot_tree(tree);
	return VZCTL_E_DELETE_SNAPSHOT;
}

int vzctl_env_mount_snapshot(unsigned veid, const fs_param *fs,
	char *mnt, char *guid)
{
	int ret;
	char fname[PATH_MAX];
	struct vzctl_snapshot_tree *tree = NULL;
	struct vzctl_mount_param param = {};

	if (fs->private == NULL)
		return vzctl_err(VZ_VE_PRIVATE_NOTSET, 0,
				"Failed to mount snapshot: "
				"CT private not set");

	if (!is_snapshot_supported(fs))
		return VZCTL_E_MOUNT_SNAPSHOT;

	GET_SNAPSHOT_XML(fname, fs->private);
	if (stat_file(fname) != 1)
		return vzctl_err(VZCTL_E_MOUNT_SNAPSHOT, 0,
				"Snapshot description file %s not found",
				fname);

	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		return VZ_RESOURCE_ERROR;

	ret = vzctl_read_snapshot_tree(fname, tree);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		goto free_tree;
	}
	if (vzctl_find_snapshot_by_guid(tree, guid) == -1) {
		logger(-1, 0, "Unable to find snapshot by uuid %s", guid);
		ret = VZCTL_E_MOUNT_SNAPSHOT;
		goto free_tree;
	}
	vzctl_free_snapshot_tree(tree);
	logger(0, 0, "Mounting snapshot %s to %s", guid, mnt);

	/* Mount read-only */
	param.ro = 1;
	param.target = mnt;
	param.guid = guid;
	return vzctl_mount_snapshot(veid, fs->private, &param);

free_tree:
	vzctl_free_snapshot_tree(tree);
	return ret;
}

int vzctl_env_umount_snapshot(unsigned veid, const fs_param *fs, char *guid)
{
	if (fs->private == NULL)
		return vzctl_err(VZ_VE_PRIVATE_NOTSET, 0,
				"Failed to umount snapshot: "
				"CT private not set");

	if (!is_snapshot_supported(fs))
		return VZCTL_E_UMOUNT_SNAPSHOT;

	logger(0, 0, "Umounting snapshot %s", guid);

	return vzctl_umount_snapshot(veid, fs->private, guid);
}
