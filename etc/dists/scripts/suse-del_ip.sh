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
# This script deletes IP alias(es) inside VE for SuSE-9.
# For usage info see ve-alias_del(5) man page.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IPs to delete, several addresses should be divided by space
# Optional parameters:
#   VE_STATE      - state of VE; could be one of:
#                     starting | stopping | running | stopped
VENET_DEV=venet0
IFCFG_DIR=/etc/sysconfig/network/
IFCFG="${IFCFG_DIR}/ifcfg-${VENET_DEV}"

function del_ip()
{
	local ip ids id

	for ip in ${IP_ADDR}; do
		ids=`grep -E "^IPADDR_.*=${ip}$" ${IFCFG_DIR}ifcfg-venet0 | sed 's/^IPADDR_\(.*\)=.*/\1/'`
		for id in ${ids}; do
			sed -e "/^IPADDR_${id}=/{
				N
				/$/d
}" < ${IFCFG} > ${IFCFG}.bak && mv -f ${IFCFG}.bak ${IFCFG}
			ifconfig  ${VENET_DEV}:${id} down 2>/dev/null
		done
	done
}

del_ip

exit 0
# end of script
