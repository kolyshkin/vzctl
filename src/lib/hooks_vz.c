/*
 *  Copyright (C) 2000-2012, Parallels, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/personality.h>
#include <linux/vzcalluser.h>
#include <linux/vzctl_venet.h>

#include "env.h"
#include "util.h"
#include "types.h"
#include "logger.h"
#include "vzerror.h"
#include "readelf.h"
#include "vzsyscalls.h"

#define ENVRETRY	3

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

static int vz_env_create_ioctl(vps_handler *h, envid_t veid, int flags)
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

static int vz_is_run(vps_handler *h, envid_t veid)
{
	int ret = vz_env_create_ioctl(h, veid, VE_TEST);

	if (ret < 0 && (errno == ESRCH || errno == ENOTTY))
		return 0;
	else if (ret < 0)
		logger(-1, errno, "Error on vz_env_create_ioctl(VE_TEST)");
	return 1;
}

static int vz_enter(vps_handler *h, envid_t veid, const char *root, int flags)
{
	int ret;

	if ((ret = vz_chroot(root)))
		return ret;

	ret = vz_env_create_ioctl(h, veid, VE_ENTER | flags);
	if (ret < 0) {
		if (errno == ESRCH)
			ret = VZ_VE_NOT_RUNNING;
		else
			ret = VZ_ENVCREATE_ERROR;
	}
	else
		ret = 0;

	close(h->vzfd);
	return ret;
}

static int vz_destroy(vps_handler *h, envid_t veid)
{
	/* Destroys automatically after reboot */
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

static int vz_env_create_data_ioctl(vps_handler *h,
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

static int _env_create(vps_handler *h, void *data)
{
	struct vzctl_env_create_data env_create_data;
	struct env_create_param3 create_param;
	int ret;
	struct arg_start *arg = data;
	envid_t veid = arg->veid;
	int wait_p = arg->wait_p;
	int err_p = arg->err_p;

	fill_container_param(arg, &create_param);

	env_create_data.veid = veid;
	env_create_data.class_id = 0;
	env_create_data.flags = VE_CREATE | VE_EXCLUSIVE;
	env_create_data.data = &create_param;
	env_create_data.datalen = sizeof(create_param);

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
		return ret;
	}

	if (arg->res->env.osrelease != NULL) {
		ret = vz_env_configure(h->vzfd, veid,
				arg->res->env.osrelease);
		if (ret != 0)
			return ret;
	}

	close(h->vzfd);
	return exec_container_init(arg, &create_param);
}

static inline int setluid(uid_t uid)
{
	return syscall(__NR_setluid, uid);
}

static int vz_setluid(envid_t veid)
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

