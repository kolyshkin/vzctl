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
#include "config.h"
#include "vzerror.h"
#include "util.h"
#include "script.h"
#include "lock.h"
#include "modules.h"
#include "create.h"
#include "destroy.h"

#define VZOSTEMPLATE	"/usr/bin/vzosname"

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
	while((p = fgets(buf, sizeof(buf), fd)) != NULL);
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

int fs_create(envid_t veid, fs_param *fs, tmpl_param *tmpl, dq_param *dq,
	char *tar_nm)
{
	char tarball[PATH_LEN];
	char tmp_dir[PATH_LEN];
	char buf[PATH_LEN];
	int ret;
	char *arg[2];
	char *env[4];
	int quota = 0;

	snprintf(tarball, sizeof(tarball), "%s/%s.tar", fs->tmpl, tar_nm);
	if (!stat_file(tarball))
		snprintf(tarball, sizeof(tarball), "%s/%s.tar.gz", fs->tmpl, tar_nm);
	if (!stat_file(tarball)) {
		logger(-1, 0, "Cached OS template %s not found", tarball);
		return VZ_OSTEMPLATE_NOT_FOUND;
	}
	/* Lock CT area */
	if (make_dir(fs->private, 0))
		return VZ_FS_NEW_VE_PRVT;
	snprintf(tmp_dir, sizeof(tmp_dir), "%s.tmp", fs->private);
	if (stat_file(tmp_dir)) {
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
	if (dq != NULL &&
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
	env[0] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_PRVT=%s", tmp_dir);
	env[1] = strdup(buf);
	env[2] = strdup(ENV_PATH);
	env[3] = NULL;
	ret = run_script(VPS_CREATE, arg, env, 0);
	free_arg(env);
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
	if (rename(tmp_dir, fs->private)) {
		logger(-1, errno, "Can't rename %s to %s", tmp_dir, fs->private);
		ret = VZ_FS_NEW_VE_PRVT;
	}

err:
	if (ret && quota) {
		quota_off(veid, 0);
		quota_ctl(veid, QUOTA_DROP);
	}
	rmdir(fs->private);
	rmdir(tmp_dir);

	return ret;
}

int vps_create(vps_handler *h, envid_t veid, vps_param *vps_p, vps_param *cmd_p,
	struct mod_action *action)
{
	int ret = 0;
	char tar_nm[256];
	char src[STR_SIZE];
	char dst[STR_SIZE];
	const char *sample_config;
	fs_param *fs = &vps_p->res.fs;
	tmpl_param *tmpl = &vps_p->res.tmpl;
	vps_param *conf_p;
	int cfg_exists;
	char *full_ostmpl;

	get_vps_conf_path(veid, dst, sizeof(dst));
	sample_config = NULL;
	cfg_exists = 0;
	if (stat_file(dst))
		cfg_exists = 1;
	if (cmd_p->opt.config != NULL) {
		snprintf(src, sizeof(src),  VPS_CONF_DIR "ve-%s.conf-sample",
			cmd_p->opt.config);
		if (!stat_file(src)) {
			logger(-1, 0, "File %s is not found", src);
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
		snprintf(src, sizeof(src),  VPS_CONF_DIR "ve-%s.conf-sample",
			vps_p->opt.config);
		/* Bail out if CONFIGFILE is wrong in /etc/vz/vz.conf */
		if (!stat_file(src)) {
			logger(-1, errno, "File %s not found", src);
			logger(-1, 0, "Fix the value of CONFIGFILE in "
					GLOBAL_CFG);
			ret = VZ_CP_CONFIG;
			goto err;
		}
		/* Do not use config if CT config exists */
		if (!cfg_exists)
			sample_config = vps_p->opt.config;
	}
	if (sample_config != NULL) {
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
	else if (check_var(fs->private, "VE_PRIVATE is not set"))
	{
		ret = VZ_VE_PRIVATE_NOTSET;
		goto err_cfg;
	}
	else if (check_var(fs->root, "VE_ROOT is not set"))
	{
		ret = VZ_VE_ROOT_NOTSET;
		goto err_cfg;
	}
	else if (stat_file(fs->private)) {
		logger(-1, 0, "Private area already exists in %s", fs->private);
		ret = VZ_FS_PRVT_AREA_EXIST;
		goto err_cfg;
	}

	if (cmd_p->res.name.name) {
		if ((ret = set_name(veid, cmd_p->res.name.name,
					cmd_p->res.name.name)))
			goto err_cfg;
	}

	if (action != NULL && action->mod_count) {
		if ((ret = mod_setup(h, veid, 0, 0, action, vps_p)))
			goto err_private;
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
		if (stat_file(VZOSTEMPLATE)) {
			full_ostmpl = get_ostemplate_name(tmpl->ostmpl);
			if (full_ostmpl != NULL) {
				free(tmpl->ostmpl);
				tmpl->ostmpl = full_ostmpl;
			}
		}
		snprintf(tar_nm, sizeof(tar_nm), "cache/%s", tmpl->ostmpl);
		logger(0, 0, "Creating container private area (%s)",
				tmpl->ostmpl);
		if ((ret = fs_create(veid, fs, tmpl, &vps_p->res.dq, tar_nm)))
			goto err_private;
	}

	if ((ret = vps_postcreate(veid, &vps_p->res.fs, &vps_p->res.tmpl)))
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
		cmd_p->res.tmpl.ostmpl = strdup(tmpl->ostmpl);
	}
	if ((ret = vps_save_config(veid, dst, cmd_p, vps_p, action)))
		goto err_names;

	if ((ret = run_pre_script(veid, USER_CREATE_SCRIPT)))
	{
		logger(0, 0, "User create script " USER_CREATE_SCRIPT
				" exited with error");
		goto err_names;
	}

	logger(0, 0, "Container private area was created");
	return 0;

err_names:
	remove_names(veid);
err_root:
	rmdir(fs->root);
err_private:
	vps_destroy_dir(veid, fs->private);
err_cfg:
	if (sample_config != NULL)
		unlink(dst);
err:
	logger(-1, 0, "Creation of container private area failed");
	return ret;
}

int vps_postcreate(envid_t veid, fs_param *fs, tmpl_param *tmpl)
{
	char buf[STR_SIZE];
	dist_actions actions;
	char *dist_name;
	char *arg[2];
	char *env[3];
	int ret;

	if (check_var(fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	dist_name = get_dist_name(tmpl);
	ret = read_dist_actions(dist_name, DIST_DIR, &actions);
	free(dist_name);
	if (ret)
		return ret;
	if (actions.post_create == NULL) {
		ret = 0;
		goto err;
	}
	ret = fsmount(veid, fs, NULL);
	if (ret)
		goto err;
	arg[0] = actions.post_create;
	arg[1] = NULL;
	snprintf(buf, sizeof(buf), "VE_ROOT=%s", fs->root);
	env[0] = buf;
	env[1] = ENV_PATH;
	env[2] = NULL;
	logger(0, 0, "Performing postcreate actions");
	ret = run_script(actions.post_create, arg, env, 0);
	fsumount(veid, fs->root);
err:
	free_dist_actions(&actions);
	return ret;
}
