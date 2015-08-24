#  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
AC_INIT(vzctl, 4.9.4, devel@openvz.org)
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

# Similar problem with localstatedir, which should be /var not /usr/var
# See https://bugzilla.openvz.org/show_bug.cgi?id=2637#c2
test $localstatedir = '${prefix}/var' && localstatedir=/var

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# If we are on an x86_64, redefine libdir from default value
if test "$target_cpu" = "x86_64" -a "$libdir" = '${exec_prefix}/lib'; then
	if debarch=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null); then
		# Debian/Ubuntu
		libdir='${exec_prefix}/'$debarch
	else
		# Assume Red Hat/SUSE: libs go to lib64
		libdir='${exec_prefix}/lib64'
	fi
fi

# Automake
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2])

# Enable silent build rules by default, requires at least
# Automake-1.11. Disable by either passing --disable-silent-rules to
# configure or passing V=1 to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Disable build of static libraries by default.
AC_DISABLE_STATIC

# If AC_USE_SYSTEM_EXTENSIONS is available (autoconf >= 2.60),
# use it. Otherwise, use old AC_GNU_SOURCE
m4_ifdef([AC_USE_SYSTEM_EXTENSIONS],
	 [AC_USE_SYSTEM_EXTENSIONS], [AC_GNU_SOURCE])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Compiler settings
CFLAGS="${CFLAGS} -Wall -Wpointer-arith -Wcast-qual -Winline -Wextra"
CFLAGS="${CFLAGS} -Wcast-align -Wno-unused-parameter"
CFLAGS="${CFLAGS} -Wno-missing-field-initializers"
CFLAGS="${CFLAGS} -D_FILE_OFFSET_BITS=64"

# We need -m64 for ppc64 in order to get proper ioctls
if test x$target_cpu = xppc64 -o x$target_cpu = xpowerpc64; then
	CFLAGS="${CFLAGS} -m64"
fi

# Checks for libraries.
AC_CHECK_LIB(dl, dlopen,
	DL_LIBS="-ldl", AC_MSG_ERROR([libdl not found]),)

AC_SUBST(DL_LIBS)

AC_CHECK_LIB(util, openpty,
	UTIL_LIBS="-lutil", AC_MSG_ERROR([libutil not found]),)

AC_SUBST(UTIL_LIBS)

AC_CHECK_LIB(m, rint,
	MATH_LIBS="-lm", AC_MSG_ERROR([libm not found]),)

AC_SUBST(MATH_LIBS)

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

AC_ARG_WITH([ploop],
            [AS_HELP_STRING([--with-ploop],
                            [Enable support for ploop])],
            [case "${withval}" in
              yes) with_ploop="+ploop";;
              no)  with_ploop="-ploop";;
              *)   AC_MSG_ERROR(bad value ${withval} for --with-ploop);;
            esac],
            [with_ploop="+ploop"])

AS_IF([test "x$with_ploop" = "x+ploop"],
	[ AC_CHECK_HEADER(ploop/dynload.h, ,
		[ AC_MSG_ERROR( [
The ploop library headers not found or way too old. You need to either
install recent ploop library headers (most probably ploop-devel package),
or use ./configure --without-ploop] ) ]
	)
	AC_CHECK_MEMBER([struct ploop_mount_param.fsck], ,
		[ AC_MSG_ERROR( [
The ploop library headers are too old. You need ploop 1.8 or later] ) ],
		[[#include "ploop/libploop.h"]])
	AC_CHECK_DECL(SYSEXIT_NOSNAP, ,
		[ AC_MSG_ERROR( [
The ploop library headers are too old. You need ploop 1.13 or later] ) ],
		[[#include "ploop/libploop.h"]])

	]
)

AS_IF([test "x$with_ploop" = "x+ploop"],
	[ AM_PATH_XML2(2.6.16, ,
		AC_MSG_ERROR([Please install libxml2 devel package]))
	  AC_DEFINE(HAVE_PLOOP, [1], [Define to enable ploop support]) ])
AM_CONDITIONAL(HAVE_PLOOP, [test "x$with_ploop" = "x+ploop"])

AC_ARG_WITH([cgroup],
            [AS_HELP_STRING([--with-cgroup],
                            [Enable support for cgroup and upstream kernel])],
            [case "${withval}" in
              yes) with_cgroup="+cgroup";;
              no)  with_cgroup="-cgroup";;
              *)   AC_MSG_ERROR(bad value ${withval} for --with-cgroup);;
            esac],
            [with_cgroup="+cgroup"])

AS_IF([test "x$with_cgroup" = "x+cgroup"],
        [ PKG_CHECK_MODULES(CGROUP, [libcgroup >= 0.37])
          AC_DEFINE(HAVE_UPSTREAM, [1], [Define to enable upstream kernel support])
          AC_CHECK_FUNCS([setns]) ]
)
AM_CONDITIONAL(HAVE_CGROUP, [test "x$with_cgroup" = "x+cgroup"])
AC_SUBST(CGROUP_LIBS)
AC_SUBST(CGROUP_CFLAGS)


AC_ARG_WITH([vz],
            [AS_HELP_STRING([--with-vz],
                            [Enable support for OpenVZ kernel])],
            [case "${withval}" in
              yes) with_vz="+vz";;
              no)  with_vz="-vz";;
              *)   AC_MSG_ERROR(bad value ${withval} for --with-vz);;
            esac],
            [with_vz="+vz"])


AS_IF([test "x$with_vz" = "x+vz"],
# vzsyscalls.h will encode the knowledge of which syscalls are supported in
# which archs, and determine whether or not the VZ_KERNEL is usable. During
# normal operation, this file in enclosed within VZ_KERNEL_SUPPORTED, so as
# to get it out of the way during compilation. To check for it, we manually
# define the constant in the test program
[AC_MSG_CHECKING([if current architecture supported by OpenVZ])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
	[#define VZ_KERNEL_SUPPORTED
	#include "${srcdir}/include/vzsyscalls.h"]])
	],
	[AC_DEFINE(VZ_KERNEL_SUPPORTED)
	 AM_CONDITIONAL(HAVE_VZ_KERNEL, true)
	 AC_MSG_RESULT([yes])
	],
	[AM_CONDITIONAL(HAVE_VZ_KERNEL, false)
	 with_vz="-vz"
	AC_MSG_RESULT([no])]
)],
[AM_CONDITIONAL(HAVE_VZ_KERNEL, false)]
)

# A way to redefine /vz is ./configure vzdir=/some/dir
AS_IF([test "x$vzdir" = "x"], vzdir=/vz)
AC_ARG_VAR(vzdir, [Common prefix for a few OpenVZ-related directories,
		   including VE_ROOT and VE_PRIVATE (default: /vz)])

# Final info page
AC_CONFIG_COMMANDS_PRE([SUMMARY="$PACKAGE_STRING configured successfully:

           CC: $CC ($($CC --version | head -n1))
       CFLAGS: '$CFLAGS'
        build: $build
         host: $host
       target: $target
       prefix: $prefix
   sysconfdir: $sysconfdir
localstatedir: $localstatedir
       libdir: $libdir
        vzdir: $vzdir
     features: $enable_bashcomp $enable_logrotate $enable_udev $with_ploop $with_cgroup $with_vz
"])

AS_IF([test "x$with_vz" = "x-vz" -a "x$with_cgroup" = "x-cgroup"],
[AC_ERROR("At least one of cgroup or vz must be enabled")]
)

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
