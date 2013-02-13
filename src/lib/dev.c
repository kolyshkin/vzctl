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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <limits.h>

#include <linux/vzcalluser.h>

#include "vzerror.h"
#include "script.h"
#include "util.h"
#include "dev.h"
#include "env.h"
#include "logger.h"
#include "script.h"

/* Create an /etc/tmpfiles.d entry for systemd from Fedora 18+ */
static int create_tmpfiles_d_entry(const char *prefix,
		const char *name, const char *alias,
		mode_t mode, dev_t dev)
{
	FILE *fp;
	char buf[PATH_MAX];

	snprintf(buf, sizeof(buf), "%setc/tmpfiles.d", prefix);
	if (stat_file(buf) != 1)
		return 0;

	if (alias == NULL)
		alias = name;

	snprintf(buf, sizeof(buf), "%setc/tmpfiles.d/device-%s.conf",
			prefix, alias);
	logger(2, 0, "Creating %s", buf);
	fp = fopen(buf, "w");
	if (fp == NULL)
		return vzctl_err(-1, errno, "Failed to create %s", buf);
	fprintf(fp, "%c /dev/%s 0700 root root - %d:%d\n",
			(mode & S_IFBLK) ? 'b' : 'c',
			name, gnu_dev_major(dev), gnu_dev_minor(dev));
	fclose(fp);

	return 0;
}

int create_static_dev(const char *root, const char *name, const char *alias,
		mode_t mode, dev_t dev)
{
	char buf[PATH_MAX];
	const char *device;
	const char *prefix = "/";
	static char *dirs[] = {
		"/dev",
		"/usr/lib/udev/devices",
		"/lib/udev/devices",
		"/etc/udev/devices",
		};
	unsigned int i;
	int ret = 0;

	if (name == NULL)
		return 0;

	if (root)
		prefix = root;

	device = strrchr(name, '/');
	if (device == NULL)
		device = name;
	else
		device++;

	for (i = 0; i < ARRAY_SIZE(dirs); i++) {
		snprintf(buf, sizeof(buf), "%s%s", prefix, dirs[i]);
		if (stat_file(buf) != 1)
			continue;

		snprintf(buf, sizeof(buf), "%s%s/%s", prefix, dirs[i], device);
		unlink(buf);
		if (mknod(buf, mode, dev)) {
			logger(-1, errno, "Failed to mknod %s", buf);
			ret = 1;
		}
	}

	create_tmpfiles_d_entry(prefix, device, alias, mode, dev);

	return ret;
}

static int dev_create(const char *root, dev_res *dev)
{
	char buf[PATH_MAX];
	struct stat st;

	if (!dev->name)
		return 0;
	if (check_var(root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;

	/* Get device information from CT0 ... */
	snprintf(buf, sizeof(buf), "/dev/%s", dev->name);
	if (stat(buf, &st)) {
		if (errno == ENOENT)
			logger(-1, 0, "Incorrect name or no such device %s",
				buf);
		else
			logger(-1, errno, "Unable to stat device %s", buf);
		return VZ_SET_DEVICES;
	}
	if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) {
		logger(-1, 0, "The %s is not block or character device", buf);
		return VZ_SET_DEVICES;
	}

	/* ... and create it in CT */
	if (create_static_dev(root, dev->name, NULL,
				st.st_mode, st.st_rdev) != 0)
		return VZ_SET_DEVICES;

	return 0;
}

/** Allow/disallow access to devices on host system from CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param root		CT root.
 * @param dev		devices list.
 * @return		0 on success.
 */
int vps_set_devperm(vps_handler *h, envid_t veid, const char *root,
		dev_param *dev)
{
	int ret = 0;
	dev_res *res;
	list_head_t *dev_h = &dev->dev;

	if (list_empty(dev_h))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to apply devperm: "
				"container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Setting devices");
	list_for_each(res, dev_h, list) {
		if (res->name)
			if ((ret = dev_create(root, res)))
				goto out;
		if ((ret = h->setdevperm(h, veid, res)))
			goto out;
	}
out:
	return ret;
}

int add_dev_param(dev_param *dev, dev_res *res)
{
	dev_res *tmp;

	tmp = malloc(sizeof(*tmp));
	if (tmp == NULL)
		return -1;
	if (list_is_init(&dev->dev))
		list_head_init(&dev->dev);
	memcpy(tmp, res, sizeof(*tmp));
	list_add_tail(&tmp->list, &dev->dev);
	res->name = NULL;

	return 0;
}

static void free_dev(list_head_t  *head)
{
	dev_res *cur, *tmp;

	list_for_each_safe(cur, tmp, head, list) {
		list_del(&cur->list);
		free(cur->name);
		free(cur);
	}
	list_head_init(head);
}

void free_dev_param(dev_param *dev)
{
	free_dev(&dev->dev);
}

static int run_pci_script(envid_t veid, int op, list_head_t *pci_h,
		const char *ve_root)
{
	char *argv[3];
	char *envp[10];
	char *script;
	int ret;
	char buf[STR_SIZE];
	int i = 0;

	if (list_empty(pci_h))
		return 0;
	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "VE_ROOT=%s", ve_root);
	envp[i++] = strdup(buf);
	snprintf(buf, sizeof(buf), "ADD=%d", op == ADD);
	envp[i++] = strdup(buf);
	envp[i++] = list2str("PCI", pci_h);
	envp[i++] = strdup(ENV_PATH);
	envp[i] = NULL;
	script = VPS_PCI;
	argv[0] = script;
	argv[1] = NULL;
	ret = run_script(script, argv, envp, 0);
	free_arg(envp);

	return ret;
}

int vps_set_pci(vps_handler *h, envid_t veid, int op, const char *root,
		pci_param *pci)
{
	list_head_t *pci_h = &pci->list;

	if (list_empty(pci_h))
		return 0;

	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to configure PCI devices: "
				"container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Setting PCI devices");

	return run_pci_script(veid, op, pci_h, root);
}
