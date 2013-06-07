#!/bin/bash
#  Copyright (C) 2006-2007 SYSTS.ORG  All rights reserved.
#  Copyright (C) 2012 Brinstar Networks  All rights reserved.
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
# Adds IP address(es) in a container running Arch Linux.

VENET_DEV=venet0
OLDCFGFILE=/etc/rc.conf

CFGPATH=/etc/network.d
NETCFG=/etc/conf.d/netcfg

CTLPATH=/etc/netctl

function old_remove_all_ve_aliases()
{
	local ve_if_name
	local ve_if

	ve_if_name=`grep "^${VENET_DEV}_" ${OLDCFGFILE}.bak | cut -d'=' -f1`

	for ve_if in ${ve_if_name}; do
	    /etc/rc.d/network ifdown ${ve_if} 2>/dev/null
	    del_param "${OLDCFGFILE}.bak" "${ve_if}"
	    del_param3 "${OLDCFGFILE}.bak" "INTERFACES" "${ve_if}"
	done
}

function old_setup_network()
{
    # create lo
    if ! grep -qe "^lo=" ${OLDCFGFILE}.bak 2>/dev/null; then
	put_param "${OLDCFGFILE}.bak" "lo" "lo 127.0.0.1"
	add_param3 "${OLDCFGFILE}.bak" "INTERFACES" "lo"
    fi

    # create venet0 and routes
    if ! grep -qe "^${VENET_DEV}=" ${OLDCFGFILE}.bak 2>/dev/null; then
	put_param "${OLDCFGFILE}.bak" "${VENET_DEV}" "${VENET_DEV} 127.0.0.1 netmask 255.255.255.255 broadcast 0.0.0.0"
	add_param3 "${OLDCFGFILE}.bak" "INTERFACES" "${VENET_DEV}"
	put_param "${OLDCFGFILE}.bak" "rt_default" "default dev ${VENET_DEV}"
	add_param3 "${OLDCFGFILE}.bak" "ROUTES" "rt_default"
    fi
}

function old_create_config()
{
	local ip=$1
	local netmask=$2
	local ifnum=$3
	# add venet0 alias to rc.conf
	put_param "${OLDCFGFILE}.bak" "${VENET_DEV}_${ifnum}" \
		"${VENET_DEV}:${ifnum} ${ip} netmask ${netmask} broadcast 0.0.0.0"

	# add venet0 alias to INTERFACES array
	add_param3 "${OLDCFGFILE}.bak" "INTERFACES" "${VENET_DEV}_${ifnum}"
}

function old_get_all_aliasid()
{
	IFNUM=-1
	IFNUMLIST=`grep -e "^${VENET_DEV}_.*$" 2> /dev/null ${OLDCFGFILE}.bak |
		   sed "s/.*${VENET_DEV}_//" | cut -d '=' -f 1`
}

function old_get_free_aliasid()
{
	local found=

	[ -z "${IFNUMLIST}" ] && old_get_all_aliasid
	while test -z ${found}; do
		let IFNUM=IFNUM+1
		echo "${IFNUMLIST}" | grep -q -E "^${IFNUM}$" 2>/dev/null ||
			found=1
	done
}


function old_add_ip()
{
	local ipm
	local add
	local iface

	cp -f ${OLDCFGFILE} ${OLDCFGFILE}.bak

	if [ "${IPDELALL}" = "yes" ]; then
		old_remove_all_ve_aliases
	fi

	old_setup_network

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if grep -e "\\<${_IP}\\>" >/dev/null 2>&1  ${OLDCFGFILE}.bak; then
			continue
		fi
		old_get_free_aliasid
		old_create_config "${_IP}" "${_NETMASK}" "${IFNUM}"
	done

	mv -f ${OLDCFGFILE}.bak ${OLDCFGFILE}
	if [ "x${VE_STATE}" = "xrunning" ]; then
		if [ ! -z ${IFNUM} ]; then
		    /etc/rc.d/network ifup ${VENET_DEV}_${IFNUM} 2>/dev/null
		fi
	fi
}

