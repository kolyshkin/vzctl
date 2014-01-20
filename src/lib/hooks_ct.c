#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sched.h>
#include <dirent.h>

#include "vzerror.h"
#include "env.h"
#include "exec.h"
#include "util.h"
#include "logger.h"
#include "script.h"
#include "cgroup.h"
#include "cpt.h"
#include "linux/vzctl_venet.h"

#define NETNS_RUN_DIR "/var/run/netns"

#ifndef HAVE_SETNS

#ifndef __NR_setns
#if defined __i386__
#define __NR_setns     346
#elif defined __x86_64__
#define __NR_setns     308
#else
#error "No setns syscall known for this arch"
#endif
#endif /* ! __NR_setns */

static int sys_setns(int fd, int nstype)
{
	return syscall(__NR_setns, fd, nstype);
}
#define setns sys_setns

#endif /* ! HAVE_SETNS */

/* These comes from bits/sched.h */
#ifndef CLONE_NEWUTS
# define CLONE_NEWUTS   0x04000000      /* New utsname group.  */
#endif
#ifndef CLONE_NEWIPC
# define CLONE_NEWIPC   0x08000000      /* New ipcs.  */
#endif
#ifndef CLONE_NEWUSER
# define CLONE_NEWUSER  0x10000000      /* New user namespace.  */
#endif
#ifndef CLONE_NEWPID
# define CLONE_NEWPID   0x20000000      /* New pid namespace.  */
#endif
#ifndef CLONE_NEWNET
# define CLONE_NEWNET   0x40000000      /* New network namespace.  */
#endif

/* From sys/mount.h */
#ifndef MS_REC
# define MS_REC 16384
#endif

#ifndef MS_PRIVATE
# define MS_PRIVATE (1 << 18)
#endif

#define UID_GID_RANGE 100000 /* how many users per container */

/* This function is there in GLIBC, but not in headers */
extern int pivot_root(const char * new_root, const char * put_old);


static int ct_is_run(vps_handler *h, envid_t veid)
{
	return container_is_running(veid);
}

static int ct_destroy(vps_handler *h, envid_t veid)
{
	char ctpath[STR_SIZE];
	int ret;

	ret = hackish_empty_container(veid);
	if (ret)
		return ret;

	snprintf(ctpath, STR_SIZE, "%s/%d", NETNS_RUN_DIR, veid);
	unlink(ctpath);

	get_state_file(veid, ctpath, sizeof(ctpath));
	unlink(ctpath);

	return destroy_container(veid);
}

int ct_chroot(const char *root)
{
	char oldroot[] = "vzctl-old-root.XXXXXX";
	int ret = VZ_RESOURCE_ERROR;

	/* root must be bind-mounted to itself to not show what is under it
	 *
	 * Linux kernel commit 5ff9d8a6
	 * "vfs: Lock in place mounts from more privileged users"
	 */
	if (mount(root, root, NULL, MS_BIND, NULL)) {
		logger(-1, errno, "Can't bind-mount root %s", root);
		return ret;
	}

	if (chdir(root)) {
		logger(-1, errno, "Can't chdir %s", root);
		return ret;
	}

	if (mount("", "/", NULL, MS_PRIVATE|MS_REC, NULL) < 0) {
		logger(-1, errno, "Can't remount root with MS_PRIVATE");
		return ret;
	}

	if (mkdtemp(oldroot) == NULL) {
		logger(-1, errno, "Can't mkdtemp %s", oldroot);
		return ret;
	}

	if (pivot_root(".", oldroot)) {
		logger(-1, errno, "Can't pivot_root(\".\", %s)", oldroot);
		goto rmdir;
	}

	if (chdir("/")) {
		logger(-1, errno, "Can't chdir /");
		goto rmdir;
	}

	/* proc and sysfs must be mounted before unmounting oldroot
	 * because of Linux kernel commit e51db7
	 * "userns: Better restrictions on when proc and sysfs can be mounted"
	 */

	if (mount("proc", "/proc", "proc", 0, 0)) {
		logger(-1, errno, "Failed to mount /proc");
		goto rmdir;
	}

	if (mount("sysfs", "/sys", "sysfs", 0, 0)) {
		logger(-1, errno, "Failed to mount /sys");
		goto rmdir;
	}

	if (umount2(oldroot, MNT_DETACH)) {
		logger(-1, 0, "Can't umount old mounts");
		goto rmdir;
	}

	ret = 0;
rmdir:
	if (rmdir(oldroot))
		logger(-1, errno, "Can't rmdir %s", oldroot);

	return ret;
}

