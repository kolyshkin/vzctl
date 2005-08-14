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
# This script deletes IP address inside VE for Slackware like systems.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IPs to delete, several addresses should be divided by space
# Optional parameters:
#   IPDELALL      - deleet all ip addresses
IFCFG=/etc/rc.d/rc.inet1.conf

function del_ip()
{
	local ip

	[ -f ${IFCFG}  ] || return 0
	for ip in ${IP_ADDR}; do
		if grep -wq "${ip}" ${IFCFG} 2>/dev/null; then
			/etc/rc.d/rc.inet1 stop
			break
		fi
	done
}

del_ip
exit 0
# end of script
