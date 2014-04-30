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
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/vzcalluser.h>
#include <linux/fairsched.h>
#include <errno.h>

#include "types.h"
#include "bitmap.h"
#include "cpu.h"
#include "env.h"
#include "vzerror.h"
#include "logger.h"
#include "vzsyscalls.h"

#ifdef VZ_KERNEL_SUPPORTED
static inline int fairsched_chwt(unsigned int id, unsigned wght)
{
	int ret;

	ret = syscall(__NR_fairsched_chwt, id, wght);
	if (ret && errno == ENOSYS)
		ret = 0;
	return ret;
}

static inline int fairsched_rate(unsigned int id, int op, unsigned rate)
{
	int ret;

	ret = syscall(__NR_fairsched_rate, id, op, rate);
	if (ret && errno == ENOSYS)
		ret = 0;
	return ret;
}

static inline int fairsched_vcpus(unsigned int id, unsigned vcpus)
{
	int ret;

	ret = syscall(__NR_fairsched_vcpus, id, vcpus);
	if (ret && errno == ENOSYS)
		ret = 0;
	return ret;
}

static inline int fairsched_cpumask(unsigned int id,
		unsigned int masksize, unsigned long *mask)
{
	int ret = ENOTSUP;
/*
 * fairsched_cpumask is available only in vz kernels based on linux 2.6.32
 * or later which do not support platforms different from x86.
 */
#if defined(__i386__) || defined(__x86_64__)

	ret = syscall(__NR_fairsched_cpumask, id, masksize, mask);
	if (ret && errno == ENOSYS)
		ret = 0;
#endif
	return ret;
}

static inline int fairsched_nodemask(unsigned int id,
		unsigned int masksize, unsigned long *mask)
{
	int ret = ENOTSUP;
/*
 * fairsched_nodemask is available only in vz kernel >= 042stab043
 * which do not support platforms different from x86.
 */
#if defined(__i386__) || defined(__x86_64__)

	ret = syscall(__NR_fairsched_nodemask, id, masksize, mask);
	if (ret && errno == ENOSYS)
		ret = 0;
#endif
	return ret;
}

#else /* ! VZ_KERNEL_SUPPORTED */
static inline int fairsched_chwt(unsigned int id, unsigned wght)
{
	return ENOTSUP;
}

static inline int fairsched_rate(unsigned int id, int op, unsigned rate)
{
	return ENOTSUP;
}

static inline int fairsched_vcpus(unsigned int id, unsigned vcpus)
{
	return ENOTSUP;
}

static inline int fairsched_cpumask(unsigned int id,
		unsigned int masksize, unsigned long *mask)
{
	return ENOTSUP;
}

static inline int fairsched_nodemask(unsigned int id,
		unsigned int masksize, unsigned long *mask)
{
	return ENOTSUP;
}
#endif /* VZ_KERNEL_SUPPORTED */

int set_cpulimit(envid_t veid, unsigned int cpulimit)
{
	unsigned cpulim1024 = (float)cpulimit * 1024 / 100;
	int op = cpulim1024 ? FAIRSCHED_SET_RATE : FAIRSCHED_DROP_RATE;

	logger(0, 0, "Setting CPU limit: %d", cpulimit);
	if (fairsched_rate(veid, op, cpulim1024) < 0) {
		logger(-1, errno, "fairsched_rate");
		return VZ_SETFSHD_ERROR;
	}
	return 0;
}

int set_cpuweight(envid_t veid, unsigned int cpuweight)
{

	if (fairsched_chwt(veid, cpuweight)) {
		logger(-1, errno, "fairsched_chwt");
		return VZ_SETFSHD_ERROR;
	}
	return 0;
}

int set_cpuunits(envid_t veid, unsigned int cpuunits)
{
	int cpuweight, ret;

	if (cpuunits < MINCPUUNITS || cpuunits > MAXCPUUNITS) {
		logger(-1, 0, "Invalid value for cpuunits: %d"
			" allowed range is %d-%d",
			cpuunits, MINCPUUNITS, MAXCPUUNITS);
		return VZ_SETFSHD_ERROR;
	}
	cpuweight = MAXCPUUNITS / cpuunits;
	logger(0, 0, "Setting CPU units: %d", cpuunits);
	ret = set_cpuweight(veid, cpuweight);
	return ret;
}