#define add_value(val, var, mult) do { if (val) { var = *val * mult; } } while (0)

static int ct_setlimits(vps_handler *h, envid_t veid, struct ub_struct *ub)
{
	unsigned long tcp = 0;
	unsigned long kmem = 0;
	unsigned long kmemall = 0;
	unsigned long mem = 0;
	unsigned long swap = 0;
	int pagesize = sysconf(_SC_PAGESIZE);

	add_value(ub->physpages, mem, pagesize);
	add_value(ub->tcpsndbuf, tcp, 1);
	add_value(ub->tcprcvbuf, tcp, 1);
	add_value(ub->swappages, swap, pagesize);

	/*
	 * OpenVZ beancounters traditionally acconted objects. Also, we could
	 * always get a very high granularity about which objects we are
	 * tracking. Our attempt in this implementation is to translate the
	 * historical beancounters into something that "makes sense" given the
	 * underlying Linux infrastructure, and provide something that would
	 * allow for more or less the kind of protection the user asked for.  A
	 * 1:1 mapping, however, is not possible - and will never be.
	 *
	 * Upstream Linux cgroup controllers went in a very different
	 * direction. First, resources tend to be viewed in its entirety. We
	 * have entities like "memory", or "kernel memory", instead of a list
	 * of all internal structures like dentry, siginfo, etc. For network
	 * buffers, we can specify the total buffer memory instead of send and
	 * receive buffers, etc.
	 *
	 * Also, all accounting is done in pages, not in objects - which is the
	 * only thing that makes sense if the accounting is done in an
	 * aggregate manner.  We don't really know the size of those
	 * structures, so we use an estimate to get a value in pages. This is
	 * not a stable API of the kernel, so it is bound to change.
	 *
	 * Here is the size in bytes of the following structs, in Linux 3.4:
	 *
	 * dcache: 248, siginfo: 128, sock: 1072, task 8128
	 */
	#define DCACHE 248
	#define SIGINFO 128
	#define SOCK 1072

	add_value(ub->kmemsize, kmem, 1);
	add_value(ub->dcachesize, kmemall, DCACHE);
	add_value(ub->numtcpsock, kmemall, SOCK);
	add_value(ub->numsiginfo, kmemall, SIGINFO);
	add_value(ub->numothersock, kmemall, SOCK);
	add_value(ub->othersockbuf, kmemall, 1);
	add_value(ub->numproc, kmemall, 2 * pagesize);
	add_value(ub->dgramrcvbuf, kmemall, SOCK);

	if (mem)
		container_apply_config(veid, MEMORY, &mem);
	if (tcp)
		container_apply_config(veid, TCP, &tcp);

	kmem = max_ul(kmem, kmemall);
	if (kmem)
		container_apply_config(veid, KMEMORY, &kmem);

	if (swap)
		container_apply_config(veid, SWAP, &swap);

	return 0;
}
#undef add_value

static int write_uid_gid_mapping(vps_handler *h, unsigned long uid, unsigned long gid, pid_t pid)
{
	char buf[STR_SIZE];
	char map[STR_SIZE];
	int fd;
	int len;
	int ret = VZ_RESOURCE_ERROR;

	len = snprintf(map, sizeof(map), "0 %ld %d", uid, UID_GID_RANGE);
	snprintf(buf, sizeof(buf), "/proc/%d/uid_map", pid);
	if ((fd = open(buf, O_WRONLY)) < 0)
		goto out;

	if ((write(fd, map, len) != len))
		goto out;

	close(fd);

	len = snprintf(map, sizeof(map), "0 %ld %d", gid, UID_GID_RANGE);
	snprintf(buf, sizeof(map), "/proc/%d/gid_map", pid);
	if ((fd = open(buf, O_WRONLY)) < 0)
		goto out;

	if ((write(fd, map, len) != len))
		goto out;
	ret = 0;
out:
	if (fd >= 0)
		close(fd);
	return ret;
}

