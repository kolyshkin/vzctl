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
#ifndef _VZCTL_H_
#define _VZCTL_H_

#define	VERSION			"3.0.11"

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
#define ACTION_CHKPNT		16
#define ACTION_RESTORE		17
#define ACTION_QUOTAON		18
#define ACTION_QUOTAOFF		19
#define ACTION_QUOTAINIT	20

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
