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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <limits.h>
#include <dirent.h>

#include "util.h"
#include "logger.h"
#include "fs.h"

#ifndef NR_OPEN
#define NR_OPEN 1024
#endif

static const char *unescapestr(char *const src)
{
	char *p1, *p2;
	int fl;

	if (src == NULL)
		return NULL;
	p1 = p2 = src;
	fl = 0;
	while (*p2) {
		if (*p2 == '\\' && !fl)	{
			fl = 1;
			p2++;
		} else {
			*p1 = *p2;
			p1++; p2++;
			fl = 0;
		}
	}
	*p1 = 0;

	return src;
}

char *parse_line(char *str, char *ltoken, int lsz, char **err)
{
	char *sp = str;
	char *ep, *p, *ret;
	int len;

	*err = NULL;
	unescapestr(str);
	while (*sp && isspace(*sp)) sp++;
	if (!*sp || *sp == '#')
		return NULL;
	ep = sp + strlen(sp) - 1;
	while (isspace(*ep) && ep >= sp) *ep-- = '\0';
	if (!(p = strchr(sp, '='))) {
		*err = "'=' not found";
		return NULL;
	}
	len = p - sp;
	if (len >= lsz) {
		*err = "too long value";
		return NULL;
	}
	strncpy(ltoken, sp, len);
	ltoken[len] = 0;
	if (*(++p) != '"')
		return p;
	/* Quoted argument requires some additional processing */
	ret = ++p; /* skip opening quote */
	/* Find the matching closing quote */
	if (!(p = strrchr(p, '"'))) {
		*err="unmatched quotes";
		return NULL;
	}
	*p = '\0';

	return ret;
}

/*
	1 - exist
	0 - does't exist
	-1 - error
*/
int stat_file(const char *file)
{
	struct stat st;

	if (stat(file, &st)) {
		if (errno != ENOENT)
			return -1;
		return 0;
	}
	return 1;
}

/** Check if a directory is empty
 * Returns:
 *  1 - empty or nonexistent
 *  0 - not empty
 * -1 - error
 */
int dir_empty(const char *dir)
{
	DIR *dp;
	struct dirent *ep;
	int ret = 1;

	dp = opendir(dir);
	if (dp == NULL) {
		if (errno == ENOENT)
			return 1;

		logger(-1, errno, "Can't opendir %s", dir);
		return -1;
	}

	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, "."))
			continue;
		if (!strcmp(ep->d_name, ".."))
			continue;
		/* Not empty */
		ret = 0;
		break;
	}
	closedir(dp);

	return ret;
}

int make_dir_mode(const char *path, int full, int mode)
{
	char buf[4096];
	const char *ps, *p;
	int len;

	if (path == NULL)
		return 0;

	ps = path + 1;
	while ((p = strchr(ps, '/'))) {
		len = p - path + 1;
		snprintf(buf, len, "%s", path);
		ps = p + 1;
		if (!stat_file(buf)) {
			if (mkdir(buf, mode)) {
				logger(-1, errno, "Can't create directory %s",
					buf);
				return 1;
			}
		}
	}
	if (!full)
		return 0;
	if (!stat_file(path)) {
		if (mkdir(path, mode)) {
			logger(-1, errno, "Can't create directory %s", path);
			return 1;
		}
	}
	return 0;
}

int make_dir(const char *path, int full)
{
	return make_dir_mode(path, full, 0755);
}

char *get_fs_root(const char *dir)
{
	struct stat st;
	dev_t id;
	size_t len;
	const char *p, *prev;
	char tmp[PATH_MAX];

	if (stat(dir, &st) < 0)
		return NULL;
	id = st.st_dev;
	len = strlen(dir);
	if (len > sizeof(tmp) - 1) {
		errno = ERANGE;
		return NULL;
	}
	p = dir + len;
	prev = p;
	while (p > dir) {
		while (p > dir && *p == '/') p--;
		while (p > dir && *p != '/') p--;
		if (p <= dir)
			break;
		len = p - dir + 1;
		strncpy(tmp, dir, len);
		tmp[len] = 0;
		if (stat(tmp, &st) < 0)
			return NULL;
		if (id != st.st_dev)
			break;
		prev = p;
	}
	len = prev - dir;
	if (len) {
		strncpy(tmp, dir, len);
		tmp[len] = 0;
		return strdup(tmp);
	}
	return NULL;
}

