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
BASHCDIR = etc/bash_completion.d

SCRIPTS=vzpid vzcpucheck vzmigrate
BASHCSCRIPT=${BASHCDIR}/vzctl.sh

all install %::
	for file in ${DIRS}; do \
		(cd $$file && ${MAKE} -f Makefile $@)  \
	done

install::
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	$(INSTALL) -d $(DESTDIR)/sbin
	for file in $(SCRIPTS); do \
		$(INSTALL) -m 755 $$file $(DESTDIR)$(SBINDIR)/$$file; \
	done
	$(INSTALL) -m 755 vznetcfg $(DESTDIR)/sbin/vznetcfg; \
	$(INSTALL) -d $(DESTDIR)/$(BASHCDIR)
	$(INSTALL) -m 644 $(BASHCSCRIPT) $(DESTDIR)/$(BASHCDIR)
	${MAKE} -C man $@
	${MAKE} -C etc/dists $@

tar:
	(VERSION=`awk '/Version:/{print $$2}' < vzctl.spec` && \
	rm -f ../vzctl-$$VERSION; ln -sf `pwd` ../vzctl-$$VERSION && \
	tar --directory ..  --exclude CVS --exclude .git --exclude \*.tar.bz2 -cvhjf vzctl-$$VERSION.tar.bz2 vzctl-$$VERSION; \
	rm -f ../vzctl-$$VERSION)

.PHONY: install tar
