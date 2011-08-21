#!/bin/sh
#  Copyright (C) 2000-2011, Parallels, Inc. All rights reserved.
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
# Sets up resolver (/etc/resolv.conf) in a container.

set_dns()
{
	local cfgfile="$1"
	local server="$2"
	local search="$3"
	local srv

	if [ -n "${search}" ]; then
		put_param2 "${cfgfile}" search "${search}"
	fi
	if [ -n "${server}" ]; then
		[ -f ${cfgfile} ] || touch ${cfgfile}
		grep -v '^\s*nameserver\s' ${cfgfile} > ${cfgfile}.$$ &&
			mv -f ${cfgfile}.$$ ${cfgfile} ||
			error "Can't change file ${cfgfile}" ${VZ_FS_NO_DISK_SPACE}
		for srv in ${server}; do
			echo "nameserver ${srv}" >> ${cfgfile}
		done
	fi
	chmod 644 ${cfgfile}
}

gen_resolvconf() {
	local ns
	[ -n "${SEARCHDOMAIN}" ] && echo "search ${SEARCHDOMAIN}"
	for ns in ${NAMESERVER}; do
		echo "nameserver $ns"
	done
}

if which resolvconf >/dev/null 2>&1; then
	gen_resolvconf | resolvconf -a venet0
else
	set_dns /etc/resolv.conf "${NAMESERVER}" "${SEARCHDOMAIN}"
fi

exit 0
