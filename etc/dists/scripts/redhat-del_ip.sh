#!/bin/bash
#  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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
# Deletes IP address(es) from a container running Red Hat like distro.

VENET_DEV=venet0
VENET_DEV_CFG=ifcfg-${VENET_DEV}
IFCFG_DIR=/etc/sysconfig/network-scripts/
IFCFG=${IFCFG_DIR}${VENET_DEV_CFG}

# Function to delete IP address for RedHat like systems
function del_ip()
{
	local ipm
	local filetodel
	local file
	local aliasid

	[ -d ${IFCFG_DIR} ] || return 0
	cd ${IFCFG_DIR} || return 0
	if [ "x${IPDELALL}" = "xyes" ]; then
		ifdown ${VENET_DEV} >/dev/null 2>&1
		rm -f ${VENET_DEV_CFG} ${VENET_DEV_CFG}:* 2>/dev/null
		del_param ${IFCFG} IPV6ADDR_SECONDARIES ""
		return 0;
	fi
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		# IPV6 processing
		if [ -n "$_IPV6ADDR" ]; then
			del_param ${IFCFG} IPV6ADDR_SECONDARIES "${_IP}\\/${_MASK}"
			ifconfig ${VENET_DEV} del ${_IP}/${_MASK}
			continue
		fi
		# find and delete a file with this alias
		filetodel=`grep -l "IPADDR=${_IP}$" \
			${VENET_DEV_CFG}:* 2>/dev/null`
		for file in ${filetodel}; do
			rm -f "${file}"
			aliasid=`echo ${file} | sed s/.*://g`
			if [ -n "${aliasid}" ]; then
				ifconfig ${VENET_DEV}:${aliasid} down >/dev/null 2>&1
			fi
		done
		# Even if file not found, try to delete IP
		for aliasid in $(ifconfig | grep -B1 "\\<inet addr:${_IP}\\>" |
				awk "/^${VENET_DEV}:/ {print \$1}"); do
			ifconfig ${aliasid} down >/dev/null 2>&1
		done
	done
}

del_ip
exit 0
# end of script
