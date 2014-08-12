/*
 *  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
#ifndef _VZCTL_PARAM_H_
#define _VZCTL_PARAM_H_

#define PARAM_IGNORED		-1
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
#define PARAM_DCACHESIZE	'x'

#define PARAM_OSTEMPLATE	13

#define PARAM_CPUWEIGHT		14
#define PARAM_CPULIMIT		15
#define PARAM_CPUUNITS		16
#define PARAM_VCPUS		17
#define PARAM_CPUMASK		171
#define PARAM_NODEMASK		172

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

#define PARAM_SAFE		44
#define PARAM_CAP		45
#define PARAM_CONFIG		47
#define PARAM_SHARED		48
#define PARAM_CFGVER		50
#define PARAM_DEVICES		51
#define PARAM_AVNUMPROC		52
#define PARAM_TEMPLATES		93
#define PARAM_DEVNODES		94
#define	PARAM_SKIPQUOTACHECK	95
#define PARAM_MEMINFO		97

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

#define PARAM_DUMPFILE		330
#define PARAM_CPTCONTEXT	331
#define PARAM_CPU_FLAGS		332
#define PARAM_KILL		333
#define PARAM_RESUME		334
#define PARAM_DUMP		335
#define PARAM_SUSPEND		336
#define PARAM_UNDUMP		337
#define PARAM_DUMPDIR		338
#define PARAM_SKIPARPDETECT	339
#define PARAM_RESET_UB		340
#define PARAM_VEID		343
#define PARAM_NAME		344
#define PARAM_APPLYCONFIG_MAP	345
#define PARAM_WAIT		346
#define PARAM_IPV6NET		347
#define PARAM_VETH_ADD		348

#define PARAM_FEATURES		350
#define PARAM_NETIF_ADD		351
#define PARAM_NETIF_ADD_CMD	352
#define PARAM_NETIF_DEL_CMD	353
#define PARAM_NETIF_MAC		354
#define PARAM_NETIF_IFNAME	355
#define PARAM_NETIF_HOST_MAC	356
#define PARAM_NETIF_HOST_IFNAME	357
#define PARAM_VERBOSE		358
#define PARAM_IOPRIO		359
#define PARAM_NETIF_MAC_FILTER	360
#define PARAM_NETIF_BRIDGE	361
#define PARAM_DESCRIPTION	362
#define PARAM_SWAPPAGES		363
#define PARAM_BOOTORDER		364
#define PARAM_PCI_ADD		365
#define PARAM_PCI_DEL		366
#define PARAM_RAM		367
#define PARAM_SWAP		368
#define PARAM_VE_LAYOUT		369

#define PARAM_SNAPSHOT_GUID	370
#define PARAM_SNAPSHOT_NAME	371
#define PARAM_SNAPSHOT_DESC	372
#define PARAM_SNAPSHOT_SKIP_SUSPEND	373
#define PARAM_SNAPSHOT_SKIP_CONFIG	375
#define PARAM_SNAPSHOT_MOUNT_TARGET	376
#define PARAM_SNAPSHOT_SKIP_RESUME	377
#define PARAM_SNAPSHOT_MUST_RESUME	378

#define PARAM_SKIP_UMOUNT	394
#define PARAM_SKIP_REMOUNT	395
#define PARAM_MOUNT_OPTS	396


#define PARAM_LOCAL_UID		397
#define PARAM_LOCAL_GID		398

#define PARAM_SKIP_FSCK		399
#define PARAM_STOP_TIMEOUT	400

#define PARAM_VM_OVERCOMMIT	401
#define PARAM_IOLIMIT		402
#define PARAM_IOLIMIT_MB	403
#define PARAM_IOPSLIMIT		404

#define PARAM_OFFLINE_RESIZE	422
#define PARAM_NETFILTER		423

#define PARAM_LINE		"e:p:f:t:i:l:k:a:b:n:x:h"
#endif
