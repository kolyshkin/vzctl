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
# Adds IP address in a container running Slackware.
# Slackware does not support IP aliases so multiple IP addresses not allowed

VENET_DEV=venet0
IFCFG_DIR=/etc/rc.d
IFCFG=${IFCFG_DIR}/rc.inet1.conf
HOSTFILE=/etc/hosts

function fix_rcinet1()
{
	local file="${IFCFG_DIR}/rc.inet1"

	[ -f "${file}" ] || return 0
	cp -fp ${file} ${file}.$$ || error "Can't copy file ${file}" ${VZ_FS_NO_DISK_SPACE}
	sed -e 's/eth/venet/g' -e 's/^[\ \t]*\/sbin\/route add default gw .*/\t\/sbin\/route add -net '${FAKEGATEWAYNET}'\/24 dev venet0; \/sbin\/route add default gw \${GATEWAY} dev venet0/' < ${file} > ${file}.$$ && mv -f ${file}.$$ ${file}
	rm -f ${file}.$$ >/dev/null 2>&1
}

function setup_network()
{
	mkdir -p ${IFCFG_DIR}
	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
	fix_rcinet1
}

function create_config()
{
	local ip=${1}
	local netmask=${2}

	echo "# /etc/rc.d/rc.inet1.conf
#
# This file contains the configuration settings for network interfaces.

IPADDR[0]=\"${ip}\"
NETMASK[0]=\"${netmask}\"

# Default gateway IP address:
GATEWAY=\"${FAKEGATEWAY}\"
" > ${IFCFG}
}

function add_ip()
{
	# In case we are starting CT
	if [ "x${VE_STATE}" = "xstarting" ]; then
		setup_network
	fi
	local ipm
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if [ "${IPDELALL}" != "yes" ]; then
			if grep -qw "${_IP}" ${IFCFG} 2>/dev/null; then
				break
			fi
		fi
		${IFCFG_DIR}/rc.inet1 stop >/dev/null 2>&1
		create_config ${_IP} ${_NETMASK}
		${IFCFG_DIR}/rc.inet1 start >/dev/null 2>&1
		break
	done
}

add_ip
exit 0
# end of script
