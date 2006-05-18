#  Copyright (C) 2000-2006 SWsoft. All rights reserved.
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

DIRS=src
INSTALL = install
PREFIX = /usr
SBINDIR = ${PREFIX}/sbin

SCRIPTS=vzpid vzcpucheck vzmigrate

all install %::
	for file in ${DIRS}; do \
		(cd $$file && ${MAKE} -f Makefile $@)  \
	done

install::
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	for file in $(SCRIPTS); do \
		$(INSTALL) -m 755 $$file $(DESTDIR)$(SBINDIR)/$$file; \
	done
	${MAKE} -C man $@
	${MAKE} -C etc/dists $@

tar:
	(VERSION=`awk '/Version:/{print $$2}' < vzctl.spec` && \
	RELEASE=`awk '/Release:/{print $$2}' < vzctl.spec` && \
	ln -sf `pwd` vzctl-$$VERSION-$$RELEASE && \
	tar -cjf vzctl-$$VERSION-$$RELEASE.tar.bz2 `cat file.list | sed "s/\(.*\)/vzctl-$$VERSION-$$RELEASE\/\1/"`; \
	rm -f vzctl-$$VERSION-$$RELEASE)

.PHONY: install tar
