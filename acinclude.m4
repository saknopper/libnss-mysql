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

baselist="$with_mysql \
          /usr \
          /usr/local \
          /usr/local/mysql \
          /opt/local \
          /opt/local/mysql"

AC_MSG_CHECKING([for MySQL headers])
for f in $baselist; do
    if test -f "$f/include/mysql/mysql.h"
    then
        MYSQL_INC_DIR="$f/include/mysql"
        break
    fi

    if test -f "$f/include/mysql.h"
    then
        MYSQL_INC_DIR="$f/include"
        break
    fi
done

if test -n "$MYSQL_INC_DIR"
then
    AC_MSG_RESULT([$MYSQL_INC_DIR])
    CPPFLAGS="-I $MYSQL_INC_DIR $CPPFLAGS"
else
    AC_MSG_ERROR([Cannot locate MySQL headers.  Try using --with-mysql=DIR])
fi

AC_MSG_CHECKING([for MySQL libraries])
for f in $baselist; do
    if test -f "$f/lib/libmysqlclient.so" -o -f "$f/lib/libmysqlclient.a"
    then
        MYSQL_LIB_DIR="$f/lib"
        break
    fi

    if test -f "$f/lib/mysql/libmysqlclient.so" -o -f "$f/lib/mysql/libmysqlclient.a"
    then
        MYSQL_LIB_DIR="$f/lib/mysql"
        break
    fi
done

if test -n "$MYSQL_LIB_DIR"
then
    AC_MSG_RESULT([$MYSQL_LIB_DIR])
    LDFLAGS="-L$MYSQL_LIB_DIR $LDFLAGS"
else
    AC_MSG_ERROR([Cannot locate MySQL libraries.  Try using --with-mysql=DIR])
fi

 ])

