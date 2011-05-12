/*
 *  Copyright (C) 2000-2011, Parallels, Inc. All rights reserved.
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
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <grp.h>
#include <sys/ioctl.h>
#include <linux/vzcalluser.h>
#include <sys/personality.h>
#include <linux/reboot.h>
#include <sys/mount.h>

#include "vzerror.h"
#include "res.h"
#include "env.h"
#include "dist.h"
#include "exec.h"
#include "logger.h"
#include "util.h"
#include "script.h"
#include "iptables.h"
#include "readelf.h"
#include "vzsyscalls.h"
#include "cpt.h"

#define ENVRETRY	3

static int env_stop(vps_handler *h, envid_t veid, const char *root,
		int stop_mode);

static inline int setluid(uid_t uid)
{
	return syscall(__NR_setluid, uid);
}

#ifdef  __x86_64__
static int set_personality(unsigned long mask)
{
	unsigned long per;

	per = personality(0xffffffff) | mask;
	logger(3, 0, "Set personality %#10.8lx", per);
	if (personality(per) == -1) {
		logger(-1, errno, "Unable to set personality PER_LINUX32");
		return  -1;
	}
	return 0;
}

static int set_personality32()
{
	if (get_arch_from_elf("/sbin/init") != elf_32)
		return 0;
	return set_personality(PER_LINUX32);
}
#endif

int vz_env_create_data_ioctl(vps_handler *h,
	struct vzctl_env_create_data *data)
{
	int errcode;
	int retry = 0;

	do {
		if (retry)
			sleep(1);
		errcode = ioctl(h->vzfd, VZCTL_ENV_CREATE_DATA, data);
	} while (errcode < 0 && errno == EBUSY && retry++ < ENVRETRY);

	if (errcode >= 0) {
		/* Clear supplementary group IDs */
		setgroups(0, NULL);
#ifdef  __x86_64__
		/* Set personality PER_LINUX32 for i386 based CTs */
		set_personality32();
#endif
	}
	return errcode;
}

int vz_env_create_ioctl(vps_handler *h, envid_t veid, int flags)
{
	struct vzctl_env_create env_create;
	int errcode;
	int retry = 0;

	memset(&env_create, 0, sizeof(env_create));
	env_create.veid = veid;
	env_create.flags = flags;
	do {
		if (retry)
			sleep(1);
		errcode = ioctl(h->vzfd, VZCTL_ENV_CREATE, &env_create);
	} while (errcode < 0 && errno == EBUSY && retry++ < ENVRETRY);
	if (errcode >= 0 && (flags & VE_ENTER)) {
		/* Clear supplementary group IDs */
		setgroups(0, NULL);
#ifdef  __x86_64__
		/* Set personality PER_LINUX32 for i386 based CTs */
		set_personality32();
#endif
	}
	return errcode;
}

/*
 * Reset standard file descriptors to /dev/null in case they are closed.
 */
static int reset_std()
{
	int ret, i, stdfd;

	stdfd = -1;
	for (i = 0; i < 3; i++) {
		ret = fcntl(i, F_GETFL);
		if (ret < 0 && errno == EBADF) {
			if (stdfd < 0) {
				if ((stdfd = open("/dev/null", O_RDWR)) < 0)
					return -1;
			}
			dup2(stdfd, i);
		}
	}
	return stdfd;
}

/** Allocate and initialize CT handler.
 *
 * @param veid		CT ID.
 * @return		handler or NULL on error.
 */
vps_handler *vz_open(envid_t veid)
{
	int vzfd, stdfd;
	vps_handler *h = NULL;

	stdfd = reset_std();
	if ((vzfd = open(VZCTLDEV, O_RDWR)) < 0) {
		logger(-1, errno, "Unable to open %s", VZCTLDEV);
		logger(-1, 0, "Please check that vzdev kernel module is loaded"
			" and you have sufficient permissions"
			" to access the file.");
		goto err;
	}
	h = calloc(1, sizeof(*h));
	if (h == NULL)
		goto err;
	h->vzfd = vzfd;
	h->stdfd = stdfd;
	if (vz_env_create_ioctl(h, 0, 0) < 0 &&
		(errno == ENOSYS || errno == EPERM))
	{
		logger(-1, 0, "Your kernel lacks support for virtual"
			" environments or modules not loaded");
		goto err;
	}
	return h;

err:
	free(h);
	if (vzfd != -1)
		close(vzfd);
	if (stdfd != -1)
		close(stdfd);
	return NULL;
}