function remove_all_ve_aliases()
{
	local ve_if
	for ve_if in $(ls -1 ${CFGPATH}/${VENET_DEV}_* 2> /dev/null | sed "s/.*${VENET_DEV}_//"); do
		ip link set "${VENET_DEV}:${ve_if}" down
		rm -f "${CFGPATH}/${VENET_DEV}_${ve_if}"
	done

	# Remove NETWORKS line in $NETCFG
	cp "$NETCFG" "${NETCFG}.bak"
	grep -v NETWORKS= "${NETCFG}.bak" > "$NETCFG"
	echo 'NETWORKS=()' >> "$NETCFG"
}

function setup_network()
{
	echo "CONNECTION='ethernet'
DESCRIPTION='${VENET_DEV}'
INTERFACE='${VENET_DEV}'
IP='static'" > "${CFGPATH}/${VENET_DEV}" || error "Could not write ${CFGPATH}/${VENET_DEV}" $VZ_FS_NO_DISK_SPACE
	if [ ! -d /etc/systemd ]; then
		echo "ADDR='127.0.0.2'
NETMASK='32'
GATEWAY='0.0.0.0'" >> "${CFGPATH}/${VENET_DEV}" || error "Could not write ${CFGPATH}/${VENET_DEV}" $VZ_FS_NO_DISK_SPACE
	else
		add_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "addr add 127.0.0.2/32 broadcast 0.0.0.0 dev ${VENET_DEV}"
		add_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "route add default dev ${VENET_DEV}"
	fi

	add_param3 "${NETCFG}" "NETWORKS" "${VENET_DEV}"

	if [ ! -f "/etc/hosts" ] ; then
		echo "127.0.0.1 localhost.localdomain localhost" > /etc/hosts || error "Could not write /etc/hosts" $VZ_FS_NO_DISK_SPACE
	fi
}

function create_config()
{
	local ip=$1
	local netmask=$2
	local ifnum=$3
	local isinet6=$4

	if [ -z "$isinet6" ] ; then
		# Don't do anything if address already exists
		grep -q "ADDR='${ip}'" "${CFGPATH}/${VENET_DEV}_*" 2> /dev/null
		if [ "$?" -eq "0" ] ; then
			return 0
		fi

		echo "CONNECTION='ethernet'
DESCRIPTION='${VENET_DEV}:${ifnum}'
INTERFACE='${VENET_DEV}:${ifnum}'
IP='static'
ADDR='${ip}'
NETMASK='${netmask}'" > "${CFGPATH}/${VENET_DEV}_${ifnum}" || error "Could not write ${CFGPATH}/${VENET_DEV}_${ifnum}" $VZ_FS_NO_DISK_SPACE
		if [ -d /etc/systemd ]; then
			add_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "addr add ${ip}/${netmask} broadcast 0.0.0.0 dev ${VENET_DEV}"
		fi
	else
		# Don't do anything if address already exists
		grep -q "ADDR6='${ip}'" "${CFGPATH}/${VENET_DEV}_*" 2> /dev/null
		if [ "$?" -eq "0" ] ; then
			return 0
		fi

		echo "CONNECTION='ethernet'
DESCRIPTION='${VENET_DEV}:${ifnum}'
INTERFACE='${VENET_DEV}:${ifnum}'
IP6='static'
POST_UP='ip route add ::/0 dev ${VENET_DEV}'
ADDR6='${ip}/${netmask}'" > "${CFGPATH}/${VENET_DEV}_${ifnum}" || error "Could not write ${CFGPATH}/${VENET_DEV}_${ifnum}" $VZ_FS_NO_DISK_SPACE
		if [ -d /etc/systemd ]; then
			add_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "addr add ${ip}/${netmask} dev ${VENET_DEV}"
			grep -q "['\\\"]-6 route add default" "${CFGPATH}/${VENET_DEV}" 2> /dev/null
			if [ "$?" -ne "0" ] ; then
				add_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "-6 route add default dev ${VENET_DEV}"
			fi
		fi
	fi

	# add device entry to NETWORKS
	add_param3 "${NETCFG}" "NETWORKS" "${VENET_DEV}_${ifnum}"
}

