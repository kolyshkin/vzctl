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

#define VZCTL_VE_DUMP_DIR	"/dump"
#define VZCTL_VE_CONF		"ve.conf"

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

int is_snapshot_supported(const char *ve_private)
{
	if (! ve_private_is_ploop(ve_private)) {
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
	char snap_ve_conf[PATH_MAX];
	struct ploop_snapshot_param image_param = {};
	struct ploop_merge_param merge_param = {};
	struct ploop_disk_images_data *di = NULL;
	struct vzctl_snapshot_tree *tree = NULL;

	if (!is_snapshot_supported(fs->private))
		return VZCTL_E_CREATE_SNAPSHOT;
	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		return VZ_RESOURCE_ERROR;
	if (param->guid == NULL) {
		if (ploop.uuid_generate(guid, sizeof(guid)))
			goto err;
	} else
		snprintf(guid, sizeof(guid), "%s", param->guid);
	GET_SNAPSHOT_XML(fname, fs->private)
	if (stat_file(fname)) {
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
	ret = ploop.create_snapshot(di, &image_param);
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
		logger(-1, 0, "Failed to rename %s -> %s", tmp, fname);
	logger(0, 0, "Snapshot %s has been successfully created",
			guid);
	ploop.free_diskdescriptor(di);
	return 0;
err2:
	// merge top_delta
	ret = ploop.merge_snapshot(di, &merge_param);
	if (ret)
		logger(-1, 0, "Rollback failed, ploop_merge_snapshot %s: %s [%d]",
				guid, ploop.get_last_error(), ret);

err1:
	if (run)
		cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0);
	unlink(tmp);
	unlink(snap_ve_conf);

err:
	logger(-1, 0, "Failed to create snapshot");
	if (di != NULL)
		ploop.free_diskdescriptor(di);
	if (tree != NULL)
		vzctl_free_snapshot_tree(tree);
	return VZCTL_E_CREATE_SNAPSHOT;
}

int vzctl_env_switch_snapshot(vps_handler *h, envid_t veid,
		vps_param *g_p,	const fs_param *fs, const char *guid)
{
	int ret, run;
	int flags = 0;
	cpt_param cpt = {};
	char fname[PATH_MAX];
	char snap_xml_tmp[PATH_MAX];
	char ve_conf_tmp[PATH_MAX] = "";
	char topdelta_fname[PATH_MAX] = "";
	char dd_tmp[PATH_MAX] = "";
	char dumpfile[PATH_MAX];
	struct vzctl_snapshot_tree *tree = NULL;
	struct ploop_disk_images_data *di = NULL;

	if (!is_snapshot_supported(fs->private))
		return VZCTL_E_SWITCH_SNAPSHOT;

	GET_SNAPSHOT_XML(fname, fs->private)
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
		/* do not destroy current top delta */
		flags = PLOOP_SNAP_SKIP_TOPDELTA_DESTROY;
		if (ploop.get_top_delta_fname(di, topdelta_fname, sizeof(topdelta_fname)))
			goto err1;
		ret = vps_chkpnt(h, veid, fs, CMD_SUSPEND, &cpt);
		if (ret)
			goto err1;
	} else if (vps_is_mounted(fs->root, fs->private)) {
		if (vps_umount(h, veid, fs, 0))
			goto err1;
	}
	/* switch snapshot */
	/* preserve DiskDescriptor.xml -> DiskDescriptor.xml.orig */
	snprintf(dd_tmp, sizeof(dd_tmp), "%s.orig", fname);
	if (cp_file(dd_tmp, fname))
		goto err1;

	ret = ploop.switch_snapshot(di, guid, flags);
	if (ret) {
		unlink(dd_tmp);
		goto err2;
	}
	/* restore ve.conf */
	vzctl_get_snapshot_ve_conf(fs->private, guid, fname, sizeof(fname));
	if (stat_file(fname)) {
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
		if (rename(ve_conf_tmp, fname))
			logger(-1, errno, "Failed to rename %s -> %s",
					ve_conf_tmp, fname);
	}
	/* resume CT in case dump file exists (no rollback, ignore error) */
	vzctl_get_snapshot_dumpfile(fs->private, guid, dumpfile, sizeof(dumpfile));
	if (stat_file(dumpfile)) {
		vps_param *param;

		param = reread_vps_config(veid);
		cpt.dumpfile = dumpfile;
		if (vps_restore(h, veid, (param) ? param : g_p,
					CMD_RESTORE, &cpt, SKIP_NONE))
			logger(-1, 0, "Failed to resume Container");
		free_vps_param(param);
	}
	GET_SNAPSHOT_XML(fname, fs->private);
	if (rename(snap_xml_tmp, fname))
		logger(-1, 0, "Failed to rename %s %s", snap_xml_tmp, fname);

	/* cleanup */
	if (dd_tmp[0] != '\0' && unlink(dd_tmp))
		logger(-1, errno, "Failed to unlink %s", dd_tmp);
	if (topdelta_fname[0] != '\0' && unlink(topdelta_fname))
		logger(-1, errno, "Failed to unlink %s", topdelta_fname);

	logger(0, 0, "Container has been successfully switched "
			"to another snapshot");
	ploop.free_diskdescriptor(di);

	return 0;

err3:
	/* Restore original DiskDescriptor.xml */
	GET_DISK_DESCRIPTOR(fname, fs->private);
	if (rename(dd_tmp, fname)) {
		logger(-1, errno, "Failed to rename %s -> %s",
				dd_tmp, fname);
		goto err2;
	}
	/* remove new top delta */
	if (ploop.get_top_delta_fname(di, topdelta_fname, sizeof(topdelta_fname)) == 0)
		if (unlink(topdelta_fname))
			logger(-1, errno, "Failed to unlink %s", topdelta_fname);

err2:
	if (cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0))
		logger(-1, 0, "Failed to resume Container on error");
err1:
	unlink(snap_xml_tmp);
err:
	logger(-1, 0, "Failed to switch to snapshot %s", guid);
	if (tree != NULL)
		vzctl_free_snapshot_tree(tree);
	if (di != NULL)
		ploop.free_diskdescriptor(di);

	return VZCTL_E_SWITCH_SNAPSHOT;
}

int vzctl_env_delete_snapshot(vps_handler *h, envid_t veid,
		const fs_param *fs, const char *guid)
{
	int ret;
	char fname[PATH_MAX];
	char tmp[PATH_MAX];
	struct vzctl_snapshot_tree *tree = NULL;

	if (!is_snapshot_supported(fs->private))
		return VZCTL_E_DELETE_SNAPSHOT;

	GET_SNAPSHOT_XML(fname, fs->private)
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
	if (stat_file(fname)) {
		logger(1, 0, "Deleting CT dump %s", fname);
		if (unlink(fname))
			logger(-1, errno, "Failed to delete dump %s",
					fname);
	}

	// delete ve.conf
	vzctl_get_snapshot_ve_conf(fs->private, guid, fname, sizeof(fname));
	if (stat_file(fname) && unlink(fname))
		logger(-1, errno, "Failed to delete %s", fname);

	// move snapshot.xml to its place
	GET_SNAPSHOT_XML(fname, fs->private);
	if (rename(tmp, fname))
		logger(-1, 0, "Failed to rename %s %s", tmp, fname);

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
