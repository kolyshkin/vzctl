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

# Generic configuration
AC_PREREQ(2.59)
AC_INIT(vzctl, 3.0.13, devel@openvz.org)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# Automake
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S

# Checks for libraries.
AC_CHECK_LIB(dl, dlopen,
	DL_LIBS="-ldl", AC_MSG_ERROR([libdl not found]),)

AC_SUBST(DL_LIBS)

AC_CHECK_LIB(util, openpty,
	UTIL_LIBS="-lutil", AC_MSG_ERROR([libutil not found]),)

AC_SUBST(UTIL_LIBS)

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.

# Compiler settings
CFLAGS="${CFLAGS} -Wall -Wpointer-arith -Wcast-qual -Winline"
CFLAGS="${CFLAGS} -Wcast-align -Wno-unused-parameter"

if test x$build_cpu = xppc64; then
	CFLAGS="${CFLAGS} -m64"
fi

# Site settings
AM_CONDITIONAL(ARCH_IA64,   test x$build_cpu = xia64)
AM_CONDITIONAL(ARCH_X86_64, test x$build_cpu = xx86_64)

# Final info page
AC_CONFIG_COMMANDS_PRE([SUMMARY="$PACKAGE_STRING configured successfully:

                       CC: $CC ($($CC --version | head -n1))
                   CFLAGS: '$CFLAGS'
                    build: $build
                     host: $host
                   target: $target

                   prefix: $prefix
"])

# Output
AC_CONFIG_FILES([bin/Makefile
                 etc/dists/Makefile
                 etc/conf/Makefile
                 etc/init.d/Makefile
                 etc/Makefile
                 man/Makefile
                 src/lib/Makefile
                 src/Makefile
                 Makefile])
AC_OUTPUT

AC_MSG_NOTICE([$SUMMARY])
