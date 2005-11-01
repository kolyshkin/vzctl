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
# This script runnning on HN and perform next postinstall tasks:
# 1. Randomizes crontab for given VE so all crontab tasks
#  of all VEs will not start at the same time.
# 2. Disables root password if it is empty.
#

function randcrontab()
{
	file=${VE_ROOT}"/etc/crontab"

	[ -f "${file}" ] || return 0

	/bin/cp -fp ${file} ${file}.$$
	cat ${file} | /bin/awk '
BEGIN { srand(); }
{
	if ($0 ~ /^[ \t]*#/ || $0 ~ /^[ \t]+*$/) {
		print $0;
        	next;
	}
	if ((n = split($0, ar, /[ \t]/)) < 7) {
		print $0;
		next;
	}
	# min
	if (ar[1] ~ /^[0-9]+$/) {
		ar[1] = int(rand() * 59);
	}
	# hour
	if (ar[2] ~ /^[0-9]+$/) {
		ar[2] = int(rand() * 6);
	}
	# day
	if (ar[3] ~ /^[0-9]+$/) {
		ar[3] = int(rand() * 31) + 1;
	}
	line = ar[1];
	for (i = 2; i <= n; i++) {
		line = line " "  ar[i];
	}
	print line;
} 
' > ${file}.$$ && /bin/mv -f ${file}.$$ ${file}
	/bin/rm -f ${file}.$$ 2>/dev/null
}

function disableroot()
{
	file=${VE_ROOT}"/etc/passwd"

	[ -f "$file" ] || return 0

	if /bin/grep -q "^root::" "${file}" 2>/dev/null; then
		/bin/sed 's/^root::/root:!!:/g' < ${file} > ${file}.$$ && \
			/bin/mv -f ${file}.$$ ${file}
		/bin/rm -f ${file}.$$ 2>/dev/null
	fi
}

[ -z "${VE_ROOT}" ] && return 1
randcrontab 
disableroot

exit 0
