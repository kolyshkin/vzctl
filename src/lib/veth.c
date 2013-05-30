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
#include <time.h>

#include <linux/vzcalluser.h>
#include <linux/vzctl_veth.h>

#include "vzerror.h"
#include "util.h"
#include "veth.h"
#include "env.h"
#include "logger.h"
#include "script.h"

void free_veth_dev(veth_dev *dev)
{
}

static void free_veth(list_head_t *head)
{
	veth_dev *tmp, *dev_t;

	if (list_empty(head))
		return;
	list_for_each_safe(dev_t, tmp, head, list) {
		free_veth_dev(dev_t);
		list_del(&dev_t->list);
		free(dev_t);
	}
	list_head_init(head);
}

void free_veth_param(veth_param *dev)
{
	free_veth(&dev->dev);
}

static int run_vznetcfg(envid_t veid, veth_dev *dev)
{
	int ret;
	char buf[16];
	char *argv[] = {VZNETCFG, "init", "veth", NULL, NULL};
	char *env[2];

	if (stat_file(VZNETCFG) != 1)
		return 0;
	argv[3] = dev->dev_name;
	snprintf(buf, sizeof(buf), "VEID=%d", veid);
	env[0] = buf;
	env[1] = NULL;
	if ((ret = run_script(VZNETCFG, argv, env, 0))) {
		logger(-1, 0, VZNETCFG " exited with error");
		ret = VZ_VETH_ERROR;
	}
	return ret;
}

/** Create/remove veth devices for CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param dev		devices list.
 * @return		0 on success.
 */
static int veth_ctl(vps_handler *h, envid_t veid, int op, veth_param *list,
	int rollback)
{
	int ret = 0;
	char buf[256];
	char *p, *ep;
	veth_dev *tmp;
	list_head_t *dev_h = &list->dev;

	if (list_empty(dev_h))
		return 0;
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to %s veth: container is not running",
			op == ADD ? "create" : "remove");
		return VZ_VE_NOT_RUNNING;
	}
	buf[0] = 0;
	p = buf;
	ep = buf + sizeof(buf) - 1;
	list_for_each(tmp, dev_h, list) {
		p += snprintf(p, ep - p, "%s ", tmp->dev_name);
		if (p >= ep)
			break;
	}
	logger(0, 0, "%s veth devices: %s",
		(op == ADD || op == CFG) ? "Configure" : "Deleting", buf);
	list_for_each(tmp, dev_h, list) {
		if (op == ADD || op == CFG) {
			if ((ret = h->veth_ctl(h, veid, ADD, tmp)))
				break;
			if ((ret = run_vznetcfg(veid, tmp)))
				break;
		} else if ((ret = h->veth_ctl(h, veid, DEL, tmp))) {
				break;
		}
	}
	/* If operation failed remove added devices.
	 * Remove devices from list to skip saving.
	 */
	if (ret && rollback) {
		list_for_each(tmp, dev_h, list) {
			if (op == ADD && tmp->flags == 1)
				h->veth_ctl(h, veid, DEL, tmp);
		}
		free_veth(dev_h);
	}
	return ret;
}

int parse_hwaddr(const char *str, char *addr)
{
	int i;
	char buf[3];
	char *endptr;

	for (i = 0; i < ETH_ALEN; i++) {
		buf[0] = str[3*i];
		buf[1] = str[3*i+1];
		buf[2] = '\0';
		addr[i] = strtol(buf, &endptr, 16);
		if (*endptr != '\0')
			return ERR_INVAL;
	}
	return 0;
}

