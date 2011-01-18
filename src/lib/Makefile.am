#  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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

INCLUDES = -I$(top_srcdir)/include

AM_CFLAGS = -DSBINDIR=\"$(sbindir)\" \
            -DPKGCONFDIR=\"$(pkgconfdir)\" \
            -DPKGLIBDIR=\"$(pkglibdir)\"

LIB_VER = 0.0.3

lib_LTLIBRARIES = libvzctl.la

libvzctl_la_SOURCES = bitmap.c \
                      cap.c \
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
                      ub.c \
                      util.c \
                      veth.c \
                      vps_configure.c \
                      vzfeatures.c \
                      io.c

libvzctl_la_LDFLAGS = -release $(LIB_VER)
