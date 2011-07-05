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
# Adds IP address(es) in a container running a etcnet-based systems.

HOSTFILE=/etc/hosts
IFACE_DIR=/etc/net/ifaces
NETFILE=/etc/sysconfig/network
VENET_DEV=venet0

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

create_venet_config()
{
	local dir="$1"; shift

	rm -rf "$dir" && mkdir -p "$dir" ||
		error "Cannot create $dir" ${VZ_FS_NO_DISK_SPACE}
	echo >"$dir/ipv4address" ||
		error "Cannot create $dir/ipv4address" ${VZ_FS_NO_DISK_SPACE}
	echo "default dev $dir" >"$dir/ipv4route" ||
		error "Cannot create $dir/ipv4route" ${VZ_FS_NO_DISK_SPACE}
	echo 'BOOTPROTO=static
ONBOOT=yes
TYPE=lo' >"$dir/options" ||
		error "Cannot create $dir/options" ${VZ_FS_NO_DISK_SPACE}
}

destroy_venet_config()
{
	local dir="$1"; shift

	rm -rf "$dir" ||
		error "Cannot destroy $dir" ${VZ_FS_NO_DISK_SPACE}
}

setup_network()
{
	# Purge old venet0 interface settings
	destroy_venet_config "$VENET_DEV"

	# Set up venet0 main interface
	[ $# -eq 0 ] || create_venet_config "$VENET_DEV"

	# Set /etc/sysconfig/network
	put_param "$NETFILE" NETWORKING yes

	# Set up /etc/hosts if necessary
	[ -s "$HOSTFILE" ] ||
		echo '127.0.0.1	localhost.localdomain localhost' >"$HOSTFILE"
}

create_alias()
{
	local ip="$1"; shift
	local mask="$1"; shift
	local ifnum="$1"; shift

	echo "$ip/$mask label $VENET_DEV:$ifnum" >>".tmp/$VENET_DEV/ipv4address" ||
		error "Cannot create .tmp/$VENET_DEV/ipv4address" ${VZ_FS_NO_DISK_SPACE}
}

backup_configs()
{
	rm -rf .tmp && mkdir -p .tmp ||
		error "Cannot create $VENET_DEV" ${VZ_FS_NO_DISK_SPACE}
	[ -d "$VENET_DEV" ] ||
		create_venet_config "$VENET_DEV"
	cp -a "$VENET_DEV" .tmp/ ||
		error "Unable to backup interface config files" ${VZ_FS_NO_DISK_SPACE}
}

move_configs()
{
	rm -rf "$VENET_DEV.orig" &&
	mv "$VENET_DEV" "$VENET_DEV.orig" &&
	mv ".tmp/$VENET_DEV" "$VENET_DEV" &&
	rm -rf "$VENET_DEV.orig" .tmp
}

find_unused_alias()
{
	local i="$1"; shift
	while grep -qs "label $VENET_DEV:$i\$" ".tmp/$VENET_DEV/ipv4address"; do
		i="$(($i+1))"
	done
	echo "$i"
}

add_ip()
{
	set -- ${IP_ADDR}

	if [ "$VE_STATE" = 'starting' ]; then
		setup_network "$@"
	fi

	if [ $# -gt 0 ]; then
		backup_configs

		local i=0 ipm
		for ipm; do
			ip_conv $ipm
			i="$(find_unused_alias "$(($i+1))")"
			create_alias "$_IP" "$_MASK" "$i"
		done

		move_configs
	fi

	if [ "$VE_STATE" = 'running' ]; then
		# synchronyze config files & interfaces
		ifdown "$VENET_DEV"
		if [ $# -gt 0 ]; then
			ifup "$VENET_DEV"
		else
			destroy_venet_config "$VENET_DEV"
		fi
	fi

	if [ $# -gt 0 -a -d /etc/hooks/add_ip.d ] &&
	   type run-parts >/dev/null 2>&1; then
		run-parts /etc/hooks/add_ip.d "$@"
	fi
}

[ -d "$IFACE_DIR" ] &&
cd "$IFACE_DIR" &&
add_ip
exit 0
