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

#include <grp.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <linux/vzcalluser.h>
#include <sys/personality.h>
#include <linux/reboot.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/vfs.h>

#include "vzerror.h"
#include "res.h"
#include "env.h"
#include "dist.h"
#include "exec.h"
#include "logger.h"
#include "util.h"
#include "script.h"
#include "iptables.h"
#include "vzsyscalls.h"
#include "cpt.h"
#include "image.h"
#include "readelf.h"
#include "destroy.h"

#ifndef PROC_SUPER_MAGIC
#define PROC_SUPER_MAGIC	0x9fa0
#endif

static int env_stop(vps_handler *h, envid_t veid, const char *root,
		int stop_mode, int timeout);

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
 * @param param		CT parameters.
 * @return		handler or NULL on error.
 */
vps_handler *vz_open(envid_t veid, vps_param *param)
{
	vps_handler *h = NULL;
	int ret = -1;

	h = calloc(1, sizeof(*h));
	if (h == NULL)
		return NULL;

	h->stdfd = reset_std();

#ifdef VZ_KERNEL_SUPPORTED
	/* Find out if we are under OpenVZ or upstream kernel */
	if (stat_file("/proc/vz") == 1)
		ret = vz_do_open(h, param);
	else
#endif
	{
		logger(0, 0, "Directory /proc/vz not found, assuming "
				"non-OpenVZ kernel");
		h->vzfd = -1;
#ifdef HAVE_UPSTREAM
		ret = ct_do_open(h, param);
#else
		logger(-1, 0, "No suitable kernel support compiled in");
#endif
	}

	if (!ret)
		return h;

	if (h->stdfd != -1)
		close(h->stdfd);
	free(h);
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
	if (veid == 0)
		return 1;
	return h->is_run(h, veid);
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
	for (i = 1; i < NSIG; ++i)
		sigaction(i, &act, NULL);
	return 0;
}

static int write_val(const char *file, const char *val)
{
	int fd, len, ret = -1;

	len = strlen(val);

	fd = open(file, O_WRONLY);
	if (fd == -1)
		return ret;
	if (write(fd, val, len) == len)
		ret = 0;
	close(fd);

	return ret;
}

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

int set_personality32()
{
	if (get_arch_from_elf("/sbin/init") != elf_32)
		return 0;
	return set_personality(PER_LINUX32);
}

void fill_container_param(struct arg_start *arg,
			 struct env_create_param3 *create_param)
{
	memset(create_param, 0, sizeof(*create_param));
	create_param->iptables_mask = get_ipt_mask(&arg->res->env);
	logger(3, 0, "Setting iptables mask %#10.8llx",
			(unsigned long long) create_param->iptables_mask);
	if (arg->res->cpu.vcpus != NULL)
		create_param->total_vcpus = *arg->res->cpu.vcpus;

	create_param->feature_mask = arg->res->env.features_mask;
	create_param->known_features = arg->res->env.features_known;

	/* sysfs enabled by default, unless explicitly disabled */
	if (! (arg->res->env.features_known & VE_FEATURE_SYSFS)) {
		create_param->feature_mask |= VE_FEATURE_SYSFS;
		create_param->known_features |= VE_FEATURE_SYSFS;
	}
	logger(3, 0, "Setting features mask %016llx/%016llx",
			create_param->feature_mask,
			create_param->known_features);
}

int exec_container_init(struct arg_start *arg,
			struct env_create_param3 *create_param)
{
	int fd, ret;
	char *argv[] = {"init", "-z", "      ", NULL};
	char *envp[] = {"HOME=/", "TERM=linux", NULL};
	struct statfs sfs;

	/* Clear supplementary group IDs */
	setgroups(0, NULL);
	/* for 32-bit userspace running over 64-bit kernels */
	set_personality32();

	/* Create /fastboot to skip run fsck */
	fd = open("/fastboot", O_CREAT | O_RDONLY, 0644);
	if (fd >= 0)
		close(fd);

	if (arg->res->misc.wait == YES) {
		if (add_reach_runlevel_mark()) {
			ret = VZ_WAIT_FAILED;
			return -1;
		}
	}

	if (mkdir("/proc", 0555) && errno != EEXIST)
		return vzctl_err(VZ_SYSTEM_ERROR, errno,
				"Can't mkdir /proc");

	if (statfs("/proc", &sfs))
		return vzctl_err(VZ_SYSTEM_ERROR, errno,
				"statfs on /proc failed");

	if (sfs.f_type != PROC_SUPER_MAGIC &&
	    mount("proc", "/proc", "proc", 0, 0))
		return vzctl_err(VZ_SYSTEM_ERROR, errno,
				"Failed to mount /proc");

