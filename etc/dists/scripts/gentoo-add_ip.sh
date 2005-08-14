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
	if [ ! -f "${SCRIPT}" ]; then
		rm -f /etc/runlevels/default/net.eth0 2>/dev/null
		mv -f /etc/init.d/net.eth0 /etc/init.d/net.${VENET_DEV}
		ln -sf /etc/init.d/net.${VENET_DEV} ${SCRIPT} 2>/dev/null
	fi
	if ! grep -qe "^iface_eth" ${IFCFG} 2>/dev/null; then
		return 0
	fi
	cp -pf ${IFCFG} ${IFCFG}.$$ || error "Unable to copy ${IFCFG}"
	sed -e 's/^iface_eth/#iface_eth/' -e 's/^alias_eth/#alias_eth/' < ${IFCFG} > ${IFCFG}.$$ && mv -f ${IFCFG}.$$ ${IFCFG} 2>/dev/null
	if [ $? -ne 0 ]; then
		rm -f ${IFCFG}.$$ 2>/dev/null
		error "Unable to create ${IFCFG}"
	fi
}

function setup_network()
{
	fix_net
	# Set up venet0 main interface as 127.0.0.1
	put_param ${IFCFG} "iface_${VENET_DEV}" "127.0.0.1 broadcast 0.0.0.0 netmask 255.255.255.255"
	put_param ${IFCFG} "alias_${VENET_DEV}" ""
	put_param ${IFCFG} "routes_${VENET_DEV}" "-net ${FAKEGATEWAYNET}/24 dev ${VENET_DEV}"
	put_param ${IFCFG} "gateway" "${VENET_DEV}/${FAKEGATEWAY}"
	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
}

function add_alias()
{
	local ip="${1}"

	cp -fp ${IFCFG} ${IFCFG}.$$ || error "unable to copy ${IFCFG}"
	sed -e "s/^alias_${VENET_DEV}=\"\\(.*\\)\"/alias_${VENET_DEV}=\"\\1 $ip\"/" < ${IFCFG} > ${IFCFG}.$$ &&
		mv -f ${IFCFG}.$$ ${IFCFG}
	if [ $? -ne 0 ]; then
		rm -f ${IFCFG}.$$
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
		put_param ${IFCFG} "alias_${VENET_DEV}" "${IP_ADDR}"
	else
		new_ips=
		for ip in ${IP_ADDR}; do
			if grep -qw "${ip}" ${IFCFG} 2>/dev/null; then
				continue
			fi
			if [ -n "${new_ips}" ]; then
				new_ips="${new_ips} ${ip}"
			else
				new_ips="${ip}"
			fi
		done
		[ -n "${new_ips}" ] && add_alias "${new_ips}"
	fi
	if [ "x${VE_STATE}" = "xrunning" ]; then
		# synchronyze config files & interfaces
		/etc/init.d/net.${VENET_DEV} restart 
	fi
}

add_ip
exit 0
# end of script