/*
 * Those devices should exist in the container, and be valid device nodes with
 * user access permission. But we need to be absolutely sure this is the case,
 * so we will provide our own versions. That could actually happen since some
 * distributions may come with emptied /dev's, waiting for udev to populate them.
 * That won't happen, we do it ourselves.
 */
static void create_devices(vps_handler *h, envid_t veid, const char *root)
{
	unsigned int i;
	char *devices[] = {
		"/dev/null",
		"/dev/zero",
		"/dev/random",
		"/dev/urandom",
	};

	/*
	 * We will tolerate errors, and keep the container running, because it is
	 * likely we will be able to boot it to a barely functional state. But
	 * be vocal about it
	 */
	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		char ct_devname[STR_SIZE];
		int ret;

		snprintf(ct_devname, sizeof(ct_devname), "%s%s", root, devices[i]);

		/*
		 * No need to be crazy about file flags. When we bind mount, the
		 * source permissions will be inherited.
		 */
		ret = open(ct_devname, O_RDWR|O_CREAT, 0);
		if (ret < 0) {
			logger(-1, errno, "Could not touch device %s", devices[i]);
			continue;
		}
		close(ret);

		ret = mount(devices[i], ct_devname, "", MS_BIND, 0);
		if (ret < 0)
			logger(-1, errno, "Could not bind mount device %s", devices[i]);
	}
}

static int _env_create(void *data)
{
	struct arg_start *arg = data;
	struct env_create_param3 create_param;
	int ret;

	if ((arg->userns_p != -1) &&
			(read(arg->userns_p, &ret, sizeof(ret)) != sizeof(ret))) {
		logger(-1, errno, "Cannot read from user namespace pipe");
		close(arg->userns_p);
		return VZ_RESOURCE_ERROR;
	}

	/*
	 * Technically, because clone will clone both fds, we would have to
	 * close the other end as well. But we don't even know what it is,
	 * since our args only include our end of the pipe. This is not a
	 * problem because right before exec_container_init, we will call
	 * close_fds and get away with all of them. And if we fail, we'll
	 * exit anywyay.
	 */
	if (arg->userns_p != -1)
		close(arg->userns_p);

	if (arg->h->can_join_userns) {
		create_devices(arg->h, arg->veid, arg->res->fs.root);
	}

	ret = ct_chroot(arg->res->fs.root);
	/* Probably means chroot failed */
	if (ret)
		return ret;

	setuid(0);
	setgid(0);

	/*
	 * If we are using the user namespace, we will have the full capability
	 * set in the target namespace. So we don't need any of that.
	 */
	if (!arg->h->can_join_userns &&
		(ret = vps_set_cap(arg->veid, &arg->res->env, &arg->res->cap, 1)))
		return ret;

	fill_container_param(arg, &create_param);

	/* Close all fds except stdin. stdin is status pipe */
	close(STDERR_FILENO); close(STDOUT_FILENO);
	close_fds(0, arg->wait_p, arg->err_p, -1);

	return exec_container_init(arg, &create_param);
}

