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

SUBDIRS = lib

INCLUDES = -I$(top_srcdir)/include

AM_CFLAGS = -DSBINDIR=\"$(sbindir)\" \
            -DPKGCONFDIR=\"$(pkgconfdir)\" \
            -DPKGLIBDIR=\"$(pkglibdir)\"

sbin_PROGRAMS = arpsend \
                ndsend \
                vzcalc \
                vzcfgvalidate \
                vzctl \
                vzlist \
                vzmemcheck \
                vzsplit

VZCTL_LIBS = $(top_builddir)/src/lib/libvzctl.la

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
vzctl_LDADD = $(VZCTL_LIBS) $(DL_LIBS) $(UTIL_LIBS)

vzlist_SOURCES = vzlist.c
vzlist_LDADD   = $(VZCTL_LIBS)

vzmemcheck_SOURCES = validate.c \
                     vzmemcheck.c
vzmemcheck_LDADD = $(VZCTL_LIBS)

vzsplit_SOURCES = vzsplit.c