function get_all_aliasid()
{
	IFNUM=-1
	IFNUMLIST=`ls -1 ${CFGPATH}/${VENET_DEV}_* 2> /dev/null | sed "s/.*${VENET_DEV}_//"`
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

	if [ "x${VE_STATE}" = "xstarting" ]; then
		remove_all_ve_aliases
	fi

	if [ "x${IPDELALL}" = "xyes" ]; then
		remove_all_ve_aliases
	fi

	if [ "${VE_STATE}" != "running" ]; then
		setup_network
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		get_free_aliasid
		if [ -z "$_IPV6ADDR" ] ; then
			create_config "${_IP}" "${_MASK}" "${IFNUM}"
		else
			create_config "${_IP}" "${_MASK}" "${IFNUM}" 1
		fi
	done

	if [ "${VE_STATE}" = "running" ] ; then
		rc.d restart net-profiles
		netcfg "${VENET_DEV}"
	fi
}

function netctl_setup_network()
{
	echo "# This configuration file is auto-generated.
Description='VPS ethernet connection'
Interface=${VENET_DEV}
Connection=ethernet
IP=static
IP6=static
" > "${CTLPATH}/${VENET_DEV}" || error "Could not write ${CTLPATH}/${VENET_DEV}" $VZ_FS_NO_DISK_SPACE

	if [ ! -f "/etc/hosts" ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > /etc/hosts || error "Could not write /etc/hosts" $VZ_FS_NO_DISK_SPACE
	fi

	netctl enable ${VENET_DEV}
}

function netctl_create_config()
{
	local ip=$1
	local netmask=$2
	local isinet6=$3

	if [ -z "$isinet6" ]; then
		# Don't do anything if address already exists
		grep -q "Address.*${ip}/${netmask}" "${CTLPATH}/${VENET_DEV}" 2> /dev/null
		if [ "$?" -eq "0" ]; then
			return 0
		fi
		add_param3 "${CTLPATH}/${VENET_DEV}" "Address" "${ip}/${netmask}"

		# Don't do anything if route already exists
		grep -q "Routes=" "${CTLPATH}/${VENET_DEV}" 2> /dev/null
		if [ "$?" -eq "0" ]; then
			return 0
		fi
		add_param3 "${CTLPATH}/${VENET_DEV}" "Routes" "default dev ${VENET_DEV}"
	else
		# Don't do anything if address already exists
		grep -q "Address6.*${ip}/${netmask}" "${CTLPATH}/${VENET_DEV}" 2> /dev/null
		if [ "$?" -eq "0" ]; then
			return 0
		fi
		add_param3 "${CTLPATH}/${VENET_DEV}" "Address6" "${ip}/${netmask}"

		# Don't do anything if route already exists
		grep -q "Routes6=" "${CTLPATH}/${VENET_DEV}" 2> /dev/null
		if [ "$?" -eq "0" ]; then
			return 0
		fi
		add_param3 "${CTLPATH}/${VENET_DEV}" "Routes6" "default dev ${VENET_DEV}"
	fi
}

function netctl_add_ip()
{
	local ipm

	if [ "x${VE_STATE}" = "xstarting" ]; then
		remove_all_ve_aliases
	fi
	if [ "x${IPDELALL}" = "xyes" ]; then
		remove_all_ve_aliases
	fi
	if [ "${VE_STATE}" != "running" ]; then
		netctl_setup_network
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if [ -z "$_IPV6ADDR" ]; then
			netctl_create_config "${_IP}" "${_MASK}"
		else
			netctl_create_config "${_IP}" "${_MASK}" 1
		fi
	done

	if [ "${VE_STATE}" = "running" ]; then
		netctl restart ${VENET_DEV}
	fi
}

newcfg=
if [ -d "${CFGPATH}" ] ; then
	newcfg=1
	add_ip
fi
if [ -d "${CTLPATH}" ] ; then
	newcfg=1
	netctl_add_ip
fi
if [ -z newcfg ] ; then
	old_add_ip
fi

exit 0
