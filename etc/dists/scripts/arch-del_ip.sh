#!/bin/bash
#  Copyright (C) 2006-2007  SYSTS.ORG All rights reserved.
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
# Deletes IP address(es) from a container running Archlinux.

VENET_DEV=venet0
CFGFILE=/etc/rc.conf

function remove_all_ve_aliases()
{
	local ve_if_name
	local ve_if

	ve_if_name=`grep "^${VENET_DEV}_" ${CFGFILE} | cut -d'=' -f1`

	for ve_if in ${ve_if_name}; do
	    /etc/rc.d/network ifdown ${ve_if} 2>/dev/null
	    del_param "${CFGFILE}" "${ve_if}"
	    del_param3 "${CFGFILE}" "INTERFACES" "${ve_if}"
	done
}

function del_ip()
{
	local ifname
	local ipm

	if [ "x${IPDELALL}" = "xyes" ]; then
		remove_all_ve_aliases
		return 0
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		ifname=`grep -B 1 -e "\\<${_IP}\\>" ${CFGFILE} |
			grep "${VENET_DEV}_" | cut -d'=' -f1`
		if [ -n "${ifname}" ]; then
		    # shutdown interface venet0_x
		    /etc/rc.d/network ifdown ${ifname} 2> /dev/null

		    # remove venet0_x from cfg
		    del_param "${CFGFILE}" "${ifname}"

		    # del venet0_x from INTERFACES array
		    del_param3 "${CFGFILE}" "INTERFACES" "${ifname}"
		fi
	done
}

del_ip

exit 0
