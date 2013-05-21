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
#include "logger.h"

#define BACKUP		0
#define DESTR		1

#define PROCMEM		"/proc/meminfo"
#define PROCCPU		"/proc/cpuinfo"
#define PROCTHR		"/proc/sys/kernel/threads-max"
#define PROCVEINFO	"/proc/vz/veinfo"

char *parse_line(char *str, char *ltoken, int lsz, char **errstr);
int stat_file(const char *file);
int dir_empty(const char *dir);
int make_dir(const char *path, int full);
int make_dir_mode(const char *path, int full, int mode);
char *get_fs_root(const char *dir);
int parse_int(const char *str, int *val);
int parse_ul(const char *str, unsigned long *val);
int check_var(const void *val, const char *message);
int cp_file(char *dst, char *src);
void get_vps_conf_path(envid_t veid, char *buf, int len);
char *arg2str(char **arg);
void free_arg(char **arg);
unsigned long min_ul(unsigned long val1, unsigned long val2);
int yesno2id(const char *str);
int get_addr_family(const char *addr);
int get_netaddr(const char *ip_str, void *ip);
char *canon_ip(const char *str);
char *subst_VEID(envid_t veid, char *src);
int get_pagesize(void);
int get_mem(unsigned long long *mem);
int get_thrmax(int *thrmax);
int get_swap(unsigned long long *swap);
int get_num_cpu(void);
int get_lowmem(unsigned long long *mem);
unsigned long max_ul(unsigned long val1, unsigned long val2);
int get_dump_file(unsigned veid, const char *dumpdir, char *buf, int size);
int get_state_file(unsigned veid, char *buf, int size);
int set_not_blk(int fd);
void close_fds(int close_std, ...);
int move_config(int veid, int action);
void remove_names(envid_t veid);

size_t vz_strlcat(char *dst, const char *src, size_t count);

int get_running_ve_list(envid_t **ves);
int ve_in_list(envid_t *list, int size, envid_t ve);

const char* ubcstr(unsigned long bar, unsigned long lim);
int is_vswap_mode(void);

int vzctl_get_normalized_guid(const char *str, char *buf, int len);

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

/* Usage: printf("MAC=" MAC2STR_FMT, MAC2STR(dev)); */
#define MAC2STR_FMT	"%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC2STR(dev)	\
	((unsigned char *)dev)[0],      \
	((unsigned char *)dev)[1],      \
	((unsigned char *)dev)[2],      \
	((unsigned char *)dev)[3],      \
	((unsigned char *)dev)[4],      \
	((unsigned char *)dev)[5]

#endif
