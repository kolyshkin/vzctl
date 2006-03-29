/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef	_TYPES_H_
#define	_TYPES_H_

#define GLOBAL_CFG              "/etc/sysconfig/vz"
#define VPS_CONF_DIR            "/etc/sysconfig/vz-scripts/"
#define LIB_DIR			"/usr/lib/vzctl/lib/"
#define VPS_SCRIPTS_DIR         "/usr/lib/vzctl/scripts/"

#define VPS_STOP		VPS_SCRIPTS_DIR "vps-stop"
#define VPS_NET_ADD		VPS_SCRIPTS_DIR "vps-net_add"
#define VPS_NET_DEL		VPS_SCRIPTS_DIR "vps-net_del"

#define envid_t 	unsigned int
#define STR_SIZE	512
#define PATH_LEN	256

#define NONE		0
#define YES		1
#define NO		2

#define ADD     	0
#define DEL     	1

/* Default enviroment variable PATH */
#define	ENV_PATH	"PATH=/bin:/sbin:/usr/bin:/usr/sbin:"

/* VPS states */
enum {
	STATE_STARTING = 1,
	STATE_RUNNING = 2,
	STATE_STOPPED = 3,
	STATE_STOPPING = 4,
};

/* Parse errro codes */
#define ERR_DUP		-1
#define ERR_INVAL	-2
#define ERR_UNK		-3
#define ERR_NOMEM	-4
#define ERR_OTHER	-5
#define ERR_INVAL_SKIP	-6

/** VPS handler.
 */
typedef struct {
        int vzfd;	/**< /dev/vzctl file deccriptor. */
} vps_handler;

typedef enum {
	SKIP_NONE =		0,
	SKIP_SETUP =		(1<<0),
	SKIP_CONFIGURE =	(1<<1),
	SKIP_ACTION_SCRIPT =	(1<<2)
} skipFlags;

#endif