int parse_int(const char *str, int *val)
{
	char *tail;
	long res;

	res = strtol(str, &tail, 10);
	if (*tail != '\0' || res < INT_MIN || res > INT_MAX)
		return 1;
	*val = (int)res;
	return 0;
}

int parse_ul(const char *str, unsigned long *val)
{
	char *tail;
	unsigned long res;

	if (!strcmp(str, "unlimited")) {
		*val = LONG_MAX;
		return 0;
	}

	res = strtoul(str, &tail, 10);
	if (*tail != '\0' || res > LONG_MAX)
		return ERR_INVAL;
	*val = res;
	return 0;
}

int check_var(const void *val, const char *message)
{
	if (val != NULL)
		return 0;
	logger(-1, 0, "%s", message);

	return 1;
}

int cp_file(char *dst, char *src)
{
	int fd_src, fd_dst, ret = 0;
	struct stat st;
	char buf[4096];

	if (stat(src, &st) < 0) {
		logger(-1, errno, "Unable to stat %s", src);
		return -1;
	}
	if ((fd_src = open(src, O_RDONLY)) < 0) {
		logger(-1, errno, "Unable to open %s", src);
		return -1;
	}
	if ((fd_dst = open(dst, O_CREAT|O_TRUNC|O_RDWR, st.st_mode)) < 0) {
		logger(-1, errno, "Unable to open %s", dst);
		close(fd_src);
		return -1;
	}
	while(1) {
		ret = read(fd_src, buf, sizeof(buf));
		if (!ret)
			break;
		else if (ret < 0) {
			logger(-1, errno, "Unable to read from %s", src);
			ret = -1;
			break;
		}
		if (write(fd_dst, buf, ret) < 0) {
			logger(-1, errno, "Unable to write to %s", dst);
			ret = -1;
			break;
		}
	}
	close(fd_src);
	close(fd_dst);

	return ret;
}

void get_vps_conf_path(envid_t veid, char *buf, int len)
{
	snprintf(buf, len, VPS_CONF_DIR "%d.conf", veid);
}

char *arg2str(char **arg)
{
	char **p;
	char *str, *sp;
	int len = 0;

	if (arg == NULL)
		return NULL;
	p = arg;
	while (*p)
		len += strlen(*p++) + 1;
	if ((str = (char *)malloc(len + 1)) == NULL)
		return NULL;
	p = arg;
	sp = str;
	while (*p)
		sp += sprintf(sp, "%s ", *p++);

	return str;
}

void free_arg(char **arg)
{
	while (*arg) free(*arg++);
}

inline unsigned long max_ul(unsigned long val1, unsigned long val2)
{
	return (val1 > val2) ? val1 : val2;
}

inline unsigned long min_ul(unsigned long val1, unsigned long val2)
{
	return (val1 < val2) ? val1 : val2;
}

int yesno2id(const char *str)
{
	if (!strcmp(str, "yes"))
		return YES;
	else if (!strcmp(str, "no"))
		 return NO;
	return -1;
}

int get_addr_family(const char *addr)
{
	if (strchr(addr, ':'))
		return AF_INET6;
	else
		return AF_INET;
}

int get_netaddr(const char *ip_str, void *ip)
{
	int family;

	family = get_addr_family(ip_str);

	if (inet_pton(family, ip_str, ip) <= 0)
		return -1;
	return family;
}

static inline int max_netmask(int family)
{
	if (family == AF_INET)
		return 32;
	else if (family == AF_INET6)
		return 128;
	else
		return -1;
}

/* Check and "canonicalize" an IP address with optional netmask
 * (in CIDR notation). Basically */
