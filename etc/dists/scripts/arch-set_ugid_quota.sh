#!/bin/bash
#  Copyright (C) 2006-2007 SYSTS.ORG  All rights reserved.
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
# Configures quota in a container running Archlinux.

SCRIPTANAME='/etc/rc.d/vzquota'
CFGFILE=/etc/rc.conf

if [ -z "$MAJOR" ]; then
	rm -f ${SCRIPTANAME} > /dev/null 2>&1
	rm -f /etc/mtab > /dev/null 2>&1
	ln -sf /proc/mounts /etc/mtab
	del_param3 ${CFGFILE} DAEMONS vzquota
	exit 0
fi

cat << EOF > ${SCRIPTANAME} || exit 1
#!/bin/sh

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
EOF
chmod 755 ${SCRIPTANAME}

# add vzquota to rc.conf
del_param3 ${CFGFILE} DAEMONS vzquota
add_param3 ${CFGFILE} DAEMONS vzquota

exit 0
