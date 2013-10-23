#  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

sbin_SCRIPTS = vzcpucheck \
               vzmigrate \
               vznetcfg \
               vzpid \
               vztmpl-dl \
               vznetaddbr \
               vzifup-post \
               vzoversell \
               vzubc

EXTRA_DIST=$(sbin_SCRIPTS:%=%.in)

CLEANFILES = $(sbin_SCRIPTS)

include $(top_srcdir)/pathsubst.am

install-redhat-from-spec:

install-redhat:
	@if [ ! -e "$(DESTDIR)/sbin/ifup-local" -a \
			! -L "$(DESTDIR)/sbin/ifup-local" ]; then \
		$(MKDIR_P) "$(DESTDIR)/sbin"; \
		$(LN_S) "..$(sbindir)/vzifup-post" \
			"$(DESTDIR)/sbin/ifup-local"; \
	elif readlink "$(DESTDIR)/sbin/ifup-local" | \
			fgrep -q vzifup-post; then \
		true; \
	else \
		echo; \
		echo " ***************************************************"; \
		echo " WARNING: file $(DESTDIR)/sbin/ifup-local is present!"; \
		echo " You have to manually edit the above file so that "; \
		echo " it calls $(sbindir)/vzifup-post"; \
		echo " ***************************************************"; \
		echo; \
	fi


install-suse:
	$(MKDIR_P) "$(DESTDIR)/etc/sysconfig/network/if-up.d"
	$(LN_S) "../../../..$(sbindir)/vzifup-post" \
		"$(DESTDIR)/etc/sysconfig/network/if-up.d/"

install-debian:

install-gentoo:
	@echo
	@echo "***************************************************"
	@echo "Please add the following code to /etc/conf.d/net:"
	@echo
	@echo "postup() {"
	@echo '	$(sbindir)/vzifup-post $${IFACE}'
	@echo "}"
	@echo "***************************************************"
	@echo

.PHONY: install-redhat install-suse install-debian install-gentoo \
	install-redhat-from-spec