void generate_mac(int veid, char *dev_name, char *mac)
{
	int len, i;
	unsigned int hash, tmp;
	char data[128];

	snprintf(data, sizeof(data), "%s:%d:%ld ", dev_name, veid, time(NULL));
	hash = veid;
	len = strlen(data) - 1;
	for (i = 0; i < len; i++) {
		hash += data[i];
		tmp = (data[i + 1] << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		hash += hash >> 11;
	}
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;
	mac[0] = (char) (SW_OUI >> 0xf);
	mac[1] = (char) (SW_OUI >> 0x8);
	mac[2] = (char) SW_OUI;
	mac[3] = (char) hash;
	mac[4] = (char) (hash >> 0x8);
	mac[5] = (char) (hash >> 0xf);
}

int add_veth_param(veth_param *veth, veth_dev *dev)
{
	veth_dev *tmp;

	tmp = malloc(sizeof(*tmp));
	if (tmp == NULL)
		return ERR_NOMEM;
	memcpy(tmp, dev, sizeof(*tmp));
	if (list_is_init(&veth->dev))
		list_head_init(&veth->dev);
	list_add_tail(&tmp->list, &veth->dev);

	return 0;
}

static veth_dev *find_veth_by_ifname(list_head_t *head, char *name)
{
	veth_dev *dev_t;

	if (list_empty(head))
		return NULL;
	list_for_each(dev_t, head, list) {
		if (!strcmp(dev_t->dev_name, name))
			return dev_t;
	}
	return NULL;
}

veth_dev *find_veth_by_ifname_ve(list_head_t *head, char *name)
{
	veth_dev *dev_t;

	if (list_empty(head))
		return NULL;
	list_for_each(dev_t, head, list) {
		if (!strcmp(dev_t->dev_name_ve, name))
			return dev_t;
	}
	return NULL;
}

veth_dev *find_veth_configure(list_head_t *head)
{
	veth_dev *dev_t;

	if (list_empty(head))
		return NULL;
	list_for_each(dev_t, head, list) {
		if (dev_t->configure)
			return dev_t;
	}
	return NULL;
}

static void fill_veth_dev(veth_dev *dst, veth_dev *src)
{
	if (src->dev_name[0] != 0)
		strcpy(dst->dev_name, src->dev_name);
	if (src->dev_bridge[0] != 0)
		strcpy(dst->dev_bridge, src->dev_bridge);
	if (src->addrlen != 0) {
		memcpy(dst->dev_addr, src->dev_addr, sizeof(dst->dev_addr));
		dst->addrlen = src->addrlen;
	}
	if (src->dev_name_ve[0] != 0)
		strcpy(dst->dev_name_ve, src->dev_name_ve);
	if (src->addrlen_ve != 0) {
		memcpy(dst->dev_addr_ve, src->dev_addr_ve, sizeof(dst->dev_addr));
		dst->addrlen_ve = src->addrlen_ve;
	}
	if (src->mac_filter) {
		dst->mac_filter = src->mac_filter;
	}
}

static int merge_veth_dev(veth_dev *old, veth_dev *new, veth_dev *merged)
{
	memset(merged, 0, sizeof(veth_dev));

	if (old != NULL)
		fill_veth_dev(merged, old);
	fill_veth_dev(merged, new);
	return 0;
}

int merge_veth_list(list_head_t *old, list_head_t *add, list_head_t *del,
	veth_param *merged)
{
	veth_dev *dev_t;
	veth_dev dev;
	list_head_t empty;

	list_head_init(&empty);
	if (old == NULL)
		old = &empty;
	if (list_is_init(old))
		list_head_init(old);
	if (add == NULL)
		add = &empty;
	if (list_is_init(add))
		list_head_init(add);
	if (del == NULL)
		del = &empty;
	if (list_is_init(del))
		list_head_init(del);

	list_for_each(dev_t, old, list) {
		veth_dev *tmp;
		/* Skip old devices that was deleted */
		if (find_veth_by_ifname_ve(del, dev_t->dev_name_ve) != NULL)
			continue;
		tmp = find_veth_by_ifname_ve(add, dev_t->dev_name_ve);
		if (tmp != NULL) {
			merge_veth_dev(dev_t, tmp, &dev);
			if (add_veth_param(merged, &dev))
				return 1;
			free_veth_dev(&dev);
			continue;
		}
		/* Add old devices */
		if (add_veth_param(merged, dev_t))
			return 1;
	}
	list_for_each(dev_t, add, list) {
		if (find_veth_by_ifname_ve(old, dev_t->dev_name_ve) == NULL) {
			if (add_veth_param(merged, dev_t))
				return 1;
		}
	}
	return 0;
}

int copy_veth_param(veth_param *dst, veth_param *src)
{
	veth_dev *dev;

	list_for_each(dev, &src->dev, list) {
		if (add_veth_param(dst, dev))
			return 1;
	}
	return 0;
}

static int read_proc_veth(envid_t veid, veth_param *veth)
{
	FILE *fp;
	char buf[256];
	char mac[MAC_SIZE + 1];
	char mac_ve[MAC_SIZE + 1];
	char dev_name[IFNAMSIZE + 1];
	char dev_name_ve[IFNAMSIZE + 1];
	envid_t id;
	veth_dev dev;

	fp = fopen(PROC_VETH, "r");
	if (fp == NULL)
		return -1;
	memset(&dev, 0, sizeof(dev));
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (sscanf(buf, "%17s %15s %17s %15s %d",
			mac, dev_name, mac_ve, dev_name_ve, &id) != 5)
		{
			continue;
		}
		if (veid != id)
			continue;
		parse_hwaddr(mac, dev.dev_addr);
		parse_hwaddr(mac_ve, dev.dev_addr_ve);
		strncpy(dev.dev_name, dev_name, IFNAMSIZE);
		dev.dev_name[IFNAMSIZE - 1] = 0;
		strncpy(dev.dev_name_ve, dev_name_ve, IFNAMSIZE);
		dev.dev_name_ve[IFNAMSIZE - 1] = 0;
		dev.active = 1;
		add_veth_param(veth, &dev);
	}
	fclose(fp);
	return 0;
}

