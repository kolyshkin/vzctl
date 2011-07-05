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
# This script configures IP alias(es) inside Gentoo based CT with
# baselayout-1/openrc used as services/startup/shutdown system.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IP address(es) to add
#                   (several addresses should be divided by space)
# Optional parameters:
#   VE_STATE      - state of CT; could be one of:
#                     starting | stopping | running | stopped
#   IPDELALL	  - delete all old interfaces
#
VENET_DEV=venet0
IFCFG=/etc/conf.d/net
SCRIPT=/etc/runlevels/default/net.${VENET_DEV}

HOSTFILE=/etc/hosts

# Return true if we have old baselayout-1.x based CT and false if not.
# Note: /etc/gentoo-release has nothing to do with init system
function is_baselayout1()
{
	[ -f /sbin/functions.sh ]
}

function comment_line_regex()
{
	cp -pf ${IFCFG} ${IFCFG}.$$ ||
		error "Failed to comment ${1}: unable to copy ${IFCFG}" ${VZ_FS_NO_DISK_SPACE}
	sed -e "s/${1}/#${1}/" < ${IFCFG} > ${IFCFG}.$$ &&
		mv -f ${IFCFG}.$$ ${IFCFG} 2>/dev/null
	if [ $? -ne 0 ]; then
		rm -f ${IFCFG}.$$ 2>/dev/null
		error "Failed to comment ${1}: unable to create ${IFCFG}."
	fi
}

function set_config()
{
	if ! grep -qe "^config_eth" ${IFCFG} 2>/dev/null; then
		comment_line_regex "^config_eth"
		comment_line_regex "^routes_eth"
	fi
	if ! is_baselayout1 ; then
		put_param ${IFCFG} "config_${VENET_DEV}" ""
		put_param ${IFCFG} "routes_${VENET_DEV}" "default"
	else
		put_param3 ${IFCFG} "config_${VENET_DEV}" ""
		add_param3 ${IFCFG} "routes_${VENET_DEV}" "default"
	fi
}

function set_rc()
{
	[ -f "${SCRIPT}" ] && return 0
	rc-update del net.eth0 &>/dev/null
	rc-update add net.lo boot &>/dev/null
	ln -sf /etc/init.d/net.lo /etc/init.d/net.${VENET_DEV}
	rc-update add net.${VENET_DEV} default &>/dev/null
}

function init_netconfig()
{
	set_rc
	set_config
	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
}

function add_ip()
{
	local ipm
	if [ "x${VE_STATE}" = "xstarting" -o "x${IPDELALL}" = "xyes" ]; then
		init_netconfig
		if [ "x${IPDELALL}" = "xyes" ]; then
			/etc/init.d/net.${VENET_DEV} stop >/dev/null 2>&1
			return 0
		fi
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if ! grep -qw "config_${VENET_DEV}=\(.*\"${_IP}[\"\/].*\)" ${IFCFG}; then
			if ! is_baselayout1 ; then
				add_param "${IFCFG}" "config_${VENET_DEV}" "${_IP}/${_MASK}"
			else
				add_param3 "${IFCFG}" "config_${VENET_DEV}" "${_IP}/${_MASK}"
			fi
		fi
	done

	if [ "x${VE_STATE}" = "xrunning" ]; then
		# synchronyze config files & interfaces
		/etc/init.d/net.${VENET_DEV} restart >/dev/null 2>&1
	fi
}

add_ip
exit 0
# end of script
