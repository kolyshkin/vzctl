#!/bin/sh
#  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
# Runs on the host system and performs these postinstall tasks
# for a particular container (specified by $VE_ROOT environment):
#
# 1. Randomizes /etc/crontab and /etc/cron.d/* files so all crontab tasks
#    of all containers will not start at the same time.
#    NOTE: if you don't want a particular file to be randomized,
#    make sure it contains the word VZCTL_POSTCREATE_SKIP somewhere.
#
# 2. Disables root password if it is empty.
#
# 3. Seeds /etc/hosts with localhost entries for IPv4 and IPv6
#
# 4. Creates empty /etc/resolv.conf
#
# 5. Sets file caps for recent Fedora releases, as they can
#    not be saved in a template tarball.

randcrontab()
{
	local file
	for file in ${VE_ROOT}/etc/crontab ${VE_ROOT}/etc/cron.d/*; do
		[ -f "${file}" ] || continue
		grep -wq VZCTL_POSTCREATE_SKIP "${file}" && continue

		/bin/cp -fp ${file} ${file}.$$
		cat ${file} | awk '
BEGIN { srand(); }
{
	if ($0 ~ /^[ \t]*#/ || $0 ~ /^[ \t]+*$/) {
		print $0;
		next;
	}
	if ((n = split($0, ar)) < 7) {
		print $0;
		next;
	}
	# min
	if (ar[1] ~ /^[0-9]+$/) {
		ar[1] = int(rand() * 59);
	} else if (ar[1] ~/^-\*\/[0-9]+$/) {
		r = int(rand() * 40) + 15;
		ar[1] = "-*/" r;
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
}' >		${file}.$$ && /bin/mv -f ${file}.$$ ${file}
		/bin/rm -f ${file}.$$ 2>/dev/null
	done
}

disableroot()
{
	local file=${VE_ROOT}"/etc/passwd"

	[ -f "$file" ] || return 0

	if /bin/grep -q "^root::" "${file}" 2>/dev/null; then
		/bin/sed 's/^root::/root:!!:/g' < ${file} > ${file}.$$ &&
			/bin/mv -f ${file}.$$ ${file}
		/bin/rm -f ${file}.$$ 2>/dev/null
	fi
}

set_network()
{
	local file=${VE_ROOT}"/etc/hosts"
	if ! grep -qw '127.0.0.1' ${file} 2>/dev/null; then
		echo '127.0.0.1 localhost.localdomain localhost' >> ${file}
	fi
	if ! grep -qw '::1' ${file} 2>/dev/null; then
		echo "::1 localhost.localdomain localhost" >> ${file}
	fi

	# Some distros' network scripts emit ugly warnings about non-existing
	# /etc/resolv.conf, so it won't hurt to create an empty one
	file=${VE_ROOT}"/etc/resolv.conf"
	if [ ! -e "${file}" ]; then
		touch ${file}
	fi
}

_sc()
{
	local val=$1
	shift
	local f file
	for f in $*; do
		file=${VE_ROOT}${f}
		if [ -e $file ]; then
			setfattr -n security.capability -v $val $file
		fi
	done
}

set_file_caps()
{
	# Perform this only for Fedora 15 to 19
	grep -qEw '1[5-9]' ${VE_ROOT}/etc/fedora-release 2>/dev/null || return

	_sc 0sAQAAAgkAAAAAAAAAAAAAAAAAAAA= /usr/libexec/pt_chown
	_sc 0sAQAAAsIAAAAAAAAAAAAAAAAAAAA= /usr/sbin/suexec
	_sc 0sAQAAAgAgAAAAAAAAAAAAAAAAAAA= /bin/ping /bin/ping6

	# also, for Fedora 18+
	grep -qEw '1[89]' ${VE_ROOT}/etc/fedora-release 2>/dev/null || return
	_sc 0sAQAAAgAgAAAAAAAAAAAAAAAAAAA= /usr/sbin/arping /usr/sbin/clockdiff
	_sc 0sAQAAAgIACAAAAAAAAAAAAAAAAAA= /usr/bin/systemd-detect-virt
}

[ -z "${VE_ROOT}" ] && exit 1
umask 0022
randcrontab
disableroot
set_network
set_file_caps

exit 0
