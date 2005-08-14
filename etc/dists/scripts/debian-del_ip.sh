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
# This script deletes IP alias(es) inside VE for Debian like distros.
# For usage info see ve-alias_del(5) man page.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IPs to delete, several addresses should be divided by space
# Optional parameters:
#   VE_STATE      - state of VE; could be one of:
#                     starting | stopping | running | stopped
VENET_DEV=venet0
CFGFILE=/etc/network/interfaces

# Function to delete IP address for Debian template
function del_ip()
{
	local ifname
	local ip

	for ip in ${IP_ADDR}; do
		ifname=`grep -B 1 -e "\\<${ip}\\>" ${CFGFILE} | \
			grep "${VENET_DEV}:" | cut -d' ' -f2`
		if [ -n "${ifname}" ]; then
			ifdown ${ifname}
			echo "/\\<${ip}\\>
-2,+2d
wq" | ed ${CFGFILE} >/dev/null 2>&1 
		fi
	done
}

del_ip
exit 0
# end of script
