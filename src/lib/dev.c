/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

#include <linux/vzcalluser.h>

#include "vzerror.h"
#include "script.h"
#include "util.h"
#include "dev.h"
#include "env.h"
#include "logger.h"
#include "script.h"

static int dev_create(const char *root, dev_res *dev)
{
	char buf1[STR_SIZE];
	char buf2[STR_SIZE];
	struct stat st, st2;
	const char* udev_paths[] = {
		"/lib/udev/devices",
		"/etc/udev/devices",
		NULL};
	int i;

	if (!dev->name[0])
		return 0;
	if (check_var(root, "VE_ROOT is not set"))
		return VZ_VE_ROOT_NOTSET;

	/* Get device information from CT0 and create it in CT */
	snprintf(buf1, sizeof(buf1), "%s/dev/%s", root, dev->name);
	snprintf(buf2, sizeof(buf2), "/dev/%s", dev->name);
	if (stat(buf2, &st)) {
		if (errno == ENOENT)
			logger(-1, 0, "Incorrect name or no such device %s",
				buf2);
		else
			logger(-1, errno, "Unable to stat device %s", buf2);
		return VZ_SET_DEVICES;
	}
	if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) {
		logger(-1, 0, "The %s is not block or character device", buf2);
		return VZ_SET_DEVICES;
	}
	if (make_dir(buf1, 0))
		return VZ_SET_DEVICES;
	unlink(buf1);
	if (mknod(buf1, st.st_mode, st.st_rdev)) {
		logger(-1, errno, "Unable to create device %s", buf1);
		return VZ_SET_DEVICES;
	}
	/* Try to create static device node for udev */
	for (i = 0; udev_paths[i] != NULL; i++) {
		if (stat(udev_paths[i], &st2) == 0) {
			if (S_ISDIR(st2.st_mode)) {
				snprintf(buf1, sizeof(buf1),
						"%s/%s/%s",
						root, udev_paths[i],
						dev->name);
				make_dir(buf1, 0);
				unlink(buf1);
				mknod(buf1, st.st_mode, st.st_rdev);
				break;
			}
		}
	}
	return 0;
}

int set_devperm(vps_handler *h, envid_t veid, dev_res *dev)
{
	int ret;
	struct vzctl_setdevperms devperms;

	devperms.veid = veid;
	devperms.dev = dev->dev;
	devperms.mask = dev->mask;
	devperms.type = dev->type;

	if ((ret = ioctl(h->vzfd, VZCTL_SETDEVPERMS, &devperms)))
		logger(-1, errno, "Unable to set devperms");

	return ret;
}

struct vzctl_ve_configure_pci {
	struct vzctl_ve_configure conf;
	struct vzctl_ve_pci_dev dev;
};

static int set_pci(vps_handler *h, envid_t veid, int op, char *dev_id)
{
	int ret;
	struct vzctl_ve_configure_pci conf_pci;
	struct vzctl_ve_configure *conf = &conf_pci.conf;
	struct vzctl_ve_pci_dev *dev = &conf_pci.dev;

	sscanf(dev_id, "%x:%x:%x.%d", &dev->domain, &dev->bus,
					&dev->slot, &dev->func);
	conf->veid = veid;
	if (op == ADD)
		conf->key = VE_CONFIGURE_ADD_PCI_DEVICE;
	else
		conf->key = VE_CONFIGURE_DEL_PCI_DEVICE;
	conf->val = 0;
	conf->size = sizeof(struct vzctl_ve_pci_dev);

	if ((ret = ioctl(h->vzfd, VZCTL_VE_CONFIGURE, &conf_pci))) {
		if (errno == EEXIST)
			return 0;
		logger(-1, errno, "Unable to move pci device %s", dev_id);
	}

	return ret;
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
		if (res->name[0])
			if ((ret = dev_create(root, res)))
				break;
		if ((ret = set_devperm(h, veid, res)))
			break;
	}
	return ret;
}

int add_dev_param(dev_param *dev, dev_res *res)
{
	dev_res *tmp;

	if (list_is_init(&dev->dev))
		list_head_init(&dev->dev);
	tmp = malloc(sizeof(*tmp));
	if (tmp == NULL)
		return -1;
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

int run_pci_script(envid_t veid, int op, list_head_t *pci_h)
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
	int ret = 0;
	list_head_t *pci_h = &pci->list;
	str_param *res;

	if (list_empty(pci_h))
		return 0;

	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to configure PCI devices: "
				"container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Setting PCI devices");

	list_for_each(res, pci_h, list)
		if ((ret = set_pci(h, veid, op, res->val)))
			break;
	if (!ret)
		ret = run_pci_script(veid, op, pci_h);
	return ret;
}