	if (stat_file("/sys") == 1)
		mount("sysfs", "/sys", "sysfs", 0, 0);

	if (create_param->feature_mask & VE_FEATURE_NFSD) {
		mount("nfsd", "/proc/fs/nfsd", "nfsd", 0, 0);
		make_dir("/var/lib/nfs/rpc_pipefs", 1);
		mount("sunrpc", "/var/lib/nfs/rpc_pipefs", "rpc_pipefs", 0, 0);
	}
	write_val("/proc/sys/net/ipv6/conf/all/forwarding", "0");

	/* Close status descriptor to report that
	 * environment is created.
	*/
	close(STDIN_FILENO);
	/* Now we wait until CT setup will be done
	   If no error, then start init, otherwise exit.
	*/

	if (read(arg->wait_p, &ret, sizeof(ret)) == 0)
		return -1;

	if ((fd = open("/dev/null", O_RDWR)) != -1) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
	}

	logger(10, 0, "Starting init");

	close_fds(0, arg->err_p, -1);

	execve("/sbin/init", argv, envp);
	execve("/etc/init", argv, envp);
	execve("/bin/init", argv, envp);
	ret = VZ_FS_BAD_TMPL;
	write(arg->err_p, &ret, sizeof(ret));
	return ret;
}

static int vz_real_env_create(vps_handler *h, envid_t veid, vps_res *res,
	int wait_p, int old_wait_p, int err_p, env_create_FN fn, void *data)

{
	struct arg_start arg;

	arg.res = res;
	arg.wait_p = wait_p;
	arg.old_wait_p = old_wait_p;
	arg.err_p = err_p;
	arg.veid = veid;
	arg.h = h;
	arg.data = data;
	arg.fn = fn;

	return h->env_create(&arg);
}

#define MAX_OSREL_LEN 128

static void read_osrelease_conf(const char *dist, char *osrelease)
{
	FILE *f;
	char str[MAX_OSREL_LEN];
	char var[MAX_OSREL_LEN];
	char value[MAX_OSREL_LEN];
	int dlen = strlen(dist);

	if ((f = fopen(OSRELEASE_CFG, "r")) == NULL) {
		logger(-1, errno, "Can't open file " OSRELEASE_CFG);
		return;
	}
	while (fgets(str, sizeof(str) - 1, f) != NULL) {
		if (str[0] == '#')
			continue;
		if (sscanf(str, " %s %s ", var, value) != 2)
			continue;
		if (strncmp(var, dist, strnlen(var, dlen)) == 0) {
			strcpy(osrelease, value);
			break;
		}
	}
	fclose(f);
	return;
}

#define KVER(a, b, c) (((a) << 16) + ((b) << 8) + (c))
static int compare_osrelease(const char *cur, const char *min)
{
	int cur_a, cur_b, cur_c = 0;
	int min_a, min_b, min_c = 0;
	int ret;

	ret = sscanf(cur, "%d.%d.%d", &cur_a, &cur_b, &cur_c);
	if (ret < 2) {
		logger(-1, 0, "Unable to parse kernel osrelease (%s)", cur);
		return -1;
	}

	ret = sscanf(min, "%d.%d.%d", &min_a, &min_b, &min_c);
	if (ret < 2) {
		logger(-1, 0, "Unable to parse kernel osrelease (%s)", min);
		return -1;
	}

	if (KVER(cur_a, cur_b, cur_c) < KVER(min_a, min_b, min_c))
		return 1; /* Current version is too old */

	return 0;
}
#undef KVER

/* Checks if the current kernel version >= min_version
 *
 * Return value:
 *	-1: error
 *	 0: current kernel is sufficient
 *	 1: current kernel is too old
 */
int check_min_kernel_version(const char *min_version)
{
	struct utsname uts;

	/* Check if currrent kernel is equal or greater than min_version */
	if (uname(&uts) != 0) {
		logger(-1, errno, "Error in uname()");
		return -1;
	}

	return compare_osrelease(uts.release, min_version);
}

/** Find out if a container needs setting osrelease,
  * and set it if needed. */
