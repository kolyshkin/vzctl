#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "vzerror.h"
#include "env.h"
#include "logger.h"
#include "cgroup.h"

static int ct_is_run(vps_handler *h, envid_t veid)
{
	return container_is_running(veid);
}

static int ct_destroy(vps_handler *h, envid_t veid)
{
	return destroy_container(veid);
}

static int ct_env_create(struct arg_start *arg)
{
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return VZ_RESOURCE_ERROR;
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
	logger(-1, 0, "%s not yet supported upstream", __func__);
	return VZ_RESOURCE_ERROR;
}

static int ct_enter(vps_handler *h, envid_t veid, const char *root, int flags)
{
	if (!h->can_join_pidns) {
		logger(-1, 0, "Kernel lacks setns for pid namespace");
		return VZ_RESOURCE_ERROR;
	}
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
