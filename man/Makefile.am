#  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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

man_MANS = arpsend.8 \
           ctid.conf.5 \
           ndsend.8 \
           vzcalc.8 \
           vzcfgvalidate.8 \
           vz.conf.5 \
           vzcpucheck.8 \
           vzctl.8 \
           vzlist.8 \
           vzmemcheck.8 \
           vzmigrate.8 \
           vzpid.8 \
           vzsplit.8 \
           vzifup-post.8 \
           vzeventd.8 \
           vzubc.8


EXTRA_DIST = $(man_MANS)

check-man-dates:
	for FILE in $(man_MANS); do \
		DATE="$$(LANG=C git log -1 --format='format:%cD' \
			$${FILE} | awk '{print $$2, $$3, $$4}')" && \
		echo $$FILE: $$DATE && \
		sed -e "1s%\(\.TH \w\w* [0-9] \"\)[0-9]* \w\w* [0-9][0-9][0-9][0-9]%\1$${DATE}%" -i $${FILE}; \
	done
	git diff $(man_MANS)
