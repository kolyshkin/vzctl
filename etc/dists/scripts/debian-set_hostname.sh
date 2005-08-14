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
# This script sets hostname inside VE for Debian like distros
# For usage info see vz-veconfig(5) man page.
#
# Some parameters are passed in environment variables.
# Required parameters:
# Optional parameters:
#   HOSTNM
#       Sets host name for this VE. Modifies /etc/hosts and
#       /etc/sysconfig/network (in RedHat) or /etc/rc.config (in SuSE)

function set_host()
{
	local cfgfile="$1"
	local var=$2
	local val=$3
	local host=

	[ -z "${val}" ] && return 0
        if grep -q -E "[[:space:]]${val}" ${cfgfile} 2>/dev/null; then
                return;
        fi
	if echo "${val}" | grep "\." >/dev/null 2>&1; then
		host=${val%%.*}
	fi
	host=" ${val} ${host}"
	put_param2 "${cfgfile}" "${var}" "${host} localhost localhost.localdomain"
}

function set_hostname()
{
	local cfgfile=$1
	local hostname=$2

	[ -z "${hostname}" ] && return 0
	echo "${hostname}" > /etc/hostname
	hostname ${hostname}
}

set_host /etc/hosts "127.0.0.1" "${HOSTNM}"
set_hostname /etc/hostname "${HOSTNM}"

exit 0
