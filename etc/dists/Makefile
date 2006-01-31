# Copyright (C) 2000-2005 SWsoft. All rights reserved.
#
# This file may be distributed under the terms of the Q Public License
# as defined by Trolltech AS of Norway and appearing in the file
# LICENSE.QPL included in the packaging of this file.
#
# This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
# WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

INSTALL = install
DISTCONFDIR = /etc/sysconfig/vz-scripts/dists
DISTSCRIPTDIR = ${DISTCONFDIR}/scripts
DISTSCRIPTS = debian-add_ip.sh debian-del_ip.sh debian-set_hostname.sh 	\
	functions postcreate.sh redhat-7.x_add_ip.sh redhat-add_ip.sh 	\
	redhat-del_ip.sh redhat-set_hostname.sh set_dns.sh  		\
	set_ugid_quota.sh set_userpass.sh slackware-9.x_add_ip.sh 	\
	slackware-9.x_del_ip.sh slackware-add_ip.sh slackware-del_ip.sh \
	slackware-set_hostname.sh suse-7.x_add_ip.sh suse-8.x_add_ip.sh \
	suse-8.x_del_ip.sh suse-add_ip.sh suse-del_ip.sh 		\
	suse-set_hostname.sh gentoo-add_ip.sh gentoo-del_ip.sh 		\
	gentoo-set_hostname.sh gentoo-set_ugid_quota.sh slackware-set_ugid_quota.sh

REDHAT_7x_VER = 7.0 7.1 7.2 7.3
REDHAT_VER = 8.0 9
CENTOS_VER = 3 4
RHEL_VER = 3 4
FC_VER = 1 2 3 4
SUSE_7x_VER = 7.3
SUSE_8x_VER = 8.0 8.1 8.2
SUSE_VER = 9.0 9.1 9.2 9.3
DEBIAN_VER = 3.0 3.1
SLACK_9x_VER = 9.0 9.1
SLACK_VER = 10.0 10.1
SLES_VER = 9

all:

redhat:
	$(INSTALL) redhat.conf $(DESTDIR)$(DISTCONFDIR)/redhat.conf
	for ver in $(REDHAT_VER); do \
		$(INSTALL) redhat.conf \
		    $(DESTDIR)$(DISTCONFDIR)/redhat-$$ver.conf; \
	done
	for ver in $(REDHAT_7x_VER); do \
		$(INSTALL) redhat-7.3.conf \
			$(DESTDIR)$(DISTCONFDIR)/redhat-$$ver.conf; \
	done
centos:
	$(INSTALL) centos.conf $(DESTDIR)$(DISTCONFDIR)/centos.conf
	for ver in $(CENTOS_VER); do \
		$(INSTALL) centos.conf \
		    $(DESTDIR)$(DISTCONFDIR)/centos-$$ver.conf; \
	done
rhel:
	for ver in $(RHEL_VER); do \
		$(INSTALL) rhel-3.conf \
		    $(DESTDIR)$(DISTCONFDIR)/rhel-$$ver.conf; \
	done
fc:
	$(INSTALL) fedora-core.conf $(DESTDIR)$(DISTCONFDIR)/fedora-core.conf
	for ver in $(FC_VER); do \
		$(INSTALL) fedora-core.conf \
		    $(DESTDIR)$(DISTCONFDIR)/fedora-core-$$ver.conf; \
	done
suse:
	for ver in $(SUSE_7x_VER); do \
		$(INSTALL) suse-7.3.conf \
		    $(DESTDIR)$(DISTCONFDIR)/suse-$$ver.conf; \
	done
	for ver in $(SUSE_8x_VER); do \
		$(INSTALL) suse-8.0.conf \
		    $(DESTDIR)$(DISTCONFDIR)/suse-$$ver.conf; \
	done
	for ver in $(SUSE_VER); do \
		$(INSTALL) suse.conf \
		    $(DESTDIR)$(DISTCONFDIR)/suse-$$ver.conf; \
	done
	$(INSTALL) suse.conf $(DESTDIR)$(DISTCONFDIR)/suse.conf
	$(INSTALL) suse.conf $(DESTDIR)$(DISTCONFDIR)/opensuse.conf
sles:
	for ver in $(SLES_VER); do \
		$(INSTALL) sles.conf \
		    $(DESTDIR)$(DISTCONFDIR)/sles-$$ver.conf; \
	done
	$(INSTALL) sles.conf $(DESTDIR)$(DISTCONFDIR)/sles.conf
debian:
	for ver in $(DEBIAN_VER); do \
		$(INSTALL) debian.conf \
		    $(DESTDIR)$(DISTCONFDIR)/debian-$$ver.conf; \
	done
	$(INSTALL) debian.conf $(DESTDIR)$(DISTCONFDIR)/debian.conf
slack:
	for ver in $(SLACK_9x_VER); do \
		$(INSTALL) slackware-9.0.conf \
		$(DESTDIR)$(DISTCONFDIR)/slackware-$$ver.conf; \
	done
	for ver in $(SLACK_VER); do \
		$(INSTALL) slackware.conf \
		    $(DESTDIR)$(DISTCONFDIR)/slackware-$$ver.conf; \
	done
	$(INSTALL) slackware.conf $(DESTDIR)$(DISTCONFDIR)/slackware.conf
gentoo:
	$(INSTALL) gentoo.conf $(DESTDIR)$(DISTCONFDIR)/gentoo.conf
mandrake:
	$(INSTALL) mandrake.conf $(DESTDIR)$(DISTCONFDIR)/mandrake.conf


inst-dirs:
	$(INSTALL) -d $(DESTDIR)$(DISTCONFDIR)
	 $(INSTALL) -d $(DESTDIR)$(DISTSCRIPTDIR)
	$(INSTALL) distribution.conf-template $(DESTDIR)$(DISTCONFDIR)/distribution.conf-template
	$(INSTALL) default $(DESTDIR)$(DISTCONFDIR)/default

inst-distscripts:
	for file in $(DISTSCRIPTS); do \
		$(INSTALL) scripts/$$file $(DESTDIR)$(DISTSCRIPTDIR)/$$file; \
	done


install: inst-dirs inst-distscripts redhat centos rhel fc suse debian slack \
	gentoo mandrake sles
	

.PHONY: install redhat centos rhel fc suse debian slack gentoo mandrake \
	inst-dirs inst-distscripts

