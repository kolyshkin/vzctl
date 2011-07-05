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
# Adds IP address(es) in a container running Red Hat-like distro.

VENET_DEV=venet0
VENET_DEV_CFG=ifcfg-$VENET_DEV
IFCFG_DIR=/etc/sysconfig/network-scripts
IFCFG=${IFCFG_DIR}/ifcfg-${VENET_DEV}
NETFILE=/etc/sysconfig/network
HOSTFILE=/etc/hosts
NETWORKRESTART=

function fix_ifup()
{
	file="/sbin/ifup"

	[ -f "${file}" ] || return 0
	[ "x${VE_STATE}" != "xstarting" ] && return 0

	if grep -q 'if \[ "\${DEVICE}" = "lo" \]; then' ${file} 2>/dev/null
	then
		${CP} ${file} ${file}.$$ || return 1
		/bin/sed -e 's/if \[ "\${DEVICE}" = "lo" \]; then/if \[ "${IPADDR}" = "127.0.0.1" \]; then/g' < ${file} > ${file}.$$ &&
			mv -f ${file}.$$ ${file}
		rm -f ${file}.$$ 2>/dev/null
	fi
}

function setup_network()
{
	# Set up venet0 main interface as 127.0.0.1
	mkdir -p ${IFCFG_DIR}
	echo "DEVICE=${VENET_DEV}
BOOTPROTO=static
ONBOOT=yes
IPADDR=127.0.0.1
NETMASK=255.255.255.255
BROADCAST=0.0.0.0" > $IFCFG || error "Can't write to file $IFCFG" $VZ_FS_NO_DISK_SPACE

	# Set /etc/sysconfig/network
	put_param $NETFILE NETWORKING yes
	# Set default route to venet0
	put_param $NETFILE GATEWAYDEV ${VENET_DEV}
	# Remove old (obsoleted) fake route
	del_param $NETFILE GATEWAY
	# setup ipv6
	setup6_network

	# Set up /etc/hosts
	if [ ! -f ${HOSTFILE} ]; then
		echo "127.0.0.1 localhost.localdomain localhost" > $HOSTFILE
	fi
	fix_ifup
}

function setup6_network()
{
	[ "${IPV6}" != "yes" ] && return 0

	if ! grep -q 'IPV6INIT="yes"' ${IFCFG}; then
		put_param ${IFCFG} IPV6INIT yes
	fi
	if ! grep -q 'NETWORKING_IPV6="yes"' ${NETFILE}; then
		put_param ${NETFILE} NETWORKING_IPV6 yes
		put_param ${NETFILE} IPV6_DEFAULTDEV ${VENET_DEV}
		NETWORKRESTART=yes
	fi
}

function create_config()
{
	local ip=$1
	local netmask=$2
	local ifnum=$3
	local file=${IFCFG_DIR}/bak/${VENET_DEV_CFG}:${ifnum}

	echo "DEVICE=${VENET_DEV}:${ifnum}
ONBOOT=yes
IPADDR=${ip}
NETMASK=${netmask}" > $file ||
	error "Can't write to file $file" ${VZ_FS_NO_DISK_SPACE}
}

function add_ip6()
{
	[ "${IPV6}" != "yes" ] && return
	if ! grep -qw "$1" ${IFCFG} 2>/dev/null; then
		setup6_network
		add_param ${IFCFG} IPV6ADDR_SECONDARIES "$1/$2"
		ifconfig ${VENET_DEV} add "$1/$2"
	fi
}

function get_all_aliasid()
{
	IFNUM=-1

	cd ${IFCFG_DIR} || return 1
	IFNUMLIST=`ls -1 bak/${VENET_DEV_CFG}:* 2>/dev/null |
		sed "s/.*${VENET_DEV_CFG}://"`
}

function get_aliasid_by_ip()
{
	local ip=$1
	local idlist

	cd ${IFCFG_DIR} || return 1
	IFNUM=`grep -l "IPADDR=${ip}$" ${VENET_DEV_CFG}:* | head -n 1 |
		sed -e 's/.*:\([0-9]*\)$/\1/'`
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

function backup_configs()
{
	local delall=$1

	rm -rf ${IFCFG_DIR}/bak/ >/dev/null 2>&1
	mkdir -p ${IFCFG_DIR}/bak
	[ -n "${delall}" ] && return 0

	cd ${IFCFG_DIR} || return 1
	if ls ${VENET_DEV_CFG}:* > /dev/null 2>&1; then
		${CP} ${VENET_DEV_CFG}:* ${IFCFG_DIR}/bak/ ||
			error "Unable to backup interface config files" ${VZ_FS_NO_DISK_SPACE}
	fi
}

function move_configs()
{
	cd ${IFCFG_DIR} || return 1
	rm -rf ${VENET_DEV_CFG}:*
	mv -f bak/* ${IFCFG_DIR}/ >/dev/null 2>&1
	rm -rf ${IFCFG_DIR}/bak
}

function add_ip()
{
	local ipm
	local new_ips
	local if_restart=

	# In case we are starting CT
	if [ "x${VE_STATE}" = "xstarting" ]; then
		# Remove all VENET config files
		rm -f ${IFCFG} ${IFCFG}:* >/dev/null 2>&1
		if [ -z "${IP_ADDR}" ]; then
			# Just enable networking
			put_param $NETFILE NETWORKING yes
			return 0
		fi
	fi
	if [ ! -f "${IFCFG}" ]; then
		setup_network
		if_restart=1
	fi
	backup_configs ${IPDELALL}
	new_ips="${IP_ADDR}"
	if [ "x${IPDELALL}" = "xyes" ]; then
		new_ips=
		for ipm in ${IP_ADDR}; do
			ip_conv $ipm
			get_aliasid_by_ip "${_IP}"
			if [ -n "${IFNUM}" ]; then
				# ip already exists just create it in bak
				create_config "${_IP}" "${_NETMASK}" "${IFNUM}"
			else
				new_ips="${new_ips} ${ipm}"
			fi
		done
	fi
	for ipm in ${new_ips}; do
		ip_conv $ipm
		if [ -z "$_IPV6ADDR" ]; then
			get_free_aliasid
			create_config "${_IP}" "${_NETMASK}" "${IFNUM}"
		else
			if [ "x${IPDELALL}" = "xyes" ]; then
				del_param ${IFCFG} IPV6ADDR_SECONDARIES ""
			fi
			add_ip6 "${_IP}" "${_MASK}"
		fi
	done
	move_configs
	if [ "x${VE_STATE}" = "xrunning" ]; then
		# synchronyze config files & interfaces
		if [ "${NETWORKRESTART}" = "yes" ]; then
			/etc/init.d/network restart
		elif ! ifconfig ${VENET_DEV}|grep -q RUNNING 2>/dev/null; then
			/etc/init.d/network restart
		elif [ -n "${if_restart}" ]; then
			ifup ${VENET_DEV}
		else
			cd /etc/sysconfig/network-scripts &&
				./ifup-aliases ${VENET_DEV}
		fi
	fi
}

add_ip
exit 0
# end of script