static int ct_env_create_real(struct arg_start *arg)
{

	long stack_size;
	char *child_stack;
	int clone_flags;
	int userns_p[2];
	int ret, fd;
	char pidpath[STR_SIZE];
	char ctpath[STR_SIZE];

	stack_size = get_pagesize();
	if (stack_size < 0)
		return VZ_RESOURCE_ERROR;

	child_stack = alloca(stack_size);
	if (child_stack == NULL) {
		logger(-1, 0, "Unable to alloc");
		return VZ_RESOURCE_ERROR;
	}

	/*
	 * Belong in the setup phase
	 */
	clone_flags = SIGCHLD;
	clone_flags |= CLONE_NEWUTS|CLONE_NEWPID|CLONE_NEWIPC;
	clone_flags |= CLONE_NEWNET|CLONE_NEWNS;

	if (!arg->h->can_join_userns) {
		logger(-1, 0, "WARNING: Running container unprivileged. USER_NS not supported, or runtime disabled");

		userns_p[0] = userns_p[1] = -1;
	} else {
		clone_flags |= CLONE_NEWUSER;
		if (pipe(userns_p) < 0) {
			logger(-1, errno, "Can not create userns pipe");
			return VZ_RESOURCE_ERROR;
		}
	}
	arg->userns_p = userns_p[0];

	get_state_file(arg->veid, pidpath, sizeof(pidpath));
	fd = open(pidpath, O_WRONLY | O_TRUNC | O_CREAT, 0600);
	if (fd == -1) {
		logger(-1, errno, "Unable to create a state file %s", pidpath);
		return VZ_RESOURCE_ERROR;
	}
	fcntl(fd, F_SETFD, FD_CLOEXEC);

#ifdef __ia64__
	ret = __clone2(_env_create, child_stack, stack_size, clone_flags, arg);
#else
	ret = clone(_env_create, child_stack + stack_size, clone_flags, arg);
#endif
	close(userns_p[0]);
	if (ret < 0) {
		logger(-1, errno, "Unable to clone");
		close(fd);
		/* FIXME: remove ourselves from container first */
		close(userns_p[1]);
		destroy_container(arg->veid);
		return VZ_RESOURCE_ERROR;
	}

	dprintf(fd, "%d", ret);
	close(fd);

	if (arg->h->can_join_userns) {
		int x = 0;
		/*
		 * Now we need to write to the mapping file. It has to be us,
		 * since CAP_SETUID is required in the parent namespace. vzctl
		 * is run as root, so we should have it. But our cloned kid
		 * will start as the overflow uid 65534 in the new namespace.
		 */
		if (write_uid_gid_mapping(arg->h, *arg->res->misc.local_uid,
					  *arg->res->misc.local_gid, ret)) {

			logger(-1, 0, "Can't write to userns mapping file");
			close(userns_p[1]);
			destroy_container(arg->veid);
			return VZ_RESOURCE_ERROR;
		}
		/*
		 * Nothing should proceed userns wide until we have the
		 * mapping.  That creates many non-deterministic behaviors
		 * since some runs will execute with the mapping already done,
		 * while others with the mapping off. This is particularly
		 * important for setuid, for instance. It will categorically
		 * fail if called before a mapping is in place.
		 */
		if ((userns_p[1] != -1) &&
				write(userns_p[1], &x, sizeof(x)) != sizeof(x)) {
			logger(-1, errno, "Unable to write to userns pipe");
			close(userns_p[1]);
			destroy_container(arg->veid);
			return VZ_RESOURCE_ERROR;
		}
		close(userns_p[1]);
	}

	snprintf(ctpath, STR_SIZE, "%s/%d", NETNS_RUN_DIR, arg->veid);
	snprintf(pidpath, STR_SIZE, "/proc/%d/ns/net", ret);
	if (symlink(pidpath, ctpath)) {
		logger(-1, errno, "Can't symlink into netns file %s", ctpath);
		destroy_container(arg->veid);
		return VZ_RESOURCE_ERROR;
	}

	return 0;
}

