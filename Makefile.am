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

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src

EXTRA_DIST = FAQ UPGRADING DEBUGGING libnss-mysql.spec git-version-gen \
             $(top_srcdir)/.version $(srcdir)/sample $(srcdir)/src/*.sym

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

install-data-hook:
	@if test ! -f ${DESTDIR}${sysconfdir}/libnss-mysql.cfg; then \
		${mkinstalldirs} ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} ${srcdir}/sample/@OS@/libnss-mysql.cfg \
			${DESTDIR}${sysconfdir}/libnss-mysql.cfg; \
	fi
	@if test ! -f ${DESTDIR}${sysconfdir}/libnss-mysql-root.cfg; then \
		${mkinstalldirs} ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} -m 600 ${srcdir}/sample/@OS@/libnss-mysql-root.cfg \
			${DESTDIR}${sysconfdir}/libnss-mysql-root.cfg; \
	fi

uninstall-hook:
	if test -f ${DESTDIR}${sysconfdir}/libnss-mysql.cfg; then \
		rm ${DESTDIR}${sysconfdir}/libnss-mysql.cfg; \
	fi
	if test -f ${DESTDIR}${sysconfdir}/libnss-mysql-root.cfg; then \
		rm ${DESTDIR}${sysconfdir}/libnss-mysql-root.cfg; \
	fi

