#!/bin/sh
#  Copyright (C) 2000-2011, Parallels, Inc. All rights reserved.
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
# Deletes IP address(es) from a container running Debian-like distros.

VENET_DEV=venet0
CFGFILE=/etc/network/interfaces

# Function to delete IP address for Debian template
del_ip()
{
	local ifname
	local ipm

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if [ -z "$_IPV6ADDR" ]; then
		    ifname=`grep -B 1 -w "${_IP}" ${CFGFILE} |
				grep "${VENET_DEV}:" | cut -d' ' -f2`
		    if [ -n "${ifname}" ]; then
				ifdown "${ifname}" 2>/dev/null
				remove_debian_interface "${ifname}" ${CFGFILE}
		    fi

		else
		    grep -v ${_IP} ${CFGFILE} > ${CFGFILE}.bak
		    ifconfig ${VENET_DEV} del ${_IP}/${_MASK}
		    mv ${CFGFILE}.bak ${CFGFILE}
		fi
	done
}

del_ip

exit 0
