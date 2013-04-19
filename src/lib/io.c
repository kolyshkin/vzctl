/*
 *  Copyright (C) 2008, Parallels, Inc. All rights reserved.
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
#include <unistd.h>
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

int ve_ioprio_set(vps_handler *h, envid_t veid, io_param *io_param)
{
	int ioprio;
	int ret;

	ioprio = io_param->ioprio;

	if (ioprio < 0)
		return 0;

	ret = ioprio_set(IOPRIO_WHO_UBC, veid,
			ioprio | IOPRIO_CLASS_BE << IOPRIO_CLASS_SHIFT);
	if (ret) {
		if (errno == EINVAL) {
			logger(-1, 0, "Warning: ioprio feature is not "
					"supported by the kernel, ioprio "
					"configuration is skipped");
			return 0;
		}

		logger(-1, errno, "Unable to set ioprio");
		return VZ_SET_IO;
	}

	return 0;
}
