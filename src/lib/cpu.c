/*
 *  Copyright (C) 2000-2006 SWsoft. All rights reserved.
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
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/vzcalluser.h>
#include <errno.h>

#include "types.h"
#include "cpu.h"
#include "env.h"
#include "vzerror.h"
#include "logger.h"

#ifndef __NR_fairsched_chwt
#ifdef __ia64__
#define __NR_fairsched_chwt    1502
#elif __x86_64__
#define __NR_fairsched_chwt    506
#else
#define __NR_fairsched_chwt    502
#endif
#endif
#ifndef __NR_fairsched_rate
#ifdef __ia64__
#define __NR_fairsched_rate    1504
#elif __x86_64__
#define __NR_fairsched_rate    508
#else
#define __NR_fairsched_rate    504
#endif
#endif

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

static int set_cpulimit(envid_t veid, unsigned int cpulimit)
{
	unsigned cpulim1024 = (float)cpulimit * 1024 / 100;

	if (fairsched_rate(veid, cpulim1024 ? 0 : 1, cpulim1024) < 0) {
		logger(0, errno, "fairsched_rate");
		return VZ_SETFSHD_ERROR;
	}
	return 0;
}

static int set_cpuweight(envid_t veid, unsigned int cpuweight)
{

	if (fairsched_chwt(veid, cpuweight)) {
		logger(0, errno, "fairsched_chwt");
		return VZ_SETFSHD_ERROR;
	}
	return 0;
}

static int set_cpuunits(envid_t veid, unsigned int cpuunits)
{
	int cpuweight, ret;

	if (cpuunits < MINCPUUNITS || cpuunits > MAXCPUUNITS) {
		logger(0, 0, "Invalid value for cpuunit: %d"
			" allowed range is %d-%d",
			cpuunits, MINCPUUNITS, MAXCPUUNITS);
		return VZ_SETFSHD_ERROR;
	}
	cpuweight = MAXCPUUNITS / cpuunits;
	ret = set_cpuweight(veid, cpuweight);
	return ret;
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

/**  Apply cpu parameters on running VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param cpu		cpu parameters.
 * @return		0 on success.
 */
int vps_set_cpu(vps_handler *h, envid_t veid, cpu_param *cpu)
{
	int ret = 0;

	if (cpu->limit == NULL &&
		cpu->units == NULL &&
		cpu->weight == NULL)
	{
		return 0;
	}
	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Unable to apply CPU parameters: VPS is not"
			" running");
		return VZ_VE_NOT_RUNNING;
	}
	if (cpu->limit != NULL) {
		logger(0, 0, "Setting CPU limit: %d", cpu->limit[0]);
		ret = set_cpulimit(veid, *cpu->limit);
	}
	if (cpu->units != NULL) {
		logger(0, 0, "Setting CPU units: %d", cpu->units[0]);
		ret = set_cpuunits(veid, *cpu->units);
	} else if (cpu->weight != NULL)
		ret = set_cpuweight(veid, *cpu->weight);

	return ret;
}