int ct_env_create(struct arg_start *arg)
{
	int ret;
	char ctpath[STR_SIZE];

	/* non-fatal */
	if ((ret = ct_destroy(arg->h, arg->veid)))
		logger(0, 0, "Could not properly cleanup container: %s",
			container_error(ret));

	snprintf(ctpath, STR_SIZE, "%s/%d", NETNS_RUN_DIR, arg->veid);
	unlink(ctpath);

	if ((ret = create_container(arg->veid))) {
		logger(-1, 0, "Container creation failed: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	if ((ret = ct_setlimits(arg->h, arg->veid, &arg->res->ub))) {
		logger(-1, 0, "Could not apply container limits: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	if ((ret = container_add_task(arg->veid))) {
		logger(-1, 0, "Can't add task creator to container: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	/* Return PID on success or -VZ_*_ERROR */
	if (arg->fn)
		ret = arg->fn(arg->h, arg->veid, arg->res,
				arg->wait_p, arg->old_wait_p, arg->err_p, arg->data);
	else
		ret = ct_env_create_real(arg);

	return ret;
}

static int ct_enter(vps_handler *h, envid_t veid, const char *root, int flags)
{
	DIR *dp;
	struct dirent *ep;
	char path[STR_SIZE]; /* long enough for any pid */
	pid_t task_pid;
	int ret = VZ_RESOURCE_ERROR;
	bool joined_mnt_ns = false;
	int fd, err;

	if (!h->can_join_pidns) {
		logger(-1, 0, "Kernel lacks setns for pid namespace");
		return VZ_RESOURCE_ERROR;
	}

	task_pid = get_pid_from_container(veid);
	if (task_pid < 0) {
		logger(-1, 0, "Container doesn't seem to be started (no pids in container cgroup)");
		return VZ_RESOURCE_ERROR;
	}

	if (snprintf(path, STR_SIZE, "/proc/%d/ns/", task_pid) < 0)
		return VZ_RESOURCE_ERROR;

	dp = opendir(path);
	if (dp == NULL)
		return VZ_RESOURCE_ERROR;

	if ((err = container_add_task(veid))) {
		logger(-1, 0, "Can't add task creator to container: %s", container_error(err));
		goto out;
	}

	/*
	 * Because all namespaces are associated with an owner userns,
	 * and capabilities may be needed for issuing setns syscalls into
	 * some key target namespaces (like the mount namespace), we will
	 * first enter the user namespace if it is available. Only then we
	 * scan all others and join them as they appear
	 */
	if (h->can_join_userns) {
		if (snprintf(path, sizeof(path), "/proc/%d/ns/user", task_pid) < 0)
			goto out;

		if ((fd = open(path, O_RDONLY)) < 0)
			goto out;

		if (setns(fd, CLONE_NEWUSER)) {
			logger(-1, errno, "Failed to set context for user namespace");
			close(fd);
			goto out;
		}
		close(fd);
		setuid(0);
		setgid(0);
	}

	ret = VZ_RESOURCE_ERROR;
	while ((ep = readdir (dp))) {
		if (!strcmp(ep->d_name, "."))
			continue;
		if (!strcmp(ep->d_name, ".."))
			continue;

		/* already joined */
		if ((!strcmp(ep->d_name, "user")))
			continue;

		if (snprintf(path, sizeof(path), "/proc/%d/ns/%s", task_pid, ep->d_name) < 0)
			goto out;
		if ((fd = open(path, O_RDONLY)) < 0)
			goto out;
		if (setns(fd, 0))
			logger(-1, errno, "Failed to set context for %s", ep->d_name);
		close(fd);

		if (!strcmp(ep->d_name, "mnt"))
			joined_mnt_ns = true;
	}

	/*
	 * If we can join the mount namespace, we don't need to call
	 * pivot_root, or any other follow up step, since we will already
	 * inherit any fs tree structure the process already has.
	 *
	 * As a matter of fact, we won't even be able to see the container
	 * directories to jump to
	 */
	if (!joined_mnt_ns && (ret = ct_chroot(root)))
		goto out;

	/*
	 * setns() of the pid namespace unlike unsharing of other namespaces
	 * does not take affect immediately.  Instead it affects the children
	 * created with fork and clone.
	 */
	task_pid = fork();
	if (task_pid < 0) {
		logger(-1, errno, "Unable to fork");
		goto out;
	}

	ret = 0;
	if (task_pid == 0)
		goto out;

	close_fds(false, -1);

	ret = env_wait(task_pid);
	exit(ret);
out:
	closedir(dp);
	return ret;
}

static int ct_setcpus(vps_handler *h, envid_t veid, struct cpu_param *cpu)
{
	int ret = 0;
	/*
	 * Need to convert both cpulimit and vcpus to something comparable.
	 * So get both in percentages
	 */
	unsigned long max_lim = ~0UL;

	if (cpu->mask)
		ret = container_apply_config(veid, CPUMASK,
					     cpumask_bits(cpu->mask));

	if (cpu->limit != NULL && *cpu->limit)
		max_lim = min_ul(*cpu->limit, max_lim);
	if (cpu->vcpus != NULL)
		max_lim = min_ul(max_lim, *cpu->vcpus * 100);
	if (max_lim != ~0UL)
		ret |= container_apply_config(veid, CPULIMIT, &max_lim);

	if (cpu->units != NULL) {
		ret |= container_apply_config(veid, CPUSHARES, cpu->units);
	} else if (cpu->weight != NULL) {
		ret |= container_apply_config(veid, CPUSHARES, cpu->weight);
	}

	return ret;
}

static int deny_devices(vps_handler *h, envid_t veid, dev_res *dev)
{
	char dev_str[STR_SIZE];
	char perms[4];
	int i = 0;

	/*
	 * Attention: what we want to do is figure out which permissions we want
	 * to mask out, so this has to be a negative test. If all of them are
	 * masked out, we don't call allow, and revoke the device entirely
	 */
	if (!(dev->mask & S_IROTH))
		perms[i++] = 'r';
	if (!(dev->mask & S_IWOTH))
		perms[i++] = 'w';

	if (i == 0)
		return 0;

	/* revoke entirely */
	if (i == 2)
		perms[i++] = 'm';

	perms[i++] = '\0';
	snprintf(dev_str, STR_SIZE, "%c %d:%d %s",
		S_ISBLK(dev->type) ? 'b' : 'c',
		major(dev->dev), minor(dev->dev), perms);

	return  container_apply_config(veid, DEVICES_DENY, &dev_str);
}

static int ct_setdevperm(vps_handler *h, envid_t veid, dev_res *dev)
{
	char dev_str[STR_SIZE];
	char perms[4];
	int i = 0;
	int ret;

	if ((dev->mask & S_IXGRP))
		logger(1, 0, "Quota setup not implemented with upstream kernels, ignoring");

	if ((ret = deny_devices(h, veid, dev)))
		return ret;

	if (dev->mask & S_IROTH)
		perms[i++] = 'r';
	if (dev->mask & S_IWOTH)
		perms[i++] = 'w';
	/*
	 * If the user has not specified any permissions, what we need to
	 * do is just remove the device from the list. In that case, we're done
	 * here
	 */
	if (i == 0)
		return 0;

	/*
	 * Since this is not specifiable from the cmdline, always give mknod
	 * permission
	 */
	perms[i++] = 'm';
	perms[i++] = '\0';

	snprintf(dev_str, STR_SIZE, "%c %d:%d %s",
		S_ISBLK(dev->type) ? 'b' : 'c',
		major(dev->dev), minor(dev->dev),
		perms);

	return container_apply_config(veid, DEVICES_ALLOW, &dev_str);
}

/*
 * This will move an existing device from host to the container.  We will
 * signal that to the network scripts by setting HNAME == VNAME.
 *
 * This is an impossible situation for a normal device pair, so it is a safe
 * thing to do, while removing the need to create yet another script just for
 * the special case of device movement. Both device creation and device
 * deletion will abide by this convention.
 */
static int ct_netdev_ctl(vps_handler *h, envid_t veid, int op, char *name)
{
	char *envp[10];
	char buf[STR_SIZE];
	int i = 0;
	int ret = 0;

	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);

	snprintf(buf, sizeof(buf), "VNAME=%s", name);
	envp[i++] = strdup(buf);

	snprintf(buf, sizeof(buf), "HNAME=%s", name);
	envp[i++] = strdup(buf);

	envp[i] = NULL;

	if (op == VE_NETDEV_ADD) {
		char *argv[] = { VPS_NETNS_DEV_ADD, NULL };
		ret = run_script(VPS_NETNS_DEV_ADD, argv, envp, 0);
	} else {
		char *argv[] = { VPS_NETNS_DEV_DEL, NULL };
		ret = run_script(VPS_NETNS_DEV_DEL, argv, envp, 0);
	}
	free_arg(envp);

	return ret;
}

static int ct_ip_ctl(vps_handler *h, envid_t veid, int op, const char *ipstr)
{
	int ret = -1;
	char *envp[5];
	char *argv[] = {NULL, NULL};
	char buf[STR_SIZE];
	int i = 0;

	if (!h->can_join_pidns) {
		logger(-1, 0, "Cannot join pid namespace: "
				"--ipadd is not supported in kernels "
				"without full pidns support");
		return VZ_BAD_KERNEL;
	}
	envp[i++] = strdup("VNAME=venet0");
	envp[i++] = strdup("BRIDGE=venet0");

	snprintf(buf, sizeof(buf), "HNAME=venet0.%d", veid);
	envp[i++] = strdup(buf);

	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);

	envp[i] = NULL;

	if (op == VE_IP_ADD)
		argv[0] = VPS_NETNS_DEV_ADD;
	else
		argv[0] = VPS_NETNS_DEV_DEL;

	ret = run_script(argv[0], argv, envp, 0);

	free_arg(envp);

	return ret;

}

/*
 * This function is the simplest one among the network handling functions.
 * It will create a veth pair, and move one of its ends to the container.
 *
 * MAC addresses and Bridge parameters are optional
 */
static int ct_veth_ctl(vps_handler *h, envid_t veid, int op, veth_dev *dev)
{
	int ret = -1;
	char *envp[11];
	char buf[STR_SIZE];
	int i = 0;

	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);

	snprintf(buf, sizeof(buf), "VNAME=%s", dev->dev_name_ve);
	envp[i++] = strdup(buf);

	if (dev->addrlen_ve) {
		snprintf(buf, sizeof(buf), "VMAC=" MAC2STR_FMT,
				MAC2STR(dev->dev_addr_ve));
		envp[i++] = strdup(buf);
	}

	if (dev->addrlen) {
		snprintf(buf, sizeof(buf), "HMAC=" MAC2STR_FMT,
				MAC2STR(dev->dev_addr));
		envp[i++] = strdup(buf);
	}

	if (dev->dev_name[0]) {
		snprintf(buf, sizeof(buf), "HNAME=%s", dev->dev_name);
		envp[i++] = strdup(buf);
	}

	if (dev->dev_bridge[0]) {
		snprintf(buf, sizeof(buf), "BRIDGE=%s", dev->dev_bridge);
		envp[i++] = strdup(buf);
	}

	if (op == CFG)
		envp[i++] = strdup("SKIP_CREATE=yes");

	envp[i] = NULL;

	if (op == ADD) {
		char *argv[] = { VPS_NETNS_DEV_ADD, NULL };

		ret = run_script(VPS_NETNS_DEV_ADD, argv, envp, 0);
	} else  {
		char *argv[] = { VPS_NETNS_DEV_DEL, NULL };

		ret = run_script(VPS_NETNS_DEV_DEL, argv, envp, 0);
	}
	free_arg(envp);

	return ret;
}

static int ct_setcontext(envid_t veid)
{
	return 0;
}

static int ct_chkpnt(vps_handler *h, envid_t veid,
			const fs_param *fs, int cmd, cpt_param *param)
{
	const char *dumpfile = NULL;
	char statefile[STR_SIZE], buf[STR_SIZE];
	char *arg[2], *env[4];
	FILE *sfile;
	pid_t pid;
	int ret;

	get_dump_file(veid, param->dumpdir, buf, sizeof(buf));
	dumpfile = strdup(buf);

	arg[0] = SCRIPTDIR "/vps-cpt";
	arg[1] = NULL;

	get_state_file(veid, statefile, sizeof(statefile));
	sfile = fopen(statefile, "r");
	if (sfile == NULL) {
		logger(-1, errno, "Unable to open %s", statefile);
		return VZ_CHKPNT_ERROR;
	}

	ret = fscanf(sfile, "%d", &pid);
	if (ret != 1) {
		logger(-1, errno, "Unable to read PID from %s", statefile);
		fclose(sfile);
		return VZ_CHKPNT_ERROR;
	}
	fclose(sfile);

	snprintf(buf, sizeof(buf), "VE_ROOT=%s", fs->root);
	env[0] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_PID=%d", pid);
	env[1] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_DUMP_DIR=%s", dumpfile);
	env[2] = strdup(buf);
	env[3] = NULL;

	ret = run_script(arg[0], arg, env, 0);
	free_arg(env);
	if (ret)
		ret=VZ_CHKPNT_ERROR;

	return ret;
}

static int ct_restore_fn(vps_handler *h, envid_t veid, const vps_res *res,
			  int wait_p, int old_wait_p, int err_p, void *data)
{
	char *argv[2], *env[9];
	const char *dumpfile = NULL;
	const char *statefile = NULL;
	cpt_param *param = data;
	veth_dev *veth;
	char buf[STR_SIZE], *pbuf;
	int ret;

	get_dump_file(veid, param->dumpdir, buf, sizeof(buf));
	dumpfile = strdup(buf);

	get_state_file(veid, buf, sizeof(buf));
	statefile = strdup(buf);

	argv[0] = SCRIPTDIR "/vps-rst";
	argv[1] = NULL;

	snprintf(buf, sizeof(buf), "VE_ROOT=%s", res->fs.root);
	env[0] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_DUMP_DIR=%s", dumpfile);
	env[1] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_STATE_FILE=%s", statefile);
	env[2] = strdup(buf);

	pbuf = buf;
	pbuf += snprintf(buf, sizeof(buf), "VE_VETH_DEVS=");
	list_for_each(veth, &res->veth.dev, list) {
		pbuf += snprintf(pbuf, sizeof(buf) - (pbuf - buf),
				"%s=%s\n", veth->dev_name_ve, veth->dev_name);
	}
	env[3] = strdup(buf);
	snprintf(buf, sizeof(buf), "VZCTL_PID=%d", getpid());
	env[4] = strdup(buf);
	snprintf(buf, sizeof(buf), "STATUSFD=%d", STDIN_FILENO);
	env[5] = strdup(buf);
	snprintf(buf, sizeof(buf), "WAITFD=%d", wait_p);
	env[6] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_NETNS_FILE=%s/%d", NETNS_RUN_DIR, veid);
	env[7] = strdup(buf);
	env[8] = NULL;

	ret = run_script(argv[0], argv, env, 0);
	free_arg(env);
	if (ret) {
		destroy_container(veid);
		return VZ_RESTORE_ERROR;
	}

	return 0;
}

static int ct_restore(vps_handler *h, envid_t veid, vps_param *vps_p, int cmd,
	cpt_param *param, skipFlags skip)
{
	return vps_start_custom(h, veid, vps_p,
			SKIP_CONFIGURE | SKIP_VETH_CREATE | skip,
			NULL, ct_restore_fn, param);
}

int ct_do_open(vps_handler *h, vps_param *param)
{
	int ret;
	struct stat st;
	unsigned long *local_uid = param->res.misc.local_uid;

	ret = container_init();
	if (ret) {
		/*
		 * We will use fprintf to stderr, and not the logger, because some commands,
		 * like vzctl status, will disable the logger altogether. We are early, and
		 * all those errors are considered fatal.
		 */
		fprintf(stderr, "Container init failed: %s\n", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	ret = mkdir(NETNS_RUN_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

	if (ret && (errno != EEXIST)) {
		fprintf(stderr, "Can't create directory %s: %s\n",
				NETNS_RUN_DIR, strerror(errno));
		return VZ_RESOURCE_ERROR;
	}

	h->can_join_pidns = !stat("/proc/self/ns/pid", &st);
	/*
	 * Being able to join the user namespace is a good indication that the
	 * user namespace is complete. For a long time, the user namespace
	 * existed, but were far away from being feature complete.  When
	 * running in such a kernel, joining the user namespace will just
	 * cripple our container, since we won't be able to do anything. It is
	 * only good for people who are okay running containers as root.
	 *
	 * It is not enough, however, for user namespaces to be present in the
	 * kernel. The container must have been setup to use it (we need the
	 * mapped user to own the files, etc. So we also need to find suitable
	 * configuration in the config files.
	 */
	h->can_join_userns = !stat("/proc/self/ns/user", &st) &&
				local_uid && (*local_uid != 0);
	h->is_run = ct_is_run;
	h->enter = ct_enter;
	h->destroy = ct_destroy;
	h->env_create = ct_env_create;
	h->env_chkpnt = ct_chkpnt;
	h->env_restore = ct_restore;
	h->setlimits = ct_setlimits;
	h->setcpus = ct_setcpus;
	h->setcontext = ct_setcontext;
	h->setdevperm = ct_setdevperm;
	h->netdev_ctl = ct_netdev_ctl;
	h->ip_ctl = ct_ip_ctl;
	h->veth_ctl = ct_veth_ctl;

	return 0;
}
