##  Copyright (C) 2002 Ben Goodwin
##  This file is part of the nss-mysql library.
##
##  The nss-mysql library is free software; you can redistribute it and/or
##  modify it under the terms of the GNU General Public License as published
##  by the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  The nss-mysql library is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with the nss-mysql library; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
## $Id$
# From Makefile.am $Revision$

AUTOMAKE_OPTIONS = foreign 1.6

if RENAME
lib_LTLIBRARIES         = nss_mysql.la
nss_mysql_la_LDFLAGS    = -module -version-info @LIBVER@:0:0 -export-symbols @OS@.sym
nss_mysql_la_SOURCES    = version.c nss_config.c nss_main.c \
                          nss_support.c memory.c mysql.c lookup.c \
                          mysql-pwd.c mysql-spwd.c mysql-grp.c
else
lib_LTLIBRARIES         = libnss_mysql.la
libnss_mysql_la_LDFLAGS = -module -version-info @LIBVER@:0:0 -export-symbols @OS@.sym
libnss_mysql_la_SOURCES = version.c nss_config.c nss_main.c \
                          nss_support.c memory.c mysql.c lookup.c \
                          mysql-pwd.c mysql-spwd.c mysql-grp.c
endif
 
noinst_HEADERS = nss_mysql.h lookup.h
EXTRA_DIST = TODO FAQ THANKS UPGRADING DEBUGGING libnss-mysql.spec \
             exports.linux \
             sample/README sample/complex/*.* sample/minimal/*.* \
             sample/linux/*.* sample/solaris/*.* sample/freebsd/*.*

install-data-hook:
	@if test ! -f ${DESTDIR}${sysconfdir}/libnss-mysql.cfg; then \
		${srcdir}/mkinstalldirs ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} ${srcdir}/sample/@OS@/libnss-mysql.cfg \
			${DESTDIR}${sysconfdir}/libnss-mysql.cfg; \
	fi
	@if test ! -f ${DESTDIR}${sysconfdir}/libnss-mysql-root.cfg; then \
		${srcdir}/mkinstalldirs ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} -m 600 ${srcdir}/sample/@OS@/libnss-mysql-root.cfg \
			${DESTDIR}${sysconfdir}/libnss-mysql-root.cfg; \
	fi
