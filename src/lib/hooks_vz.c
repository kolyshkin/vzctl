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
#include <stdio.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/vzcalluser.h>
#include <linux/vzctl_veth.h>
#include <linux/vzctl_venet.h>
#include <linux/cpt_ioctl.h>

#include "env.h"
#include "exec.h"
#include "cpt.h"
#include "util.h"
#include "types.h"
#include "logger.h"
#include "vzerror.h"
#include "vzsyscalls.h"

#define ENVRETRY	3

static int create_hardlink_dir(void);

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
		/* Set personality PER_LINUX32 for i386 based CTs */
		set_personality32();
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

	return errcode;
}

static int _env_create(vps_handler *h, void *data)
{
	struct vzctl_env_create_data env_create_data;
	struct env_create_param3 create_param;
	int ret;
	struct arg_start *arg = data;
	envid_t veid = arg->veid;

	clean_hardlink_dir("/");
	if (create_hardlink_dir())
		return VZ_SYSTEM_ERROR;
	fill_container_param(arg, &create_param);

	env_create_data.veid = veid;
	env_create_data.class_id = 0;
	env_create_data.flags = VE_CREATE | VE_EXCLUSIVE;
	env_create_data.data = &create_param;
	env_create_data.datalen = sizeof(create_param);

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
		if ((ret = vps_set_cap(veid, &res->env, &res->cap, 0)))
			goto env_err;
		if (fn == NULL) {
			ret = _env_create(h, (void *)arg);
		} else {
			ret = fn(h, veid, res, wait_p, old_wait_p, err_p, data);
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
	int ret;

	if (cpu->limit) {
		ret = set_cpulimit(veid, *cpu->limit);
		if (ret)
			return ret;
	}

	if (cpu->units) {
		ret = set_cpuunits(veid, *cpu->units);
		if (ret)
			return ret;
	}
	else if (cpu->weight) {
		ret = set_cpuweight(veid, *cpu->weight);
		if (ret)
			return ret;
	}

	if (cpu->vcpus) {
		ret = env_set_vcpus(veid, *cpu->vcpus);
		if (ret)
			return ret;
	}

	if (cpu->mask && !cpu->cpumask_auto) {
		ret = set_cpumask(veid, cpu->mask);
		if (ret)
			return ret;
	}

	if (cpu->nodemask) {
		ret = set_nodemask(veid, cpu->nodemask);
		if (ret)
			return ret;

		if (!cpu->mask || cpu->cpumask_auto) {
			cpumask_t mask;

			ret = get_node_cpumask(cpu->nodemask, &mask);
			if (ret)
				return ret;

			ret = set_cpumask(veid, &mask);
			if (ret)
				return ret;
		}
	}

	return 0;
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

	ret = ioctl(h->vzfd, VENETCTL_VE_IP_MAP, &ip_map);
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

static int veth_dev_mac_filter(vps_handler *h, envid_t veid, veth_dev *dev)
{
	struct vzctl_ve_hwaddr veth;
	int ret;

	veth.op = dev->mac_filter == YES ? VE_ETH_DENY_MAC_CHANGE :
							VE_ETH_ALLOW_MAC_CHANGE;
	veth.veid = veid;
	memcpy(veth.dev_name, dev->dev_name, IFNAMSIZE);
	memcpy(veth.dev_name_ve, dev->dev_name_ve, IFNAMSIZE);
	ret = ioctl(h->vzfd, VETHCTL_VE_HWADDR, &veth);
	if (ret) {
		if (errno != ENODEV) {
			logger(-1, errno, "Unable to set mac filter");
			ret = VZ_VETH_ERROR;
		} else
			ret = 0;
	}
	return ret;
}

static int veth_dev_create(vps_handler *h, envid_t veid, veth_dev *dev)
{
	struct vzctl_ve_hwaddr veth;

	if (!dev->dev_name[0] || dev->addrlen != ETH_ALEN)
		return VZ_VETH_ERROR;
	if (dev->addrlen_ve != 0 && dev->addrlen_ve != ETH_ALEN)
		return VZ_VETH_ERROR;
	veth.op = VE_ETH_ADD;
	veth.veid = veid;
	veth.addrlen = dev->addrlen;
	veth.addrlen_ve = dev->addrlen_ve;
	memcpy(veth.dev_addr, dev->dev_addr, ETH_ALEN);
	memcpy(veth.dev_addr_ve, dev->dev_addr_ve, ETH_ALEN);
	memcpy(veth.dev_name, dev->dev_name, IFNAMSIZE);
	memcpy(veth.dev_name_ve, dev->dev_name_ve, IFNAMSIZE);
	if (ioctl(h->vzfd, VETHCTL_VE_HWADDR, &veth) != 0) {
		if (errno == ENOTTY) {
			logger(-1, 0, "Error: veth feature is"
				" not supported by kernel");
			logger(-1, 0, "Please check that vzethdev"
				" kernel module is loaded");
		} else {
			logger(-1, errno, "Unable to create veth");
		}
		return VZ_VETH_ERROR;
	}

	return 0;
}

static int veth_dev_remove(vps_handler *h, envid_t veid, veth_dev *dev)
{
	struct vzctl_ve_hwaddr veth;
	int ret;

	if (!dev->dev_name[0])
		return EINVAL;
	veth.op = VE_ETH_DEL;
	veth.veid = veid;
	memcpy(veth.dev_name, dev->dev_name, IFNAMSIZE);
	ret = ioctl(h->vzfd, VETHCTL_VE_HWADDR, &veth);
	if (ret) {
		if (errno != ENODEV) {
			logger(-1, errno, "Unable to remove veth");
			ret = VZ_VETH_ERROR;
		} else
			ret = 0;
	}
	return ret;
}

static int vz_veth_ctl(vps_handler *h, envid_t veid, int op, veth_dev *dev)
{
	int ret = 0;
	if (op == ADD) {
		if (!dev->active) {
			if ((ret = veth_dev_create(h, veid, dev)))
				return ret;
		}
		dev->flags = 1;
		if (dev->mac_filter) {
			if ((ret = veth_dev_mac_filter(h, veid, dev)))
				return ret;
		}
	} else {
		if (!dev->active)
			return ret;
		ret = veth_dev_remove(h, veid, dev);
	}
	return ret;
}

/* with mix of md5sum: try generate unique name */
#define CPT_HARDLINK_DIR "/.cpt_hardlink_dir_a920e4ddc233afddc9fb53d26c392319"

void clean_hardlink_dir(const char *mntdir)
{
	char buf[PATH_MAX];
	struct dirent *ep;
	struct stat st;
	DIR *dp;

	snprintf(buf, sizeof(buf), "%s%s", mntdir, CPT_HARDLINK_DIR);
	if (stat(buf, &st))
		/* If we can't stat it we can't clean it */
		return;
	if (!S_ISDIR(st.st_mode)) {
		unlink(buf);	/* if file was created by someone */
		return;
	}

	if (!(dp = opendir(buf)))
		return;
	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;

		snprintf(buf, sizeof(buf), "%s%s/%s",
				mntdir, CPT_HARDLINK_DIR, ep->d_name);
		if (unlink(buf))
			logger(-1, errno, "Warning: unlink %s failed", buf);
	}
	closedir(dp);
}

static int create_hardlink_dir(void) {
	struct stat st;
	int ret;

	ret = stat(CPT_HARDLINK_DIR, &st);
	if (ret && errno != ENOENT) {
		logger(-1, errno, "Can't stat %s", CPT_HARDLINK_DIR);
		return -1;
	}

	if (ret == 0 && S_ISDIR(st.st_mode)) {
		/* dir exists -- make sure mode/ownership is right */
		if ((st.st_mode & 07777) != 0700)
			chmod(CPT_HARDLINK_DIR, 0700);
		if ((st.st_uid != 0) || (st.st_gid != 0))
			chown(CPT_HARDLINK_DIR, 0, 0);
		return 0;
	}

	/* CPT_HARDLINK_DIR doesn't exist, or is not a directory */
	if (unlink(CPT_HARDLINK_DIR) && errno != ENOENT)
		logger(-1, errno, "Warning: can't unlink %s",
				CPT_HARDLINK_DIR);

	if (mkdir(CPT_HARDLINK_DIR, 0700) && errno != EEXIST)
		return vzctl_err(VZ_SYSTEM_ERROR, errno,
				"Unable to create hardlink directory %s",
				CPT_HARDLINK_DIR);
	return 0;
}

static int setup_hardlink_dir(int cpt_fd)
{
	int fd, res = 0;

	if (create_hardlink_dir())
		return -1;
	fd = open(CPT_HARDLINK_DIR, O_RDONLY | O_NOFOLLOW | O_DIRECTORY);
	if (fd < 0) {
		logger(-1, errno, "Error: Unable to open "
				"hardlink directory %s", CPT_HARDLINK_DIR);
		return 1;
	}

	if (ioctl(cpt_fd, CPT_LINKDIR_ADD, fd) < 0) {
		if (errno != EINVAL) {
			res = 1;
			logger(-1, errno, "Cannot set linkdir in kernel");
		}
	}

	close(fd);
	return res;
}

static int real_chkpnt(int cpt_fd, envid_t veid, const char *root,
		cpt_param *param, int cmd)
{
	int ret, len, len1;
	char buf[PIPE_BUF];
	int err_p[2];

	if ((ret = vz_chroot(root)))
		return VZ_CHKPNT_ERROR;
	if (pipe(err_p) < 0) {
		logger(-1, errno, "Can't create pipe");
		return VZ_CHKPNT_ERROR;
	}
	fcntl(err_p[0], F_SETFL, O_NONBLOCK);
	fcntl(err_p[1], F_SETFL, O_NONBLOCK);
	if (ioctl(cpt_fd, CPT_SET_ERRORFD, err_p[1]) < 0) {
		logger(-1, errno, "Can't set errorfd");
		return VZ_CHKPNT_ERROR;
	}
	close(err_p[1]);
	if (cmd == CMD_CHKPNT || cmd == CMD_SUSPEND) {
		logger(0, 0, "\tsuspend...");
		if (ioctl(cpt_fd, CPT_SUSPEND, 0) < 0) {
			logger(-1, errno, "Can not suspend container");
			goto err_out;
		}
	}
	if (cmd == CMD_CHKPNT || cmd == CMD_DUMP) {
		logger(0, 0, "\tdump...");
		clean_hardlink_dir("/");
		if (setup_hardlink_dir(cpt_fd))
			goto err_out;
		if (ioctl(cpt_fd, CPT_DUMP, 0) < 0) {
			logger(-1, errno, "Can not dump container");
			if (cmd == CMD_CHKPNT) {
				clean_hardlink_dir("/");
				if (ioctl(cpt_fd, CPT_RESUME, 0) < 0)
					logger(-1, errno, "Can not "
							"resume container");
			}
			goto err_out;
		}
	}
	if (cmd == CMD_CHKPNT) {
		logger(0, 0, "\tkill...");
		if (ioctl(cpt_fd, CPT_KILL, 0) < 0) {
			logger(-1, errno, "Can not kill container");
			goto err_out;
		}
	}
	if (cmd == CMD_SUSPEND && !param->ctx) {
		logger(0, 0, "\tget context...");
		if (ioctl(cpt_fd, CPT_GET_CONTEXT, veid) < 0) {
			logger(-1, errno, "Can not get context");
			goto err_out;
		}
	}
	close(err_p[0]);
	return 0;
err_out:
	while ((len = read(err_p[0], buf, PIPE_BUF)) > 0) {
		do {
			len1 = write(STDERR_FILENO, buf, len);
			len -= len1;
		} while (len > 0 && len1 > 0);
		if (cmd == CMD_SUSPEND && param->ctx) {
			/* destroy context */
			if (ioctl(cpt_fd, CPT_PUT_CONTEXT, veid) < 0)
				logger(-1, errno, "Can't put context");
		}
	}
	fflush(stderr);
	close(err_p[0]);
	return VZ_CHKPNT_ERROR;
}

#define GET_DUMP_FILE(req_cmd)							\
do {										\
	dumpfile = param->dumpfile;						\
	if (dumpfile == NULL) {							\
		if (cmd == req_cmd) {						\
			logger(-1,  0, "Error: dumpfile is not specified");	\
			goto err;						\
		}								\
		get_dump_file(veid, param->dumpdir, buf, sizeof(buf));		\
		dumpfile = buf;							\
	}									\
} while (0)

static int vz_chkpnt(vps_handler *h, envid_t veid,
		     const fs_param *fs, int cmd, cpt_param *param)
{
	int dump_fd = -1;
	char buf[PATH_LEN];
	const char *dumpfile = NULL;
	int cpt_fd, pid, ret;
	const char *root = fs->root;

	ret = VZ_CHKPNT_ERROR;

	logger(0, 0, "Setting up checkpoint...");
	if ((cpt_fd = open(PROC_CPT, O_RDWR)) < 0) {
		if (errno == ENOENT)
			logger(-1, errno, "Error: No checkpointing"
				" support, unable to open " PROC_CPT);
		else
			logger(-1, errno, "Unable to open " PROC_CPT);
		return VZ_CHKPNT_ERROR;
	}
	if ((cmd == CMD_CHKPNT || cmd == CMD_DUMP)) {
		GET_DUMP_FILE(CMD_DUMP);
		make_dir(dumpfile, 0);
		dump_fd = open(dumpfile, O_CREAT|O_TRUNC|O_RDWR, 0600);
		if (dump_fd < 0) {
			logger(-1, errno, "Can not create dump file %s",
					dumpfile);
			goto err;
		}
	}
	if (param->ctx || cmd > CMD_SUSPEND) {
		logger(0, 0, "\tjoin context..");
		if (ioctl(cpt_fd, CPT_JOIN_CONTEXT, param->ctx ? : veid) < 0) {
			logger(-1, errno, "Can not join cpt context");
			goto err;
		}
	} else {
		if (ioctl(cpt_fd, CPT_SET_VEID, veid) < 0) {
			logger(-1, errno, "Can not set CT ID");
			goto err;
		}
	}
	if (dump_fd != -1) {
		if (ioctl(cpt_fd, CPT_SET_DUMPFD, dump_fd) < 0) {
			logger(-1, errno, "Can not set dump file");
			goto err;
		}
	}
	if (param->cpu_flags) {
		logger(0, 0, "\tset CPU flags..");
		if (ioctl(cpt_fd, CPT_SET_CPU_FLAGS, param->cpu_flags) < 0) {
			logger(-1, errno, "Can not set CPU flags");
			goto err;
		}
	}
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can't fork");
		ret = VZ_RESOURCE_ERROR;
		goto err;
	} else if (pid == 0) {
		if ((ret = h->setcontext(veid)))
			exit(ret);
		if ((pid = fork()) < 0) {
			logger(-1, errno, "Can't fork");
			exit(VZ_RESOURCE_ERROR);
		} else if (pid == 0) {
			ret = real_chkpnt(cpt_fd, veid, root, param, cmd);
			exit(ret);
		}
		ret = env_wait(pid);
		exit(ret);
	}
	close(cpt_fd);
	cpt_fd = -1;
	ret = env_wait(pid);
	if (ret)
		goto err;
	ret = 0;
	logger(0, 0, "Checkpointing completed successfully");
err:
	if (ret) {
		ret = VZ_CHKPNT_ERROR;
		logger(-1, 0, "Checkpointing failed");
		if (cmd == CMD_CHKPNT || cmd == CMD_DUMP)
			if (dumpfile)
				unlink(dumpfile);
	}
	if (dump_fd != -1) {
		if (ret == 0)
			fsync(dump_fd);
		close(dump_fd);
	}
	if (cpt_fd != -1)
		close(cpt_fd);

	return ret;
}

