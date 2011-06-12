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
AC_INIT(vzctl, 3.0.28, devel@openvz.org)
AC_CONFIG_SRCDIR([src/vzctl.c])

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
m4_ifdef([AC_USE_SYSTEM_EXTENSIONS], [AC_USE_SYSTEM_EXTENSIONS])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Compiler settings
CFLAGS="${CFLAGS} -Wall -Wpointer-arith -Wcast-qual -Winline -Wextra"
CFLAGS="${CFLAGS} -Wcast-align -Wno-unused-parameter"
CFLAGS="${CFLAGS} -D_FILE_OFFSET_BITS=64"

if test x$build_cpu = xppc64 -o x$build_cpu = xpowerpc64; then
	CFLAGS="${CFLAGS} -m64"
fi

# Checks for libraries.
AC_CHECK_LIB(dl, dlopen,
	DL_LIBS="-ldl", AC_MSG_ERROR([libdl not found]),)

AC_SUBST(DL_LIBS)

AC_CHECK_LIB(util, openpty,
	UTIL_LIBS="-lutil", AC_MSG_ERROR([libutil not found]),)

AC_SUBST(UTIL_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h \
	sys/file.h sys/ioctl.h sys/mount.h sys/param.h sys/socket.h \
	sys/vfs.h termios.h],,
	AC_MSG_ERROR([some needed header(s) not found]))
#AC_HEADER_MAJOR

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_CHECK_MEMBERS([struct stat.st_rdev],,
	AC_MSG_ERROR([no st_rdev member in struct stat]))
AC_TYPE_SIZE_T
AC_TYPE_UID_T
m4_ifdef([AC_TYPE_UINT16_T], [AC_TYPE_UINT16_T])
m4_ifdef([AC_TYPE_UINT32_T], [AC_TYPE_UINT32_T])
m4_ifdef([AC_TYPE_UINT8_T], [AC_TYPE_UINT8_T])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([alarm bzero dup2 endgrent endpwent memset mkdir mkfifo \
	realpath rmdir select socket strcasecmp strchr strdup strerror \
	strrchr strstr strtol strtoul strtoull uname],,
	AC_MSG_ERROR([some needed function(s) not found]))

AC_ARG_ENABLE([bashcomp],
              [AS_HELP_STRING([--enable-bashcomp],
                              [Enable bashcompletion support])],
              [case "${enableval}" in
                yes) enable_bashcomp="+bashcomp";;
                no)  enable_bashcomp="-bashcomp";;
                *)   AC_MSG_ERROR(bad value ${enableval} for --enable-bashcomp);;
              esac],
              [enable_bashcomp="-bashcomp"])
AM_CONDITIONAL(ENABLE_BASHCOMP, test "x$enable_bashcomp" = "x+bashcomp")

AC_ARG_ENABLE([logrotate],
              [AS_HELP_STRING([--enable-logrotate],
                              [Enable logrotate support])],
              [case "${enableval}" in
                yes) enable_logrotate="+logrotate";;
                no)  enable_logrotate="-logrotate";;
                *)   AC_MSG_ERROR(bad value ${enableval} for --enable-logrotate);;
              esac],
              [enable_logrotate="-logrotate"])
AM_CONDITIONAL(ENABLE_LOGROTATE, test "x$enable_logrotate" = "x+logrotate")

AC_ARG_ENABLE([udev],
              [AS_HELP_STRING([--disable-udev],
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
                 include/version.h
                 man/Makefile
                 scripts/Makefile
                 src/lib/Makefile
                 src/Makefile
                 Makefile])
AC_OUTPUT

AC_MSG_NOTICE([$SUMMARY])
