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
# Deletes IP address(es) from a container running SuSE.

VENET_DEV=venet0
IFCFG_DIR=/etc/sysconfig/network/
IFCFG="${IFCFG_DIR}/ifcfg-${VENET_DEV}"

function del_ip()
{
	local ipm ids id

	if [ "x${IPDELALL}" = "xyes" ]; then
		ifdown ${VENET_DEV} 2>/dev/null
		rm -f ${IFCFG} 2>/dev/null
		return
	fi
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		ids=`grep -E "^IPADDR_.*=${_IP}$" ${IFCFG} 2>/dev/null |
			sed 's/^IPADDR_\(.*\)=.*/\1/'`
		for id in ${ids}; do
			sed -e "/^IPADDR_${id}=/{
				N
				/$/d
}" < ${IFCFG} > ${IFCFG}.bak && mv -f ${IFCFG}.bak ${IFCFG}
			ifconfig ${VENET_DEV}:${id} down 2>/dev/null
		done
	done
}

del_ip

exit 0
# end of script
