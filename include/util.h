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
#ifndef _UTIL_H_
#define	_UTIL_H_

#include <stdlib.h>
#include "types.h"
#include "res.h"
#include "logger.h"

#define BACKUP		0
#define DESTR		1

#define PROCMEM		"/proc/meminfo"
#define PROCTHR		"/proc/sys/kernel/threads-max"

char *parse_line(char *str, char *ltoken, int lsz);
int stat_file(const char *file);
int make_dir(char *path, int full);
int parse_int(const char *str, int *val);
int parse_ul(const char *str, unsigned long *val);
int check_var(const void *val, const char *message);
int cp_file(char *dst, char *src);
void get_vps_conf_path(envid_t veid, char *buf, int len);
char *arg2str(char **arg);
void free_arg(char **arg);
unsigned long min_ul(unsigned long val1, unsigned long val2);
int yesno2id(const char *str);
int get_netaddr(const char *ip_str, void *ip);
const char *get_netname(unsigned int *ip, int family);
char *subst_VEID(envid_t veid, char *src);
int get_pagesize();
int get_mem(unsigned long long *mem);
int get_thrmax(int *thrmax);
int get_swap(unsigned long long *swap);
int get_num_cpu();
int get_lowmem(unsigned long long *mem);
unsigned long max_ul(unsigned long val1, unsigned long val2);
void str_tolower(const char *from, char *to);
char *get_file_name(char *str);
const char *get_vps_state_str(int vps_state);
int get_dump_file(unsigned veid, const char *dumpdir, char *buf, int size);
int set_not_blk(int fd);
void close_fds(int close_std, ...);
int move_config(int veid, int action);
void remove_names(envid_t veid);

size_t vz_strlcat(char *dst, const char *src, size_t count);

void get_osrelease(vps_res *res);

#define logger_enomem(log_level, err, size, file, line)			\
	logger(log_level, err, "%s:%i: Can't allocate %lu bytes",	\
		file, line, (unsigned long)size)

#define vz_malloc(size)						\
	({							\
		void *p = malloc(size);				\
		if (!p)						\
			logger_enomem(-1, ENOMEM, size,		\
					__FILE__, __LINE__);	\
		p;						\
	})

#define vz_strdup(str)						\
	({							\
		char *p = strdup(str);				\
		if (!p)						\
			logger_enomem(-1, ENOMEM, strlen(str),	\
					__FILE__, __LINE__);	\
		p;						\
	})

#define for_each_strtok(p, str, sep)				\
	for (p = strtok(str, sep); p; p = strtok(NULL, sep))

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#endif
