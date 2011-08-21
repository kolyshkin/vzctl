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
# Sets user:passwd in a container, adding user if necessary.

CFGFILE="/etc/passwd"

set_serrpasswd()
{
	local userpw="$1"
	local user=${userpw/:*/}
	local passwd=${userpw:${#user}+1}

	if [ -z "${user}" -o  -z "${passwd}" ]; then
		exit $VZ_CHANGEPASS
	fi
	if ! grep -E "^${user}:" ${CFGFILE} 2>&1 >/dev/null; then
		useradd -m "${user}" 2>&1 || exit $VZ_CHANGEPASS
	fi
	echo "${passwd}" | passwd --stdin "${user}" 2>/dev/null
	if [ $? -ne 0 ]; then
		echo "${user}:${passwd}" | chpasswd 2>&1 || exit $VZ_CHANGEPASS
	fi
}

[ -z "${USERPW}" ] && return 0
set_serrpasswd "${USERPW}"

exit 0
