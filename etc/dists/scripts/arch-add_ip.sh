#!/bin/bash
#  Copyright (C) 2006-2007 SYSTS.ORG  All rights reserved.
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
# Adds IP address(es) in a container running Archlinux.

VENET_DEV=venet0
CFGFILE=/etc/rc.conf

function remove_all_ve_aliases()
{
	local ve_if_name
	local ve_if

	ve_if_name=`grep "^${VENET_DEV}_" ${CFGFILE}.bak | cut -d'=' -f1`

	for ve_if in ${ve_if_name}; do
	    /etc/rc.d/network ifdown ${ve_if} 2>/dev/null
	    del_param "${CFGFILE}.bak" "${ve_if}"
	    del_param3 "${CFGFILE}.bak" "INTERFACES" "${ve_if}"
	done
}

function setup_network()
{
    # create lo
    if ! grep -qe "^lo=" ${CFGFILE}.bak 2>/dev/null; then
	put_param "${CFGFILE}.bak" "lo" "lo 127.0.0.1"
	add_param3 "${CFGFILE}.bak" "INTERFACES" "lo"
    fi

    # create venet0 and routes
    if ! grep -qe "^${VENET_DEV}=" ${CFGFILE}.bak 2>/dev/null; then
	put_param "${CFGFILE}.bak" "${VENET_DEV}" "${VENET_DEV} 127.0.0.1 netmask 255.255.255.255 broadcast 0.0.0.0"
	add_param3 "${CFGFILE}.bak" "INTERFACES" "${VENET_DEV}"
	put_param "${CFGFILE}.bak" "rt_default" "default dev ${VENET_DEV}"
	add_param3 "${CFGFILE}.bak" "ROUTES" "rt_default"
    fi
}

function create_config()
{
	local ip=$1
	local netmask=$2
	local ifnum=$3
	# add venet0 alias to rc.conf
	put_param "${CFGFILE}.bak" "${VENET_DEV}_${ifnum}" \
		"${VENET_DEV}:${ifnum} ${ip} netmask ${netmask} broadcast 0.0.0.0"

	# add venet0 alias to INTERFACES array
	add_param3 "${CFGFILE}.bak" "INTERFACES" "${VENET_DEV}_${ifnum}"
}

function get_all_aliasid()
{
	IFNUM=-1
	IFNUMLIST=`grep -e "^${VENET_DEV}_.*$" 2> /dev/null ${CFGFILE}.bak |
		   sed "s/.*${VENET_DEV}_//" | cut -d '=' -f 1`
}

function get_free_aliasid()
{
	local found=

	[ -z "${IFNUMLIST}" ] && get_all_aliasid
	while test -z ${found}; do
		let IFNUM=IFNUM+1
		echo "${IFNUMLIST}" | grep -q -E "^${IFNUM}$" 2>/dev/null ||
			found=1
	done
}


function add_ip()
{
	local ipm
	local add
	local iface

	cp -f ${CFGFILE} ${CFGFILE}.bak

	if [ "${IPDELALL}" = "yes" ]; then
		remove_all_ve_aliases
	fi

	setup_network

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if grep -e "\\<${_IP}\\>" >/dev/null 2>&1  ${CFGFILE}.bak; then
			continue
		fi
		get_free_aliasid
		create_config "${_IP}" "${_NETMASK}" "${IFNUM}"
	done

	mv -f ${CFGFILE}.bak ${CFGFILE}
	if [ "x${VE_STATE}" = "xrunning" ]; then
		if [ ! -z ${IFNUM} ]; then
		    /etc/rc.d/network ifup ${VENET_DEV}_${IFNUM} 2>/dev/null
		fi
	fi
}

add_ip

exit 0