static void get_osrelease(vps_res *res)
{
	char *dist;
	char osrelease[MAX_OSREL_LEN] = "";
	struct utsname uts;
	char *suffix;
	int len;

	dist = get_dist_name(&res->tmpl);
	if (!dist)
		return;

	read_osrelease_conf(dist, osrelease);
	if (osrelease[0] == '\0') {
		free(dist);
		return;
	}

	logger(1, 0, "Found osrelease %s for dist %s", osrelease, dist);
	free(dist);

	/* Check if current osrelease is sufficient */
	if (uname(&uts) != 0) {
		logger(-1, errno, "Error in uname()");
		return;
	}

	if (compare_osrelease(uts.release, osrelease) < 1)
		/* -1: error; 0: current version is good enough */
		return;

	/* Yes we need to set osrelease for this container */

	/* Make version look like our kernel, i.e. add suffix
	 * like -028stab078.10 to osrelease
	 */
	if ((suffix = strchr(uts.release, '-')) != NULL) {
		len = sizeof(osrelease) - strlen(osrelease);
		strncat(osrelease, suffix, len);
		osrelease[sizeof(osrelease) - 1] = 0;
	}

	logger(1, 0, "Set osrelease=%s", osrelease);
	res->env.osrelease = strdup(osrelease);
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
		*cpu->units = DEF_CPUUNITS;
	}
}

static int check_local_ugid(vps_handler *h, const char *private,
		unsigned long *uid, unsigned long *gid)
{
	struct stat st;

	if (is_vz_kernel(h) && !h->can_join_userns)
		return 0;

	if (stat(private, &st) < 0) {
		logger(-1, errno, "Can't stat %s", private);
		return VZ_SYSTEM_ERROR;
	}

	if ((uid && (st.st_uid != *uid)) || (gid && (st.st_gid != *gid))) {
		logger(-1, 0, "Container private area is owned by %d:%d, "
				"but configuration file says we should run "
				"with %lu:%lu.\nRefusing to run.",
				st.st_uid, st.st_gid,
				uid ? *uid : 0,
				gid ? *gid : 0);
		return VZ_FS_BAD_TMPL;
	}

	return 0;
}

