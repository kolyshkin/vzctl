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
# This script configures quota startup script inside VE
#
# Parameters are passed in environment variables.
# Required parameters:
#   MINOR	- root device minor number 
#   MAJOR	- root device major number
SCRIPTANAME='/etc/init.d/vzquota'
RCDIRS="/etc/rc.d /etc"

if [ -z "$MAJOR" ]; then
	rm -f ${SCRIPTANAME} > /dev/null 2>&1
	rm -f /etc/mtab > /dev/null 2>&1
	ln -sf /proc/mounts /etc/mtab
	exit 0
fi
echo -e '#!/bin/sh
start() {
	rm -f /etc/mtab >/dev/null 2>&1
	echo "/dev/'${DEVFS}' / reiserfs rw,usrquota,grpquota 0 0" > /etc/mtab
	mnt=`grep -v " / " /proc/mounts`
	if [ $? == 0 ]; then
		echo "$mnt" >> /etc/mtab
	fi 
	quotaon -aug
}
case "$1" in
  start)
        start
        ;;
  *)
	exit
esac ' > ${SCRIPTANAME} || {
	echo "Unable to create ${SCRIPTNAME}"
	exit 1
}
chmod 755 ${SCRIPTANAME}

RC=
for RC in ${RCDIRS}; do
	[ -d ${RC}/rc3.d ] && break
done

if [ -z "${RC}" ]; then
	echo "Unable to find runlevel derectories"
	exit 1
fi

for dir in `ls -d ${RC}/rc[0-6].d`; do
	ln -sf ${SCRIPTANAME} ${dir}/S10vzquota
done

exit 0

