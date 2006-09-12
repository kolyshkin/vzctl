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
     INSTALL = install
      PREFIX = /usr
     SBINDIR = $(PREFIX)/sbin
      LIBDIR = $(PREFIX)/lib/vzctl/lib
     CONFDIR = /etc/vz
   GLBCONFIG = $(CONFDIR)/vz.conf
  VPSCONFDIR = $(CONFDIR)/conf
    NAMESDIR = $(CONFDIR)/names
LIBSCRIPTSDIR = /usr/lib/vzctl/scripts/
  SCRIPTSDIR = /usr/share/vzctl/scripts
NETSCRIPTDIR = /etc/sysconfig/network-scripts
    CRONDDIR = /etc/cron.d
    INITDDIR = /etc/init.d
UDEVRULESDIR = /etc/udev/rules.d
       VZDIR = /vz
    CACHEDIR = $(VZDIR)/template/cache
   VZLOCKDIR = $(VZDIR)/lock
   VZDUMPDIR = $(VZDIR)/dump
 VEIPDUMPDIR = /var/lib/vzctl/veip
     LOGRDIR = /etc/logrotate.d
     LIB_VER = 0.0.2

export LD_LIBRARY_PATH=.
CC = gcc
CFLAGS = -Wall -g2
SHAREDLIB_CFLAGS = -fPIC
#VZKERNEL_HEADERS ?= /lib/modules/$(shell uname -r)/build/include
INC = -I ../include
ARCH ?= $(shell uname -m)
# We need -m64 for ppc64 in order to get proper ioctls
# Do NOT use ARCH since it is always set to ppc from the spec file
ifeq ($(shell uname -m),ppc64)
override CFLAGS += -m64
endif

OBJ_libvzctl = lib/env.lo lib/ub.lo lib/net.lo lib/logger.lo lib/exec.lo \
	lib/cap.lo lib/cpu.lo lib/res.lo lib/util.lo lib/dist.lo lib/list.lo \
	lib/script.lo lib/dev.lo lib/config.lo lib/lock.lo lib/quota.lo \
	lib/modules.lo lib/iptables.lo lib/fs.lo lib/create.lo lib/readelf.lo \
	lib/cpt.lo lib/destroy.lo lib/vps_configure.lo lib/meminfo.lo \
	lib/veth.lo
OBJ_simfs=lib/fs_simfs.lo
OBJ_vzctl = vzctl.o vzctl-actions.o modules.o enter.o
OBJ_vzlist = vzlist.o
OBJ_arp = arpsend.o
OBJ_split = vzsplit.o
OBJ_memcheck = vzmemcheck.o validate.o
OBJ_cfgvalidate = vzcfgvalidate.o validate.o
OBJ_calc = vzcalc.o
OBJ = $(OBJ_vzctl) $(OBJ_vzlist) $(OBJ_arp) $(OBJ_split) $(OBJ_memcheck) \
	$(OBJ_cfgvalidate) $(OBJ_calc)
LOBJ = $(OBJ_libvzctl) $(OBJ_simfs)
LIB_vzctl = libvzctl.so.$(LIB_VER)
LIB_simfs = libvzctl-simfs.so.$(LIB_VER)
LIB_fs = $(LIB_simfs)

LDFLAGS += -Wl,-rpath=$(LIBDIR)
LIBSCRIPTS = vps-create vps-postcreate vps-net_add vps-net_del vps-stop \
     vps-functions

PROGS = vzctl vzlist arpsend vzsplit vzcfgvalidate vzmemcheck vzcalc
LIBS = $(LIB_vzctl)

INITSCRIPTS = vz
CRONSCRIPTS = vpsreboot vpsnetclean
UDEVRULES = 60-vzctl.rules
SCRIPTS = vpsreboot vpsnetclean
NETSCRIPTS = ifcfg-venet0 ifdown-venet ifup-venet
VE0CONFIG = 0.conf
VECONFIG = ve-light.conf-sample ve-vps.basic.conf-sample 
VECONFIG_IA64 = ve-light.conf-sample ve-vps.basic.conf-sample
VECONFIG_X86_64 = ve-light.conf-sample ve-vps.basic.conf-sample

default: all

all: $(PROGS) $(LIBS)

check-version:
	@SPEC_VERSION=`awk ' \
	 /^Version:/ {printf "%s", $$2}'< ../vzctl.spec`; \
	H_VERSION=`awk '/#define[\t\ ]VERSION/ {print $$3}' < \
	../include/vzctl.h | tr -d \"`; \
	if test $$SPEC_VERSION != $$H_VERSION; then \
		echo "Version in vzctl.spec is $$SPEC_VERSION"; \
		echo "Version in vzctl.h is $$H_VERSION"; \
		echo "VERSION mismatch; please fix" 1>&2; \
		exit 1; \
	fi

%.o: %.c
	$(CC) -c $(CFLAGS) $(INC) $< -o $@

%.lo: %.c
	$(CC) -c $(CFLAGS) $(SHAREDLIB_CFLAGS) $(INC) $< -o $@

$(LIB_fs) : $(OBJ_simfs) lib/logger.lo 
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -shared -o $@

libvzctl.a: $(OBJ_libvzctl) $(OBJ_simfs)
	ar scq $@ $+
	ranlib $@

$(LIB_vzctl) : $(OBJ_simfs) $(OBJ_libvzctl)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ $(LD_FLAGS) -shared -o $@

vzctl-static: $(OBJ_vzctl) libvzctl.a
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -rdynamic -ldl -lutil -o $@

vzctl: check-version $(OBJ_vzctl) $(LIB_vzctl)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $(OBJ_vzctl) $(LIB_vzctl) -ldl -lutil -o $@

vzlist: $(OBJ_vzlist) $(LIB_vzctl) 
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -o $@

