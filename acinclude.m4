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

headerlist="$with_mysql_inc \
            /usr/include \
            /usr/include/mysql \
            /usr/local/include \
            /usr/local/include/mysql \
            /usr/local/mysql/include \
            /opt/local/include \
            /opt/local/mysql/include"

for f in $headerlist; do
    if test -f "$f/mysql.h"
    then
        MYSQL_INCLUDE_DIR=$f
        break
    fi
done

if test -n "$MYSQL_INCLUDE_DIR"; then
    CPPFLAGS="-I $MYSQL_INCLUDE_DIR $CPPFLAGS"
fi

liblist="$with_mysql_lib \
         /usr/lib \
         /usr/lib/mysql \
         /usr/local/lib \
         /usr/local/lib/mysql \
         /usr/local/mysql/lib \
         /opt/local/lib \
         /opt/local/mysql/lib"

for f in $liblist; do
    if test -f "$f/libmysqlclient.so" -o -f "$f/libmysqlclient.a"
    then
        MYSQL_LIB_DIR=$f
        break
    fi
done

if test -n "$MYSQL_LIB_DIR"; then
    LDFLAGS="-L$MYSQL_LIB_DIR $LDFLAGS"
fi


 ])


