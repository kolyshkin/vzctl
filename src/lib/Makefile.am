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

include $(top_srcdir)/paths.am

AM_CPPFLAGS = -I$(top_srcdir)/include \
              -DSBINDIR=\"$(sbindir)\" \
              -DPKGCONFDIR=\"$(pkgconfdir)\" \
              -DVPSCONFDIR=\"$(vpsconfdir)\" \
              -DSCRIPTDIR=\"$(scriptdir)\" \
              -DVZDIR=\"$(vzdir)\" \
              -DVEPIDDIR=\"$(vepiddir)\" \
              $(XML_CPPFLAGS)

AM_CFLAGS = $(CGROUP_CFLAGS)

LIB_VER = $(VERSION)

lib_LTLIBRARIES = libvzctl.la

libvzctl_la_SOURCES = bitmap.c \
                      cap.c \
                      cleanup.c \
                      config.c \
                      cpt.c \
                      cpu.c \
                      create.c \
                      destroy.c \
                      dev.c \
                      dist.c \
                      env.c \
                      exec.c \
                      fs.c \
                      fs_simfs.c \
                      image.c \
                      io.c \
                      iptables.c \
                      list.c \
                      lock.c \
                      logger.c \
                      meminfo.c \
                      modules.c \
                      net.c \
                      quota.c \
                      readelf.c \
                      res.c \
                      script.c \
                      util.c \
                      veth.c \
                      vps_configure.c \
                      ub.c \
                      vzfeatures.c

if HAVE_PLOOP
libvzctl_la_SOURCES += snapshot.c xml.c
endif

libvzctl_la_LDFLAGS = -release $(LIB_VER)
libvzctl_la_LIBADD = $(XML_LIBS) $(CGROUP_LIBS) $(DL_LIBS)

if HAVE_CGROUP
libvzctl_la_SOURCES += cgroup.c hooks_ct.c

lib_LTLIBRARIES += libvzchown.la
libvzchown_la_SOURCES = chown_preload.c
libvzchown_la_LDFLAGS = -release 1.0
libvzchown_la_LIBADD = $(DL_LIBS)
endif

if HAVE_VZ_KERNEL
libvzctl_la_SOURCES += hooks_vz.c
endif
