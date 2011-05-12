/*
 *  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/cpt_ioctl.h>
#include <linux/vzcalluser.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <dirent.h>


#include "cpt.h"
#include "env.h"
#include "exec.h"
#include "script.h"
#include "config.h"
#include "vzerror.h"
#include "logger.h"
#include "util.h"

static int setup_hardlink_dir(const char *mntdir, int cpt_fd);

int cpt_cmd(vps_handler *h, envid_t veid, int action, cpt_param *param,
	vps_param *vps_p)
{
	int fd;
	int err, ret = 0;
	const char *file;

	if (!vps_is_run(h, veid)) {
		logger(0, 0, "Container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	if (action == CMD_CHKPNT) {
		file = PROC_CPT;
		err = VZ_CHKPNT_ERROR;
	} else if (action == CMD_RESTORE) {
		file = PROC_RST;
		err = VZ_RESTORE_ERROR;
	} else {
		logger(-1, 0, "cpt_cmd: Unsupported cmd");
		return -1;
	}
	if ((fd = open(file, O_RDWR)) < 0) {
		if (errno == ENOENT)
			logger(-1, errno, "Error: No checkpointing"
				" support, unable to open %s", file);
		else
			logger(-1, errno, "Unable to open %s", file);
		return err;
	}
	if ((ret = ioctl(fd, CPT_JOIN_CONTEXT, param->ctx ? : veid)) < 0) {
		logger(-1, errno, "Can not join cpt context %d", param->ctx);
		goto err;
	}
	switch (param->cmd) {
	case CMD_KILL:
		logger(0, 0, "Killing...");
		if ((ret = ioctl(fd, CPT_KILL, 0)) < 0) {
			logger(-1, errno, "Can not kill container");
			goto err;
		}
		break;
	case CMD_RESUME:
		logger(0, 0, "Resuming...");
		clean_hardlink_dir(vps_p->res.fs.root);
		if ((ret = ioctl(fd, CPT_RESUME, 0)) < 0) {
			logger(-1, errno, "Can not resume container");
			goto err;
		}
		if (action == CMD_CHKPNT) {
			/* restore arp/routing cleared on dump stage */
			run_net_script(veid, ADD, &vps_p->res.net.ip,
				STATE_RUNNING,
				vps_p->res.net.skip_arpdetect);
		}
		break;
	}
	if (!param->ctx) {
		logger(2, 0, "\tput context");
		if ((ret = ioctl(fd, CPT_PUT_CONTEXT, 0)) < 0) {
			logger(-1, errno, "Can not put context");
			goto err;
		}
	}
err:
	close(fd);
	return ret ? err : 0;
}

int real_chkpnt(int cpt_fd, envid_t veid, const char *root, cpt_param *param,
	int cmd)
{
	int ret, len, len1;
	char buf[PIPE_BUF];
	int err_p[2];

	if ((ret = vz_chroot(root)))
		return VZ_CHKPNT_ERROR;
	if (pipe(err_p) < 0) {
		logger(-1, errno, "Can't create pipe");
		return VZ_CHKPNT_ERROR;
	}
	fcntl(err_p[0], F_SETFL, O_NONBLOCK);
	fcntl(err_p[1], F_SETFL, O_NONBLOCK);
	if (ioctl(cpt_fd, CPT_SET_ERRORFD, err_p[1]) < 0) {
		logger(-1, errno, "Can't set errorfd");
		return VZ_CHKPNT_ERROR;
	}
	close(err_p[1]);
	if (cmd == CMD_CHKPNT || cmd == CMD_SUSPEND) {
		logger(0, 0, "\tsuspend...");
		if (ioctl(cpt_fd, CPT_SUSPEND, 0) < 0) {
			logger(-1, errno, "Can not suspend container");
			goto err_out;
		}
	}
	if (cmd == CMD_CHKPNT || cmd == CMD_DUMP) {
		logger(0, 0, "\tdump...");
		clean_hardlink_dir("/");
		if (setup_hardlink_dir("/", cpt_fd))
			goto err_out;
		if (ioctl(cpt_fd, CPT_DUMP, 0) < 0) {
			logger(-1, errno, "Can not dump container");
			if (cmd == CMD_CHKPNT) {
				clean_hardlink_dir("/");
				if (ioctl(cpt_fd, CPT_RESUME, 0) < 0)
					logger(-1, errno, "Can not "
							"resume container");
			}
			goto err_out;
		}
	}
	if (cmd == CMD_CHKPNT) {
		logger(0, 0, "\tkill...");
		if (ioctl(cpt_fd, CPT_KILL, 0) < 0) {
			logger(-1, errno, "Can not kill container");
			goto err_out;
		}
	}
	if (cmd == CMD_SUSPEND && !param->ctx) {
		logger(0, 0, "\tget context...");
		if (ioctl(cpt_fd, CPT_GET_CONTEXT, veid) < 0) {
			logger(-1, errno, "Can not get context");
			goto err_out;
		}
	}
	close(err_p[0]);
	return 0;
err_out:
	while ((len = read(err_p[0], buf, PIPE_BUF)) > 0) {
		do {
			len1 = write(STDERR_FILENO, buf, len);
			len -= len1;
		} while (len > 0 && len1 > 0);
		if (cmd == CMD_SUSPEND && param->ctx) {
			/* destroy context */
			if (ioctl(cpt_fd, CPT_PUT_CONTEXT, veid) < 0)
				logger(-1, errno, "Can't put context");
		}
	}
	fflush(stderr);
	close(err_p[0]);
	return VZ_CHKPNT_ERROR;
}

