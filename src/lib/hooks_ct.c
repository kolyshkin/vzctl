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
#include "util.h"
#include "logger.h"
#include "script.h"
#include "cgroup.h"

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
	return destroy_container(veid);
}

int ct_chroot(const char *root)
{
	char oldroot[] = "vzctl-old-root.XXXXXX";
	int ret = VZ_RESOURCE_ERROR;

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

static int _env_create(void *data)
{
	struct arg_start *arg = data;
	struct env_create_param3 create_param;
	int ret;

	if ((ret = ct_chroot(arg->res->fs.root)))
		return ret;

	if ((ret = vps_set_cap(arg->veid, &arg->res->env, &arg->res->cap, 1)))
		return ret;

	fill_container_param(arg, &create_param);

	/* Close all fds except stdin. stdin is status pipe */
	close(STDERR_FILENO); close(STDOUT_FILENO);
	close_fds(0, arg->wait_p, arg->err_p, -1);

	return exec_container_init(arg, &create_param);
}

static int ct_env_create(struct arg_start *arg)
{

	long stack_size = sysconf(_SC_PAGESIZE);
	void *child_stack = (char *)alloca(stack_size) + stack_size;
	int clone_flags;
	int ret;
	char procpath[STR_SIZE];
	char ctpath[STR_SIZE];


	/* non-fatal */
	if ((ret = ct_destroy(arg->h, arg->veid)))
		logger(0, 0, "Could not properly cleanup container: %s",
			container_error(ret));

	if (child_stack == NULL) {
		logger(-1, errno, "Unable to alloc");
		return VZ_RESOURCE_ERROR;
	}
	snprintf(ctpath, STR_SIZE, "%s/%d", NETNS_RUN_DIR, arg->veid);
	unlink(ctpath);

	if ((ret = create_container(arg->veid))) {
		logger(-1, 0, "Container creation failed: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	if ((ret = container_add_task(arg->veid))) {
		logger(-1, 0, "Can't add task creator to container: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	/*
	 * Belong in the setup phase
	 */
	clone_flags = SIGCHLD;
	/* FIXME: USERNS is still work in progress */
	clone_flags |= CLONE_NEWUTS|CLONE_NEWPID|CLONE_NEWIPC;
	clone_flags |= CLONE_NEWNET|CLONE_NEWNS;

	ret = clone(_env_create, child_stack, clone_flags, arg);
	if (ret  < 0) {
		logger(-1, errno, "Unable to clone");
		/* FIXME: remove ourselves from container first */
		destroy_container(arg->veid);
		return VZ_RESOURCE_ERROR;
	}

	snprintf(procpath, STR_SIZE, "/proc/%d/ns/net", ret);
	ret = symlink(procpath, ctpath);
	if (ret) {
		logger(-1, errno, "Can't symlink into netns file %s", ctpath);
		destroy_container(arg->veid);
		return VZ_RESOURCE_ERROR;
	}

	return 0;
}

static int __ct_enter(vps_handler *h, envid_t veid, int flags)
{
	DIR *dp;
	struct dirent *ep;
	char path[STR_SIZE]; /* long enough for any pid */
	pid_t task_pid;
	int ret = VZ_RESOURCE_ERROR;

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

	if ((ret = container_add_task(veid))) {
		logger(-1, 0, "Can't add task creator to container: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}

	ret = VZ_RESOURCE_ERROR;
	while ((ep = readdir (dp))) {
		int fd;
		if (!strcmp(ep->d_name, "."))
			continue;
		if (!strcmp(ep->d_name, ".."))
			continue;

		if (snprintf(path, sizeof(path), "/proc/%d/ns/%s", task_pid, ep->d_name) < 0)
			goto out;
		if ((fd = open(path, O_RDONLY)) < 0)
			goto out;
		if (setns(fd, 0))
			logger(-1, errno, "Failed to set context for %s", ep->d_name);
	}
	ret = 0;

out:
	closedir(dp);
	return ret;
}

/*
 * We need to do chroot only after the context is set. Otherwise, we can't find the proc files
 * we need to operate on the ns files
 */
static int ct_enter(vps_handler *h, envid_t veid, const char *root, int flags)
{
	int ret;
	if ((ret = __ct_enter(h, veid, flags)))
		return ret;
	if ((ret = ct_chroot(root)))
		return ret;
	return 0;
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
	if (max_lim != ~0ULL)
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
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
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
	char *envp[10];
	char buf[STR_SIZE];
	int i = 0;

	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);

	snprintf(buf, sizeof(buf), "VNAME=%s", dev->dev_name_ve);
	envp[i++] = strdup(buf);

	if (dev->dev_addr_ve) {
		snprintf(buf, sizeof(buf), "VMAC=%s", dev->dev_addr_ve);
		envp[i++] = strdup(buf);
	}

	if (dev->dev_addr) {
		snprintf(buf, sizeof(buf), "HMAC=%s", dev->dev_addr);
		envp[i++] = strdup(buf);
	}

	if (dev->dev_name) {
		snprintf(buf, sizeof(buf), "HNAME=%s", dev->dev_name);
		envp[i++] = strdup(buf);
	}

	if (dev->dev_bridge) {
		snprintf(buf, sizeof(buf), "BRIDGE=%s", dev->dev_bridge);
		envp[i++] = strdup(buf);
	}

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

int ct_do_open(vps_handler *h)
{
	int ret;
	char path[STR_SIZE];
	struct stat st;

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

	if (snprintf(path, sizeof(path), "/proc/%d/ns/pid", getpid()) < 0)
		return VZ_RESOURCE_ERROR;

	ret = mkdir(NETNS_RUN_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

	if (ret && (errno != EEXIST)) {
		fprintf(stderr, "Can't create directory %s (%s\n)", NETNS_RUN_DIR, strerror(errno));
		return VZ_RESOURCE_ERROR;
	}

	h->can_join_pidns = !stat(path, &st);
	h->is_run = ct_is_run;
	h->enter = ct_enter;
	h->destroy = ct_destroy;
	h->env_create = ct_env_create;
	h->setlimits = ct_setlimits;
	h->setcpus = ct_setcpus;
	h->setcontext = ct_setcontext;
	h->setdevperm = ct_setdevperm;
	h->netdev_ctl = ct_netdev_ctl;
	h->ip_ctl = ct_ip_ctl;
	h->veth_ctl = ct_veth_ctl;

	return 0;
}
