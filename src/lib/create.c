/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
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

#define BACKUP		0
#define DESTR		1
#define VZOSTEMPLATE	"/usr/bin/vzosname"

int destroydir(char *dir);

/* Renames (to "*.destroyed" if action == MOVE) or removes config,
 * (if action == DESTR)
 * Also, appropriate mount/umount scripts are linked.
 */
static int move_config(int veid, int action)
{
	char conf[PATH_LEN];
	char newconf[PATH_LEN];

	snprintf(conf, sizeof(conf), VPS_CONF_DIR "%d.conf", veid);
	snprintf(newconf, sizeof(newconf), "%s." DESTR_PREFIX, conf);
	action == BACKUP ? rename(conf, newconf) : unlink(newconf);

	snprintf(conf, sizeof(conf), VPS_CONF_DIR "%d." MOUNT_PREFIX, veid);
	snprintf(newconf, sizeof(newconf), "%s." DESTR_PREFIX, conf);
	action == BACKUP ? rename(conf, newconf) : unlink(newconf);
	
	snprintf(conf, sizeof(conf), VPS_CONF_DIR "%d." UMOUNT_PREFIX, veid);
	snprintf(newconf, sizeof(newconf), "%s." DESTR_PREFIX, conf);
	action == BACKUP ? rename(conf, newconf) : unlink(newconf);

	return 0;
}

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

static int destroy(envid_t veid, char *dir)
{
        int ret;

	if (!quota_ctl(veid, QUOTA_STAT)) {
		if ((ret = quota_off(veid, 0)))
			if ((ret = quota_off(veid, 1)))
				return ret;
	}
	quota_ctl(veid, QUOTA_DROP);	
	if ((ret = destroydir(dir)))
		return ret;
        return 0;
}

