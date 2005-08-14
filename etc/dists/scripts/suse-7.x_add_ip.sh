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
# This script configure IP alias(es) inside VE for SuSE-8 like distros.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IP address(es) to add
#                   (several addresses should be divided by space)
# Optional parameters:
#   VE_STATE      - state of VE; could be one of:
#                     starting | stopping | running | stopped
VENET_DEV=venet0
FAKEGATEWAY=191.255.255.1
FAKEGATEWAYNET=191.255.255.0
CFGFILE=/etc/rc.config

function setup_network()
{
	NETCONFIG_ID=0
	[ -f $CFGFILE ] || return 0
	# Set up venet0 interface config file inside VE
	put_param "$CFGFILE" NETCONFIG "_${NETCONFIG_ID}"
	put_param "$CFGFILE" IPADDR_0 "127.0.0.1"
	put_param "$CFGFILE" NETDEV_0 venet0
	put_param "$CFGFILE" IFCONFIG_0 "127.0.0.1 netmask 255.255.255.255 up"
	put_param "$CFGFILE" IP_FORWARD yes

	# set default routes
	routefile=/etc/route.conf
	if ! grep -qE "127\.0\.0\.1.*255\.255\.255\.255[ 	]*lo" \
		$routefile 2>/dev/null
	then
		echo "127.0.0.1 *     255.255.255.255 lo" >> $routefile
	fi
	if ! grep -qE "default.*0\.0\.0\.0[ 	]*venet0" \
		$routefile 2>/dev/null
	then
		echo "default *       0.0.0.0 venet0" >> $routefile
	fi
}

function get_net_ids()
{
	
	NETCONFIG_ID=`grep -E "^NETCONFIG=" ${CFGFILE} | head -n1 | sed  -e 's/NETCONFIG=\(.*\)$/\1/' -e 's/["_]//g'`
	[ -z "${NETCONFIG_ID}" ] && NETCONFIG_ID=0 
}

function add_ip()
{
	local ip netconfig aliasid

	# In case we are starting VE
	if [ "x$VE_STATE" = "xstarting" -o "${IPDELALL}" = "yes" ]; then
		setup_network
	else
		get_net_ids
	fi
	# create NETCONFIG, IPADDR_x, NETDEV_x and IFCONFIG_x strings
	let IP_NUM=0
	# Get last id
	for id in ${NETCONFIG_ID}; do
		netconfig="${netconfig} _${id}"
		IP_NUM=${id}
	done
	# create appropriate records for each given IP addr
	for ip in ${IP_ADDR}; do
		aliasid=${IP_NUM}
		let IP_NUM=IP_NUM+1
		# build 'IPADDR_x' records
		put_param ${CFGFILE} IPADDR_${IP_NUM} "${ip}"
		# build 'NETDEV_x' records
		put_param ${CFGFILE} NETDEV_${IP_NUM} "venet0:${aliasid}"
		# build 'IFCONFIG_x' records
		put_param ${CFGFILE} IFCONFIG_${IP_NUM} "${ip} up"
		netconfig="${netconfig} _${IP_NUM}"
	done
	put_param ${CFGFILE} NETCONFIG "${netconfig}"
	if [ "x$VE_STATE" = "xrunning" ]; then
		/etc/init.d/network restart
	fi
	chmod 644 $CFGFILE
}

add_ip

exit 0
# end of script
