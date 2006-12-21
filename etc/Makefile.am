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

SUBDIRS = dists

pkgconfdir = $(sysconfdir)/vz
vpsconfdir = $(pkgconfdir)/conf
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

VE0CONFIG = 0.conf

VECONFIG = ve-light.conf-sample ve-vps.basic.conf-sample

if ARCH_IA64
	VECONFIG_EXT = .ia64
else
if ARCH_X86_64
	VECONFIG_EXT = .x86_64
else
	VECONFIG_EXT =
endif
endif


BASHCSCRIPTS = vzctl.sh
CRONSCRIPTS  = vpsreboot \
               vpsnetclean
INITSCRIPTS  = vz vz-gentoo
LOGRSCRIPTS  = vzctl
NETSCRIPTS   = ifcfg-venet0 ifdown-venet ifup-venet
UDEVRULES    = 60-vzctl.rules

EXTRA_DIST = $(pkglibscripts_SCRIPTS:%=%.in) \
             $(pkgdata_SCRIPTS:%=%.in) \
             $(BASHCSCRIPTS:%=bash_completion.d/%.in) \
             $(CRONSCRIPTS:%=cron.d/%.in) \
             $(INITSCRIPTS:%=init.d/%.in) \
             $(LOGRSCRIPTS:%=logrotate.d/%) \
             $(NETSCRIPTS:%=%.in) \
             $(UDEVRULES:%=udev/%) \
             $(VE0CONFIG) \
             $(VE0CONFIG:%=%.ia64) \
             $(VE0CONFIG:%=%.x86_64) \
             $(VECONFIG) \
             $(VECONFIG:%=%.ia64) \
             $(VECONFIG:%=%.x86_64) \
             vz.conf

pathsubst_RULES = s!@'SBINDIR'@!$(sbindir)!g; \
                  s!@'PKGCONFDIR'@!$(pkgconfdir)!g; \
                  s!@'PKGDATADIR'@!$(pkgdatadir)!g; \
                  s!@'PKGLIBDIR'@!$(pkglibdir)!g;

pathsubst = sed -e '$(pathsubst_RULES)'

%: %.in
	$(pathsubst) $< > $@

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(pkgconfdir)
	$(mkinstalldirs) $(DESTDIR)$(vpsconfdir)
	$(mkinstalldirs) $(DESTDIR)$(namesdir)
	$(INSTALL_DATA) $(srcdir)/vz.conf $(DESTDIR)$(pkgconfdir)/vz.conf
	for file in $(VECONFIG); do \
		$(INSTALL_DATA) $(srcdir)/$$file$(VECONFIG_EXT) $(DESTDIR)$(vpsconfdir)/$$file; \
	done
	$(INSTALL_DATA) $(srcdir)/$(VE0CONFIG)$(VECONFIG_EXT) $(DESTDIR)$(vpsconfdir)/$(VE0CONFIG)

install-bashcomp: $(BASHCSCRIPTS)
	$(mkinstalldirs) $(DESTDIR)$(bashcdir)
	for file in $(BASHCSCRIPTS); do \
		$(INSTALL_DATA) $(builddir)/$$file $(DESTDIR)$(bashcdir)/$$file; \
	done

install-cron: $(CRONSCRIPTS)
	$(mkinstalldirs) $(DESTDIR)$(cronddir)
	for file in $(CRONSCRIPTS); do \
		$(INSTALL_SCRIPT) $(builddir)/$$file $(DESTDIR)$(cronddir)/$$file; \
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

install-redhat: init.d/vz $(NETSCRIPTS)
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/sysconfig/network-scripts
	$(INSTALL_SCRIPT) init.d/vz  $(DESTDIR)$(initddir)/vz
	for file in $(NETSCRIPTS); do \
		$(INSTALL_SCRIPT) $(builddir)/$$file \
			$(DESTDIR)$(sysconfdir)/sysconfig/network-scripts/$$file; \
	done

install-gentoo: init.d/vz-gentoo
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/conf.d
	$(INSTALL_SCRIPT) init.d/vz-gentoo $(DESTDIR)$(initddir)/vz
	$(LN_S) $(pkgconfdir)/vz.conf $(DESTDIR)$(sysconfdir)/conf.d/vz
