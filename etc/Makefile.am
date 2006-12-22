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

SUBDIRS = \
	bash_completion.d \
	conf \
	cron.d \
	dists \
	init.d \
	logrotate.d \
	network-scripts \
	udev

pkgconfdir = $(sysconfdir)/vz
namesdir   = $(pkgconfdir)/names

pkglibscriptsdir = $(pkglibdir)/scripts

pkglibscripts_SCRIPTS = vps-create \
                        vps-net_add \
                        vps-net_del \
                        vps-stop \
                        vps-functions

pkgdata_SCRIPTS = vpsnetclean \
                  vpsreboot

pkgconf_DATA = vz.conf

EXTRA_DIST = $(pkglibscripts_SCRIPTS:%=%.in) \
             $(pkgdata_SCRIPTS:%=%.in) \
             $(PKGCONF_DATA)

include $(top_srcdir)/pathsubst.am

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(namesdir)

install-redhat:
	$(MAKE) $(AM_MAKEFLAGS) -C init.d $@
	$(MAKE) $(AM_MAKEFLAGS) -C network-scripts $@

install-gentoo:
	$(MAKE) $(AM_MAKEFLAGS) -C init.d $@
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/conf.d
	$(LN_S) $(pkgconfdir)/vz.conf $(DESTDIR)$(sysconfdir)/conf.d/vz
