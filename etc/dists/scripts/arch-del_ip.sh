#!/bin/bash
#  Copyright (C) 2006-2007  SYSTS.ORG All rights reserved.
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
# Deletes IP address(es) from a container running Arch Linux.

VENET_DEV=venet0
OLDCFGFILE=/etc/rc.conf

CFGPATH=/etc/network.d
NETCFG=/etc/conf.d/netcfg

CTLPATH=/etc/netctl

function old_remove_all_ve_aliases()
{
	local ve_if_name
	local ve_if

	ve_if_name=`grep "^${VENET_DEV}_" ${OLDCFGFILE} | cut -d'=' -f1`

	for ve_if in ${ve_if_name}; do
	    /etc/rc.d/network ifdown ${ve_if} 2>/dev/null
	    del_param "${OLDCFGFILE}" "${ve_if}"
	    del_param3 "${OLDCFGFILE}" "INTERFACES" "${ve_if}"
	done
}

function old_del_ip()
{
	local ifname
	local ipm

	if [ "x${IPDELALL}" = "xyes" ]; then
		old_remove_all_ve_aliases
		return 0
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		ifname=$(grep -Fw "${_IP}" ${OLDCFGFILE} |
			 grep "^${VENET_DEV}_" | cut -d'=' -f1)
		if [ -n "${ifname}" ]; then
		    # shutdown interface venet0:x
		    /etc/rc.d/network ifdown ${ifname/_/:} 2> /dev/null

		    # remove venet0_x from cfg
		    del_param "${OLDCFGFILE}" "${ifname}"

		    # del venet0_x from INTERFACES array
		    del_param3 "${OLDCFGFILE}" "INTERFACES" "${ifname}"
		fi
	done
}

function remove_all_ve_aliases()
{
	local ve_if
	for ve_if in $(ls -1 ${CFGPATH}/${VENET_DEV}_* 2> /dev/null | sed "s/.*${VENET_DEV}_//"); do
		ip link set "${VENET_DEV}:${ve_if}" down
		rm -f "${CFGPATH}/${VENET_DEV}_${ve_if}"

		del_param3 "${NETCFG}" "NETWORKS" "${VENET_DEV}_${ve_if}"
	done
}

function del_ip()
{
	local ve_if
	local ipm

	if [ "x${IPDELALL}" = "xyes" ]; then
		remove_all_ve_aliases
		return 0
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm

		if [ -z "$_IPV6ADDR" ] ; then
			ve_if=`grep -H "ADDR='${_IP}'" "${CFGPATH}/${VENET_DEV}"_* 2> /dev/null | awk -F: '{print$1}' | sed "s/.*${VENET_DEV}_//"`
			del_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "addr add ${_IP}/${_MASK} broadcast 0.0.0.0 dev ${VENET_DEV}"
		else
			ve_if=`grep -H "ADDR6='${_IP}/128'" "${CFGPATH}/${VENET_DEV}"_* 2> /dev/null | awk -F: '{print$1}' | sed "s/.*${VENET_DEV}_//"`
			del_param3 "${CFGPATH}/${VENET_DEV}" "IPCFG" "addr add ${_IP}/${_MASK} dev ${VENET_DEV}"
		fi

		if [ -z "$ve_if" ] ; then
			break
		fi

		ip link set "${VENET_DEV}:${ve_if}" down
		rm -f "${CFGPATH}/${VENET_DEV}_${ve_if}"

		del_param3 "${NETCFG}" "NETWORKS" "${VENET_DEV}_${ve_if}"
	done

	netcfg "${VENET_DEV}"
}

function netctl_del_ip()
{
	local ipm

	if [ "x${IPDELALL}" = "xyes" ]; then
		remove_all_ve_aliases
		return 0
	fi

	for ipm in ${IP_ADDR}; do
		ip_conv $ipm
		if [ -z "$_IPV6ADDR" ]; then
			del_param3 "${CTLPATH}/${VENET_DEV}" "Address" "$_IP/$_MASK"
		else
			del_param3 "${CTLPATH}/${VENET_DEV}" "Address6" "$_IP/$_MASK"
		fi
	done

	if [ "${VE_STATE}" = "running" ]; then
		netctl restart ${VENET_DEV}
	fi
}

newcfg=
if [ -d "${CFGPATH}" ] ; then
	newcfg=1
	del_ip
fi
if [ -d "${CTLPATH}" ] ; then
	newcfg=1
	netctl_del_ip
fi
if [ -z $newcfg ] ; then
	old_del_ip
fi

exit 0
