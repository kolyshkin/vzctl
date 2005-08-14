# Copyright (C) 2000-2005 SWsoft. All rights reserved.
#
# This file may be distributed under the terms of the Q Public License
# as defined by Trolltech AS of Norway and appearing in the file
# LICENSE.QPL included in the packaging of this file.
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#

DIRS=src
INSTALL = install
PREFIX = /usr
SBINDIR = ${PREFIX}/sbin

SCRIPTS=vzpid vzcpucheck

all install %::
	for file in ${DIRS}; do \
		(cd $$file && ${MAKE} -f Makefile $@)  \
	done

install::
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	(cd etc/dists && ${MAKE} -f Makefile $@)
	for file in $(SCRIPTS); do \
		$(INSTALL) $$file $(DESTDIR)$(SBINDIR)/$$file; \
	done
	${MAKE} -C man $@
	${MAKE} -C etc/dists $@

tar:
	(VERSION=`awk '/Version:/{print $$2}' < vzctl.spec` && \
	RELEASE=`awk '/Release:/{print $$2}' < vzctl.spec` && \
	ln -sf `pwd` vzctl-$$VERSION-$$RELEASE && \
	tar -cjf vzctl-$$VERSION-$$RELEASE.tar.bz2 `cat file.list | sed "s/\(.*\)/vzctl-$$VERSION-$$RELEASE\/\1/"`; \
	rm -f vzctl-$$VERSION-$$RELEASE)
