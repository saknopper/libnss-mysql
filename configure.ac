dnl Copyright (C) 2002 Ben Goodwin
dnl This file is part of the nss-mysql library.
dnl
dnl The nss-mysql library is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU General Public License as published
dnl by the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl The nss-mysql library is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with the nss-mysql library; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
dnl
dnl $Id$ */

AC_INIT([libnss-mysql],[m4_esyscmd(./git-version-gen .tarball-version)],[https://github.com/saknopper/libnss-mysql/issues],[libnss-mysql],[https://github.com/saknopper/libnss-mysql])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_PREREQ([2.69])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR([src/nss_main.c])

AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

AC_PROG_CC

dnl Older versions don't have AM_PROG_AR, while newer versions complain if you don't use it
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_INIT

AC_DISABLE_STATIC

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debug (see DEBUGGING)]))

AS_IF([test "x$enable_debug" = "xyes"], [
  AC_DEFINE([DEBUG], 1, [Enable debug])
])

build_freebsd=no
build_linux=no

case "${host_os}" in
    freebsd*)
        LIBVER=1
        OS=freebsd
		build_freebsd=yes
        ;;
    linux*)
        LIBVER=2
        OS=linux
		build_linux=yes
        ;;
    *)
	    AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

AM_CONDITIONAL([FREEBSD], [test "x$build_freebsd" = "xyes"])
AM_CONDITIONAL([LINUX], [test "x$build_linux" = "xyes"])

WITH_MYSQL()
MYSQL_USE_CLIENT_API()
MYSQL_SUBST()

AC_SUBST(LIBVER)
AC_SUBST(OS)

AC_CHECK_LIB(socket, getsockname)
AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(m, floor)
AC_CHECK_LIB(z, compress)

AC_CHECK_HEADERS(syslog.h stdbool.h nss.h shadow.h)

AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([socklen_t], , , [
#include <unistd.h>
#include <sys/socket.h>
])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
