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
# Configures quota startup script in a container.

SCRIPTANAME='/etc/init.d/vzquota'
RCDIRS="/etc/rc.d /etc"

if [ -z "$MAJOR" ]; then
	/usr/sbin/update-rc.d vzquota remove > /dev/null 2>&1
	/sbin/chkconfig --del vzquota >/dev/null 2>&1
	rm -f ${SCRIPTANAME} > /dev/null 2>&1
	rm -f /etc/mtab > /dev/null 2>&1
	ln -sf /proc/mounts /etc/mtab
	exit 0
fi
cat << EOF > ${SCRIPTANAME} || exit 1
#!/bin/sh
# chkconfig: 2345 10 90
# description: prepare container to use OpenVZ 2nd-level disk quotas

### BEGIN INIT INFO
# Provides: vzquota
# Required-Start: \$local_fs \$time \$syslog
# Required-Stop: \$local_fs
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: Start vzquota at the end of boot
# Description: Configure OpenVZ disk quota for a container.
### END INIT INFO

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

case "\$1" in
  start)
	start
	;;
  *)
	exit
esac
EOF
chmod 755 ${SCRIPTANAME}

# Debian/Ubuntu case
if test -x /usr/sbin/update-rc.d; then
	/usr/sbin/update-rc.d vzquota defaults > /dev/null
	exit
fi

# RedHat/RHEL/CentOS case
if test -x /sbin/chkconfig; then
	/sbin/chkconfig --add vzquota >/dev/null
	exit
fi

# Other cases, try to put into runlevels manually
for RC in ${RCDIRS} ""; do
	[ -d ${RC}/rc3.d ] && break
done

if [ -z "${RC}" ]; then
	echo "Unable to find init.d runlevel directories (tried $RCDIRS)"
	exit 1
fi

for dir in `ls -d ${RC}/rc[0-6].d`; do
	ln -sf ${SCRIPTANAME} ${dir}/S10vzquota
done

exit 0

