#!/bin/bash
#  Copyright (C) 2010-2011, Parallels, Inc. All rights reserved.
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
# Sets up name resolution for a Funtoo Linux container.
#
# Associated DNS information with localhost interface so resolvconf works
# as expected.
 
ezdns() {
        # This function generates a resolv.conf file, which ezresolv() passes to resolvconf
	[ -n "${SEARCHDOMAIN}" ] && echo "search ${SEARCHDOMAIN}"
	for ns in ${NAMESERVER}; do
			echo "nameserver $ns"
			done
}

resolvconf -a lo << EOF
`ezdns`
EOF

exit 0
