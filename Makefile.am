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

lib_LTLIBRARIES = libnss_mysql.la

libnss_mysql_la_SOURCES = nss_config.c nss_main.c nss_structures.c \
                          nss_support.c memory.c mysql.c lookup.c \
                          mysql-pwd.c mysql-spwd.c mysql-grp.c
libnss_mysql_la_LDFLAGS = -module -version-info @LIBVER@:0:0
 
noinst_HEADERS = nss_mysql.h lookup.h
CPPFLAGS = @CPPFLAGS@ -Wall -D_REENTRANT
EXTRA_DIST = INTERNALS TODO FAQ sample/*.cfg sample/*.sql

install-exec-local:
	-rm -f $(DESTDIR)$(libdir)/libnss_mysql.la
if SOLARIS
	-rm -f $(DESTDIR)$(libdir)/libnss_mysql.so
	-rm -f $(DESTDIR)$(libdir)/libnss_mysql.so.@LIBVER@
	-mv -f $(DESTDIR)$(libdir)/libnss_mysql.so.@LIBVER@.0.0 \
           $(DESTDIR)$(libdir)/nss_mysql.so.1
endif

install-data-local:
	@if test ! -f ${DESTDIR}${sysconfdir}/nss_mysql.cfg; then \
		${srcdir}/mkinstalldirs ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} ${srcdir}/sample/nss_mysql.cfg \
			${DESTDIR}${sysconfdir}/nss_mysql.cfg; \
	fi
	@if test ! -f ${DESTDIR}${sysconfdir}/nss_mysql_root.cfg; then \
		${srcdir}/mkinstalldirs ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} -m 600 ${srcdir}/sample/nss_mysql_root.cfg \
			${DESTDIR}${sysconfdir}/nss_mysql_root.cfg; \
	fi
