#  Copyright (C) 2000-2012, Parallels, Inc. All rights reserved.
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

include $(top_srcdir)/pathsubst.am

SUBDIRS = \
	bash_completion.d \
	conf \
	dists \
	init.d \
	logrotate.d \
	network/if-up.d \
	network-scripts \
	udev

EXTRA_DIST = vz.conf.in osrelease.conf download.conf oom-groups.conf
CLEANFILES = vz.conf
pkgconf_DATA = $(EXTRA_DIST:%.in=%)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(namesdir)

install-redhat-from-spec: install-redhat
install-redhat:
	$(MAKE) $(AM_MAKEFLAGS) -C init.d $@
	$(MAKE) $(AM_MAKEFLAGS) -C network-scripts $@

install-suse:
	$(MAKE) $(AM_MAKEFLAGS) -C init.d $@

install-debian:
	$(MAKE) $(AM_MAKEFLAGS) -C init.d $@
	$(MAKE) $(AM_MAKEFLAGS) -C network/if-up.d $@

install-gentoo:
	$(MAKE) $(AM_MAKEFLAGS) -C init.d $@
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/conf.d
	$(LN_S) $(pkgconfdir)/vz.conf $(DESTDIR)$(sysconfdir)/conf.d/vz