char *canon_ip(const char *str)
{
	const char *ipstr, *maskstr;
	int mask, family;
	unsigned int ip[4];
	static char dst[INET6_ADDRSTRLEN + sizeof("/128")];

	maskstr = strchr(str, '/');
	if (maskstr) {
		ipstr = strndupa(str, maskstr - str);
		maskstr++;
	}
	else {
		ipstr = str;
	}
	family = get_netaddr(ipstr, ip);
	if (family < 0)
		return NULL;
	if ((inet_ntop(family, ip, dst, sizeof(dst))) == NULL)
		return NULL;

	if (maskstr == NULL)
		goto out;

	/* Parse netmask */
	if (parse_int(maskstr, &mask) != 0)
		return NULL;
	if (mask > max_netmask(family) || mask < 0)
		return NULL;
	sprintf(dst + strlen(dst), "/%d", mask);

out:
	return dst;
}

char *subst_VEID(envid_t veid, char *src)
{
	char *srcp;
	char str[STR_SIZE];
	char *sp, *se;
	int r;
	unsigned int len, veidlen;

	if (src == NULL)
		return NULL;
	/* Skip end '/' */
	se = src + strlen(src) - 1;
	while (se != str && *se == '/') {
		*se = 0;
		se--;
	}
	if ((srcp = strstr(src, "$VEID")))
		veidlen = sizeof("$VEID") - 1;
	else if ((srcp = strstr(src, "${VEID}")))
		veidlen = sizeof("${VEID}") - 1;
	else
		return strdup(src);

	sp = str;
	se = str + sizeof(str);
	len = srcp - src; /* Length of src before $VEID */
	if (len >= sizeof(str))
		return NULL;
	memcpy(str, src, len);
	sp += len;
	r = snprintf(sp, se - sp, "%d", veid);
	sp += r;
	if ((r < 0) || (sp >= se))
		return NULL;
	if (*srcp) {
		r = snprintf(sp, se - sp, "%s", srcp + veidlen);
		sp += r;
		if ((r < 0) || (sp >= se))
			return NULL;
	}
	return strdup(str);
}

int get_pagesize(void)
{
	long pagesize;

	if ((pagesize = sysconf(_SC_PAGESIZE)) == -1) {
		logger(-1, errno, "Unable to get page size");
		return -1;
	}
	return pagesize;
}

int get_mem(unsigned long long *mem)
{
	long pages, pagesize;
	if ((pages = sysconf(_SC_PHYS_PAGES)) == -1) {
		logger(-1, errno, "Unable to get total phys pages");
		return -1;
	}
	if ((pagesize = get_pagesize()) < 0)
		return -1;
	*mem = (unsigned long long) pages * pagesize;
	return 0;
}

int get_thrmax(int *thrmax)
{
	FILE *fd;
	char str[128];

	if (thrmax == NULL)
		return 1;
	if ((fd = fopen(PROCTHR, "r")) == NULL) {
		logger(-1, errno, "Unable to open " PROCTHR);
		return 1;
	}
	if (fgets(str, sizeof(str), fd) == NULL) {
		fclose(fd);
		return 1;
	}
	fclose(fd);
	if (sscanf(str, "%du", thrmax) != 1)
		return 1;
	return 0;
}

int get_swap(unsigned long long *swap)
{
	FILE *fd;
	char str[128];

	if ((fd = fopen(PROCMEM, "r")) == NULL)	{
		logger(-1, errno, "Cannot open " PROCMEM);
		return -1;
	}
	while (fgets(str, sizeof(str), fd)) {
		if (sscanf(str, "SwapTotal: %llu", swap) == 1) {
			*swap *= 1024;
			fclose(fd);
			return 0;
		}
	}
	logger(-1, errno, "Swap: is not found in " PROCMEM );
	fclose(fd);

	return -1;
}

int get_num_cpu(void)
{
	FILE *fd;
	char str[128];
	int ncpu = 0;

	if ((fd = fopen(PROCCPU, "r")) == NULL)	{
		logger(-1, errno, "Cannot open " PROCCPU);
		return 1;
	}
	while (fgets(str, sizeof(str), fd)) {
		char const proc_ptrn[] = "processor";
		if (!strncmp(str, proc_ptrn, sizeof(proc_ptrn) - 1))
			ncpu++;
	}
	fclose(fd);
	return !ncpu ? 1 : ncpu;
}

