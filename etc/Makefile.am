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
	dists \
	conf

pkgconfdir = $(sysconfdir)/vz
namesdir   = $(pkgconfdir)/names

bashcdir   = $(sysconfdir)/bash_completion.d
cronddir   = $(sysconfdir)/cron.d
initddir   = $(sysconfdir)/init.d
logrdir    = $(sysconfdir)/logrotate.d
udevdir    = $(sysconfdir)/udev/rules.d

pkglibscriptsdir = $(pkglibdir)/scripts

pkglibscripts_SCRIPTS = vps-create \
                        vps-net_add \
                        vps-net_del \
                        vps-stop \
                        vps-functions

pkgdata_SCRIPTS = vpsnetclean \
                  vpsreboot

BASHCSCRIPTS = bash_completion.d/vzctl.sh
CRONSCRIPTS  = vpsreboot \
               vpsnetclean
INITSCRIPTS  = vz vz-gentoo
LOGRSCRIPTS  = vzctl
NETSCRIPTS   = ifcfg-venet0 ifdown-venet ifup-venet
UDEVRULES    = 60-vzctl.rules

EXTRA_DIST = $(pkglibscripts_SCRIPTS:%=%.in) \
             $(pkgdata_SCRIPTS:%=%.in) \
             $(BASHCSCRIPTS:%=%.in) \
             $(CRONSCRIPTS:%=cron.d/%.in) \
             $(INITSCRIPTS:%=init.d/%.in) \
             $(LOGRSCRIPTS:%=logrotate.d/%) \
             $(NETSCRIPTS:%=%.in) \
             $(UDEVRULES:%=udev/%) \
             vz.conf

include $(top_srcdir)/pathsubst.am

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(pkgconfdir)
	$(mkinstalldirs) $(DESTDIR)$(namesdir)
	$(INSTALL_DATA) $(srcdir)/vz.conf $(DESTDIR)$(pkgconfdir)/vz.conf

install-bashcomp: $(BASHCSCRIPTS)
	$(mkinstalldirs) $(DESTDIR)$(bashcdir)
	for file in $(BASHCSCRIPTS); do \
		$(INSTALL_DATA) $$file $(DESTDIR)$(bashcdir)/; \
	done

install-cron: $(CRONSCRIPTS)
	$(mkinstalldirs) $(DESTDIR)$(cronddir)
	for file in $(CRONSCRIPTS); do \
		$(INSTALL_SCRIPT) $$file $(DESTDIR)$(cronddir)/$$file; \
	done

install-logrotate:
	$(mkinstalldirs) $(DESTDIR)$(logrdir)
	for file in $(LOGRSCRIPTS); do \
		$(INSTALL_DATA) $(srcdir)/logrotate.d/$$file $(DESTDIR)$(logrdir)/$$file; \
	done

install-udev:
	$(mkinstalldirs) $(DESTDIR)$(udevdir)
	for file in $(UDEVRULES); do \
		$(INSTALL_DATA) $(srcdir)/udev/$$file $(DESTDIR)$(udevdir)/$$file; \
	done

install-redhat: init.d/vz $(NETSCRIPTS) install-logrotate install-udev install-cron
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/sysconfig/network-scripts
	$(INSTALL_SCRIPT) init.d/vz  $(DESTDIR)$(initddir)/vz
	for file in $(NETSCRIPTS); do \
		$(INSTALL_SCRIPT) $$file \
			$(DESTDIR)$(sysconfdir)/sysconfig/network-scripts/$$file; \
	done

install-gentoo: init.d/vz-gentoo
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/conf.d
	$(INSTALL_SCRIPT) init.d/vz-gentoo $(DESTDIR)$(initddir)/vz
	$(LN_S) $(pkgconfdir)/vz.conf $(DESTDIR)$(sysconfdir)/conf.d/vz
