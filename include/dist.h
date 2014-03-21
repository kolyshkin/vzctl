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
#ifndef	_DIST_H_
#define	_DIST_H_

#define DIST_CONF_DEF		"default"
#define	DIST_FUNC		"functions"
#define	DIST_SCRIPTS		"scripts"

enum {
	ADD_IP = 1,
	DEL_IP,
	SET_HOSTNAME,
	SET_DNS,
	SET_USERPASS,
	SET_UGID_QUOTA,
	POST_CREATE,
	PRE_START,
	SET_CONSOLE,
};

typedef struct {
	char *def_ostmpl;
	char *ostmpl;
	char *dist;
} tmpl_param;

/* Data structure for distribution specific action scripts.
 */
typedef struct dist_actions {
	char *add_ip;		/**< setup ip address. */
	char *del_ip;		/**< delete ip address. */
	char *set_hostname;	/**< setup hostname. */
	char *set_dns;		/**< setup dns rescords. */
	char *set_userpass;	/**< setup user password. */
	char *set_ugid_quota;	/**< setup 2level quota. */
	char *post_create;	/**< sostcreate actions. */
	char *pre_start;	/**< pre start actions. */
	char *set_console;	/**< for vzctl console. */
} dist_actions;

/* Read distribution specific actions configuration file.
 *
 * @param dist_name	distribution name.
 * @param dir		directory distribution config file will be searched.
 * @param dist		filled dist_actions
 * @return		0 on success
 */
int read_dist_actions(char *dist_name, char *dir, dist_actions *dist);

/** Get distribution name form tmpl_param structure.
 *
 * @param tmpl		distribution data.
 * @return		malloc'ed name.
 */
char *get_dist_name(tmpl_param *tmpl);
void free_dist_actions(dist_actions *dist);

#endif
