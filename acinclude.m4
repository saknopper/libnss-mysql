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