static void fill_veth_dev_name(veth_param *configured, veth_param *new)
{
	veth_dev *it, *dev;

	if (list_empty(&configured->dev))
		return;
	list_for_each(it, &new->dev, list) {
		dev = find_veth_by_ifname_ve(&configured->dev, it->dev_name_ve);
		if (dev != NULL) {
			if (*it->dev_name == '\0')
				strcpy(it->dev_name, dev->dev_name);
			it->active = 1;
		} else {
			logger(-1, 0, "Container does not have "
					"configured veth: %s, skipped",
					it->dev_name_ve);
		}
	}
}

int vps_setup_veth(vps_handler *h, envid_t veid, dist_actions *actions,
	const char *root, veth_param *veth_add, veth_param *veth_del,
	int state, int skip)
{
	int ret;
	veth_param veth_old;

	if (list_empty(&veth_add->dev) &&
		list_empty(&veth_del->dev) &&
		veth_add->delall != YES)
	{
		return 0;
	}
	ret = 0;
	memset(&veth_old, 0, sizeof(veth_old));
	list_head_init(&veth_old.dev);
	if (state != STATE_STARTING)
		read_proc_veth(veid, &veth_old);
	if (veth_add->delall == YES) {
		veth_ctl(h, veid, DEL, &veth_old, 0);
		if (!list_empty(&veth_old.dev))
			free_veth_param(&veth_old);
	} else if (!list_empty(&veth_del->dev)) {
		fill_veth_dev_name(&veth_old, veth_del);
		veth_ctl(h, veid, DEL, veth_del, 0);
	}
	if (!list_empty(&veth_add->dev)) {
		int op = (skip & SKIP_VETH_CREATE) ? CFG : ADD;

		fill_veth_dev_name(&veth_old, veth_add);
		ret = veth_ctl(h, veid, op, veth_add, 1);
	}
	if (!list_empty(&veth_old.dev))
		free_veth_param(&veth_old);
	return ret;
}

int check_veth_param(envid_t veid, veth_param *veth_old, veth_param *veth_new,
	veth_param *veth_del)
{
	int merge;
	veth_dev *dev_t, *dev;

	/* merge data for --veth_del */
	list_for_each(dev, &veth_del->dev, list) {
		if (dev->dev_name[0] == 0)
			continue;
		dev_t = find_veth_by_ifname(&veth_old->dev, dev->dev_name);
		if (dev_t != NULL)
			fill_veth_dev(dev, dev_t);
	}

	dev_t = find_veth_configure(&veth_new->dev);
	if (dev_t == NULL)
		return 0;
	if (dev_t->dev_name_ve[0] == 0) {
		logger(-1, 0, "Invalid usage.  Option --ifname not specified");
		return VZ_INVALID_PARAMETER_SYNTAX;
	}
	/* merge --netif_add & --ifname */
	merge = 0;
	list_for_each(dev, &veth_new->dev, list) {
		if (dev != dev_t &&
		    !strcmp(dev->dev_name_ve, dev_t->dev_name_ve))
		{
			fill_veth_dev(dev_t, dev);
			dev_t->configure = 0;
			list_del(&dev->list);
			free_veth_dev(dev);
			free(dev);
			merge = 1;
			break;
		}
	}
	/* Is corresponding device configured for --ifname <iface> */
	if (!merge &&
	    (veth_old == NULL ||
	    find_veth_by_ifname_ve(&veth_old->dev, dev_t->dev_name_ve) == NULL))
	{
		logger(-1, 0, "Invalid usage: veth device %s is"
			" not configured, use --netif_add option first",
			dev_t->dev_name_ve);
		return VZ_INVALID_PARAMETER_SYNTAX;
	}
	return 0;
}
