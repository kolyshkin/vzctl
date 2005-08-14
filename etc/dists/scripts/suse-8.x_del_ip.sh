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
# This script deletes IP alias(es) inside VE for SuSE-8 like systems.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IPs to delete, several addresses should be divided by space
# Optional parameters:
#   IPDELALL      - deleet all ip addresses
VENET_DEV=venet0
VENET_DEV_CFG=ifcfg-${VENET_DEV}
IFCFG_DIR=/etc/sysconfig/network/

function del_ip()
{
	local ip
	local filetodel
	local file
	local aliasid

	[ -d ${IFCFG_DIR} ] || return 0
	cd ${IFCFG_DIR} || return 0
	if [ "x${IPDELALL}" = "xyes" ]; then
		ifdown ${VENET_DEV}
		rm -rf ${VENET_DEV_CFG}:* >/dev/null 2>&1
		ifup ${VENET_DEV}
		return 0;
	fi
	for ip in ${IP_ADDR}; do
		# find and delete a file with this alias
		filetodel=`grep -l "IPADDR=${ip}$" \
			${VENET_DEV_CFG}:* 2>/dev/null`
		for file in ${filetodel}; do
			rm -f "${file}"
			aliasid=`echo ${file} | sed s/.*://g`
			if [ -n "${aliasid}" ]; then
				ifconfig  ${VENET_DEV}:${aliasid} down >/dev/null 2>&1
			fi
		done
	done
}

del_ip
exit 0
# end of script
