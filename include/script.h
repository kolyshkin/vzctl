/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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
#ifndef _SCRIPT_H_
#define _SCRIPT_H_

#include <sys/types.h>

/* Second-level quota */
struct setup_env_quota_param {
	char dev_name[256];
	dev_t dev;
};
int setup_env_quota(const struct setup_env_quota_param *p);

/* Misc scripts */
int read_script(const char *fname, char *include, char **buf);
int run_script(const char *f, char *argv[], char *envp[], int quiet);
int run_pre_script(int veid, char *script);
int add_reach_runlevel_mark();
int wait_on_fifo(void *data);

#endif /* _SCRIPT_H_ */