/** Close CT handler.
 *
 * @param h		CT handler.
 */
void vz_close(vps_handler *h)
{
	if (h == NULL)
		return;
	close(h->vzfd);
	if (h->stdfd != -1)
		close(h->stdfd);
	free(h);
}

/** Get CT status.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @return		1 - CT is running
 *			0 - CT is stopped.
 */
int vps_is_run(vps_handler *h, envid_t veid)
{
	int ret;
	int errno;

	if (veid == 0)
		return 1;
	ret = vz_env_create_ioctl(h, veid, VE_TEST);

	if (ret < 0 && (errno == ESRCH || errno == ENOTTY))
		return 0;
	else if (ret < 0)
		logger(-1, errno, "error on vz_env_create_ioctl(VE_TEST)");
	return 1;
}

/** Change root to specified directory
 *
 * @param		CT root
 * @return		0 on success
 */
int vz_chroot(const char *root)
{
	int i;
	sigset_t sigset;
	struct sigaction act;

	if (root == NULL) {
		logger(-1, 0, "vz_chroot: Container root (VE_ROOT) "
				"not specified");
		return VZ_VE_ROOT_NOTSET;
	}
	if (chdir(root)) {
		logger(-1, errno, "unable to change dir to %s",
			root);
		return VZ_RESOURCE_ERROR;
	}
	if (chroot(root)) {
		logger(-1, errno, "chroot %s failed", root);
		return VZ_RESOURCE_ERROR;
	}
	setsid();
	sigemptyset(&sigset);
	sigprocmask(SIG_SETMASK, &sigset, NULL);
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_DFL;
	act.sa_flags = 0;
	for (i = 1; i <= NSIG; ++i)
		sigaction(i, &act, NULL);
	return 0;
}

int vz_setluid(envid_t veid)
{
	if (setluid(veid) == -1) {
		if (errno == ENOSYS)
			logger(-1, 0, "Error: kernel does not support"
				" user resources. Please, rebuild with"
				" CONFIG_USER_RESOURCE=y");
		return VZ_SETLUID_ERROR;
	}
	return 0;
}

static int vz_env_configure(int fd, envid_t veid, const char *osrelease)
{
	int ret = 0;
	struct vzctl_ve_configure *cparam;
	int len;

	len = strlen(osrelease) + 1;
	cparam = calloc(1, sizeof(struct vzctl_ve_configure) + len);
	if (cparam == NULL)
		return VZ_RESOURCE_ERROR;

	cparam->veid = veid;
	cparam->key = VE_CONFIGURE_OS_RELEASE;
	cparam->size = len;
	strcpy(cparam->data, osrelease);

	if (ioctl(fd, VZCTL_VE_CONFIGURE, cparam) != 0)
		if (errno != ENOTTY)
			ret = VZ_SET_OSRELEASE;

	free(cparam);

	return ret;
}

