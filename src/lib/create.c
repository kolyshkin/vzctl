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
#include "destroy.h"
#include "image.h"
#include "cleanup.h"

#define VPS_CREATE	SCRIPTDIR "/vps-create"
#define VPS_DOWNLOAD	SBINDIR "/vztmpl-dl"
#define VZOSTEMPLATE	"/usr/bin/vzosname"

static int vps_postcreate(envid_t veid, vps_res *res);

static char *get_ostemplate_name(char *ostmpl)
{
	FILE *fd;
	char buf[STR_SIZE];
	char *p;
	int status;

	snprintf(buf, sizeof(buf), VZOSTEMPLATE " %s", ostmpl);
	if ((fd = popen(buf, "r")) == NULL) {
		logger(-1, errno, "Error in popen(%s)", buf);
		return NULL;
	}
	*buf = 0;
	while((fgets(buf, sizeof(buf), fd)) != NULL);
	status = pclose(fd);
	if (WEXITSTATUS(status) || *buf == 0) {
		logger(-1, 0, "Unable to get full ostemplate name for %s",
			ostmpl);
		return NULL;
	}
	if ((p = strchr(buf, '\n')))
		*p = 0;
	return strdup(buf);
}

static int download_template(char *tmpl)
{
	char *arg[3];
	char *env[] = { NULL, NULL };
	char *val;
	char buf[64];

	arg[0] = VPS_DOWNLOAD;
	arg[1] = tmpl;
	arg[2] = NULL;
	val = getenv("HOME");
	if (val != NULL) {
		snprintf(buf, sizeof(buf), "HOME=%s", val);
		env[0] = buf;
	}
	return run_script(VPS_DOWNLOAD, arg, env, 0);
}

struct destroy_ve {
	envid_t veid;
	char *private;
	int layout;
};

static void cleanup_destroy_ve(void *data)
{
	struct destroy_ve *d = data;

	vps_destroy_dir(d->veid, d->private, d->layout);
}

#ifdef HAVE_PLOOP
static void cleanup_umount_ploop(void *data)
{
	vzctl_umount_image((const char *)data);
}
#endif

/* Check if a directory is a mount point
 *
 * Returns:
 *   1: a mount point
 *   0: not a mount point
 *  -1: error
 */
int is_mount_point(const char *dir)
{
	struct stat st1, st2;
	char parent[PATH_MAX];

	if (!dir)
		return -1;

	if (stat(dir, &st1)) {
		if (errno == ENOENT) /* not a mount point */
			return 0;
		logger(-1, errno, "stat(%s)", dir);
		return -1;
	}

	if (!S_ISDIR(st1.st_mode)) {
		logger(-1, 0, "Path %s is not a directory", dir);
		return -1;
	}

	snprintf(parent, sizeof(parent), "%s/..", dir);
	if (stat(parent, &st2)) {
		logger(-1, errno, "stat(%s)", parent);
		return -1;
	}

	return (st1.st_dev != st2.st_dev);
}

