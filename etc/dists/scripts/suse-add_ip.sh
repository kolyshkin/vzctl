#!/bin/bash
# Copyright (C) 2000-2005 SWsoft. All rights reserved.
#
# This file may be distributed under the terms of the Q Public License
# as defined by Trolltech AS of Norway and appearing in the file
# LICENSE.QPL included in the packaging of this file.
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# This script configure IP alias(es) inside VE for SuSE-9
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IP address(es) to add
#                   (several addresses should be divided by space)
# Optional parameters:
#   VE_STATE      - state of VE; could be one of:
#                     starting | stopping | running | stopped
#
# this should set up networking for SuSE-based VE
VENET_DEV=venet0
FAKEGATEWAYNET=191.255.255.0
FAKEGATEWAY=191.255.255.1
IFCFG_DIR=/etc/sysconfig/network/
IFCFG=${IFCFG_DIR}/ifcfg-${VENET_DEV}
ROUTES=${IFCFG_DIR}/ifroute-${VENET_DEV}
HOSTFILE=/etc/hosts

function get_aliases()
{
	IFNUMLIST=

	[ -f ${IFCFG} ] || return
	IFNUMLIST=`grep -e "^LABEL_" ${IFCFG} | sed 's/^LABEL_\(.*\)=.*/\1/'`
}

function init()
{

	mkdir -p ${IFCFG_DIR}
	echo "STARTMODE=onboot
BOOTPROTO=static
BROADCAST=0.0.0.0
NETMASK=255.255.255.255
IPADDR=127.0.0.1" > ${IFCFG} || \
	error "Can't write to file ${IFCFG_DIR}/${VENET_DEV_CFG}" ${VZ_FS_NO_DISK_SPACE}

	echo "${FAKEGATEWAYNET}	0.0.0.0 255.255.255.0	${VENET_DEV}	
default	${FAKEGATEWAY}	0.0.0.0	${VENET_DEV}" > ${ROUTES} || \
	error "Can't write to file $IFCFG" $VZ_FS_NO_DISK_SPACE
	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
}

function create_config()
{
	local ip=$1
	local ifnum=$2

	echo "IPADDR_${ifnum}=${ip}
LABEL_${ifnum}=${ifnum}" >> ${IFCFG} || \
	error "Can't write to file ${IFCFG_DIR}/${VENET_DEV_CFG}" ${VZ_FS_NO_DISK_SPACE}
}

function add_ip()
{
	local ip
	local ifnum=-1

	if [ "x${IPDELALL}" = "xyes" -o "x${VE_STATE}" = "xstarting" ]; then
		init
	fi
	get_aliases
	for ip in ${IP_ADDR}; do
		found=
		if grep -q -w "${ip}" ${IFCFG}; then
			continue
		fi
		while test -z ${found}; do
			let ifnum++
			if ! echo "${IFNUMLIST}" | grep -w -q "${ifnum}"; then
				found=1
			fi
		done
		create_config ${ip} ${ifnum}
	done
	if [ "x${VE_STATE}" = "xrunning" ]; then
		ifdown $VENET_DEV  >/dev/null 2>&1
		ifup $VENET_DEV  >/dev/null 2>&1
	fi
}

add_ip

exit 0
# end of script
