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
dnl $Id$

AC_DEFUN([FIND_MYSQL],[

headerlist="$MYSQL_INCLUDE_DIR/mysql.h \
            /usr/include/mysql.h \
            /usr/include/mysql/mysql.h \
            /usr/local/include/mysql.h \
            /usr/local/include/mysql/mysql.h"

for f in $headerlist; do
    if test -f "$f"
    then
        foo=${f%/mysql.h}
        MYSQL_INCLUDE_DIR=$foo
        break
    fi
done

if test -n "$MYSQL_INCLUDE_DIR"; then
    CPPFLAGS="-I $MYSQL_INCLUDE_DIR $CPPFLAGS"
    export CPPFLAGS
fi

liblist="$MYSQL_LIB_DIR/libmysqlclient.so \
         /usr/lib/libmysqlclient.so \
         /usr/local/lib/libmysqlclient.so"

for f in $liblist; do
    if test -f "$f"
    then
        foo=${f%/libmysqlclient.so}
        MYSQL_LIB_DIR=$foo
        break
    fi
done

if test -n "$MYSQL_LIB_DIR"; then
    LIBS="-L$MYSQL_LIB_DIR $LIBS"
    export LIBS
fi


 ])


