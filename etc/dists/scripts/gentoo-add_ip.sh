#!/bin/bash
# Copyright (C) 2000-2005 SWsoft. All rights reserved.
#
# This file may be distributed under the terms of the Q Public License
# as defined by Trolltech AS of Norway and appearing in the file
# LICENSE.QPL included in the packaging of this file.
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# This script configure IP alias(es) inside Gentoo like VE.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IP address(es) to add
#                   (several addresses should be divided by space)
# Optional parameters:
#   VE_STATE      - state of VE; could be one of:
#                     starting | stopping | running | stopped
#   IPDELALL	  - delete all old interfaces
#
VENET_DEV=venet0

FAKEGATEWAY=191.255.255.1
FAKEGATEWAYNET=191.255.255.0

IFCFG_DIR=/etc/conf.d
IFCFG=${IFCFG_DIR}/net

SCRIPT=/etc/runlevels/default/net.${VENET_DEV}

HOSTFILE=/etc/hosts

function fix_net()
{
	[ -f "${SCRIPT}" ] && return 0
	rc-update del net.eth0 &>/dev/null
	ln -sf /etc/init.d/net.lo /etc/init.d/net.${VENET_DEV}
	rc-update add net.lo boot &>/dev/null
	rc-update add net.venet0 default &>/dev/null
	if ! grep -qe "^config_eth" ${IFCFG} 2>/dev/null; then
		return 0
	fi
	cp -pf ${IFCFG} ${IFCFG}.$$ || error "Unable to copy ${IFCFG}"
	sed -e 's/^config_eth/#config_eth/' -e 's/^routes_eth/#routes_eth/' < ${IFCFG} > ${IFCFG}.$$ && mv -f ${IFCFG}.$$ ${IFCFG} 2>/dev/null
	if [ $? -ne 0 ]; then
		rm -f ${IFCFG}.$$ 2>/dev/null
		error "Unable to create ${IFCFG}"
	fi
}

function setup_network()
{
	fix_net
	put_param3 ${IFCFG} "config_${VENET_DEV}" ""
	# add fake route
	put_param3 ${IFCFG} "routes_${VENET_DEV}" "-net ${FAKEGATEWAYNET}/24"
	add_param3 ${IFCFG} "routes_${VENET_DEV}" "default via ${FAKEGATEWAY}"
	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
}

function add_ip()
{
	local ip
	local new_ips

	# In case we are starting VE
	if [ "x${VE_STATE}" = "xstarting" ]; then
		setup_network
	fi

	if [ "x${IPDELALL}" = "xyes" ]; then
		put_param3 "${IFCFG}" "config_${VENET_DEV}" ""
		shift
	fi

	for ip in ${IP_ADDR}; do
		grep -qw "${ip}" ${IFCFG} || \
			add_param3 "${IFCFG}" "config_${VENET_DEV}" "${ip}/32"
	done

	if [ "x${VE_STATE}" = "xrunning" ]; then
		# synchronyze config files & interfaces
		/etc/init.d/net.${VENET_DEV} restart 
	fi
}

add_ip
exit 0
# end of script
