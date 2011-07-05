#!/bin/sh
#  Copyright (C) 2006-2007 Dmitry V. Levin <ldv@altlinux.org>
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
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
#
#
# Deletes IP address(es) from a container running a etcnet-based system.

VENET_DEV=venet0
IFACE_DIR=/etc/net/ifaces

# Function to quote argument for sed regexp.
quote_sed_regexp()
{
	local out="$*"
	if [ -z "${out##*[\[\].^\$\\/]*}" ]; then
		out="$(printf %s "$out" |sed 's/[].^$[\/]/\\&/g')" ||
			return 1
	fi
	printf %s "$out"
}

destroy_venet_config()
{
	local dir="$1"; shift

	rm -rf "$dir" ||
		error "Cannot destroy $dir" ${VZ_FS_NO_DISK_SPACE}
}

# Function to delete IP addresses for etcnet-based systems.
del_ip()
{
	set -- ${IP_ADDR}

	if [ "$IPDELALL" = 'yes' ]; then
		ifdown "$VENET_DEV"
		destroy_venet_config "$VENET_DEV"

		if [ -d /etc/hooks/del_ip.d ] &&
		   type run-parts >/dev/null 2>&1; then
			run-parts /etc/hooks/del_ip.d all
		fi
		return 0
	fi

	local ipm quoted
	for ipm; do
		ip_conv $ipm
		quoted="$(quote_sed_regexp "$_IP/$_MASK")"
		sed -i -e "/^$quoted/d" "$VENET_DEV/ipv4address"
		ip addr del dev "$VENET_DEV" "$_IP/$_MASK"
	done

	if [ $# -gt 0 -a -d /etc/hooks/del_ip.d ] &&
	   type run-parts >/dev/null 2>&1; then
		run-parts /etc/hooks/del_ip.d "$@"
	fi
}

[ -d "$IFACE_DIR" ] &&
cd "$IFACE_DIR" &&
del_ip
exit 0