int set_cpumask(envid_t veid, cpumask_t *mask)
{
	static char maskstr[CPUMASK_NBITS * 2];

	bitmap_snprintf(maskstr, CPUMASK_NBITS * 2,
			mask->bits, CPUMASK_NBITS);
	logger(0, 0, "Setting CPU mask: %s", maskstr);
	if (fairsched_cpumask(veid, sizeof(mask->bits), mask->bits)) {
		logger(-1, errno, "fairsched_cpumask");
		return VZ_SETFSHD_ERROR;
	}
	return 0;
}

int set_nodemask(envid_t veid, nodemask_t *mask)
{
	static char maskstr[NODEMASK_NBITS * 2];

	bitmap_snprintf(maskstr, NODEMASK_NBITS * 2,
			mask->bits, NODEMASK_NBITS);
	logger(0, 0, "Setting NUMA node mask: %s", maskstr);
	if (fairsched_nodemask(veid, sizeof(mask->bits), mask->bits)) {
		logger(-1, errno, "fairsched_nodemask");
		return VZ_SETFSHD_ERROR;
	}
	return 0;
}

static int numa_node_to_cpu(int nid, unsigned long *cpumask, int size,
		int suppress_warning)
{
	DIR *d;
	struct dirent *de;
	char path[64];
	char *endp;
	int nmaskbits = size * 8;

	sprintf(path, "/sys/devices/system/node/node%d", nid);
	d = opendir(path);
	if (!d) {
		if (errno == ENOENT && suppress_warning)
			return 0;
		return vzctl_err(-1, errno, "NUMA: Failed to open %s", path);
	}

	while ((de = readdir(d)) != NULL) {
		int cpu;

		if (strncmp(de->d_name, "cpu", 3))
			continue;
		cpu = strtoul(de->d_name + 3, &endp, 10);
		if (!*endp && cpu >= 0 && cpu < nmaskbits)
			bitmap_set_bit(cpu, cpumask);
	}
	closedir(d);

	return 0;
}

int get_node_cpumask(nodemask_t *nodemask, cpumask_t *cpumask)
{
	int n;
	int nmaskbits = sizeof(nodemask->bits) * 8;
	int all;

	/* Find out if all bits are set, i.e. --nodemask all
	 * Used to silence warnings about absent NUMA nodes
	 * printed by from numa_node_to_cpu().
	 *
	 * Unfortunately, there is no way to distinguish
	 * --nodemask 0-4095 from from --nodemask all so
	 * there will be no warnings in either case.
	 */
	all = (nmaskbits == bitmap_find_first_zero_bit(
				nodemask->bits, nmaskbits));

	bzero(cpumask->bits, sizeof(cpumask->bits));
	for (n = 0; n < nmaskbits; n++) {
		if (!bitmap_test_bit(n, nodemask->bits))
			continue;

		if (numa_node_to_cpu(n, cpumask->bits,
					sizeof(cpumask->bits), all))
			continue;
	}

	return 0;
}

/** Change number of CPUs available in the running CT.
 *
 * @param veid		CT ID
 * @param vcpu		number of CPUs
 */
int env_set_vcpus(envid_t veid, unsigned int vcpus)
{
	logger(0, 0, "Setting CPUs: %d", vcpus);
	if (fairsched_vcpus(veid, vcpus) != 0) {
		logger(-1, errno, "Unable to set cpus");
		return VZ_SETFSHD_ERROR;
	}

	return 0;
}


/**  Apply cpu parameters on Host system.
 *
 * @param cpu		cpu parameters.
 * @return		0 on success.
 */
int hn_set_cpu(cpu_param *cpu)
{
	if (cpu->units == NULL)
		return 0;
	return set_cpuunits(2147483647, *cpu->units);
}

/**  Apply cpu parameters on running CT.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param cpu		cpu parameters.
 * @return		0 on success.
 */
int vps_set_cpu(vps_handler *h, envid_t veid, cpu_param *cpu)
{
	if (!cpu->limit && !cpu->units && !cpu->weight && !cpu->vcpus &&
			!cpu->mask && !cpu->nodemask && !cpu->cpumask_auto)
	{
		return 0;
	}
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to apply CPU parameters: "
			"container is not running");
		return VZ_VE_NOT_RUNNING;
	}

	return h->setcpus(h, veid, cpu);
}
