## Process this file with automake to produce Makefile.in
lib_LTLIBRARIES = libnss_mysql.la
libnss_mysql_la_SOURCES = nss_config.c nss_mysql.c nss_structures.c \
                          nss_support.c
libnss_mysql_la_LDFLAGS = -version-info 2:0:0
noinst_HEADERS = nss_mysql.h
CPPFLAGS = @CPPFLAGS@ -Wall -D_REENTRANT
EXTRA_DIST = INTERNALS TODO FAQ sample/*.cfg
install-data-local:
	@if test ! -e ${DESTDIR}${sysconfdir}/nss_mysql.cfg; then \
		${srcdir}/mkinstalldirs ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} ${srcdir}/sample/nss_mysql.cfg \
			${DESTDIR}${sysconfdir}/nss_mysql.cfg; \
	fi
	@if test ! -e ${DESTDIR}${sysconfdir}/nss_mysql_root.cfg; then \
		${srcdir}/mkinstalldirs ${DESTDIR}${sysconfdir}; \
		${INSTALL_DATA} -m 600 ${srcdir}/sample/nss_mysql_root.cfg \
			${DESTDIR}${sysconfdir}/nss_mysql_root.cfg; \
	fi
