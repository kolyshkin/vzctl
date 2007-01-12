#!/bin/bash
#  Copyright (C) 2000-2007 SWsoft. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
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
		ifname=`grep -B 1 -w "${ip}" ${CFGFILE} | \
			grep "${VENET_DEV}:" | cut -d' ' -f2`
		if [ -n "${ifname}" ]; then
			ifdown "${ifname}" 2>/dev/null
			remove_debian_interface "${ifname}" ${CFGFILE}
		fi
	done
}

del_ip
exit 0
# end of script
