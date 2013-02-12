/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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
#ifndef _VZERROR_H_
#define _VZERROR_H_

/*****************************
    System errors
******************************/
#define VZ_SETUBC_ERROR			1
#define VZ_SETFSHD_ERROR		2
#define VZ_SYSTEM_ERROR			3

/* not a VZ-capable kernel */
#define VZ_BAD_KERNEL			5
#define VZ_RESOURCE_ERROR		6
#define VZ_ENVCREATE_ERROR		7

#define VZ_COMMAND_EXECUTION_ERROR	8
#define VZ_LOCKED			9
/* global config file not found */
#define VZ_NOCONFIG			10
#define VZ_NOSCRIPT			11
#define VZ_NO_ACCES			12
#define VZ_SET_CAP			13
#define VZ_NOVECONFIG			14
#define VZ_EXEC_TIMEOUT			15
#define VZ_CHKPNT_ERROR			16
#define VZ_RESTORE_ERROR		17
#define	VZ_SETLUID_ERROR		18

/****************************
    Argument errors
 ****************************/
#define VZ_INVALID_PARAMETER_SYNTAX	20
#define VZ_INVALID_PARAMETER_VALUE	21
#define VZ_VE_ROOT_NOTSET		22
#define VZ_VE_PRIVATE_NOTSET		23
#define VZ_VE_TMPL_NOTSET		24
#define VZ_NOTENOUGHPARAMS		27
#define VZ_NOTENOUGHUBCPARAMS		28
#define VZ_VE_OSTEMPLATE_NOTSET		29
/*****************************
    CT errors
 *****************************/
#define VZ_VE_NOT_RUNNING		31
#define VZ_VE_RUNNING			32
#define VZ_STOP_ERROR			33
#define VZ_CANT_ADDIP			34
#define VZ_VALIDATE_ERROR		35
#define VZ_OVERCOMMIT_ERROR		36

/****************************
    Filesystem errros
 ****************************/
/* private area is not mounted */
#define VZ_FS_NOT_MOUNTED		40
/* private area is already mounted */
#define VZ_FS_MOUNTED			41
/* no private area with this id */
#define VZ_FS_NOPRVT			43
/* private area with this id already exists */
#define VZ_FS_PRVT_AREA_EXIST		44
#define VZ_FS_NO_DISK_SPACE		46
/* template private area is not properly created */
#define VZ_FS_BAD_TMPL			47
/* cannot create new private area */
#define VZ_FS_NEW_VE_PRVT		48
/*  cannot create mounpoint */
#define VZ_FS_MPOINTCREATE		49
/* cannot mount ve private area */
#define VZ_FS_CANTMOUNT			50
/* cannot umount ve private area */
#define VZ_FS_CANTUMOUNT		51
/*  error deleting ve private area */
#define VZ_FS_DEL_PRVT			52
/* private area doesn't exist */
#define VZ_UNK_MOUNT_TYPE		53
#define VZ_CANT_CREATE_DIR		54
#define VZ_NOTENOUGH_QUOTA_LIMITS	55
/*********************************
   Disk quota errors
 *********************************/
/* disk quota not supported */
#define VZ_DQ_ON			60
#define VZ_DQ_INIT			61
#define VZ_DQ_SET			62
#define VZ_DISKSPACE_NOT_SET		63
#define VZ_DISKINODES_NOT_SET		64
#define VZ_ERROR_SET_USER_QUOTA		65
#define VZ_DQ_OFF			66
#define VZ_DQ_UGID_NOTINITIALIZED	67
#define VZ_GET_QUOTA_USAGE		68
/* add some more codes here */

/*********************************
   "vzctl set" errors
 *********************************/
/* incorrect hostname */
#define VZ_BADHOSTNAME			70
/* incorrect ip address */
#define VZ_BADIP			71
/* incorrect dns nameserver address */
#define VZ_BADDNSSRV			72
/* incorrect dns domain name */
#define VZ_BADDNSSEARCH			73
/* error changing password */
#define VZ_CHANGEPASS			74
#define VZ_VE_LCKDIR_NOTSET		77
#define VZ_IP_INUSE			78
#define VZ_ACTIONSCRIPT_ERROR		79
#define VZ_CP_CONFIG			82
#define VZ_INVALID_CONFIG		85
#define VZ_SET_DEVICES			86
#define VZ_INSTALL_APPS_ERROR		87
#define	VZ_IP_NA			89

/* Template Error */
#define VZ_OSTEMPLATE_NOT_FOUND		91

/* ploop might not be supported by kernel or vzctl */
#define VZ_PLOOP_UNSUP			99

#define VZ_GET_IP_ERROR			100
#define VZ_NETDEV_ERROR			104
#define VZ_VE_START_DISABLED		105
#define VZ_SET_IPTABLES			106
#define VZ_NO_DISTR_CONF		107
#define	VZ_NO_DISTR_ACTION_SCRIPT	108
#define VZ_APPLY_CONFIG_ERROR		109
#define VZ_CUSTOM_REINSTALL_ERROR	128
#define VZ_SET_MEMINFO_ERROR		129
#define VZ_VETH_ERROR			130
#define VZ_SET_NAME_ERROR		131
#define VZ_WAIT_FAILED			133

#define VZ_CONFIG_SAVE_ERROR		139

#define VZ_SET_OSRELEASE		144
#define VZ_SET_IO			148

/* ploop */
#define VZCTL_E_CREATE_IMAGE		151
#define VZCTL_E_MOUNT_IMAGE		152
#define VZCTL_E_UMOUNT_IMAGE		153
#define VZCTL_E_RESIZE_IMAGE		154
#define VZCTL_E_CONVERT_IMAGE		155
#define VZCTL_E_CREATE_SNAPSHOT		156
#define VZCTL_E_MERGE_SNAPSHOT		157
#define VZCTL_E_DELETE_SNAPSHOT		158
#define VZCTL_E_SWITCH_SNAPSHOT		159
#define VZCTL_E_MOUNT_SNAPSHOT		160
#define VZCTL_E_COMPACT_IMAGE		161
#define VZCTL_E_LIST_SNAPSHOT		162

#endif /* _VZ_ERROR_H_ */
