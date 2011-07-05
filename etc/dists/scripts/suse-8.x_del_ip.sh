#!/bin/bash
#  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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
# Deletes IP address(es) from a container running SuSE 8.
VENET_DEV=venet0
VENET_DEV_CFG=ifcfg-${VENET_DEV}
IFCFG_DIR=/etc/sysconfig/network/

function del_ip()
{
	local ipm
	local filetodel
	local file
	local aliasid

	[ -d ${IFCFG_DIR} ] || return 0
	cd ${IFCFG_DIR} || return 0
	if [ "x${IPDELALL}" = "xyes" ]; then
		ifdown ${VENET_DEV}
		rm -f ${VENET_DEV_CFG} ${VENET_DEV_CFG}:* >/dev/null 2>&1
		return 0;
	fi
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		# find and delete a file with this alias
		filetodel=`grep -l "IPADDR=${_IP}$" \
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
