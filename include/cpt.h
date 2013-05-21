#ifndef _CPT_H_
#define _CPT_H_
#include "types.h"
#include "res.h"

#define PROC_CPT	"/proc/cpt"
#define PROC_RST	"/proc/rst"

#define CMD_CHKPNT		1
#define CMD_SUSPEND		2
#define CMD_DUMP		3
#define CMD_RESTORE		4
#define CMD_UNDUMP		5

#define CMD_KILL		10
#define CMD_RESUME		11

#define GET_DUMP_FILE(req_cmd)							\
do {										\
	dumpfile = param->dumpfile;						\
	if (dumpfile == NULL) {							\
		if (cmd == req_cmd) {						\
			logger(-1,  0, "Error: dumpfile is not specified");	\
			goto err;						\
		}								\
		get_dump_file(veid, param->dumpdir, buf, sizeof(buf));		\
		dumpfile = buf;							\
	}									\
} while (0)

int cpt_cmd(vps_handler *h, envid_t veid, const char *root,
		int action, int cmd, unsigned int ctx);
int vps_chkpnt(vps_handler *h, envid_t veid, const fs_param *fs,
		int cmd, cpt_param *param);
int vps_restore(vps_handler *h, envid_t veid, struct vps_param *vps_p, int cmd,
	cpt_param *param, skipFlags skip);
void clean_hardlink_dir(const char *mntdir);

#endif