int vps_chkpnt(vps_handler *h, envid_t veid, vps_param *vps_p, int cmd,
	cpt_param *param)
{
	int dump_fd = -1;
	char dumpfile[PATH_LEN];
	int cpt_fd, pid, ret;
	const char *root = vps_p->res.fs.root;

	ret = VZ_CHKPNT_ERROR;
	if (root == NULL) {
		logger(-1, 0, "Container root (VE_ROOT) is not set");
		return VZ_VE_ROOT_NOTSET;
	}
	if (!vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to setup checkpointing: "
			"container is not running");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Setting up checkpoint...");
	if ((cpt_fd = open(PROC_CPT, O_RDWR)) < 0) {
		if (errno == ENOENT)
			logger(-1, errno, "Error: No checkpointing"
				" support, unable to open " PROC_CPT);
		else
			logger(-1, errno, "Unable to open " PROC_CPT);
		return VZ_CHKPNT_ERROR;
	}
	if ((cmd == CMD_CHKPNT || cmd == CMD_DUMP)) {
		if (param->dumpfile == NULL) {
			if (cmd == CMD_DUMP) {
				logger(-1,  0, "Error: dumpfile is not"
					" specified.");
				goto err;
			}
			get_dump_file(veid, vps_p->res.cpt.dumpdir,
					dumpfile, sizeof(dumpfile));
		}
		dump_fd = open(param->dumpfile ? : dumpfile,
			O_CREAT|O_TRUNC|O_RDWR, 0600);
		if (dump_fd < 0) {
			logger(-1, errno, "Can not create dump file %s",
				param->dumpfile ? : dumpfile);
			goto err;
		}
	}
	if (param->ctx || cmd > CMD_SUSPEND) {
		logger(0, 0, "\tjoin context..");
		if (ioctl(cpt_fd, CPT_JOIN_CONTEXT, param->ctx ? : veid) < 0) {
			logger(-1, errno, "Can not join cpt context");
			goto err;
		}
	} else {
		if (ioctl(cpt_fd, CPT_SET_VEID, veid) < 0) {
			logger(-1, errno, "Can not set CT ID");
			goto err;
		}
	}
	if (dump_fd != -1) {
		if (ioctl(cpt_fd, CPT_SET_DUMPFD, dump_fd) < 0) {
			logger(-1, errno, "Can not set dump file");
			goto err;
		}
	}
	if (param->cpu_flags) {
		logger(0, 0, "\tset CPU flags..");
		if (ioctl(cpt_fd, CPT_SET_CPU_FLAGS, param->cpu_flags) < 0) {
			logger(-1, errno, "Can not set CPU flags");
			goto err;
		}
	}
	if ((pid = fork()) < 0) {
		logger(-1, errno, "Can't fork");
		ret = VZ_RESOURCE_ERROR;
		goto err;
	} else if (pid == 0) {
		if ((ret = vz_setluid(veid)))
			exit(ret);
		if ((pid = fork()) < 0) {
			logger(-1, errno, "Can't fork");
			exit(VZ_RESOURCE_ERROR);
		} else if (pid == 0) {
			ret = real_chkpnt(cpt_fd, veid, root, param, cmd);
			exit(ret);
		}
		ret = env_wait(pid);
		exit(ret);
	}
	close(cpt_fd);
	cpt_fd = -1;
	ret = env_wait(pid);
	if (ret)
		goto err;
	if (cmd == CMD_CHKPNT || cmd == CMD_DUMP) {
		/* Clear CT network configuration */
		run_net_script(veid, DEL, &vps_p->res.net.ip, STATE_RUNNING,
			vps_p->res.net.skip_arpdetect);
		if (cmd == CMD_CHKPNT)
			vps_umount(h, veid, root, 0);
	}
	ret = 0;
	logger(0, 0, "Checkpointing completed succesfully");
err:
	if (ret) {
		ret = VZ_CHKPNT_ERROR;
		logger(-1, 0, "Checkpointing failed");
		if (cmd == CMD_CHKPNT || cmd == CMD_DUMP)
			unlink(param->dumpfile ? : dumpfile);
	}
	if (dump_fd != -1)
		close(dump_fd);
	if (cpt_fd != -1)
		close(cpt_fd);

	return ret;
}