static int _env_create(vps_handler *h, envid_t veid, int wait_p, int err_p,
	void *data)
{
	struct vzctl_env_create_data env_create_data;
	struct env_create_param3 create_param;
	int fd, ret;
	vps_res *res;
	char *argv[] = {"init", "-z", "      ", NULL};
	char *envp[] = {"HOME=/", "TERM=linux", NULL};

	res = (vps_res *) data;
	memset(&create_param, 0, sizeof(create_param));
	create_param.iptables_mask = get_ipt_mask(res->env.ipt_mask);
	logger(3, 0, "Set iptables mask %#10.8llx",
			(unsigned long long) create_param.iptables_mask);
	clean_hardlink_dir("/");
	if (res->cpu.vcpus != NULL)
		create_param.total_vcpus = *res->cpu.vcpus;
	env_create_data.veid = veid;
	env_create_data.class_id = 0;
	env_create_data.flags = VE_CREATE | VE_EXCLUSIVE;
	env_create_data.data = &create_param;
	env_create_data.datalen = sizeof(create_param);

	create_param.feature_mask = res->env.features_mask;
	create_param.known_features = res->env.features_known;
	/* sysfs enabled by default, unless explicitly disabled */
	if (! (res->env.features_known & VE_FEATURE_SYSFS)) {
		create_param.feature_mask |= VE_FEATURE_SYSFS;
		create_param.known_features |= VE_FEATURE_SYSFS;
	}
	logger(3, 0, "Set features mask %016Lx/%016Lx",
			create_param.feature_mask,
			create_param.known_features);

	/* Close all fds except stdin. stdin is status pipe */
	close(STDERR_FILENO); close(STDOUT_FILENO);
	close_fds(0, wait_p, err_p, h->vzfd, -1);
try:
	ret = vz_env_create_data_ioctl(h, &env_create_data);
	if (ret < 0) {
		switch(errno) {
			case EINVAL:
				ret = VZ_ENVCREATE_ERROR;
				/* Run-time kernel did not understand the
				 * latest create_param -- so retry with
				 * the old env_create_param structs.
				 */
				switch (env_create_data.datalen) {
				case sizeof(struct env_create_param3):
					env_create_data.datalen =
						sizeof(struct env_create_param2);
					goto try;
				case sizeof(struct env_create_param2):
					env_create_data.datalen =
						sizeof(struct env_create_param);
					goto try;
				}
				break;
			case EACCES:
			/* License is not loaded */
				ret = VZ_NO_ACCES;
				break;
			case ENOTTY:
			/* Some vz modules are not present */
				ret = VZ_BAD_KERNEL;
				break;
			default:
				logger(-1, errno, "env_create error");
				ret = VZ_ENVCREATE_ERROR;
				break;
		}
		goto env_err;
	}
	if (res->env.osrelease != NULL) {
		ret = vz_env_configure(h->vzfd, veid,
				res->env.osrelease);
		if (ret != 0)
			goto env_err;
	}

	close(h->vzfd);
	/* Create /fastboot to skip run fsck */
	fd = open("/fastboot", O_CREAT | O_RDONLY, 0644);
	close(fd);

	if (res->misc.wait == YES) {
		if (add_reach_runlevel_mark()) {
			ret = VZ_WAIT_FAILED;
			goto env_err;
		}
	}

	mount("proc", "/proc", "proc", 0, 0);
	if (stat_file("/sys"))
		mount("sysfs", "/sys", "sysfs", 0, 0);

	if (create_param.feature_mask & VE_FEATURE_NFSD) {
		mount("nfsd", "/proc/fs/nfsd", "nfsd", 0, 0);
		make_dir("/var/lib/nfs/rpc_pipefs", 1);
		mount("sunrpc", "/var/lib/nfs/rpc_pipefs", "rpc_pipefs", 0, 0);
	}

	if (res->dq.ugidlimit != NULL)
		mk_quota_link();

	/* Close status descriptor to report that
	 * environment is created.
	*/
	close(STDIN_FILENO);
	/* Now we wait until CT setup will be done
	   If no error, then start init, otherwise exit.
	*/
	if (read(wait_p, &ret, sizeof(ret)) == 0)
		return 0;
	if ((fd = open("/dev/null", O_RDWR)) != -1) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
	}
	logger(10, 0, "Starting init");
	execve("/sbin/init", argv, envp);
	execve("/etc/init", argv, envp);
	execve("/bin/init", argv, envp);

	ret = VZ_FS_BAD_TMPL;
	write(err_p, &ret, sizeof(ret));
env_err:
	return ret;
}

static int vz_real_env_create(vps_handler *h, envid_t veid, vps_res *res,
	int wait_p, int old_wait_p, int err_p, env_create_FN fn, void *data)
{
	int ret, pid;

	if ((ret = vz_chroot(res->fs.root)))
		return ret;
	if ((ret = vz_setluid(veid)))
		return ret;
	if ((ret = setup_resource_management(h, veid, res)))
		return ret;
	/* Create another process for proper resource accounting */
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Unable to fork");
		return VZ_RESOURCE_ERROR;
	} else if (pid == 0) {
		if ((ret = vps_set_cap(veid, &res->env, &res->cap)))
			goto env_err;
		if (fn == NULL) {
			ret = _env_create(h, veid, wait_p, err_p, (void *)res);
		} else {
			ret = fn(h, veid, wait_p, old_wait_p, err_p, data);
		}
env_err:
		if (ret)
			write(STDIN_FILENO, &ret, sizeof(ret));
		exit(ret);
	}
	return 0;
}

