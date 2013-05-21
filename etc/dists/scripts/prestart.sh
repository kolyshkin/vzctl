#!/bin/sh
#  Copyright (C) 2013, Parallels, Inc. All rights reserved.
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
# Most distributions will need some kind of adjustment when running under
# user namespaces. One example is overriding the loginuid PAM module, that
# is, so far, meaningless inside a container. This script will apply various
# fixups if needed.

# Legacy udev will try to mount its own /dev in tmpfs, which will in turn
# destroy all our hand crafted setup. We need to undo it here.
fixup_udev()
{
	[ -f /etc/fedora-release ] && return
	[ -f /etc/redhat-release ] || return

	# rc.sysinit will touch this file after it finishes.
	timestamp=$(stat -c %x /.autofsck 2>/dev/null)
	i=0
	while true; do
		newstamp=$(stat -c %x /.autofsck 2>/dev/null)
		if [ "x$newstamp" = "x$timestamp" ]; then
			sleep 0.5
			i=$((i+1))
			[ $i -gt 10 ] && return
			continue
		fi
		break
	done

	umount /dev/pts
	umount /dev/shm
	umount /dev -l
}

fixup_loginuid()
{
	local pam_permit="security/pam_permit.so"
	local pam_loginuid="security/pam_loginuid.so"

	for dir in lib lib64 lib/x86_64-linux-gnu lib/i386-linux-gnu; do
		[ -f $dir/$pam_loginuid ] || continue
		mount --bind $dir/$pam_permit $dir/$pam_loginuid
		break
	done
}

[ "x$VZ_KERNEL" = "xyes" ] && exit 0
[ "x$USERNS" = "xno" ] && exit 0

fixup_udev &
fixup_loginuid

exit 0
