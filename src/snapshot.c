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

#define VZCTL_VE_DUMP_DIR     "/dump"

#define SNAPSHOT_XML	"Snapshots.xml"
#define GET_SNAPSHOT_XML(buf, ve_private) \
	snprintf(buf, sizeof(buf), "%s/" SNAPSHOT_XML, ve_private);

#define GET_SNAPSHOT_XML_TMP(buf, ve_private) \
	snprintf(buf, sizeof(buf), "%s/" SNAPSHOT_XML ".tmp", ve_private);


static void vzctl_get_snapshot_dumpfile(const char *private, const char *guid,
		char *buf, int len)
{
	snprintf(buf, len, "%s" VZCTL_VE_DUMP_DIR "/%s", private, guid);
}

static int is_snapshot_supported(const char *ve_private)
{
	if (! ve_private_is_ploop(ve_private)) {
		logger(-1, 0,"Snapshot feature is only available "
				"for ploop-based CTs");
		return 0;
	}
	return 1;
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
	struct ploop_snapshot_param image_param = {};
	struct ploop_merge_param merge_param = {};
	struct ploop_disk_images_data *di = NULL;
	struct vzctl_snapshot_tree *tree = NULL;

	if (!is_snapshot_supported(fs->private))
		return VZCTL_E_CREATE_SNAPSHOT;
	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		return VZ_RESOURCE_ERROR;
	di = ploop_alloc_diskdescriptor();
	if (di == NULL)
		goto err;
	if (param->guid == NULL) {
		if (ploop_uuid_generate(guid, sizeof(guid)))
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
	if (ploop_read_diskdescriptor(fname, di)) {
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
	image_param.snap_guid = 1;
	ret = ploop_create_snapshot(di, &image_param);
	if (ret) {
		logger(-1, 0, "Failed to create snapshot: %s [%d]",
				ploop_get_last_error(), ret);
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
	ploop_free_diskdescriptor(di);
	return 0;
err2:
	// merge top_delta
	ret = ploop_merge_snapshot(di, &merge_param);
	if (ret)
		logger(-1, 0, "Rollback failed, ploop_merge_snapshot %s: %s [%d]",
				guid, ploop_get_last_error(), ret);

err1:
	if (run)
		cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0);
	unlink(tmp);

err:
	logger(-1, 0, "Failed to create snapshot");
	if (di != NULL)
		ploop_free_diskdescriptor(di);
	if (tree != NULL)
		vzctl_free_snapshot_tree(tree);
	return VZCTL_E_CREATE_SNAPSHOT;
}

int vzctl_env_switch_snapshot(vps_handler *h, envid_t veid,
		vps_param *g_p,	const fs_param *fs, const char *guid)
{
	int ret, run;
	cpt_param cpt = {};
	char fname[PATH_MAX];
	char tmp[PATH_MAX];
	char prev_top_guid[39];
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
	di = ploop_alloc_diskdescriptor();
	if (di == NULL)
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
	ret = ploop_read_diskdescriptor(fname, di);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		goto err;
	}
	logger(0, 0, "Switching to snapshot %s", guid);
	vzctl_snapshot_tree_set_current(tree, guid);
	GET_SNAPSHOT_XML_TMP(tmp, fs->private);
	ret = vzctl_store_snapshot_tree(tmp, tree);
	if (ret) {
		logger(-1, 0, "Failed to store %s", tmp);
		goto err;
	}

	run = vps_is_run(h, veid);
	if (run == -1)
		goto err1;
	/* freeze */
	if (run) {
		snprintf(prev_top_guid, sizeof(prev_top_guid), "%s", di->top_guid);
		ret = vps_chkpnt(h, veid, fs, CMD_SUSPEND, &cpt);
		if (ret)
			goto err1;
	} else if (vps_is_mounted(fs->root)) {
		if (vps_umount(h, veid, fs, 0))
			goto err1;
	}
	/* switch snapshot */
	ret = ploop_switch_snapshot(di, guid, (run ? PLOOP_LEAVE_TOP_DELTA : 0));
	if (ret)
		goto err2;
	/* stop CT */
	if (run) {
		ret = cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_KILL, 0);
		if (ret)
			goto err3;
		if (vps_umount(h, veid, fs, 0))
			goto err3;
		if (ploop_delete_snapshot(di, prev_top_guid))
			logger(-1, 0, "Failed to remove old top guid %s",
				prev_top_guid);
	}
	/* resume CT in case dump file exists (no rollback, ignore error) */
	vzctl_get_snapshot_dumpfile(fs->private, guid, dumpfile, sizeof(dumpfile));
	if (stat_file(dumpfile)) {
		cpt.dumpfile = dumpfile;
		if (vps_restore(h, veid, g_p, CMD_RESTORE, &cpt))
			logger(-1, 0, "Failed to resume Container");
	}
	GET_SNAPSHOT_XML(fname, fs->private);
	if (rename(tmp, fname))
		logger(-1, 0, "Failed to rename %s %s", tmp, fname);

	logger(0, 0, "Container has been successfully switched "
			"to another snapshot");
	ploop_free_diskdescriptor(di);
	return 0;

err3:
	/* FIXME rollback ploop_switch_snapshot:
	   ploop_di_remove_image(di, di->top_guid, &fname);
	   free(di->top_guid);
	   di->top_guid = strdup(prev_top_guid);
	   ploop_store_diskdescriptor(di);
	   unlink(fname);
	   */
err2:
	if (cpt_cmd(h, veid, fs->root, CMD_CHKPNT, CMD_RESUME, 0))
		logger(-1, 0, "Failed to resume Container on error");
err1:
	unlink(tmp);
err:
	logger(-1, 0, "Failed to switch to snapshot %s", guid);
	if (tree != NULL)
		vzctl_free_snapshot_tree(tree);
	if (di != NULL)
		ploop_free_diskdescriptor(di);
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
	GET_SNAPSHOT_XML(tmp, fs->private);
	strcat(tmp, ".tmp");
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
			logger(-1, 0, "Failed to delete dump %s",
					fname);
	}

	// move sbapshot.xml on place
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

static void print_snapshot_data(struct vzctl_snapshot_data *data)
{
	printf("%38s %c%38s %19s %s\n",
			data->parent_guid,
			data->current ? '*' : ' ',
			data->guid,
			data->date,
			data->name);
}

int vzctl_env_list_snapshot_tree(const char *ve_private)
{
	char fname[PATH_MAX];
	struct vzctl_snapshot_tree *tree = NULL;
	int i, ret = 0;

	if (ve_private == NULL)
		return vzctl_err(VZ_VE_PRIVATE_NOTSET, 0, "VE_PRIVATE is not specified");

	GET_SNAPSHOT_XML(fname, ve_private)
	if (stat_file(fname) != 1)
		return 0;

	tree = vzctl_alloc_snapshot_tree();
	if (tree == NULL)
		return VZ_RESOURCE_ERROR;

	ret = vzctl_read_snapshot_tree(fname, tree);
	if (ret) {
		logger(-1, 0, "Failed to read %s", fname);
		goto err;
	}

	printf("%-38s  %-38s %-19s %-s\n",
		"PARENT_UUID", "UUID", "DATE", "NAME");
	for (i = 0; i < tree->nsnapshots; i++)
		print_snapshot_data(tree->snapshots[i]);

err:
	vzctl_free_snapshot_tree(tree);
	return ret;

}