int vz_env_create(vps_handler *h, envid_t veid, vps_res *res,
		int wait_p[2], int old_wait_p[2], int err_p[2],
				env_create_FN fn, void *data)
{
	int ret, pid, errcode;
	int old_wait_fd;
	int status_p[2];
	struct sigaction act, actold;

	if (check_var(res->fs.root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (pipe(status_p) < 0) {
		logger(-1, errno, "Can not create pipe");
		return VZ_RESOURCE_ERROR;
	}
	sigaction(SIGCHLD, NULL, &actold);
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	get_osrelease(res);

	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can not fork");
		ret =  VZ_RESOURCE_ERROR;
		goto err;
	} else if (pid == 0) {
		dup2(status_p[1], STDIN_FILENO);
		close(status_p[0]);
		close(status_p[1]);
		fcntl(STDIN_FILENO, F_SETFD, FD_CLOEXEC);
		fcntl(err_p[1], F_SETFD, FD_CLOEXEC);
		close(err_p[0]);
		fcntl(wait_p[0], F_SETFD, FD_CLOEXEC);
		close(wait_p[1]);
		if (old_wait_p) {
			fcntl(old_wait_p[0], F_SETFD, FD_CLOEXEC);
			close(old_wait_p[1]);
			old_wait_fd = old_wait_p[0];
		} else
			old_wait_fd = -1;

		ret = vz_real_env_create(h, veid, res, wait_p[0],
					old_wait_fd, err_p[1], fn, data);
		if (ret)
			write(STDIN_FILENO, &ret, sizeof(ret));
		exit(ret);
	}
	/* Wait for environment created */
	close(status_p[1]);
	close(wait_p[0]);
	if (old_wait_p)
		close(old_wait_p[0]);
	close(err_p[1]);
	ret = read(status_p[0], &errcode, sizeof(errcode));
	if (ret > 0) {
		ret = errcode;
		switch(ret) {
		case VZ_NO_ACCES:
			logger(-1, 0, "Permission denied");
			break;
		case VZ_BAD_KERNEL:
			logger(-1, 0, "Invalid kernel, or some kernel"
				" modules are not loaded");
			break;
		case VZ_SET_CAP:
			logger(-1, 0, "Unable to set capability");
			break;
		case VZ_RESOURCE_ERROR:
			logger(-1, 0, "Not enough resources"
				" to start environment");
			break;
		case VZ_WAIT_FAILED:
			logger(0, 0, "Unable to set"
				" wait functionality");
			break;
		case VZ_SET_OSRELEASE:
			logger(-1, 0, "Unable to set osrelease to %s",
					res->env.osrelease);
			break;
		}
	}
err:
	close(status_p[1]);
	close(status_p[0]);
	sigaction(SIGCHLD, &actold, NULL);

	return ret;
}

static void fix_numiptent(ub_param *ub)
{
	unsigned long min_ipt;

	if (ub->numiptent == NULL)
		return;
	min_ipt = min_ul(ub->numiptent[0], ub->numiptent[1]);
	if (min_ipt < MIN_NUMIPTENT) {
		logger(-1, 0, "Warning: NUMIPTENT %lu:%lu is less"
			" than minimally allowable value, set to %d:%d",
			ub->numiptent[0], ub->numiptent[1],
			MIN_NUMIPTENT, MIN_NUMIPTENT);
		ub->numiptent[0] = MIN_NUMIPTENT;
		ub->numiptent[1] = MIN_NUMIPTENT;
	}
}

static void fix_cpu(cpu_param *cpu)
{
	if (cpu->units == NULL && cpu->weight == NULL) {
		cpu->units = malloc(sizeof(*cpu->units));
		*cpu->units = UNLCPUUNITS;
	}
}

int vps_start_custom(vps_handler *h, envid_t veid, vps_param *param,
	skipFlags skip, struct mod_action *mod,
	env_create_FN fn, void *data)
{
	int wait_p[2];
	int old_wait_p[2];
	int err_p[2];
	int ret, err;
	char buf[64];
	char *dist_name;
	struct sigaction act;
	vps_res *res = &param->res;
	dist_actions actions;

	memset(&actions, 0, sizeof(actions));
	if (check_var(res->fs.root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "Container is already running");
		return VZ_VE_RUNNING;
	}
	if ((ret = check_ub(&res->ub)))
		return ret;
	dist_name = get_dist_name(&res->tmpl);
	ret = read_dist_actions(dist_name, DIST_DIR, &actions);
	free(dist_name);
	if (ret)
		return ret;
	logger(0, 0, "Starting container ...");
	if (vps_is_mounted(res->fs.root)) {
		/* if CT is mounted -- umount first, to cleanup mount state */
		vps_umount(h, veid, res->fs.root, skip);
	}
	if (!vps_is_mounted(res->fs.root)) {
		/* increase quota to perform setup */
		quota_inc(&res->dq, 100);
		if ((ret = vps_mount(h, veid, &res->fs, &res->dq, skip)))
			return ret;
		quota_inc(&res->dq, -100);
	}
	if (pipe(wait_p) < 0) {
		logger(-1, errno, "Can not create pipe");
		return VZ_RESOURCE_ERROR;
	}
	/* old_wait_p is needed for backward compatibility with older kernels,
	 * while for recent ones (that support CPT_SET_LOCKFD2) we use wait_p.
	 *
	 * If old_wait_p is closed without writing any data, it's "OK to go"
	 * signal, and if data are received from old_wait_p it's "no go"
	 * signal". Note that such thing doesn't work if vzctl segfaults,
	 * because in this case the descriptor will be closed without
	 * sending data.
	 */
	if (pipe(old_wait_p) < 0) {
		logger(-1, errno, "Can not create pipe");
		return VZ_RESOURCE_ERROR;
	}
	if (pipe(err_p) < 0) {
		close(wait_p[0]);
		close(wait_p[1]);
		logger(-1, errno, "Can not create pipe");
		return VZ_RESOURCE_ERROR;
	}
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);
	fix_numiptent(&res->ub);
	fix_cpu(&res->cpu);

	ret = vz_env_create(h, veid, res, wait_p,
				old_wait_p, err_p, fn, data);
	if (ret)
		goto err;

	if ((ret = vps_setup_res(h, veid, &actions, &res->fs, param,
		STATE_STARTING, skip, mod)))
	{
		goto err;
	}
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), VPS_CONF_DIR "%d.%s", veid,
			START_PREFIX);
		if (stat_file(buf)) {
			if (vps_exec_script(h, veid, res->fs.root, NULL, NULL,
				buf, NULL, 0))
			{
				ret = VZ_ACTIONSCRIPT_ERROR;
				goto err;
			}
		}
	}
	/* Tell the child that it's time to start /sbin/init */
	err = 0;
	if (write(wait_p[1], &err, sizeof(err)) != sizeof(err))
		logger(-1, errno, "Unable to write to waitfd to start init");
	close(wait_p[1]);
	close(old_wait_p[1]);