arpsend: $(OBJ_arp)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -o $@

vzsplit: $(OBJ_split)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -o $@

vzmemcheck: $(OBJ_memcheck) $(LIB_vzctl)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -o $@

vzcfgvalidate: $(OBJ_cfgvalidate) $(LIB_vzctl)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -o $@

vzcalc: $(OBJ_calc) $(LIB_vzctl)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $^ -o $@

inst-sbin :$(PROGS)
	for file in $(PROGS); do \
		$(INSTALL) -m 755 $$file $(DESTDIR)$(SBINDIR)/$$file; \
	done
install-lib: $(LIB_vzctl)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d $(DESTDIR)$(LIBSCRIPTSDIR)
	$(INSTALL) $(LIB_vzctl) $(DESTDIR)$(LIBDIR)/$(LIB_vzctl)
	for file in $(LIBSCRIPTS); do 					\
		$(INSTALL) -m 755 ../etc/$$file $(DESTDIR)$(LIBSCRIPTSDIR)/$$file; \
	done
inst-scripts:
	for file in $(SCRIPTS); do \
		$(INSTALL) -m755 ../etc/$$file $(DESTDIR)$(SCRIPTSDIR)/$$file; \
	done
inst-netscripts:
ifneq "" "$(NETSCRIPTS)"
	$(INSTALL) -d $(DESTDIR)$(NETSCRIPTDIR)
	for file in $(NETSCRIPTS); do \
		$(INSTALL) -m 755 ../etc/$$file $(DESTDIR)$(NETSCRIPTDIR)/$$file; \
	done
endif
inst-configs:
	$(INSTALL) -m 644 ../etc/vz.conf $(DESTDIR)$(GLBCONFIG)
ifeq ($(ARCH),ia64)
	for file in $(VECONFIG_IA64); do \
		$(INSTALL) -m 644 ../etc/$$file.ia64 $(DESTDIR)$(VPSCONFDIR)/$$file; \
	done
	$(INSTALL) -m 644 ../etc/$(VE0CONFIG).ia64 $(DESTDIR)$(VPSCONFDIR)/$(VE0CONFIG)
else
ifeq ($(ARCH),x86_64)
	for file in $(VECONFIG_X86_64); do \
		$(INSTALL) -m 644 ../etc/$$file.x86_64 $(DESTDIR)$(VPSCONFDIR)/$$file; \
	done
	$(INSTALL) -m 644 ../etc/$(VE0CONFIG).x86_64 $(DESTDIR)$(VPSCONFDIR)/$(VE0CONFIG)
else
	for file in $(VECONFIG); do \
		$(INSTALL) -m 644 ../etc/$$file $(DESTDIR)$(VPSCONFDIR)/$$file; \
	done
	$(INSTALL) -m 644 ../etc/$(VE0CONFIG) $(DESTDIR)$(VPSCONFDIR)/$(VE0CONFIG)
endif
endif
inst-initd:
	for file in $(INITSCRIPTS); do \
		$(INSTALL) -m 755 ../etc/init.d/$$file $(DESTDIR)$(INITDDIR)/$$file; \
	done

inst-cron:
	for file in $(CRONSCRIPTS); do \
		$(INSTALL) -m 644 ../etc/cron.d/$$file $(DESTDIR)$(CRONDDIR)/$$file; \
	done

inst-udev:
	for file in $(UDEVRULES); do \
		$(INSTALL) -m 644 ../etc/udev/$$file $(DESTDIR)$(UDEVRULESDIR)/$$file; \
	done

inst-dirs:
	$(INSTALL) -d $(DESTDIR)$(CONFDIR)
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	$(INSTALL) -d $(DESTDIR)$(VPSCONFDIR)
	$(INSTALL) -d $(DESTDIR)$(LIBSCRIPTSDIR)
	$(INSTALL) -d $(DESTDIR)$(SCRIPTSDIR)
	$(INSTALL) -d $(DESTDIR)$(CRONDDIR)
	$(INSTALL) -d $(DESTDIR)$(INITDDIR)
	$(INSTALL) -d $(DESTDIR)$(UDEVRULESDIR)
	$(INSTALL) -d $(DESTDIR)$(VZLOCKDIR)
	$(INSTALL) -d $(DESTDIR)$(VZDUMPDIR)
	$(INSTALL) -d $(DESTDIR)$(NAMESDIR)
	$(INSTALL) -d $(DESTDIR)$(LOGRDIR)
	$(INSTALL) -d $(DESTDIR)$(CACHEDIR)
	$(INSTALL) -d $(DESTDIR)$(VEIPDUMPDIR)
	$(INSTALL) -m 644 ../etc/logrotate.d/vzctl $(DESTDIR)$(LOGRDIR)/vzctl

install: $(PROGS) inst-dirs install-lib inst-sbin inst-scripts \
	inst-configs inst-cron inst-udev

install-redhat: inst-netscripts inst-initd

install-gentoo:
	$(INSTALL) -m 755 ../etc/init.d/vz-gentoo $(DESTDIR)/$(INITDDIR)/vz
	ln -s $(DESTDIR)/$(CONFDIR)/vz.conf $(DESTDIR)/conf.d/vz

clean:
	rm -f *.o lib/*.lo $(PROGS) $(LIB_vzctl) $(LIB_fs) $(LIB_fs) .depend

.depend:: $(OBJ:.o=.c) $(LOBJ:.lo=.c)
	$(CC) -M $(FLAGS) $(DEPFLAGS) $(INC) $^ >.depend

depend:: .depend

include .depend


.PHONY: clean depend install inst-dirs inst-config check-version \
	inst-initd install-cron all default inst-config inst-lib inst-vzdir \
	install-redhat install-gentoo
