/*
 *  Copyright (C) 2000-2012, Parallels, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/vzcalluser.h>

#include "env.h"
#include "types.h"
#include "logger.h"

static int vz_is_run(vps_handler *h, envid_t veid)
{
	int ret = vz_env_create_ioctl(h, veid, VE_TEST);

	if (ret < 0 && (errno == ESRCH || errno == ENOTTY))
		return 0;
	else if (ret < 0)
		logger(-1, errno, "Error on vz_env_create_ioctl(VE_TEST)");
	return 1;
}

int vz_do_open(vps_handler *h)
{
	if ((h->vzfd = open(VZCTLDEV, O_RDWR)) < 0) {
		logger(-1, errno, "Unable to open %s", VZCTLDEV);
		logger(-1, 0, "Please check that vzdev kernel module is loaded"
			" and you have sufficient permissions"
			" to access the file.");
		return -1;
	}

	if (vz_env_create_ioctl(h, 0, 0) < 0 &&
		(errno == ENOSYS || errno == EPERM))
	{
		logger(-1, 0, "Your kernel lacks support for virtual"
			" environments or modules not loaded");
		goto err;
	}

	h->is_run = vz_is_run;
	return 0;
err:
	close(h->vzfd);
	return -1;
}