static char *get_ostemplate_name(char *ostmpl)
{
	FILE *fd;
	char buf[STR_SIZE];
	char *p;
	int status;

	snprintf(buf, sizeof(buf), VZOSTEMPLATE " %s", ostmpl);
	if ((fd = popen(buf, "r")) == NULL) {
		logger(0, errno, "Error in popen(%s)", buf);
		return NULL;
	}
	*buf = 0;
	while((p = fgets(buf, sizeof(buf), fd)) != NULL);
	status = pclose(fd);
	if (WEXITSTATUS(status) || *buf == 0) {
		logger(0, 0, "Unable to get full ostemplate name for %s",
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
	
	snprintf(tarball, sizeof(tarball), "%s/%s.tar.gz", fs->tmpl, tar_nm);
	if (!stat_file(tarball)) {
		logger(0, 0, "Cached os template %s not found",	tarball);
		return VZ_PKGSET_NOT_FOUND;
	}
	/* Lock VPS area */
	if (make_dir(fs->private, 0))
		return VZ_FS_NEW_VE_PRVT;
	snprintf(tmp_dir, sizeof(tmp_dir), "%s.tmp", fs->private);
	if (stat_file(tmp_dir)) {
		logger(0, 0, "Warning: Temp dir %s already exists, deleting",
			tmp_dir);
		if (del_dir(tmp_dir)) {
			ret = VZ_FS_NEW_VE_PRVT;
			goto err;
		}
	}
	if (make_dir(tmp_dir, 1)) {
		logger(0, errno, "Unable to create directory %s", tmp_dir);
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
		quota_off(veid, 0);
		quota_set(veid, fs->private, dq);
		quota = 0;
	}
	/* Unlock VPS area */
	rmdir(fs->private);
	if (rename(tmp_dir, fs->private)) {
		logger(0, errno, "Can't rename %s to %s", tmp_dir, fs->private);
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
	int ret;
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
			logger(0, 0, "File %s is not found", src);
			return VZ_CP_CONFIG;
		}
		if (cfg_exists) {
			logger(0, 0, "Warning: VPS config file already exists,"
				" will be rewrited with %s", src);
			unlink(dst);
		}
		sample_config = cmd_p->opt.config;
	} else if (vps_p->opt.config != NULL) {
		snprintf(src, sizeof(src),  VPS_CONF_DIR "ve-%s.conf-sample",
			vps_p->opt.config);
		/* Do not use config if VPS config exists */
		if (!cfg_exists && stat_file(src)) 
			sample_config = vps_p->opt.config;
	}
	if (sample_config != NULL) {
		if (cp_file(dst, src))
			return VZ_CP_CONFIG;
		if ((conf_p = init_vps_param()) == NULL)
			return VZ_RESOURCE_ERROR;
		vps_parse_config(veid, src, conf_p, action);
		merge_vps_param(vps_p, conf_p);
		if (conf_p->opt.origin_sample == NULL)
			cmd_p->opt.origin_sample = strdup(sample_config);
		free_vps_param(conf_p);
	}
	if (check_var(fs->tmpl, "TEMPLATE is not set"))
		return VZ_VE_TMPL_NOTSET;
	if (check_var(fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (check_var(fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (stat_file(fs->private)) {
		logger(0, 0, "Private area already exists in %s", fs->private);
		return VZ_FS_PRVT_AREA_EXIST;
	}
	merge_vps_param(vps_p, cmd_p);
	logger(0, 0, "Creating VPS private area: %s", fs->private);
	if (action != NULL && action->mod_count) {
		ret = mod_setup(h, veid, 0, 0, action, vps_p);
	} else {
		/* Set default ostemplate if not specified */
		if (cmd_p->res.tmpl.ostmpl == NULL &&
			tmpl->ostmpl == NULL &&
			tmpl->def_ostmpl != NULL)
		{
			tmpl->ostmpl = strdup(tmpl->def_ostmpl);
		}
		if (check_var(tmpl->ostmpl, "OS template is not specified"))
			return VZ_VE_PKGSET_NOTSET;
		if (stat_file(VZOSTEMPLATE)) {
			full_ostmpl = get_ostemplate_name(tmpl->ostmpl);
			if (full_ostmpl != NULL) {
				free(tmpl->ostmpl);
				tmpl->ostmpl = full_ostmpl;
			}
		}
		snprintf(tar_nm, sizeof(tar_nm), "cache/%s", tmpl->ostmpl);
		ret = fs_create(veid, fs, tmpl, &vps_p->res.dq, tar_nm);
	}
	if (ret) {
		if (sample_config != NULL)
			unlink(dst);
		destroy(veid, fs->private);
		logger(0, 0, "Creation of VPS private area failed");
		return ret;
	}
	vps_postcreate(veid, &vps_p->res.fs, &vps_p->res.tmpl);
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
	vps_save_config(veid, dst, cmd_p, vps_p, action);
	logger(0, 0, "VPS private area was created");

	return 0;
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
	if ((ret = read_dist_actions(dist_name, DIST_DIR, &actions)))
		return ret;
	if (dist_name != NULL)
		free(dist_name);
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

static char *get_destroy_root(char *dir)
{
	struct stat st;
	int id, len;
	char *p, *prev;
	char tmp[STR_SIZE];

	if (stat(dir, &st) < 0)
		return NULL;
	id = st.st_dev;
	p = dir + strlen(dir) - 1;
	prev = p;
	while (p > dir) {
		while (p > dir && (*p == '/' || *p == '.')) p--;
		while (p > dir && *p != '/') p--;
		if (p <= dir)
			break;
		len = p - dir + 1;
		strncpy(tmp, dir, len);
		tmp[len] = 0;
		if (stat(tmp, &st) < 0)
			return NULL;
		if (id != st.st_dev)
			break;
		prev = p;
	}
	len = prev - dir;
	if (len) {
		strncpy(tmp, dir, len);
		tmp[len] = 0;
		return strdup(tmp);
	}
	return NULL;
}

char *maketmpdir(const char *dir)
{
        char buf[STR_SIZE];
        char *tmp;
        char *tmp_dir;
	int len;

        snprintf(buf, sizeof(buf), "%s/XXXXXXX", dir);
	if ((tmp = mkdtemp(buf)) == NULL) {
		logger(0, errno, "Error in mkdtemp(%s)", buf);
		return NULL;
	}
	len = strlen(dir);
        tmp_dir = (char *)malloc(strlen(tmp) - len);
	if (tmp_dir == NULL)
		return NULL;
        strcpy(tmp_dir, tmp + len + 1);

        return tmp_dir;
}

static void _destroydir(char *root)
{
	char buf[STR_SIZE];
        struct stat st;
        struct dirent *ep;
        DIR *dp;
	int del;

	do {
		if (!(dp = opendir(root)))
			return;
		del = 0;
		while ((ep = readdir(dp))) {
			if (!strcmp(ep->d_name, ".") ||
				!strcmp(ep->d_name, ".."))
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
			system(buf);
			del = 1;
		}
		closedir(dp);
	} while(del);
}

int destroydir(char *dir)
{
	char buf[STR_SIZE];
	char tmp[STR_SIZE];
	char *root;
	char *tmp_nm;
	int fd_lock, pid;
	struct sigaction act, actold;
	int ret = 0;
	struct stat st;

	if (stat(dir, &st)) {
		if (errno != ENOENT) {
			logger(0, errno, "Unable to stat %s", dir);
			return -1;
		}
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		logger(0, 0, "Warning: VPS private area is not a directory");
		if (unlink(dir)) {
			logger(0, errno, "Unable to unlink %s", dir);
			return -1;
		}
		return 0;
	}
	root = get_destroy_root(dir);
	if (root == NULL) {
		logger(0, 0, "Unable to get root for %s", dir);
		return -1;
	}
	snprintf(tmp, sizeof(buf), "%s/tmp", root);
	free(root);
	if (!stat_file(tmp)) {
		if (mkdir(tmp, 0755)) {
			logger(0, errno, "Can't create tmp dir %s", tmp);
			return VZ_FS_DEL_PRVT;
		}
	}
	/* First move to del */
	if ((tmp_nm = maketmpdir(tmp)) == NULL)	{
		logger(0, 0, "Unable to generate temporary name in %s", tmp);
		return VZ_FS_DEL_PRVT;
	}
	snprintf(buf, sizeof(tmp), "%s/%s", tmp, tmp_nm);
	free(tmp_nm);
	if (rename(dir, buf)) {
		logger(0, errno, "Can't move %s -> %s", dir, buf);
		rmdir(buf);
		return VZ_FS_DEL_PRVT;
	}
	snprintf(buf, sizeof(buf), "%s/rm.lck", tmp);
	if ((fd_lock = _lock(buf, 0)) == -2) {
		/* Already locked */
		_unlock(fd_lock, NULL);
		return 0;
	} else if (fd_lock == -1)
		return VZ_FS_DEL_PRVT;
	
	sigaction(SIGCHLD, NULL, &actold);
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	if (!(pid = fork())) {
		int fd;

		setsid();
		fd = open("/dev/null", O_WRONLY);
		if (fd != -1) {
			close(0);
			close(1);
			close(2);
			dup2(fd, 1);
			dup2(fd, 2);
		}
		_destroydir(tmp);
		_unlock(fd_lock, buf);
		exit(0);
	} else if (pid < 0)  {
		logger(0, errno, "destroydir: Unable to fork");
		ret = VZ_RESOURCE_ERROR;
	}
	sleep(1);
	sigaction(SIGCHLD, &actold, NULL);
	return ret;
}

int vps_destroy(vps_handler *h, envid_t veid, fs_param *fs)
{
	int ret;
	
	if (check_var(fs->private, "VE_PRIVATE is not set"))
		return VZ_VE_PRIVATE_NOTSET;
	if (check_var(fs->root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (vps_is_mounted(fs->root)) {
		logger(0, 0, "VPS is currently mounted (umount first)");
                return VZ_FS_MOUNTED;
	}
	logger(0, 0, "Destroying VPS private area: %s", fs->private);
	if ((ret = destroy(veid, fs->private)))
		return ret;
	move_config(veid, BACKUP);
	rmdir(fs->root);
	logger(0, 0, "VPS private area was destroyed");

	return 0;
}