static int restore_fn(vps_handler *h, envid_t veid, const vps_res *res,
			int wait_p, int old_wait_p, int err_p, void *data)
{
	int status, len, len1, ret;
	cpt_param *param = (cpt_param *) data;
	char buf[PIPE_BUF];
	int error_pipe[2];
	const char *fail = "";

	status = VZ_RESTORE_ERROR;
	if (param == NULL)
		goto err_pipe;
	/* Close all fds */
	close_fds(0, wait_p, old_wait_p, err_p, param->rst_fd, h->vzfd, -1);
	if (ioctl(param->rst_fd, CPT_SET_VEID, veid) < 0) {
		logger(-1, errno, "Can't set CT ID %d", param->rst_fd);
		goto err_pipe;
	}
	if (pipe(error_pipe) < 0 ) {
		logger(-1, errno, "Can't create pipe");
		goto err_pipe;
	}
	fcntl(error_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(error_pipe[1], F_SETFL, O_NONBLOCK);
	if (ioctl(param->rst_fd, CPT_SET_ERRORFD, error_pipe[1]) < 0) {
		logger(-1, errno, "Can't set errorfd");
		goto err;
	}
	close(error_pipe[1]);

	ret = ioctl(param->rst_fd, CPT_SET_LOCKFD2, wait_p);
	if (ret < 0 && errno == EINVAL) {
		logger(0, 0, "Warning: old kernel -- CPT_SET_LOCKFD2 "
				"not supported");
		ret = ioctl(param->rst_fd, CPT_SET_LOCKFD, old_wait_p);
	}
	if (ret < 0) {
		logger(-1, errno, "Can't set lockfd");
		goto err;
	}

	if (ioctl(param->rst_fd, CPT_SET_STATUSFD, STDIN_FILENO) < 0) {
		logger(-1, errno, "Can't set statusfd");
		goto err;
	}
	/* Close status descriptor to report that
	* environment is created.
	*/
	close(STDIN_FILENO);

	ioctl(param->rst_fd, CPT_HARDLNK_ON);

	logger(0, 0, "\tundump...");
	if (ioctl(param->rst_fd, CPT_UNDUMP, 0) != 0) {
		fail = "undump failed";
		goto err_undump;
	}
	/* Now we wait until CT setup will be done */
	read(wait_p, &len, sizeof(len));
	if (param->cmd == CMD_RESTORE) {
		clean_hardlink_dir("/");
		logger(0, 0, "\tresume...");
		if (ioctl(param->rst_fd, CPT_RESUME, 0)) {
			fail = "resume failed";
			goto err_undump;
		}
	} else if (param->cmd == CMD_UNDUMP && !param->ctx) {
		logger(0, 0, "\tget context...");
		if (ioctl(param->rst_fd, CPT_GET_CONTEXT, veid) < 0) {
			fail = "can not get context";
			goto err_undump;
		}
	}
	status = 0;

err:
	close(error_pipe[0]);
err_pipe:
	if (status)
		write(err_p, &status, sizeof(status));
	return status;

err_undump:
	logger(-1, errno, "Restore error, %s", fail);
	while ((len = read(error_pipe[0], buf, PIPE_BUF)) > 0) {
		do {
			len1 = write(STDERR_FILENO, buf, len);
			len -= len1;
		} while (len > 0 && len1 > 0);
	}
	fflush(stderr);
	close(error_pipe[0]);
	write(err_p, &status, sizeof(status));
	return status;
}

static int vz_restore(vps_handler *h, envid_t veid, vps_param *vps_p,
			int cmd, cpt_param *param, skipFlags skip)
{
	int ret, rst_fd;
	int dump_fd = -1;
	char buf[PATH_LEN];
	const char *dumpfile = NULL;

	logger(0, 0, "Restoring container ...");
	ret = VZ_RESTORE_ERROR;
	if ((rst_fd = open(PROC_RST, O_RDWR)) < 0) {
		if (errno == ENOENT)
			logger(-1, errno, "Error: No checkpointing"
				" support, unable to open " PROC_RST);
		else
			logger(-1, errno, "Unable to open " PROC_RST);
		return VZ_RESTORE_ERROR;
	}
	if (param->ctx) {
		if (ioctl(rst_fd, CPT_JOIN_CONTEXT, param->ctx) < 0) {
			logger(-1, errno, "Can not join cpt context");
			goto err;
		}
	}
	GET_DUMP_FILE(CMD_UNDUMP);
	if (cmd == CMD_RESTORE || cmd == CMD_UNDUMP) {
		dump_fd = open(dumpfile, O_RDONLY);
		if (dump_fd < 0) {
			logger(-1, errno, "Unable to open %s", dumpfile);
			goto err;
		}
	}
	if (dump_fd != -1) {
		if (ioctl(rst_fd, CPT_SET_DUMPFD, dump_fd)) {
			logger(-1, errno, "Can't set dumpfile");
			goto err;
		}
	}
	param->rst_fd = rst_fd;
	param->cmd = cmd;
	ret = vps_start_custom(h, veid, vps_p,
			SKIP_CONFIGURE | skip,
			NULL, restore_fn, param);
	if (ret)
		goto err;
err:
	close(rst_fd);
	if (dump_fd != -1)
		close(dump_fd);
	if (!ret) {
		logger(0, 0, "Restoring completed successfully");
		if ((cmd == CMD_RESTORE) && (dumpfile) &&
				!(skip & SKIP_DUMPFILE_UNLINK))
			unlink(dumpfile);
	}
	return ret;
}

#undef GET_DUMP_FILE

int vz_do_open(vps_handler *h, vps_param *param)
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
	h->env_chkpnt = vz_chkpnt;
	h->env_restore = vz_restore;
	h->setlimits = set_ublimit;
	h->setcpus = vz_setcpu;
	h->setcontext = vz_setluid;
	h->setdevperm = vz_set_devperm;
	h->netdev_ctl = vz_netdev_ctl;
	h->ip_ctl = vz_ip_ctl;
	h->veth_ctl = vz_veth_ctl;

	return 0;
err:
	close(h->vzfd);
	return -1;
}