int get_lowmem(unsigned long long *mem)
{
	FILE *fd;
	char str[128];

	if ((fd = fopen(PROCMEM, "r")) == NULL)	{
		logger(-1, errno, "Cannot open " PROCMEM);
		return -1;
	}
	*mem = 0;
	while (fgets(str, sizeof(str), fd)) {
		if (sscanf(str, "LowTotal: %llu", mem) == 1)
			break;
		/* Use MemTotal in case LowTotal not found */
		sscanf(str, "MemTotal: %llu", mem);
	}
	fclose(fd);
	if (*mem == 0) {
		fprintf(stderr, "Neither LowTotal nor MemTotal found in the "
				PROCMEM "\n");
		return -1;
	}
	*mem *= 1024;
	return 0;
}

int get_dump_file(unsigned veid, const char *dumpdir, char *buf, int size)
{
	return snprintf(buf, size, "%s/" DEF_DUMPFILE,
			dumpdir != NULL ? dumpdir : DEF_DUMPDIR, veid);
}

int get_state_file(unsigned veid, char *buf, int size)
{
	return snprintf(buf, size, DEF_STATEDIR "/%d.pid", veid);
}

int set_not_blk(int fd)
{
	int oldfl, ret;

	if ((oldfl = fcntl(fd, F_GETFL)) == -1)
		return -1;
	oldfl |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, oldfl);

	return ret;
}

/** Close all fd.
 * @param close_std	flag for closing the [0-2] fds
 * @param ...		list of fds to skip (-1 is the end mark)
*/
void close_fds(int close_std, ...)
{
	int fd, max;
	unsigned int i;
	va_list ap;
	int skip_fds[255];

	max = sysconf(_SC_OPEN_MAX);
	if (max < NR_OPEN)
		max = NR_OPEN;
	if (close_std) {
		fd = open("/dev/null", O_RDWR);
		if (fd != -1) {
			dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
			close(fd);
		} else {
			close(0); close(1); close(2);
		}
	}
	/* build array of skipped fds */
	va_start(ap, close_std);
	skip_fds[0] = -1;
	for (i = 0; i < ARRAY_SIZE(skip_fds); i++) {
		fd = va_arg(ap, int);
		skip_fds[i] = fd;
		if (fd == -1)
			break;
	}
	va_end(ap);
	for (fd = 3; fd < max; fd++) {
		for (i = 0; skip_fds[i] != fd && skip_fds[i] != -1; i++);
		if (skip_fds[i] == fd)
			continue;
		close(fd);
	}
}

static void __move_config(int veid, int action, const char *prefix)
{
	char conf[PATH_LEN];
	char newconf[PATH_LEN];

	snprintf(conf, sizeof(conf), VPS_CONF_DIR "%d.%s", veid, prefix);
	snprintf(newconf, sizeof(newconf), "%s." DESTR_PREFIX, conf);
	action == BACKUP ? rename(conf, newconf) : unlink(newconf);
}

/* Renames or removes CT config and various CT scripts.
 */
int move_config(int veid, int action)
{
	__move_config(veid, action, "conf");
	__move_config(veid, action, MOUNT_PREFIX);
	__move_config(veid, action, UMOUNT_PREFIX);
	__move_config(veid, action, PRE_MOUNT_PREFIX);
	__move_config(veid, action, POST_UMOUNT_PREFIX);
	__move_config(veid, action, START_PREFIX);
	__move_config(veid, action, STOP_PREFIX);

	return 0;
}

