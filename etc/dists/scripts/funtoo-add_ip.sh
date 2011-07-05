#!/bin/bash
#  Copyright (C) 2010-2011, Parallels, Inc. All rights reserved.
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
# This script configures IP alias(es) inside Funtoo-based CT with
# OpenRC used as services/startup/shutdown system.
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
# Note that modern versions of vzctl runs this script even if there
# are not venet IPs to add to the CT, which resulted in netif.venet0
# being created even when venet wasn't enabled for this container.
# This could indicate an upstream bug.

VENET_DEV=venet0
IFCFG=/etc/conf.d/netif.${VENET_DEV}
SCRIPT=/etc/runlevels/default/netif.${VENET_DEV}

HOSTFILE=/etc/hosts

function comment_line_regex()
{
	cp -pf ${IFCFG} ${IFCFG}.$$ ||
		error "Failed to comment ${1}: unable to copy ${IFCFG}" ${VZ_FS_NO_DISK_SPACE}
	sed -e "s/${1}/#${1}/" < ${IFCFG} > ${IFCFG}.$$ &&
		mv -f ${IFCFG}.$$ ${IFCFG} 2>/dev/null
	if [ $? -ne 0 ]; then
		rm -f ${IFCFG}.$$ 2>/dev/null
		error "Failed to comment ${1}: unable to create ${IFCFG}" ${VZ_FS_NO_DISK_SPACE}
	fi
}

function set_rc()
{
	[ -f "${SCRIPT}" ] && return 0
	ln -sf netif.tmpl /etc/init.d/netif.${VENET_DEV}
	rc-update add netif.${VENET_DEV} default &>/dev/null
}

unset_rc()
{
	# used for disabling venet if we are using veth and no IPs are specified
	rc-update del netif.${VENET_DEV} default &>/dev/null
	rm -f /etc/init.d/netif.${VENET_DEV}
	rm -f /etc/conf.d/netif.${VENET_DEV}
	ip link set ${VENET_DEV} down > /dev/null 2>&1
}

function init_netconfig()
{
	if [ "$IP_ADDR" != "" ]
	then
		set_rc
	else
		unset_rc
	fi
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
			/etc/init.d/netif.${VENET_DEV} stop >/dev/null 2>&1
			return 0
		fi
	fi

	local ips=""
	if [ "${VENET_DEV}" = "venet0" ]
	then
		ips="127.0.0.1/32"
		put_param "${IFCFG}" "route" "default dev venet0"
	fi
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		ips="$ips $_IP/$_MASK"
	done
	put_param "${IFCFG}" "template" "interface"
	put_param "${IFCFG}" "ipaddrs" "$ips"

	if [ "x${VE_STATE}" = "xrunning" ]; then
		# synchronize config files & interfaces
		/etc/init.d/netif.${VENET_DEV} restart >/dev/null 2>&1
	fi
}

add_ip
exit 0
# end of script