err:
	free_dist_actions(&actions);
	if (ret) {
		/* Kill environment */
		logger(-1, 0, "Container start failed (try to check kernel "
				"messages, e.g. \"dmesg | tail\")");
		/* Close wait fd without writing anything to it
		 * to signal the child that we have failed to configure
		 * the environment, so it should not start /sbin/init
		 */
		close(wait_p[1]);
		write(old_wait_p[1], &err, sizeof(err));
		close(old_wait_p[1]);
	} else {
		if (!read(err_p[0], &ret, sizeof(ret))) {
			if (res->misc.wait == YES) {
				logger(0, 0, "Container start in progress"
					", waiting ...");
				err = vps_execFn(h, veid, res->fs.root,
					wait_on_fifo, NULL, 0);
				if (err) {
					logger(0, 0, "Container wait failed%s",
						err == VZ_EXEC_TIMEOUT ? \
						" - timeout expired" : "");
					ret = VZ_WAIT_FAILED;
				} else {
					logger(0, 0, "Container started"
						" successfully");
				}
			} else {
				logger(0, 0, "Container start in progress...");
			}
		} else {
			if (ret == VZ_FS_BAD_TMPL)
				logger(-1, 0, "Unable to start init, probably"
					" incorrect template");
			logger(-1, 0, "Container start failed");
		}
	}
	if (ret) {
		if (vps_is_run(h, veid))
			env_stop(h, veid, res->fs.root, M_KILL);
		/* restore original quota values */
		vps_set_quota(veid, &res->dq);
		if (vps_is_mounted(res->fs.root))
			vps_umount(h, veid, res->fs.root, skip);
	}
	close(wait_p[0]);
	close(wait_p[1]);
	close(err_p[0]);
	close(err_p[1]);

	return ret;
}

/** Start and configure CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param param		CT parameters.
 * @param skip		flags to skip CT setup (SKIP_SETUP|SKIP_ACTION_SCRIPT)
 * @param mod		modules list, used to call setup() callback
 * @return		0 on success.
 */