int vps_start_custom(vps_handler *h, envid_t veid, vps_param *param,
	skipFlags skip, struct mod_action *mod,
	env_create_FN fn, void *data)
{
	int wait_p[2];
	int old_wait_p[2];
	int err_p[2];
	int ret;
	char buf[64];
	char *dist_name;
	struct sigaction act;
	vps_res *res = &param->res;
	dist_actions actions;
	int ploop;

	memset(&actions, 0, sizeof(actions));
	if (check_var(res->fs.root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (vps_is_run(h, veid)) {
		logger(-1, 0, "Container is already running");
		return VZ_VE_RUNNING;
	}
	ret = check_local_ugid(h, res->fs.private,
			res->misc.local_uid, res->misc.local_gid);
	if (ret)
		return ret;

	if ((ret = check_ub(h, &res->ub)))
		return ret;

	ploop = ve_private_is_ploop(&res->fs);
	if (ploop && !is_ploop_supported())
		return VZ_PLOOP_UNSUP;

	dist_name = get_dist_name(&res->tmpl);
	ret = read_dist_actions(dist_name, DIST_DIR, &actions);
	free(dist_name);
	if (ret)
		return ret;

	if (!(skip & SKIP_REMOUNT)) {
		/* if CT is mounted -- umount first, to cleanup mount state */
		if (vps_is_mounted(res->fs.root, res->fs.private) == 1) {
			vps_umount(h, veid, &res->fs, skip);
		}
#ifdef HAVE_PLOOP
		else if (ploop && (is_image_mounted(res->fs.private)))
			vzctl_umount_image(res->fs.private);
#endif
	}

	if (vps_is_mounted(res->fs.root, res->fs.private) != 1) {
		/* increase quota to perform setup */
		if (!ploop)
			quota_inc(&res->dq, 100);
		if ((ret = vps_mount(h, veid, &res->fs, &res->dq, skip)))
			return ret;
		if (!ploop)
			quota_inc(&res->dq, -100);
	}

	if (!(skip & SKIP_ACTION_SCRIPT)) {
		ret = run_pre_script(veid, VPS_PRESTART);
		if (ret)
			return ret;
	}
	if ((ret = fill_vswap_ub(&res->ub, &res->ub)))
		return ret;

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
		close(wait_p[0]);
		close(wait_p[1]);
		logger(-1, errno, "Can not create pipe");
		return VZ_RESOURCE_ERROR;
	}
	if (pipe(err_p) < 0) {
		close(old_wait_p[0]);
		close(old_wait_p[1]);
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

	if ((ret = vps_setup_res(h, veid, &actions, &res->fs, NULL, param,
		STATE_STARTING, skip, mod)))
	{
		goto err;
	}
	if (!(skip & SKIP_ACTION_SCRIPT) && !fn) {

		/* Run dist actions PRE_START script */
		if (actions.pre_start) {
			char buf[32];
			char buf_ns[32];
			char *envp[3];

			snprintf(buf, sizeof(buf), "VZ_KERNEL=%s",
					is_vz_kernel(h) ? "yes" : "no");
			envp[0] = buf;
			snprintf(buf_ns, sizeof(buf_ns), "USERNS=%s",
					h->can_join_userns ? "yes" : "no");
			envp[1] = buf_ns;
			envp[2] = NULL;
			if (vps_exec_script(h, veid, res->fs.root, NULL, envp,
						actions.pre_start, NULL, 0)) {
				ret = VZ_ACTIONSCRIPT_ERROR;
				goto err;
			}
		}

		/* Run per-CT $VEID.start script */
		snprintf(buf, sizeof(buf), VPSCONFDIR "/%d.%s", veid,
			START_PREFIX);
		if (stat_file(buf) == 1) {
			if (vps_exec_script(h, veid, res->fs.root, NULL, NULL,
				buf, NULL, 0))
			{
				ret = VZ_ACTIONSCRIPT_ERROR;
				goto err;
			}
		}
	}
	/* Tell the child that it's time to start /sbin/init */
	if (write(wait_p[1], &ret, sizeof(ret)) != sizeof(ret))
		logger(-1, errno, "Unable to write to waitfd to start init");
	close(wait_p[1]);
	close(old_wait_p[1]);
err:
	free_dist_actions(&actions);
	if (ret) {
		/* Kill environment */
		logger(-1, 0, "Container %s failed (try to check kernel "
				"messages, e.g. \"dmesg | tail\")",
				(fn) ? "restore" : "start");
		/* Close wait fd without writing anything to it
		 * to signal the child that we have failed to configure
		 * the environment, so it should not start /sbin/init
		 */
		close(wait_p[1]);
		write(old_wait_p[1], &ret, sizeof(ret));
		close(old_wait_p[1]);
	} else {
		if (!read(err_p[0], &ret, sizeof(ret))) {
			if (res->misc.wait == YES) {
				logger(0, 0, "Container start in progress"
					", waiting ...");
				ret = vps_execFn(h, veid, res->fs.root,
					wait_on_fifo, NULL, 0);
				if (ret) {
					logger(0, 0, "Container wait failed%s",
						ret == VZ_EXEC_TIMEOUT ? \
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
			logger(-1, 0, "Container %s failed",
					(fn) ? "restore" : "start");
		}
	}
	if (ret) {
		if (vps_is_run(h, veid))
			env_stop(h, veid, res->fs.root, M_KILL, 0);
		/* restore original quota values */
		if (!ploop)
			vps_set_quota(veid, &res->dq);
		if ((vps_is_mounted(res->fs.root, res->fs.private) == 1) &&
				!(skip & SKIP_UMOUNT))
			vps_umount(h, veid, &res->fs, skip);
	}
	close(old_wait_p[0]);
	close(old_wait_p[1]);
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
	int ret;

	logger(0, 0, "Starting container...");
	ret = vps_start_custom(h, veid, param, skip, mod, NULL, NULL);

	if (ret == 0) {
		/* Start was successful, remove the default dump file:
		 * it is now useless and inconsistent with the fs state
		 */
		destroy_dump(veid, param->res.cpt.dumpdir);
	}

	return ret;
}

static int real_env_stop(vps_handler *h, envid_t veid, const char *vps_root,
	int stop_mode)
{
	int ret;

	if ((ret = h->setcontext(veid)))
		return ret;
	close_fds(1, h->vzfd, -1);
	ret = h->enter(h, veid, vps_root, 0);
	if (ret == VZ_VE_NOT_RUNNING)
		/* Ignore "VE not running" error here */
		return 0;
	else if (ret) {
		logger(-1, errno, "Container enter failed");
		return ret;
	}

	/* Disable fsync. The fsync will be done by umount() */
	write_val("/proc/sys/fs/fsync-enable", "0");

	switch (stop_mode) {
		case M_REBOOT:
		{
			char *argv[] = {"reboot", NULL};
			execvep(argv[0], argv, NULL);
			return VZ_STOP_ERROR;
			break;
		}
		case M_HALT:
		{
			char *argv[] = {"halt", NULL};
			execvep(argv[0], argv, NULL);
			return VZ_STOP_ERROR;
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

static int wait_child(int pid, int ignore_kill)
{
	int status, ret;

	while ((ret = waitpid(pid, &status, 0)) == -1)
		if (errno != EINTR)
			break;

	if (ret < 0) {
		logger(-1, errno, "Error in waitpid(%d)", pid);
		return VZ_SYSTEM_ERROR;
	}

	ret = 0;
	if (WIFEXITED(status) && (ret = WEXITSTATUS(status)))
		logger(-1, 0, "Child %d exited with status %d", pid, ret);
	else if (!ignore_kill && WIFSIGNALED(status)) {
		logger(-1, 0, "Child %d terminated with signal %d",
				pid, WTERMSIG(status));
		ret = VZ_SYSTEM_ERROR;
	}

	return ret;
}

static int env_stop(vps_handler *h, envid_t veid, const char *root,
		int stop_mode, int timeout)
{
	int i, pid, ret, tout = 0;

	if (timeout <= 0)
		timeout = DEF_STOP_TIMEOUT;

	if (stop_mode == M_KILL)
		goto kill_vps;

	if (!is_vz_kernel(h) && !h->can_join_pidns) {
		logger(-1, 0, "Due to lack of proper support in this kernel, "
		"container can't be cleanly\n"
		"stopped from the host system. Please stop it from inside, "
		"or use --fast option\n"
		"to forcibly kill it (note it is unsafe operation).");
		ret = VZ_BAD_KERNEL;
		goto out;
	}
	logger(0, 0, "Stopping container ...");
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can not fork");
		ret = VZ_RESOURCE_ERROR;
		goto out;
	} else if (pid == 0) {
		ret = real_env_stop(h, veid, root, stop_mode);
		exit(ret);
	}

	ret = wait_child(pid, 0);
	/* Sometimes reboot/halt returns 1 but still stops the CT */
	if (ret != 0 && ret != 1) /* failed, retry with kill */
		goto kill_vps;

	for (i = 0; i < timeout; i++) {
		sleep(1);
		if (!vps_is_run(h, veid)) {
			ret = 0;
			goto out;
		}
	}

kill_vps:
	logger(0, 0, "Killing container ...");
	ret = h->destroy(h, veid);
	if (!is_vz_kernel(h))
		goto wait;

	if ((pid = fork()) < 0) {
		ret = VZ_RESOURCE_ERROR;
		logger(-1, errno, "Can not fork");
		goto out;

	} else if (pid == 0) {
		ret = real_env_stop(h, veid, root, M_KILL);
		exit(ret);
	}
	ret = wait_child(pid, 1);
	if (ret)
		goto out;

wait:
	ret = VZ_STOP_ERROR;
	for (i = 0; i < timeout; i++) {
		usleep(500000);
		if (!vps_is_run(h, veid)) {
			ret = 0;
			break;
		}
	}
	tout = 1;
out:
	if (ret)
		logger(-1, 0, "Unable to stop container%s",
				tout ? ": operation timed out" : "");
	else
		logger(0, 0, "Container was stopped");

	return ret;
}

/** Stop CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param param		CT parameters.
 * @param stop_mode	stop mode, one of (M_REBOOT M_HALT M_KILL).
 * @param skip		steps to skip (SKIP_ACTION_SCRIPT, SKIP_UMOUNT)
 * @param action	modules list, used to call cleanup() callback.
 * @return		0 on success.
 */
int vps_stop(vps_handler *h, envid_t veid, vps_param *param, int stop_mode,
	skipFlags skip, struct mod_action *action)
{
	int ret;
	char buf[64];
	vps_res *res = &param->res;
	int tm = res->misc.stop_timeout;

	if (check_var(res->fs.root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to stop: container is not running");
		return 0;
	}
	if (!(skip & SKIP_ACTION_SCRIPT)) {
		snprintf(buf, sizeof(buf), VPSCONFDIR "/%d.%s", veid,
			STOP_PREFIX);
		if (stat_file(buf) == 1) {
			if (vps_exec_script(h, veid, res->fs.root, NULL, NULL,
				buf, NULL, 0))
			{
				return VZ_ACTIONSCRIPT_ERROR;
			}
		}
	}

	/* get CT IP addresses for cleanup */
	if (is_vz_kernel(h))
		get_vps_ip(h, veid, &param->del_res.net.ip);

	if ((ret = env_stop(h, veid, res->fs.root, stop_mode, tm)))
		goto end;

	mod_cleanup(h, veid, action, param);

	/* Cleanup CT IPs */
	if (is_vz_kernel(h))
		run_net_script(veid, DEL, &param->del_res.net.ip,
			STATE_STOPPING, param->res.net.skip_arpdetect);

	if (!(skip & SKIP_UMOUNT))
		ret = vps_umount(h, veid, &res->fs, skip);

end:
	free_str_param(&param->del_res.net.ip);
	return ret;
}
