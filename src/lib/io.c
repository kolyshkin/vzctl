/*
 *  Copyright (C) 2007-2013, Parallels, Inc. All rights reserved.
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

#include <errno.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/vziolimit.h>
#include "list.h"
#include "res.h"
#include "vzerror.h"
#include "logger.h"
#include "io.h"
#include "vzsyscalls.h"

static int ioprio_set(int which, int who, int ioprio)
{
	return syscall(__NR_ioprio_set, which, who, ioprio);
}

int vzctl_set_ioprio(vps_handler *h, envid_t veid, int ioprio)
{
	int ret;

	if (ioprio < 0)
		return 0;

	ret = ioprio_set(IOPRIO_WHO_UBC, veid,
			ioprio | IOPRIO_CLASS_BE << IOPRIO_CLASS_SHIFT);
	if (ret) {
		if (errno == EINVAL) {
			logger(-1, 0, "Warning: ioprio not supported "
					"by the kernel, skipping");
			return 0;
		}

		logger(-1, errno, "Unable to set ioprio");
		return VZ_SET_IO;
	}

	return 0;
}

int vzctl_set_iolimit(vps_handler *h, envid_t veid, int limit)
{
	int ret;
	struct iolimit_state io;

	if (limit < 0)
		return 0;

	io.id = veid;
	/* Values hardcoded according to PCLIN-27604 */
	io.speed = limit;
	io.burst = limit * 3;
	io.latency = 10*1000;
	logger(0, 0, "Setting iolimit: %d %s", limit,
			(limit != 0) ? "bytes/sec" : "(unlimited)");
	ret = ioctl(h->vzfd, VZCTL_SET_IOLIMIT, &io);
	if (ret) {
		if (errno == ESRCH)
			return vzctl_err(VZ_VE_NOT_RUNNING, 0,
					"Container is not running");
		else if (errno == ENOTTY)
			return vzctl_err(0, 0, "Warning: iolimit not "
					"supported by the kernel, skipping");

		return vzctl_err(VZ_SET_IO, errno, "Unable to set iolimit");
	}

	return 0;
}

int vzctl_set_iopslimit(vps_handler *h, envid_t veid, int limit)
{
	int ret;
	struct iolimit_state io;

	if (limit < 0)
		return 0;

	io.id = veid;
	/* Values hardcoded according to PCLIN-27604 */
	io.speed = limit;
	io.burst = limit * 3;
	io.latency = 10*1000;
	logger(0, 0, "Setting iopslimit: %d %s", limit,
			(limit != 0) ? "iops" : "(unlimited)");
	ret = ioctl(h->vzfd, VZCTL_SET_IOPSLIMIT, &io);
	if (ret) {
		if (errno == ESRCH)
			return vzctl_err(VZ_VE_NOT_RUNNING, 0,
					"Container is not running");
		else if (errno == ENOTTY)
			return vzctl_err(0, 0, "Warning: iopslimit not "
					"supported by the kernel, skipping");

		return vzctl_err(VZ_SET_IO, errno,
				"Unable to set iopslimit");
	}

	return 0;
}

int vps_set_io(vps_handler *h, envid_t veid, io_param *io)
{
	int ret;

	if (!io)
		return 0;

	ret = vzctl_set_ioprio(h, veid, io->ioprio);
	if (ret)
		return ret;

	ret = vzctl_set_iolimit(h, veid, io->iolimit);
	if (ret)
		return ret;

	ret = vzctl_set_iopslimit(h, veid, io->iopslimit);

	return ret;
}

int vzctl_get_iolimit(int vzfd, envid_t veid, int *limit)
{
	int ret;
	struct iolimit_state io;

	io.id = veid;
	ret = ioctl(vzfd, VZCTL_GET_IOLIMIT, &io);
	if (ret == 0)
		*limit = io.speed;

	return ret;
}

int vzctl_get_iopslimit(int vzfd, envid_t veid, int *limit)
{
	int ret;
	struct iolimit_state io;

	io.id = veid;
	ret = ioctl(vzfd, VZCTL_GET_IOPSLIMIT, &io);
	if (ret == 0)
		*limit = io.speed;

	return ret;
}
