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

SUBDIRS = bin \
          etc \
          man \
          src

noinst_HEADERS = $(wildcard include/*.h) \
                 $(wildcard include/linux/*.h)

vzdir       = /vz
vzcachedir  = $(vzdir)/template/cache
vzlockdir   = $(vzdir)/lock
vzdumpdir   = $(vzdir)/dump
veipdumpdir = $(localstatedir)/vzctl/veip
modulesdir  = $(pkglibdir)/modules

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(vzdir)
	$(mkinstalldirs) $(DESTDIR)$(vzcachedir)
	$(mkinstalldirs) $(DESTDIR)$(vzlockdir)
	$(mkinstalldirs) $(DESTDIR)$(vzdumpdir)
	$(mkinstalldirs) $(DESTDIR)$(veipdumpdir)
	$(mkinstalldirs) $(DESTDIR)$(modulesdir)

ETC_TARGETS = install-bashcomp \
              install-logrotate \
              install-udev \
              install-redhat \
              install-gentoo

$(ETC_TARGETS):
	$(MAKE) $(AM_MAKEFLAGS) -C etc $@
