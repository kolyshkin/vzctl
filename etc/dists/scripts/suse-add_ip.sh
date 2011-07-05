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
# Adds IP address(es) in a container running SuSE.

VENET_DEV=venet0
IFCFG_DIR=/etc/sysconfig/network/
IFCFG=${IFCFG_DIR}/ifcfg-${VENET_DEV}
ROUTES=${IFCFG_DIR}/ifroute-${VENET_DEV}
HOSTFILE=/etc/hosts

function get_aliases()
{
	IFNUMLIST=

	[ -f ${IFCFG} ] || return
	IFNUMLIST=`grep -e "^LABEL_" ${IFCFG} | sed 's/^LABEL_\(.*\)=.*/\1/'`
}

# Fix a bug in sles9 ifup-route script
function fix_ifup_route()
{
	local file=/etc/sysconfig/network/scripts/ifup-route
	local str='run_iproute $ACTION to $TYPE $DEST via $GWAY $IFACE $IPOPTS'
	if grep -q "$str" $file; then
		sed -i -e "/$str/s/via \$GWAY/\${GWAY:+via \$GWAY}/" $file
	fi
}

function init_config()
{

	mkdir -p ${IFCFG_DIR}
	echo "STARTMODE=onboot
BOOTPROTO=static
BROADCAST=0.0.0.0
NETMASK=255.255.255.255
IPADDR=127.0.0.1" > ${IFCFG} ||
	error "Can't write to file ${IFCFG}" ${VZ_FS_NO_DISK_SPACE}

	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
	cat << EOF > ${ROUTES}
default - - ${VENET_DEV}
default :: - ${VENET_DEV}
EOF
	fix_ifup_route
}

function create_config()
{
	local ip=$1
	local ifnum=$2

	echo "IPADDR_${ifnum}=${ip}
LABEL_${ifnum}=${ifnum}" >> ${IFCFG} ||
	error "Can't write to file ${IFCFG}" ${VZ_FS_NO_DISK_SPACE}
}

function add_ip()
{
	local ipm
	local ifnum=-1
	local found

	if [ "x${VE_STATE}" = "xstarting" ]; then
		init_config
	elif [ "x${IPDELALL}" = "xyes" ]; then
		init_config
	elif [ ! -f "${IFCFG}" ]; then
		init_config
	fi

	get_aliases
	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		found=
		if grep -q -w "${_IP}" ${IFCFG}; then
			continue
		fi
		while test -z ${found}; do
			let ifnum++
			if ! echo "${IFNUMLIST}" | grep -w -q "${ifnum}"; then
				found=1
			fi
		done
		create_config ${_IP} ${ifnum}
	done
	if [ "x${VE_STATE}" = "xrunning" ]; then
		ifdown $VENET_DEV  >/dev/null 2>&1
		ifup $VENET_DEV  >/dev/null 2>&1
	fi
}

add_ip

exit 0
# end of script
