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
#ifndef _VZCTL_H_
#define _VZCTL_H_

#define	VERSION			"2.7.0-22"

/* Action numeration should begin with 1. Value 0 is reserved. */
#define ACTION_CREATE		1
#define ST_CREATE		"creating"
#define ACTION_DESTROY		2
#define ST_DESTROY		"destroying"
#define ACTION_MOUNT		3
#define ST_MOUNT		"mounting"
#define ACTION_UMOUNT		4
#define ST_UMOUNT		"unmounting"
#define ACTION_START		5
#define ST_START		"starting"
#define ACTION_STOP		6
#define ST_STOP			"stopping"
#define ACTION_RESTART		7
#define ST_RESTART		"restarting"
#define ACTION_SET		8
#define ST_SET			"setting"
#define ACTION_STATUS		9
#define ACTION_EXEC		10
#define ACTION_EXEC2		11
#define ACTION_EXEC3		12
#define ACTION_ENTER		13
#define ST_QUOTA_INIT		"initializing-quota"
#define	ACTION_RUNSCRIPT	14
#define	ACTION_CUSTOM		15

/* default cpu units values */
#define LHTCPUUNITS             250
#define UNLCPUUNITS             1000
#define HNCPUUNITS              1000

/* setmode flags */
enum {
        SET_NONE = 0,
        SET_IGNORE,
        SET_RESTART,
};

#endif
