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
# This script sets user:passwd inside VE
#
# Some parameters are passed in environment variables.
# Required parameters:
#   USERPW  - Sets password for user, adding this user if it doesn't exist.
CFGFILE="/etc/passwd"

function set_serrpasswd()
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