static int vz_do_env_create(struct arg_start *arg)
{
	int ret, pid;
	int wait_p = arg->wait_p;
	int old_wait_p = arg->old_wait_p;
	int err_p = arg->err_p;
	env_create_FN fn = arg->fn;
	void *data = arg->data;
	struct vps_res *res = arg->res;
	vps_handler *h = arg->h;
	envid_t veid = arg->veid;

	if ((ret = vz_chroot(res->fs.root)))
		return ret;
	if ((ret = vz_setluid(veid)))
		return ret;
	if ((ret = set_ublimit(h, veid, &res->ub)))
		return ret;
	/* Create another process for proper resource accounting */
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Unable to fork");
		return VZ_RESOURCE_ERROR;
	} else if (pid == 0) {
		if ((ret = vps_set_cap(veid, &res->env, &res->cap)))
			goto env_err;
		if (fn == NULL) {
			ret = _env_create(h, (void *)arg);
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

static int vz_setcpu(vps_handler *h, envid_t veid, cpu_param *cpu)
{
	int ret = 0;

	if (cpu->limit != NULL)
		ret = set_cpulimit(veid, *cpu->limit);

	if (cpu->units != NULL)
		ret = set_cpuunits(veid, *cpu->units);
	else if (cpu->weight != NULL)
		ret = set_cpuweight(veid, *cpu->weight);

	if (cpu->vcpus != NULL)
		ret = env_set_vcpus(veid, *cpu->vcpus);
	if (cpu->mask != NULL)
		ret = set_cpumask(veid, cpu->mask);
	return ret;
}

static int vz_set_devperm(vps_handler *h, envid_t veid, dev_res *dev)
{
	struct vzctl_setdevperms devperms;

	devperms.veid = veid;
	devperms.dev = dev->dev;
	devperms.mask = dev->mask;
	devperms.type = dev->type;

	if (ioctl(h->vzfd, VZCTL_SETDEVPERMS, &devperms)) {
		logger(-1, errno, "Error setting device permissions");
		return VZ_SET_DEVICES;
	}

	return 0;
}

static int vz_netdev_ctl(vps_handler *h, envid_t veid, int op, char *name)
{
	struct vzctl_ve_netdev ve_netdev;

	ve_netdev.veid = veid;
	ve_netdev.op = op;
	ve_netdev.dev_name = name;
	if (ioctl(h->vzfd, VZCTL_VE_NETDEV, &ve_netdev) < 0)
		return VZ_NETDEV_ERROR;
	return 0;
}

static int vz_ip_ctl(vps_handler *h, envid_t veid, int op, const char *ipstr)
{
	struct vzctl_ve_ip_map ip_map;
	int family;
	unsigned int ip[4];
	int ret;

	union {
		struct sockaddr_in  a4;
		struct sockaddr_in6 a6;
	} addr;

	if ((family = get_netaddr(ipstr, ip)) < 0)
		return 0;


	if (family == AF_INET) {
		addr.a4.sin_family = AF_INET;
		addr.a4.sin_addr.s_addr = ip[0];
		addr.a4.sin_port = 0;
		ip_map.addrlen = sizeof(addr.a4);
	} else if (family == AF_INET6) {
		addr.a6.sin6_family = AF_INET6;
		memcpy(&addr.a6.sin6_addr, ip, 16);
		addr.a6.sin6_port = 0;
		ip_map.addrlen = sizeof(addr.a6);
	} else {
		return -EAFNOSUPPORT;
	}

	ip_map.veid = veid;
	ip_map.op = op;
	ip_map.addr = (struct sockaddr*) &addr;

	ret = ioctl(h->vzfd, VENETCTL_VE_IP_MAP, ip_map);
	if (ret) {
		switch (errno) {
			case EADDRINUSE:
				ret = VZ_IP_INUSE;
				break;
			case ESRCH:
				ret = VZ_VE_NOT_RUNNING;
				break;
			case EADDRNOTAVAIL:
				if (op == VE_IP_DEL)
					return 0;
				ret = VZ_IP_NA;
				break;
			default:
				ret = VZ_CANT_ADDIP;
				break;
		}
		logger(-1, errno, "Unable to %s IP %s",
			op == VE_IP_ADD ? "add" : "del", ipstr);
	}
	return ret;

}

int vz_do_open(vps_handler *h)
{
	if ((h->vzfd = open(VZCTLDEV, O_RDWR)) < 0) {
		logger(-1, errno, "Unable to open %s", VZCTLDEV);
		logger(-1, 0, "Please check that vzdev kernel module is loaded"
			" and you have sufficient permissions"
			" to access the file.");
		return -1;
	}

	if (vz_env_create_ioctl(h, 0, 0) < 0 &&
		(errno == ENOSYS || errno == EPERM))
	{
		logger(-1, 0, "Your kernel lacks support for virtual"
			" environments or modules not loaded");
		goto err;
	}

	h->is_run = vz_is_run;
	h->enter = vz_enter;
	h->destroy = vz_destroy;
	h->env_create = vz_do_env_create;
	h->setlimits = set_ublimit;
	h->setcpus = vz_setcpu;
	h->setcontext = vz_setluid;
	h->setdevperm = vz_set_devperm;
	h->netdev_ctl = vz_netdev_ctl;
	h->ip_ctl = vz_ip_ctl;
	return 0;
err:
	close(h->vzfd);
	return -1;
}
