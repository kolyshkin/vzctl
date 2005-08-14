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
#ifndef	_DIST_H_
#define	_DIST_H_

#define DIST_CONF_DEF		"default"
#define	DIST_FUNC		"functions"
#define	DIST_SCRIPTS		"scripts"

#define	ADD_IP			1
#define	DEL_IP			2
#define	SET_HOSTNAME		3
#define	SET_DNS			4
#define	SET_USERPASS		5
#define	SET_UGID_QUOTA		6
#define	POST_CREATE		7

#define DIST_DIR		"/etc/sysconfig/vz-scripts/dists"

typedef struct {
	char *def_ostmpl;
	char *ostmpl;
	char *pkgset;
	char *pkgver;
	char *dist;
} tmpl_param;

/* Data structure for distribution specific action scripts.
 */
typedef struct {
	char *add_ip;		/**< setup ip address. */
	char *del_ip;		/**< delete ip address. */
	char *set_hostname;	/**< setup hostname. */
	char *set_dns;		/**< setup dns rescords. */
	char *set_userpass;	/**< setup user password. */
	char *set_ugid_quota;	/**< setup 2level quota. */
	char *post_create;	/**< sostcreate actions. */
} dist_actions;

/* Read distribution specific actions configuration file.
 *
 * @param dist_name	distribution name.
 * @param dir		directory distribution config file will be searched.
 * @param dist		filled dist_actions
 * @return 		0 on success
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
