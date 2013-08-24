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

include $(top_srcdir)/paths.am

SUBDIRS = bin \
          etc \
          man \
          scripts \
          src

noinst_HEADERS = $(wildcard include/*.h) \
                 $(wildcard include/linux/*.h)

EXTRA_DIST = vzctl.spec include/version.h.in

vzcachedir  = $(vzdir)/template/cache
vzlockdir   = $(vzdir)/lock
vzdumpdir   = $(vzdir)/dump
vzprivdir   = $(vzdir)/private
vzrootdir   = $(vzdir)/root

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(vzdir)
	$(mkinstalldirs) $(DESTDIR)$(vzcachedir)
	$(mkinstalldirs) $(DESTDIR)$(vzlockdir)
	$(mkinstalldirs) $(DESTDIR)$(vzdumpdir)
	$(mkinstalldirs) -m 700 $(DESTDIR)$(vzprivdir)
	$(mkinstalldirs) -m 700 $(DESTDIR)$(vzrootdir)
	$(mkinstalldirs) $(DESTDIR)$(veipdumpdir)
	$(mkinstalldirs) $(DESTDIR)$(vzrebootdir)
	$(mkinstalldirs) $(DESTDIR)$(vepiddir)
	$(mkinstalldirs) $(DESTDIR)$(modulesdir)

DISTRO_TARGETS = \
	install-redhat \
	install-gentoo \
	install-suse \
	install-debian

DISTRO_TARGETS_ALL = \
	$(DISTRO_TARGETS) \
	install-redhat-from-spec

$(DISTRO_TARGETS_ALL):
	$(MAKE) $(AM_MAKEFLAGS) -C etc $@
	$(MAKE) $(AM_MAKEFLAGS) -C bin $@

install-data-hook:
	@echo
	@echo "	*********************************************************"
	@echo "	If you haven't done it yet, now it's time to install some"
	@echo "	distribution-specific scripts. The following make targets"
	@echo "	are available:"
	@echo
	@echo "	$(DISTRO_TARGETS)"
	@echo
	@echo "	Please invoke make with the target for your distribution!"
	@echo "	*********************************************************"

rpms: dist-bzip2
	cp $(distdir).tar.bz2 `rpm --eval %_sourcedir`
	rpmbuild -ba vzctl.spec

.PHONY: rpms

# Add optional local rules
-include Makefile.local
