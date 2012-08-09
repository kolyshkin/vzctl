#  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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

nobase_distconf_SCRIPTS = \
	$(wildcard scripts/*.sh) \
	scripts/functions
	scripts/README

distconf_DATA = \
	default \
	distribution.conf-template

EXTRA_DIST = \
	$(nobase_distconf_SCRIPTS) \
	$(distconf_DATA) \
	$(wildcard *.conf)

# Some older versions of distros require specific conf files
ALT_2x_VER    = 2.4
DEBIAN_3x_VER = 3.0 3.1
REDHAT_7x_VER = 7.0 7.1 7.2 7.3
SLACK_9x_VER  = 9.0 9.1
SUSE_7x_VER   = 7.3
SUSE_8x_VER   = 8.0 8.1 8.2
UBUNTU_6x_VER = 6.06

alpine:
	$(INSTALL_DATA) $(srcdir)/alpine.conf $(DESTDIR)$(distconfdir)/alpine.conf

altlinux:
	$(INSTALL_DATA) $(srcdir)/altlinux.conf \
		$(DESTDIR)$(distconfdir)/altlinux.conf
	for ver in $(ALT_2x_VER); do \
		$(INSTALL_DATA) $(srcdir)/redhat-7.3.conf \
		    $(DESTDIR)$(distconfdir)/altlinux-$$ver.conf; \
	done

arch:
	$(INSTALL_DATA) $(srcdir)/arch.conf $(DESTDIR)$(distconfdir)/arch.conf

centos:
	$(INSTALL_DATA) $(srcdir)/redhat.conf \
		$(DESTDIR)$(distconfdir)/centos.conf

debian:
	for ver in $(DEBIAN_3x_VER); do \
		$(INSTALL_DATA) $(srcdir)/debian-3.conf \
		    $(DESTDIR)$(distconfdir)/debian-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/debian.conf \
		$(DESTDIR)$(distconfdir)/debian.conf

fedora:
	$(INSTALL_DATA) $(srcdir)/fedora-core.conf \
		$(DESTDIR)$(distconfdir)/fedora-core.conf
	$(INSTALL_DATA) $(srcdir)/fedora-core.conf \
		$(DESTDIR)$(distconfdir)/fedora.conf

funtoo:
	$(INSTALL_DATA) $(srcdir)/funtoo.conf $(DESTDIR)$(distconfdir)/funtoo.conf

gentoo:
	$(INSTALL_DATA) $(srcdir)/gentoo.conf \
		$(DESTDIR)$(distconfdir)/gentoo.conf

mandrake:
	$(INSTALL_DATA) $(srcdir)/mandrake.conf \
		$(DESTDIR)$(distconfdir)/mandrake.conf

owl:
	$(INSTALL_DATA) $(srcdir)/redhat-7.3.conf \
		$(DESTDIR)$(distconfdir)/owl.conf

redhat:
	$(INSTALL_DATA) $(srcdir)/redhat.conf \
		$(DESTDIR)$(distconfdir)/redhat.conf
	for ver in $(REDHAT_7x_VER); do \
		$(INSTALL_DATA) $(srcdir)/redhat-7.3.conf \
			$(DESTDIR)$(distconfdir)/redhat-$$ver.conf; \
	done

rhel:
	$(INSTALL_DATA) $(srcdir)/redhat.conf \
		$(DESTDIR)$(distconfdir)/rhel.conf

scientific:
	$(INSTALL_DATA) $(srcdir)/redhat.conf \
		$(DESTDIR)$(distconfdir)/scientific.conf

slack:
	$(INSTALL_DATA) $(srcdir)/slackware.conf \
		$(DESTDIR)$(distconfdir)/slackware.conf
	for ver in $(SLACK_9x_VER); do \
		$(INSTALL_DATA) $(srcdir)/slackware-9.0.conf \
			$(DESTDIR)$(distconfdir)/slackware-$$ver.conf; \
	done

suse:
	$(INSTALL_DATA) $(srcdir)/suse.conf \
		$(DESTDIR)$(distconfdir)/suse.conf
	$(INSTALL_DATA) $(srcdir)/suse.conf \
		$(DESTDIR)$(distconfdir)/opensuse.conf
	$(INSTALL_DATA) $(srcdir)/suse.conf \
		$(DESTDIR)$(distconfdir)/sles.conf
	for ver in $(SUSE_7x_VER); do \
		$(INSTALL_DATA) $(srcdir)/suse-7.3.conf \
		    $(DESTDIR)$(distconfdir)/suse-$$ver.conf; \
	done
	for ver in $(SUSE_8x_VER); do \
		$(INSTALL_DATA) $(srcdir)/suse-8.0.conf \
		    $(DESTDIR)$(distconfdir)/suse-$$ver.conf; \
	done

ubuntu:
	for ver in $(UBUNTU_6x_VER); do \
		$(INSTALL_DATA) $(srcdir)/debian-3.conf \
		    $(DESTDIR)$(distconfdir)/ubuntu-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/debian.conf \
		$(DESTDIR)$(distconfdir)/ubuntu.conf

install-data-hook: alpine \
                   altlinux \
                   arch \
                   centos \
                   debian \
                   fedora \
                   funtoo \
                   gentoo \
                   mandrake \
                   owl \
                   redhat \
                   rhel \
                   scientific \
                   slack \
                   suse \
                   ubuntu
