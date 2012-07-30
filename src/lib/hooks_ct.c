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
#include "cgroup.h"

#define NETNS_RUN_DIR "/var/run/netns"

static int ct_is_run(vps_handler *h, envid_t veid)
{
	return container_is_running(veid);
}

static int ct_destroy(vps_handler *h, envid_t veid)
{
	char ctpath[STR_SIZE];

	snprintf(ctpath, STR_SIZE, "%s/%d", NETNS_RUN_DIR, veid);
	umount2(ctpath, MNT_DETACH);
	return destroy_container(veid);
}

static int _env_create(void *data)
{
	struct arg_start *arg = data;
	struct env_create_param3 create_param;
	int ret;

	if ((ret = vz_chroot(arg->res->fs.root)))
		return ret;

	if ((ret = vps_set_cap(arg->veid, &arg->res->env, &arg->res->cap)))
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

	snprintf(ctpath, STR_SIZE, "%s/%d", NETNS_RUN_DIR, arg->veid);
	if ((ret = open(ctpath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR)) < 0) {
		logger(-1, errno, "Can't create container netns file");
		return VZ_RESOURCE_ERROR;
	}
	close(ret);

	if (child_stack == NULL) {
		logger(-1, errno, "Unable to alloc");
		return VZ_RESOURCE_ERROR;
	}

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

	umount2(ctpath, MNT_DETACH);
	ret = mount(procpath, ctpath, "none", MS_MGC_VAL|MS_BIND, NULL);
	if (ret) {
		logger(-1, errno, "Can't mount into netns file %s", ctpath);
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
			logger(-1, 0, "Failed to set context for %s", ep->d_name);
	}
	ret = 0;

	if ((ret = container_add_task(veid))) {
		logger(-1, 0, "Can't add task creator to container: %s", container_error(ret));
		return VZ_RESOURCE_ERROR;
	}
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
	if ((ret = vz_chroot(root)))
		return ret;
	return 0;
}

static int ct_setlimits(vps_handler *h, envid_t veid, struct ub_struct *ub)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
}

static int ct_setcpus(vps_handler *h, envid_t veid, struct cpu_param *cpu)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
}

static int ct_setdevperm(vps_handler *h, envid_t veid, dev_res *dev)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
}

static int ct_netdev_ctl(vps_handler *h, envid_t veid, int op, char *name)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
}

static int ct_ip_ctl(vps_handler *h, envid_t veid, int op, const char *ipstr)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
}

static int ct_veth_ctl(vps_handler *h, envid_t veid, int op, veth_dev *dev)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return 0;
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
		logger(-1, 0, "Container init failed: %s", container_error(ret));
		return ret;
	}

	if (snprintf(path, sizeof(path), "/proc/%d/ns/pid", getpid()) < 0)
		return VZ_RESOURCE_ERROR;

	ret = mkdir(NETNS_RUN_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

	if (ret && (errno != EEXIST)) {
		logger(-1, errno, "Can't create directory ", NETNS_RUN_DIR);
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
