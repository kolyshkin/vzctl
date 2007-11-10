#  Copyright (C) 2000-2007 SWsoft. All rights reserved.
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

distconf_DATA = \
	default \
	distribution.conf-template

EXTRA_DIST = \
	$(nobase_distconf_SCRIPTS) \
	$(distconf_DATA) \
	$(wildcard *.conf)

ALT_2x_VER    = 2.4
ALT_VER       = 3.0 4.0
ARCH_VER      = 0.7 0.8
CENTOS_VER    = 3 4
DEBIAN_VER    = 3.0 3.1 4.0
FC_VER        = 1 2 3 4 5 6 7
FEDORA_VER    = 7
REDHAT_7x_VER = 7.0 7.1 7.2 7.3
REDHAT_VER    = 8.0 9
RHEL_VER      = 3 4
SLACK_9x_VER  = 9.0 9.1
SLACK_VER     = 10.0 10.1
SLES_VER      = 9
SUSE_7x_VER   = 7.3
SUSE_8x_VER   = 8.0 8.1 8.2
SUSE_VER      = 9.0 9.1 9.2 9.3
UBUNTU_VER    = 5.1

altlinux:
	$(INSTALL_DATA) $(srcdir)/altlinux.conf $(DESTDIR)$(distconfdir)/altlinux.conf
	for ver in $(ALT_2x_VER); do \
		$(INSTALL_DATA) $(srcdir)/redhat-7.3.conf \
		    $(DESTDIR)$(distconfdir)/altlinux-$$ver.conf; \
	done
	for ver in $(ALT_VER); do \
		$(INSTALL_DATA) $(srcdir)/altlinux.conf \
		    $(DESTDIR)$(distconfdir)/altlinux-$$ver.conf; \
	done

centos:
	$(INSTALL_DATA) $(srcdir)/centos.conf $(DESTDIR)$(distconfdir)/centos.conf
	for ver in $(CENTOS_VER); do \
		$(INSTALL_DATA) $(srcdir)/centos.conf \
		    $(DESTDIR)$(distconfdir)/centos-$$ver.conf; \
	done

debian:
	for ver in $(DEBIAN_VER); do \
		$(INSTALL_DATA) $(srcdir)/debian.conf \
		    $(DESTDIR)$(distconfdir)/debian-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/debian.conf $(DESTDIR)$(distconfdir)/debian.conf

fc:
	$(INSTALL_DATA) $(srcdir)/fedora-core.conf $(DESTDIR)$(distconfdir)/fedora-core.conf
	for ver in $(FC_VER); do \
		$(INSTALL_DATA) $(srcdir)/fedora-core.conf \
		    $(DESTDIR)$(distconfdir)/fedora-core-$$ver.conf; \
	done

fedora:
	for ver in $(FEDORA_VER); do \
		$(INSTALL_DATA) $(srcdir)/fedora-core.conf \
			$(DESTDIR)$(distconfdir)/fedora-$$ver.conf; \
	done

gentoo:
	$(INSTALL_DATA) $(srcdir)/gentoo.conf $(DESTDIR)$(distconfdir)/gentoo.conf

mandrake:
	$(INSTALL_DATA) $(srcdir)/mandrake.conf $(DESTDIR)$(distconfdir)/mandrake.conf

owl:
	$(INSTALL_DATA) $(srcdir)/redhat-7.3.conf $(DESTDIR)$(distconfdir)/owl.conf

redhat:
	$(INSTALL_DATA) $(srcdir)/redhat.conf $(DESTDIR)$(distconfdir)/redhat.conf
	for ver in $(REDHAT_7x_VER); do \
		$(INSTALL_DATA) $(srcdir)/redhat-7.3.conf \
			$(DESTDIR)$(distconfdir)/redhat-$$ver.conf; \
	done
	for ver in $(REDHAT_VER); do \
		$(INSTALL_DATA) $(srcdir)/redhat.conf \
		    $(DESTDIR)$(distconfdir)/redhat-$$ver.conf; \
	done

rhel:
	for ver in $(RHEL_VER); do \
		$(INSTALL_DATA) $(srcdir)/rhel-3.conf \
		    $(DESTDIR)$(distconfdir)/rhel-$$ver.conf; \
	done

slack:
	for ver in $(SLACK_9x_VER); do \
		$(INSTALL_DATA) $(srcdir)/slackware-9.0.conf \
		$(DESTDIR)$(distconfdir)/slackware-$$ver.conf; \
	done
	for ver in $(SLACK_VER); do \
		$(INSTALL_DATA) $(srcdir)/slackware.conf \
		    $(DESTDIR)$(distconfdir)/slackware-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/slackware.conf $(DESTDIR)$(distconfdir)/slackware.conf

sles:
	for ver in $(SLES_VER); do \
		$(INSTALL_DATA) $(srcdir)/sles.conf \
		    $(DESTDIR)$(distconfdir)/sles-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/sles.conf $(DESTDIR)$(distconfdir)/sles.conf

suse:
	for ver in $(SUSE_7x_VER); do \
		$(INSTALL_DATA) $(srcdir)/suse-7.3.conf \
		    $(DESTDIR)$(distconfdir)/suse-$$ver.conf; \
	done
	for ver in $(SUSE_8x_VER); do \
		$(INSTALL_DATA) $(srcdir)/suse-8.0.conf \
		    $(DESTDIR)$(distconfdir)/suse-$$ver.conf; \
	done
	for ver in $(SUSE_VER); do \
		$(INSTALL_DATA) $(srcdir)/suse.conf \
		    $(DESTDIR)$(distconfdir)/suse-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/suse.conf $(DESTDIR)$(distconfdir)/suse.conf
	$(INSTALL_DATA) $(srcdir)/suse.conf $(DESTDIR)$(distconfdir)/opensuse.conf

ubuntu:
	for ver in $(UBUNTU_VER); do \
		$(INSTALL_DATA) $(srcdir)/debian.conf \
		    $(DESTDIR)$(distconfdir)/ubuntu-$$ver.conf; \
	done
	$(INSTALL_DATA) $(srcdir)/debian.conf $(DESTDIR)$(distconfdir)/ubuntu.conf

arch:
	$(INSTALL_DATA) $(srcdir)/arch.conf $(DESTDIR)$(distconfdir)/arch.conf
	for ver in $(ARCH_VER); do \
		$(INSTALL_DATA) $(srcdir)/arch.conf \
		    $(DESTDIR)$(distconfdir)/arch-$$ver.conf; \
	done

install-data-hook: altlinux \
                   arch \
                   centos \
                   debian \
                   fc \
                   fedora \
                   gentoo \
                   mandrake \
                   owl \
                   redhat \
                   rhel \
                   slack \
                   sles \
                   suse \
                   ubuntu