void remove_names(envid_t veid)
{
	char buf[STR_SIZE];
	char content[STR_SIZE];
	struct stat st;
	struct dirent *ep;
	DIR *dp;
	int r;
	envid_t id;

	if (!(dp = opendir(VENAME_DIR)))
		return;
	while ((ep = readdir(dp))) {
		snprintf(buf, sizeof(buf), VENAME_DIR "/%s", ep->d_name);
		if (lstat(buf, &st))
			continue;
		if (!S_ISLNK(st.st_mode))
			continue;
		r = readlink(buf, content, sizeof(content) - 1);
		if (r < 0)
			continue;
		content[r] = 0;
		if (sscanf(basename(content), "%d.conf", &id) == 1 && veid == id)
			unlink(buf);
	}
	closedir(dp);
}

size_t vz_strlcat(char *dst, const char *src, size_t count)
{
	size_t dsize = strlen(dst);
	size_t len = strlen(src);
	size_t res = dsize + len;

	if (dsize >= count)
		return dsize;

	dst += dsize;
	count -= dsize;
	if (len >= count)
		len = count - 1;

	memcpy(dst, src, len);
	dst[len] = 0;

	return res;
}

static int envid_sort_fn(const void *val1, const void *val2)
{
	const envid_t *r1 = (const envid_t *)val1;
	const envid_t *r2 = (const envid_t *)val2;

	return (*r1 - *r2);
}

/** Returns a sorted array of all running CTs.
 * Caller needs to free() it after use
 */
int get_running_ve_list(envid_t **ves)
{
	FILE *fp;
	int res;
	envid_t veid;
	int venum = 0;
	int ves_size = 256;


	*ves = malloc(ves_size * sizeof(envid_t));
	if (*ves == NULL)
		return -ENOMEM;

	if ((fp = fopen(PROCVEINFO, "r")) == NULL) {
		return -errno;
	}
	while (!feof(fp)) {
		res = fscanf(fp, "%d %*[^\n]", &veid);
		if (res != 1 || !veid)
			continue;
		if (venum >= ves_size)
			ves_size *= 2;
		*ves = realloc(*ves, ves_size * sizeof(envid_t));
		if (*ves == NULL)
		{
			venum=-ENOMEM;
			goto out;
		}
		(*ves)[venum++] = veid;
	}
	qsort(*ves, venum, sizeof(envid_t), envid_sort_fn);
out:
	fclose(fp);
	return venum;
}

/* Searches for ve in velist */
int ve_in_list(envid_t *list, int size, envid_t ve)
{
	return bsearch(&ve, list, size, sizeof(envid_t),
			envid_sort_fn) != NULL;
}

const char* ubcstr(unsigned long bar, unsigned long lim)
{
	static char str[64];
	char *p = str;
	char *e = p + sizeof(str) - 1;

#define PRINT_UBC(val) \
	if (val == LONG_MAX) \
		p += snprintf(p, e - p, "unlimited"); \
	else \
		p += snprintf(p, e - p, "%lu", val)

	PRINT_UBC(bar);

	if (bar == lim)
		return str;

	*p++=':';
	PRINT_UBC(lim);

	return str;
}

int is_vswap_mode(void)
{
	return (access("/proc/vz/vswap", F_OK) == 0);
}

#define UUID_LEN 36
/* Check for valid GUID, add curly brackets if necessary:
 *	fbcdf284-5345-416b-a589-7b5fcaa87673 ->
 *	{fbcdf284-5345-416b-a589-7b5fcaa87673}
 */
int vzctl_get_normalized_guid(const char *str, char *buf, int len)
{
	int i;
	const char *in;
	char *out;

	 if (len < UUID_LEN + 3)
		 return -1;

	in = (str[0] == '{') ? str + 1 : str;
	buf[0] = '{';
	out = buf + 1;

	for (i = 0; i < UUID_LEN; i++) {
		if (in[i] == '\0')
			break;
		if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
			if (in[i] != '-' )
				break;
		} else if (!isxdigit(in[i])) {
			break;
		}
		out[i] = in[i];
	}
	if (i < UUID_LEN)
		return 1;
	if (in[UUID_LEN] != '\0' &&
			(in[UUID_LEN] != '}' || in[UUID_LEN + 1] != '\0'))
		return 1;

	out[UUID_LEN] = '}';
	out[UUID_LEN+1] = '\0';

	return 0;
}