int vps_start(vps_handler *h, envid_t veid, vps_param *param,
	skipFlags skip, struct mod_action *mod)
{
	return vps_start_custom(h, veid, param, skip, mod, NULL, NULL);
}

static int real_env_stop(vps_handler *h, envid_t veid, const char *vps_root,
	int stop_mode)
{
	int ret;

	if ((ret = vz_chroot(vps_root)))
		return ret;
	if ((ret = vz_setluid(veid)))
		return ret;
	close_fds(1, h->vzfd, -1);
	if ((ret = vz_env_create_ioctl(h, veid, VE_ENTER)) < 0) {
		if (errno == ESRCH)
			return 0;
		logger(-1, errno, "Container enter failed");
		return ret;
	}
	close(h->vzfd);
	switch (stop_mode) {
		case M_REBOOT:
		{
			char *argv[] = {"reboot", NULL};
			execvep(argv[0], argv, NULL);
			break;
		}
		case M_HALT:
		{
			char *argv[] = {"halt", NULL};
			execvep(argv[0], argv, NULL);
			break;
		}
		case M_KILL:
		{
			syscall(__NR_reboot, LINUX_REBOOT_MAGIC1,
					LINUX_REBOOT_MAGIC2,
					LINUX_REBOOT_CMD_POWER_OFF, NULL);
			break;
		}
	}
	return 0;
}

static int env_stop(vps_handler *h, envid_t veid, const char *root,
		int stop_mode)
{
	struct sigaction act, actold;
	int i, pid, ret = 0;

	sigaction(SIGCHLD, NULL, &actold);
	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	logger(0, 0, "Stopping container ...");
	if (stop_mode == M_KILL)
		goto kill_vps;
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can not fork");
		ret = VZ_RESOURCE_ERROR;
		goto out;
	} else if (pid == 0) {
		ret = real_env_stop(h, veid, root, stop_mode);
		exit(ret);
	}
	for (i = 0; i < MAX_SHTD_TM; i++) {
		sleep(1);
		if (!vps_is_run(h, veid)) {
			ret = 0;
			goto out;
		}
	}

kill_vps:
	if ((pid = fork()) < 0) {
		ret = VZ_RESOURCE_ERROR;
		logger(-1, errno, "Can not fork");
		goto err;

	} else if (pid == 0) {
		ret = real_env_stop(h, veid, root, M_KILL);
		exit(ret);
	}
	ret = VZ_STOP_ERROR;
	for (i = 0; i < MAX_SHTD_TM; i++) {
		usleep(500000);
		if (!vps_is_run(h, veid)) {
			ret = 0;
			break;
		}
	}
out:
	if (ret)
		logger(-1, 0, "Unable to stop container: operation timed out");
	else
		logger(0, 0, "Container was stopped");
err:
	sigaction(SIGCHLD, &actold, NULL);
	return ret;
}

/** Stop CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param param		CT parameters.
 * @param stop_mode	stop mode, one of (M_REBOOT M_HALT M_KILL).
 * @param skip		flag to skip run action script (SKIP_ACTION_SCRIPT)
 * @param action	modules list, used to call cleanup() callback.
 * @return		0 on success.
 */
int vps_stop(vps_handler *h, envid_t veid, vps_param *param, int stop_mode,
	skipFlags skip, struct mod_action *action)
{
	int ret;
	char buf[64];
	vps_res *res = &param->res;

	if (check_var(res->fs.root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to stop: container is not running");
		return 0;
	}
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), VPS_CONF_DIR "%d.%s", veid,
			STOP_PREFIX);
		if (stat_file(buf)) {
			if (vps_exec_script(h, veid, res->fs.root, NULL, NULL,
				buf, NULL, 0))
			{
				return VZ_ACTIONSCRIPT_ERROR;
			}
		}
	}
	/* get CT IP addresses for cleanup */
	get_vps_ip(h, veid, &param->del_res.net.ip);
	if ((ret = env_stop(h, veid, res->fs.root, stop_mode)))
		goto end;
	mod_cleanup(h, veid, action, param);
	/* Cleanup CT IPs */
	run_net_script(veid, DEL, &param->del_res.net.ip,
			STATE_STOPPING, param->res.net.skip_arpdetect);
	ret = vps_umount(h, veid, res->fs.root, skip);

end:
	free_str_param(&param->del_res.net.ip);
	return ret;
}
