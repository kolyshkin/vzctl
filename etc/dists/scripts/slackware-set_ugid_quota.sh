#!/bin/bash
#  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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
# Configures quota startup script in a container.

SCRIPTANAME=/etc/rc.d/rc.quota
RC_LOCAL=/etc/rc.d/rc.local

if [ -z "$MAJOR" ]; then
	rm -f ${SCRIPTANAME} > /dev/null 2>&1
	rm -f /etc/mtab > /dev/null 2>&1
	ln -sf /proc/mounts /etc/mtab
	exit 0
fi

cat << EOF > ${SCRIPTANAME} || exit 1
start() {
	dev=\$(awk '(\$2 == "/") && (\$4 ~ /usrquota/) && (\$4 ~ /grpquota/) {print \$1}' /etc/mtab)
	if test -z "\$dev"; then
		dev="/dev/${DEVFS}"
		rm -f /etc/mtab >/dev/null 2>&1
		echo "/dev/${DEVFS} / reiserfs rw,usrquota,grpquota 0 0" > /etc/mtab
		grep -v " / " /proc/mounts >> /etc/mtab 2>/dev/null
		chmod 644 /etc/mtab
	fi
	[ -e "\$dev" ] || mknod \$dev b $MAJOR $MINOR
	quotaon -aug
}
case "$1" in
  start)
	start
	;;
  *)
	exit
esac
EOF
chmod 755 ${SCRIPTANAME}

if ! grep -q "${SCRIPTANAME}" ${RC_LOCAL} 2>/dev/null; then
	echo "${SCRIPTANAME} start" >> /etc/rc.d/rc.local
fi

exit 0

