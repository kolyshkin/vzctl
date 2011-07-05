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
# This script deletes IP alias(es) inside CT for Gentoo like distros.
# For usage info see ve-alias_del(5) man page.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IPs to delete, several addresses should be divided by space
# Optional parameters:
#   VE_STATE      - state of CT; could be one of:
#                     starting | stopping | running | stopped
VENET_DEV=venet0
CFGFILE=/etc/conf.d/net

# Return true if we have old baselayout-1.x based CT and false if not.
# Note: /etc/gentoo-release has nothing to do with init system
function is_baselayout1()
{
	[ -f /sbin/functions.sh ]
}

function del_ip()
{
	local ipm
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if grep -qw "${_IP}/${_MASK}" ${CFGFILE}; then
			if ! is_baselayout1 ; then
				del_param "${CFGFILE}" "config_${VENET_DEV}" "${_IP}/${_MASK}"
			else
				del_param3 "${CFGFILE}" "config_${VENET_DEV}" "${_IP}/${_MASK}"
			fi
			if [ "x${VE_STATE}" = "xrunning" ]; then
				/etc/init.d/net.${VENET_DEV} restart >/dev/null 2>&1
			fi
		fi
	done
}

del_ip
exit 0
# end of script
