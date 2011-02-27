#  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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
AC_INIT(vzctl, 3.0.26, devel@openvz.org)

# Change sysconfdir default since otherwise some important files
# (an initscript, files for logrotate, and udev) will be installed
# to some nonsence/non-working place like under /usr/local/etc/
#
# NOTE: this changes expected configure behavior: if you specify --prefix
#        it will not change sysconfdir, instead now one have to specify
#        --sysconfdir explicitly. To make it more clear we print
#        current sysconfdir value at the end of configure run.
#
# FIXME: dirty hack, may not work with some autoconf versions.
test $sysconfdir = '${prefix}/etc' && sysconfdir=/etc

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# Automake
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2])

# Enable silent build rules by default, requires at least
# Automake-1.11. Disable by either passing --disable-silent-rules to
# configure or passing V=1 to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Disable build of static libraries by default.
AC_DISABLE_STATIC

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
CFLAGS="${CFLAGS} -Wall -Wpointer-arith -Wcast-qual -Winline -Wextra"
CFLAGS="${CFLAGS} -Wcast-align -Wno-unused-parameter"
CFLAGS="${CFLAGS} -D_FILE_OFFSET_BITS=64"

if test x$build_cpu = xppc64 -o x$build_cpu = xpowerpc64; then
	CFLAGS="${CFLAGS} -m64"
fi

# Site settings
AM_CONDITIONAL(ARCH_IA64,   test x$build_cpu = xia64)
AM_CONDITIONAL(ARCH_X86_64, test x$build_cpu = xx86_64)

AC_ARG_ENABLE([bashcomp],
              [AC_HELP_STRING([--enable-bashcomp],
                              [Enable bashcompletion support])],
              [case "${enableval}" in
                yes) enable_bashcomp="+bashcomp";;
                no)  enable_bashcomp="-bashcomp";;
                *)   AC_MSG_ERROR(bad value ${enableval} for --enable-bashcomp);;
              esac],
              [enable_bashcomp="-bashcomp"])
AM_CONDITIONAL(ENABLE_BASHCOMP, test "x$enable_bashcomp" = "x+bashcomp")

AC_ARG_ENABLE([logrotate],
              [AC_HELP_STRING([--enable-logrotate],
                              [Enable logrotate support])],
              [case "${enableval}" in
                yes) enable_logrotate="+logrotate";;
                no)  enable_logrotate="-logrotate";;
                *)   AC_MSG_ERROR(bad value ${enableval} for --enable-logrotate);;
              esac],
              [enable_logrotate="-logrotate"])
AM_CONDITIONAL(ENABLE_LOGROTATE, test "x$enable_logrotate" = "x+logrotate")

AC_ARG_ENABLE([udev],
              [AC_HELP_STRING([--disable-udev],
                              [Disable udev support])],
              [case "${enableval}" in
                yes) enable_udev="+udev";;
                no)  enable_udev="-udev";;
                *)   AC_MSG_ERROR(bad value ${enableval} for --enable-udev);;
              esac],
              [enable_udev="+udev"])
AM_CONDITIONAL(ENABLE_UDEV, test "x$enable_udev" = "x+udev")


# Final info page
AC_CONFIG_COMMANDS_PRE([SUMMARY="$PACKAGE_STRING configured successfully:

         CC: $CC ($($CC --version | head -n1))
     CFLAGS: '$CFLAGS'
      build: $build
       host: $host
     target: $target
     prefix: $prefix
 sysconfdir: $sysconfdir
   features: $enable_bashcomp $enable_logrotate $enable_udev
"])

# Output
AC_CONFIG_FILES([bin/Makefile
                 etc/bash_completion.d/Makefile
                 etc/conf/Makefile
                 etc/dists/Makefile
                 etc/init.d/Makefile
                 etc/logrotate.d/Makefile
                 etc/network/if-up.d/Makefile
                 etc/network-scripts/Makefile
                 etc/udev/Makefile
                 etc/Makefile
                 man/Makefile
                 scripts/Makefile
                 src/lib/Makefile
                 src/Makefile
                 Makefile])
AC_OUTPUT

AC_MSG_NOTICE([$SUMMARY])
