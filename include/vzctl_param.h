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
#ifndef _VZCTL_PARAM_H_
#define _VZCTL_PARAM_H_

/*	UB	*/
#define PARAM_KMEMSIZE		'k'
#define PARAM_LOCKEDPAGES	'l'
#define PARAM_PRIVVMPAGES	1
#define PARAM_SHMPAGES		2
#define PARAM_NUMPROC		'p'
#define PARAM_PHYSPAGES		3
#define PARAM_VMGUARPAGES	4
#define PARAM_OOMGUARPAGES	5
#define PARAM_NUMTCPSOCK	6
#define PARAM_NUMFLOCK		'f'
#define PARAM_NUMPTY		't'
#define PARAM_NUMSIGINFO	'i'
#define PARAM_TCPSNDBUF		7
#define PARAM_TCPRCVBUF		'b'
#define PARAM_OTHERSOCKBUF	8
#define PARAM_DGRAMRCVBUF	9
#define PARAM_NUMOTHERSOCK	10
#define PARAM_NUMIPTENT		'e'
#define PARAM_NUMFILE		'n'
#define PARAM_DCACHE		'x'

#define PARAM_PKGVER		11
#define PARAM_PKGSET		12
#define PARAM_OSTEMPLATE	13

#define PARAM_CPUWEIGHT		14
#define PARAM_CPULIMIT		15
#define PARAM_CPUUNITS		16

#define PARAM_IP_ADD		18
#define PARAM_IP_DEL		19
#define PARAM_HOSTNAME		20
#define PARAM_SEARCHDOMAIN	22
#define PARAM_NAMESERVER	23
#define PARAM_USERPW		24
#define PARAM_TEMPLATE		25
#define PARAM_IP		28
#define PARAM_BASH		30
#define PARAM_SAVE		31
#define PARAM_ROOT		32
#define PARAM_PRIVATE		33
#define PARAM_ONBOOT		34
#define PARAM_IPHOST		35

#define PARAM_DISKSPACE		36
#define PARAM_DISKINODES	37
#define PARAM_QUOTATIME		38
#define PARAM_QUOTAUGIDLIMIT	39
#define PARAM_DISK_QUOTA	40

#define PARAM_NOATIME		43
#define PARAM_SAFE		44
#define PARAM_CAP		45
#define PARAM_CONFIG		47
#define PARAM_SHARED		48
#define PARAM_CFGVER            50
#define PARAM_DEVICES           51
#define PARAM_AVNUMPROC		52
#define PARAM_TEMPLATES		93
#define PARAM_DEVNODES		94 
#define	PARAM_SKIPQUOTACHECK	95

#define PARAM_FAST		310
#define PARAM_APPLYCONFIG	311
#define PARAM_IPTABLES		316
#define PARAM_NETDEV_ADD	317
#define PARAM_NETDEV_DEL	318
#define PARAM_DISABLED		319
#define	PARAM_FORCE		320
#define PARAM_SKIP_VE_SETUP	321
#define PARAM_CONFIG_SAMPLE	322
#define PARAM_CONFIG_CUSTOMIZE	323
#define PARAM_SETMODE		324

#define PARAM_LOGFILE		325
#define	PARAM_LOGGING		326
#define	PARAM_LOGLEVEL		327
#define	PARAM_DEF_OSTEMPLATE	328
#define	PARAM_LOCKDIR		329

#define PARAM_LINE		"e:p:f:t:i:l:k:a:b:n:x:h"
#endif
