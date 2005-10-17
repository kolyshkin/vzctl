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
# This script deletes IP alias(es) inside VE for Gentoo like distros.
# For usage info see ve-alias_del(5) man page.
#
# Parameters are passed in environment variables.
# Required parameters:
#   IP_ADDR       - IPs to delete, several addresses should be divided by space
# Optional parameters:
#   VE_STATE      - state of VE; could be one of:
#                     starting | stopping | running | stopped
VENET_DEV=venet0
CFGFILE=/etc/conf.d/net

# Function to delete IP address for Debian template
function del_ip()
{
	local found=
	local ip e_ip

	for ip in ${IP_ADDR}; do
		grep -qw "${ip}" ${CFGFILE} && found=true && \
			del_param3 "${CFGFILE}" "config_${VENET_DEV}" "${ip}/32"
	done
	if [ -n "${found}" ]; then
		/etc/init.d/net.${VENET_DEV} restart 2>/dev/null 1>/dev/null
	fi
}

del_ip
exit 0
# end of script