static int fs_create(envid_t veid, vps_handler *h, vps_param *vps_p)
{
	char tarball[PATH_LEN];
	char tmp_dir[PATH_LEN];
	char buf[PATH_LEN];
	int ret;
	char *arg[2];
	char *env[6];
	int quota = 0;
	int i;
	char *untar_to;
	const char *ext[] = { "", ".gz", ".bz2", ".xz", NULL };
	const char *errmsg_ext = "[.gz|.bz2|.xz]";
	dq_param *dq = &vps_p->res.dq;
	fs_param *fs = &vps_p->res.fs;
	tmpl_param *tmpl = &vps_p->res.tmpl;
	unsigned long uid_offset = 0;
	unsigned long gid_offset = 0;
	int ploop = (fs->layout == VE_LAYOUT_PLOOP);
	struct destroy_ve ddata;
	struct vzctl_cleanup_handler *ch;
#ifdef HAVE_PLOOP
	struct vzctl_cleanup_handler *ploop_ch = NULL;
#endif

	/*
	 * All other users will test directly for h->can_join_userns.  Create
	 * is special, because we still don't have the container config file
	 * yet, and the user may be requesting it to be disabled in the command
	 * line. So that value may be outdated.
	 *
	 * By now cmd_p is already merged into vps_p. So what we need to do is
	 * just to test it again. We will force it to false if it is disabled
	 * here, or keep the old value otherwise.
	 */
	if (!(vps_p->res.misc.local_uid) || (!(*vps_p->res.misc.local_uid)))
		h->can_join_userns = 0;

	if (h->can_join_userns && vps_p->res.misc.local_uid) {
		uid_offset = *vps_p->res.misc.local_uid;
		if (uid_offset && vps_p->res.misc.local_gid)
			gid_offset = *vps_p->res.misc.local_gid;
	}

	if (ploop && (!dq->diskspace || dq->diskspace[1] <= 0)) {
		logger(-1, 0, "Error: diskspace not set (required for ploop)");
		return VZ_DISKSPACE_NOT_SET;
	}
find:
	for (i = 0; ext[i] != NULL; i++) {
		snprintf(tarball, sizeof(tarball), "%s/cache/%s.tar%s",
				fs->tmpl, tmpl->ostmpl, ext[i]);
		logger(1, 0, "Looking for %s", tarball);
		if (stat_file(tarball) == 1)
			break;
	}
	if (ext[i] == NULL) {
		if (download_template(tmpl->ostmpl) == 0)
			goto find;
		logger(-1, 0, "Cached OS template %s/cache/%s.tar%s not found",
				fs->tmpl, tmpl->ostmpl, errmsg_ext);
		return VZ_OSTEMPLATE_NOT_FOUND;
	}

	/* Check if VE_PRIVATE is not a mount point (#3166) */
	ret = is_mount_point(fs->private);
	if (ret == 1)
		logger(-1, 0, "Can't create: private dir (%s) is a mount "
				"point. Suggestion: use --private %s/private",
				fs->private, fs->private);
	if (ret)
		return VZ_FS_NEW_VE_PRVT;

	/* Lock CT area */
	if (make_dir(fs->private, 1))
		return VZ_FS_NEW_VE_PRVT;
	snprintf(tmp_dir, sizeof(tmp_dir), "%s.tmp", fs->private);
	if (stat_file(tmp_dir) == 1) {
		logger(-1, 0, "Warning: Temp dir %s already exists, deleting",
			tmp_dir);
		if (del_dir(tmp_dir)) {
			ret = VZ_FS_NEW_VE_PRVT;
			goto err;
		}
	}
	if (make_dir(tmp_dir, 1)) {
		logger(-1, errno, "Unable to create directory %s", tmp_dir);
		ret = VZ_FS_NEW_VE_PRVT;
		goto err;
	}
	untar_to = tmp_dir;

	ddata.veid = veid;
	ddata.private = tmp_dir;
	ddata.layout = fs->layout;
	ch = add_cleanup_handler(cleanup_destroy_ve, &ddata);

	if (ploop) {
#ifndef HAVE_PLOOP
		ret = VZ_PLOOP_UNSUP;
		goto err;
#else
		/* Create and mount ploop image */
		struct vzctl_create_image_param param = {};
		struct vzctl_mount_param mount_param = {};
		int ploop_mode = fs->mode;

		if (ploop_mode < 0)
			ploop_mode = PLOOP_EXPANDED_MODE;
		param.mode = ploop_mode;
		param.size = dq->diskspace[1]; // limit
		if (dq->diskinodes)
			param.inodes = dq->diskinodes[1];
		ret = vzctl_create_image(tmp_dir, &param);
		if (ret)
			goto err;

		if (make_dir(fs->root, 1)) {
			logger(-1, 0, "Can't create mount point %s", fs->root);
			ret = VZ_FS_MPOINTCREATE;
			goto err;
		}
		mount_param.target = fs->root;
		mount_param.mount_data = fs->mount_opts;
		ret = vzctl_mount_image(tmp_dir, &mount_param);
		if (ret)
			goto err;

		/* If interrupted, umount ploop */
		ploop_ch = add_cleanup_handler(cleanup_umount_ploop, &tmp_dir);

		untar_to = fs->root;
#endif
	}
	if (!ploop &&
		dq != NULL &&
		dq->enable == YES &&
		dq->diskspace != NULL &&
		dq->diskinodes != NULL)
	{
		if (!quota_ctl(veid, QUOTA_STAT))
			quota_off(veid, 0);
		quota_ctl(veid, QUOTA_DROP);
		quota_init(veid, tmp_dir, dq);
		quota_on(veid, tmp_dir, dq);
		quota = 1;
	}
	arg[0] = VPS_CREATE;
	arg[1] = NULL;
	snprintf(buf, sizeof(buf), "PRIVATE_TEMPLATE=%s", tarball);
	i = 0;
	env[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_PRVT=%s", untar_to);
	env[i++] = strdup(buf);
	if (!is_vz_kernel(h) && h->can_join_userns) {
		snprintf(buf, sizeof(buf), "UID_OFFSET=%lu", uid_offset);
		env[i++] = strdup(buf);
		snprintf(buf, sizeof(buf), "GID_OFFSET=%lu", gid_offset);
		env[i++] = strdup(buf);
	}
	env[i++] = strdup(ENV_PATH);
	env[i] = NULL;
	logger(0, 0, "Creating container private area (%s)", tmpl->ostmpl);
	ret = run_script(VPS_CREATE, arg, env, 0);
	free_arg(env);
#ifdef HAVE_PLOOP
	if (ploop)
		vzctl_umount_image(tmp_dir);
	del_cleanup_handler(ploop_ch);
#endif
	if (ret)
		goto err;
	if (quota) {
		if ((ret = quota_off(veid, 0)) != 0)
			goto err;
		if ((ret = quota_set(veid, fs->private, dq)) != 0)
			goto err;
		quota = 0;
	}
	/* Unlock CT area */
	rmdir(fs->private);
	del_cleanup_handler(ch);
	if (rename(tmp_dir, fs->private)) {
		logger(-1, errno, "Can't rename %s to %s", tmp_dir, fs->private);
		ret = VZ_FS_NEW_VE_PRVT;
	}

err:
	if (ret && quota) {
		quota_off(veid, 0);
		quota_ctl(veid, QUOTA_DROP);
	}
	rmdir(fs->private); /* remove VE_PRIVATE if empty */
	del_dir(tmp_dir);

	return ret;
}

static void cleanup_destroy_file(void *data)
{
	unlink(data);
}

int vps_create(vps_handler *h, envid_t veid, vps_param *vps_p, vps_param *cmd_p,
	struct mod_action *action)
{
	int ret = 0;
	char src[STR_SIZE];
	char dst[STR_SIZE];
	const char *sample_config;
	fs_param *fs = &vps_p->res.fs;
	tmpl_param *tmpl = &vps_p->res.tmpl;
	vps_param *conf_p;
	int cfg_exists;
	char *full_ostmpl;
	struct vzctl_cleanup_handler *ch = NULL;

	get_vps_conf_path(veid, dst, sizeof(dst));
	sample_config = NULL;
	cfg_exists = (stat_file(dst) == 1);
	if (cmd_p->opt.config != NULL) {
		snprintf(src, sizeof(src), VPSCONFDIR "/ve-%s.conf-sample",
			cmd_p->opt.config);
		if (stat_file(src) != 1) {
			logger(-1, errno, "Sample config %s not found", src);
			ret = VZ_CP_CONFIG;
			goto err;
		}
		if (cfg_exists) {
			logger(-1, 0, "Error: container config file %s "
					"already exists, can not use --config "
					"option", dst);
			ret = VZ_CP_CONFIG; /* FIXME */
			goto err;
		}
		sample_config = cmd_p->opt.config;
	} else if (vps_p->opt.config != NULL) {
		snprintf(src, sizeof(src), VPSCONFDIR "/ve-%s.conf-sample",
			vps_p->opt.config);
		/* Bail out if CONFIGFILE is wrong in /etc/vz/vz.conf */
		if (stat_file(src) != 1) {
			logger(-1, errno, "Sample config %s not found", src);
			logger(-1, 0, "Fix the value of CONFIGFILE in "
					GLOBAL_CFG);
			ret = VZ_CP_CONFIG;
			goto err;
		}
		/* Do not use config if CT config exists */
		if (!cfg_exists)
			sample_config = vps_p->opt.config;
		else {
			logger(0, 0, "Warning: CT config file already "
				"exists, not applying a default config "
				"sample.");
			logger(0, 0, "It might lead to incomplete CT "
				"configuration, you can use --applyconfig "
				"to fix.");
		}
	}
	if (sample_config != NULL) {
		ch = add_cleanup_handler(cleanup_destroy_file, &dst);
		if (cp_file(dst, src))
		{
			ret = VZ_CP_CONFIG;
			goto err;
		}
		if ((conf_p = init_vps_param()) == NULL)
		{
			ret = VZ_RESOURCE_ERROR;
			goto err_cfg;
		}
		vps_parse_config(veid, src, conf_p, action);
		merge_vps_param(vps_p, conf_p);
		if (conf_p->opt.origin_sample == NULL)
			cmd_p->opt.origin_sample = strdup(sample_config);
		free_vps_param(conf_p);
	}
	merge_vps_param(vps_p, cmd_p);
	if (check_var(fs->tmpl, "TEMPLATE is not set"))
	{
		ret = VZ_VE_TMPL_NOTSET;
		goto err_cfg;
	}
	if (check_var(fs->private, "VE_PRIVATE is not set"))
	{
		ret = VZ_VE_PRIVATE_NOTSET;
		goto err_cfg;
	}
	if (check_var(fs->root, "VE_ROOT is not set"))
	{
		ret = VZ_VE_ROOT_NOTSET;
		goto err_cfg;
	}
	if (dir_empty(fs->private) != 1) {
		logger(-1, 0, "Private area already exists in %s", fs->private);
		ret = VZ_FS_PRVT_AREA_EXIST;
		goto err_cfg;
	}

	if (vps_p->res.fs.layout == 0) {
#ifdef HAVE_PLOOP
		vps_p->res.fs.layout = VE_LAYOUT_PLOOP;
#else
		vps_p->res.fs.layout = VE_LAYOUT_SIMFS;
#endif
	}

	if (vps_p->res.fs.layout == VE_LAYOUT_PLOOP && !is_ploop_supported()) {
		logger(-1, 0, "Alternatively, if you can't or don't want to use ploop, please");
		logger(-1, 0, "add --layout simfs option, or set VE_LAYOUT=simfs in " GLOBAL_CFG);
		ret = VZ_PLOOP_UNSUP;
		goto err_cfg;
	}

	if (cmd_p->res.name.name) {
		if ((ret = set_name(veid, cmd_p->res.name.name,
					cmd_p->res.name.name)))
			goto err_cfg;
	}

	if (action != NULL && action->mod_count) {
		if ((ret = mod_setup(h, veid, 0, 0, action, vps_p)))
			goto err_cfg;
	} else {
		/* Set default ostemplate if not specified */
		if (cmd_p->res.tmpl.ostmpl == NULL &&
			tmpl->ostmpl == NULL &&
			tmpl->def_ostmpl != NULL)
		{
			tmpl->ostmpl = strdup(tmpl->def_ostmpl);
		}

		if (check_var(tmpl->ostmpl, "OS template is not specified"))
		{
			ret = VZ_VE_OSTEMPLATE_NOTSET;
			goto err_cfg;
		}
		if (stat_file(VZOSTEMPLATE) == 1) {
			full_ostmpl = get_ostemplate_name(tmpl->ostmpl);
			if (full_ostmpl != NULL) {
				free(tmpl->ostmpl);
				tmpl->ostmpl = full_ostmpl;
			}
		}
		ret = fs_create(veid, h, vps_p);
		if (ret)
			goto err_cfg;
	}

	if ((ret = vps_postcreate(veid, &vps_p->res)))
		goto err_root;
	move_config(veid, DESTR);
	/* store root, private, ostemplate in case default used */
	if (cmd_p->res.fs.root_orig == NULL &&
		fs->root_orig != NULL)
	{
		cmd_p->res.fs.root_orig = strdup(fs->root_orig);
	}
	if (cmd_p->res.fs.private_orig == NULL &&
		fs->private_orig != NULL)
	{
		cmd_p->res.fs.private_orig = strdup(fs->private_orig);
	}
	/* Store full ostemplate name */
	if (tmpl->ostmpl != NULL) {
		if (cmd_p->res.tmpl.ostmpl != NULL)
			free(cmd_p->res.tmpl.ostmpl);
		cmd_p->res.tmpl.ostmpl = strdup(basename(tmpl->ostmpl));
	}
	/* For upstream kernel, in case userns is not available
	 * at the time of creation, we should store
	 * LOCAL_UID=0 and LOCAL_GID=0 to CT config,
	 * otherwise it will fail to start.
	 */
	if (!is_vz_kernel(h) && !h->can_join_userns) {
		if (!cmd_p->res.misc.local_uid)
			cmd_p->res.misc.local_uid =
				vz_malloc(sizeof(unsigned long));
		if (!cmd_p->res.misc.local_uid)
			goto err_names;

		if (!cmd_p->res.misc.local_gid)
			cmd_p->res.misc.local_gid =
				vz_malloc(sizeof(unsigned long));
		if (!cmd_p->res.misc.local_gid)
			goto err_names;

		*cmd_p->res.misc.local_uid = 0;
		*cmd_p->res.misc.local_gid = 0;
	}
	if ((ret = vps_save_config(veid, dst, cmd_p, vps_p, action)))
		goto err_names;

	if ((ret = run_pre_script(veid, USER_CREATE_SCRIPT)))
	{
		logger(0, 0, "User create script " USER_CREATE_SCRIPT
				" exited with error");
		goto err_names;
	}

	del_cleanup_handler(ch);
	logger(0, 0, "Container private area was created");
	return 0;

err_names:
	remove_names(veid);
err_root:
	rmdir(fs->root);
	vps_destroy_dir(veid, fs->private, fs->layout);
err_cfg:
	if (sample_config != NULL)
		unlink(dst);
err:
	logger(-1, 0, "Creation of container private area failed");
	return ret;
}

static int vps_postcreate(envid_t veid, vps_res *res)
{
	char buf[STR_SIZE];
	dist_actions actions;
	char *dist_name;
	char *arg[2];
	char *env[3];
	int ret;

	if (check_var(res->fs.root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	dist_name = get_dist_name(&res->tmpl);
	ret = read_dist_actions(dist_name, DIST_DIR, &actions);
	free(dist_name);
	if (ret)
		return ret;
	if (actions.post_create == NULL) {
		ret = 0;
		goto err;
	}
	ret = fsmount(veid, &res->fs, &res->dq, 0);
	if (ret)
		goto err;
	arg[0] = actions.post_create;
	arg[1] = NULL;
	snprintf(buf, sizeof(buf), "VE_ROOT=%s", res->fs.root);
	env[0] = buf;
	env[1] = ENV_PATH;
	env[2] = NULL;
	logger(0, 0, "Performing postcreate actions");
	ret = run_script(actions.post_create, arg, env, 0);
	fsumount(veid, &res->fs);
err:
	free_dist_actions(&actions);
	return ret;
}
