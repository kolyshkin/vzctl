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

SUBDIRS = lib

AM_CPPFLAGS = -I$(top_srcdir)/include \
              -I$(top_builddir)/include \
              -DSBINDIR=\"$(sbindir)\" \
              -DPKGCONFDIR=\"$(pkgconfdir)\" \
              -DVPSCONFDIR=\"$(vpsconfdir)\" \
              -DSCRIPTDIR=\"$(scriptdir)\" \
              -DVZDIR=\"$(vzdir)\" \
              -DMODULESDIR=\"$(modulesdir)\"

sbin_PROGRAMS = arpsend \
                ndsend \
                vzcalc \
                vzcfgvalidate \
                vzcptcheck \
                vzctl \
                vzfsync \
                vznnc \
                vzlist \
                vzmemcheck \
                vzsplit \
                vzeventd

VZCTL_LIBS = lib/libvzctl.la

vzfsync_SOURCES = vzfsync.c

vznnc_SOURCES = vznnc.c

arpsend_SOURCES = arpsend.c

ndsend_SOURCES = ndsend.c

vzcalc_SOURCES = vzcalc.c
vzcalc_LDADD   = $(VZCTL_LIBS)

vzcfgvalidate_SOURCES = validate.c \
                        vzcfgvalidate.c
vzcfgvalidate_LDADD   = $(VZCTL_LIBS)

vzctl_SOURCES = enter.c \
                modules.c \
                vzctl-actions.c \
                vzctl.c
if HAVE_PLOOP
vzctl_SOURCES += snapshot.c snapshot-list.c
endif

vzctl_LDADD = $(VZCTL_LIBS) $(DL_LIBS) $(UTIL_LIBS)

vzlist_SOURCES = vzlist.c
vzlist_LDADD   = $(VZCTL_LIBS) $(MATH_LIBS)

vzmemcheck_SOURCES = validate.c \
                     vzmemcheck.c
vzmemcheck_LDADD = $(VZCTL_LIBS)

vzsplit_SOURCES = vzsplit.c
vzsplit_LDADD   = $(VZCTL_LIBS)

vzeventd_SOURCES = vzeventd.c
vzeventd_LDADD = $(VZCTL_LIBS)