static int restore_fn(vps_handler *h, envid_t veid, int wait_p,
		int old_wait_p, int err_p, void *data)
{
	int status, len, len1, ret;
	cpt_param *param = (cpt_param *) data;
	char buf[PIPE_BUF];
	int error_pipe[2];

	status = VZ_RESTORE_ERROR;
	if (param == NULL)
		goto err;
	/* Close all fds */
	close_fds(0, wait_p, old_wait_p, err_p, h->vzfd, param->rst_fd, -1);
	if (ioctl(param->rst_fd, CPT_SET_VEID, veid) < 0) {
		logger(-1, errno, "Can't set CT ID %d", param->rst_fd);
		goto err;
	}
	if (pipe(error_pipe) < 0 ) {
		logger(-1, errno, "Can't create pipe");
		goto err;
	}
	fcntl(error_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(error_pipe[1], F_SETFL, O_NONBLOCK);
	if (ioctl(param->rst_fd, CPT_SET_ERRORFD, error_pipe[1]) < 0) {
		logger(-1, errno, "Can't set errorfd");
		goto err;
	}
	close(error_pipe[1]);

	ret = ioctl(param->rst_fd, CPT_SET_LOCKFD2, wait_p);
	if (ret < 0 && errno == EINVAL)
		ret = ioctl(param->rst_fd, CPT_SET_LOCKFD, old_wait_p);
	if (ret < 0) {
		logger(-1, errno, "Can't set lockfd");
		goto err;
	}

	if (ioctl(param->rst_fd, CPT_SET_STATUSFD, STDIN_FILENO) < 0) {
		logger(-1, errno, "Can't set statusfd");
		goto err;
	}
	/* Close status descriptor to report that
	* environment is created.
	*/
	close(STDIN_FILENO);

	ioctl(param->rst_fd, CPT_HARDLNK_ON);

	logger(0, 0, "\tundump...");
	if (ioctl(param->rst_fd, CPT_UNDUMP, 0) != 0) {
		logger(-1, errno, "Error: undump failed");
		goto err_undump;
	}
	/* Now we wait until CT setup will be done */
	read(wait_p, &len, sizeof(len));
	if (param->cmd == CMD_RESTORE) {
		clean_hardlink_dir("/");
		logger(0, 0, "\tresume...");
		if (ioctl(param->rst_fd, CPT_RESUME, 0)) {
			logger(-1, errno, "Error: resume failed");
			goto err_undump;
		}
	} else if (param->cmd == CMD_UNDUMP && !param->ctx) {
		logger(0, 0, "\tget context...");
		if (ioctl(param->rst_fd, CPT_GET_CONTEXT, veid) < 0) {
			logger(-1, 0, "Can not get context");
			goto err_undump;
		}
	}
	status = 0;
err:
	close(error_pipe[0]);
	if (status)
		write(err_p, &status, sizeof(status));
	return status;
err_undump:
	logger(-1, 0, "Restoring failed:");
	while ((len = read(error_pipe[0], buf, PIPE_BUF)) > 0) {
		do {
			len1 = write(STDERR_FILENO, buf, len);
			len -= len1;
		} while (len > 0 && len1 > 0);
	}
	fflush(stderr);
	close(error_pipe[0]);
	write(err_p, &status, sizeof(status));
	return status;
}

int vps_restore(vps_handler *h, envid_t veid, vps_param *vps_p, int cmd,
	cpt_param *param)
{
	int ret, rst_fd;
	int dump_fd = -1;
	char dumpfile[PATH_LEN];

	if (vps_is_run(h, veid)) {
		logger(-1, 0, "Unable to perform restore: "
			"container already running");
		return VZ_VE_NOT_RUNNING;
	}
	logger(0, 0, "Restoring container ...");
	ret = VZ_RESTORE_ERROR;
	if ((rst_fd = open(PROC_RST, O_RDWR)) < 0) {
		if (errno == ENOENT)
			logger(-1, errno, "Error: No checkpointing"
				" support, unable to open " PROC_RST);
		else
			logger(-1, errno, "Unable to open " PROC_RST);
		return VZ_RESTORE_ERROR;
	}
	if (param->ctx) {
		if (ioctl(rst_fd, CPT_JOIN_CONTEXT, param->ctx) < 0) {
			logger(-1, errno, "Can not join cpt context");
			goto err;
		}
	}
	if (param->dumpfile == NULL) {
		if (cmd == CMD_UNDUMP) {
			logger(-1, 0, "Error: dumpfile is not specified");
			goto err;
		}

			get_dump_file(veid, vps_p->res.cpt.dumpdir,
					dumpfile, sizeof(dumpfile));
	}
	if (cmd == CMD_RESTORE || cmd == CMD_UNDUMP) {
		dump_fd = open(param->dumpfile ? : dumpfile, O_RDONLY);
		if (dump_fd < 0) {
			logger(-1, errno, "Unable to open %s",
				param->dumpfile ? : dumpfile);
			goto err;
		}
	}
	if (dump_fd != -1) {
		if (ioctl(rst_fd, CPT_SET_DUMPFD, dump_fd)) {
			logger(-1, errno, "Can't set dumpfile");
			goto err;
		}
	}
	param->rst_fd = rst_fd;
	param->cmd = cmd;
	ret = vps_start_custom(h, veid, vps_p, SKIP_CONFIGURE,
		NULL, restore_fn, param);
	if (ret)
		goto err;
	/* Restore second-level quota links & quota device */
	if ((cmd == CMD_RESTORE || cmd == CMD_UNDUMP) &&
		vps_p->res.dq.ugidlimit != NULL &&
		*vps_p->res.dq.ugidlimit != 0)
	{
		logger(0, 0, "Restore second-level quota");
		if (vps_execFn(h, veid, vps_p->res.fs.root, mk_quota_link, NULL,
			VE_SKIPLOCK))
		{
			logger(-1, 0, "Warning: restoring second-level "
					"quota links failed");
		}
	}
err:
	close(rst_fd);
	if (dump_fd != -1)
		close(dump_fd);
	if (!ret)
		logger(0, 0, "Restoring completed succesfully");
	return ret;
}

/* with mix of md5sum: try generate unique name */
#define CPT_HARDLINK_DIR ".cpt_hardlink_dir_a920e4ddc233afddc9fb53d26c392319"

void clean_hardlink_dir(const char *mntdir)
{
	char buf[STR_SIZE];
	struct dirent *ep;
	DIR *dp;

	snprintf(buf, sizeof(buf), "%s/%s", mntdir, CPT_HARDLINK_DIR);

	unlink(buf);		/* if file was created by someone */
	if (!(dp = opendir(buf)))
		return;
	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;

		snprintf(buf, sizeof(buf), "%s/%s/%s",
				mntdir, CPT_HARDLINK_DIR, ep->d_name);
		unlink(buf);
	}
	closedir(dp);

	snprintf(buf, sizeof(buf), "%s/%s", mntdir, CPT_HARDLINK_DIR);
	rmdir(buf);
}

static int setup_hardlink_dir(const char *mntdir, int cpt_fd)
{
	char buf[STR_SIZE];
	int fd, res = 0;

	snprintf(buf, sizeof(buf), "%s/%s", mntdir, CPT_HARDLINK_DIR);
	mkdir(buf, 0711);

	fd = open(buf, O_RDONLY | O_NOFOLLOW | O_DIRECTORY);
	if (fd < 0) {
		logger(-1, errno, "Error: Unable to open "
				"hardlink directory %s", buf);
		return 1;
	}

	if (ioctl(cpt_fd, CPT_LINKDIR_ADD, fd) < 0) {
		if (errno != EINVAL) {
			res = 1;
			logger(-1, errno, "Cannot set linkdir in kernel");
		}
		rmdir(buf);
	}

	close(fd);
	return res;
}
