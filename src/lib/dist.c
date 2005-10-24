/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "util.h"
#include "dist.h"
#include "logger.h"
#include "vzerror.h"

static struct distr_conf {
        char *name;
        int id;
} action2id[] = {
	{"ADD_IP", ADD_IP},
	{"DEL_IP", DEL_IP},
	{"SET_HOSTNAME", SET_HOSTNAME},
	{"SET_DNS", SET_DNS},
	{"SET_USERPASS", SET_USERPASS},
	{"SET_UGID_QUOTA", SET_UGID_QUOTA},
	{"POST_CREATE", POST_CREATE}
};

static int get_action_id(char *name)
{
	int i;

	for (i = 0; i < sizeof(action2id) / sizeof(*action2id); i++)
		if (!strcmp(name, action2id[i].name))
			return action2id[i].id;
	return -1;
}

static int add_dist_action(dist_actions *d_actions, char *name, char *action,
	char *dir)
{
	char file[256];
	int id;

	if (!action[0])
		return 0;
	if ((id = get_action_id(name)) < 0)
		return 0;
	snprintf(file, sizeof(file), "%s/%s/%s", dir, DIST_SCRIPTS, action);
	if (!stat_file(file)) {
		logger(0, 0, "Action script %s does not found", file);
		return VZ_NO_DISTR_ACTION_SCRIPT;
	}
	switch (id) {
		case ADD_IP:
			if (d_actions->add_ip != NULL)
				break;
			d_actions->add_ip = strdup(file);
			break;
		case DEL_IP:
			if (d_actions->del_ip != NULL)
				break;
			d_actions->del_ip = strdup(file);
			break;
		case SET_HOSTNAME:
			if (d_actions->set_hostname != NULL)
				break;
			d_actions->set_hostname = strdup(file);
			break;
		case SET_DNS:
			if (d_actions->set_dns != NULL)
				break;
			d_actions->set_dns = strdup(file);
			break;
		case SET_USERPASS:
			if (d_actions->set_userpass != NULL)
				break;
			d_actions->set_userpass = strdup(file);
			break;
		case SET_UGID_QUOTA:
			if (d_actions->set_ugid_quota != NULL)
				break;
			d_actions->set_ugid_quota = strdup(file);
			break;
		case POST_CREATE:
			if (d_actions->post_create != NULL)
				break;
			d_actions->post_create = strdup(file);
			break;
	}
	return 0;
}

void free_dist_actions(dist_actions *d_actions)
{
	if (d_actions == NULL)
		return;
	if (d_actions->add_ip != NULL)
		free(d_actions->add_ip);
	if (d_actions->del_ip != NULL)
		free(d_actions->del_ip);
	if (d_actions->set_hostname != NULL)
		free(d_actions->set_hostname);
	if (d_actions->set_dns != NULL)
		free(d_actions->set_dns);
	if (d_actions->set_userpass != NULL)
		free(d_actions->set_userpass);
	if (d_actions->set_ugid_quota != NULL)
		free(d_actions->set_ugid_quota);
	if (d_actions->post_create != NULL)
		free(d_actions->post_create);
}

static int get_dist_conf_name(char *dist_name, char *dir, char *file, int len)
{
	char buf[256];
	char *ep;

	if (dist_name != NULL) {
		snprintf(buf, sizeof(buf), "%s", dist_name);
		ep = buf + strlen(buf);
		do {
			snprintf(file, len, "%s/%s.conf", dir, buf);
			if (stat_file(file))
				return 0;
			while (ep > buf && *ep !=  '-') --ep;
			*ep = 0;
		} while (ep > buf);
		snprintf(file, len, "%s/%s", dir, DIST_CONF_DEF);
		logger(0, 0, "Warning: configuration file"
			" for distribution %s not found default used",
			dist_name);
	} else {
		snprintf(file, len, "%s/%s", dir, DIST_CONF_DEF);
		logger(0, 0, "Warning: distribution not specified"
			" default used %s", file);
	}
	if (!stat_file(file)) {
		logger(0, 0, "Distribution configuration not found %s", file);
		return VZ_NO_DISTR_CONF;
	}
	return 0;
}

/* Read distribution specific actions configuration file.
 *
 * @param dist_name	distribution name.
 * @param dir		directory distribution config file will be searched.
 * @param dist		filled dist_actions
 * @return 		0 on success
 */
int read_dist_actions(char *dist_name, char *dir, dist_actions *actions)
{
	char buf[256];
	char ltoken[256];
	char file[256];
	char *rtoken;
	FILE *fp;
	int ret = 0;

	memset(actions, 0, sizeof(*actions));
	if ((ret = get_dist_conf_name(dist_name, dir, file, sizeof(file))))
		return ret;
	if ((fp = fopen(file, "r")) == NULL) {
		logger(0, errno, "unable to open %s", file);
		return VZ_NO_DISTR_CONF;
	}
	while (!feof(fp)) { 
		buf[0] = 0;
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		if ((rtoken = parse_line(buf, ltoken, sizeof(ltoken))) == NULL)
			continue;
		if ((ret = add_dist_action(actions, ltoken, rtoken, dir))) {
			free_dist_actions(actions);
			break;
		}
	}
	fclose(fp);
	return ret;
}

/** Get distribution name form tmpl_param structure.
 *
 * @param tmpl		distribution data.
 * @return		malloc'ed name.
 */
char *get_dist_name(tmpl_param *tmpl)
{
	const char *p = NULL;

	if (tmpl->dist != NULL)
		p = tmpl->dist;
	else if (tmpl->ostmpl != NULL)
		p = tmpl->ostmpl;
	if (p != NULL)
		return strdup(p);
	return NULL;
}
