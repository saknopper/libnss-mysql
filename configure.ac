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

AC_INIT([libnss-mysql],[1.6-SNAPSHOT])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_PREREQ([2.69])
AC_CONFIG_SRCDIR([src/nss_main.c])
AM_INIT_AUTOMAKE([foreign 1.11 -Wall silent-rules color-tests])
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CC_C99

AM_PROG_AR

LT_INIT

AC_DISABLE_STATIC

AC_ARG_ENABLE(debug,
            [  --enable-debug          Enable debug (see DEBUGGING)],
            [AC_DEFINE([DEBUG], 1, [Enable debug])])

WITH_MYSQL()
MYSQL_USE_CLIENT_API()
MYSQL_SUBST()

LIBVER=2
OS=linux

AC_SUBST(LIBVER)
AC_SUBST(OS)

AC_CHECK_LIB(socket, getsockname)
AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(m, floor)
AC_CHECK_LIB(dl, dlsym)
AC_CHECK_LIB(z, compress)

AC_HEADER_STDC
AC_CHECK_HEADERS(syslog.h stdint.h stdbool.h nss.h nss_common.h shadow.h)

AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([socklen_t], , , [
#include <unistd.h>
#include <sys/socket.h>
])

EXPANDED_SYSCONFDIR=`eval echo $sysconfdir`
AC_DEFINE_UNQUOTED([MAINCFG], "$EXPANDED_SYSCONFDIR/libnss-mysql.cfg",
                   [Main config file])
AC_DEFINE_UNQUOTED([ROOTCFG],"$EXPANDED_SYSCONFDIR/libnss-mysql-root.cfg",
                   [Root config file])

AC_CONFIG_FILES([Makefile src/Makefile src/version.c])
AC_OUTPUT

