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
# This script sets up resolver inside VE
# For usage info see vz-veconfig(5) man page.
#
# Some parameters are passed in environment variables.
# Required parameters:
#   SEARCHDOMAIN
#       Sets search domain(s). Modifies /etc/resolv.conf
#   NAMESERVER
#       Sets name server(s). Modifies /etc/resolv.conf
function set_dns()
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
		sed "/nameserver.*/d" < ${cfgfile} > ${cfgfile}.$$ && \
			mv -f ${cfgfile}.$$ ${cfgfile} || \
			error "Can't change file ${cfgfile}" ${VZ_FS_NO_DISK_SPACE} 
		for srv in ${server}; do
			echo "nameserver ${srv}" >> ${cfgfile}
		done
	fi
	chmod 644 ${cfgfile}
}


set_dns /etc/resolv.conf "${NAMESERVER}" "${SEARCHDOMAIN}"

exit 0
