dnl
dnl configure.ac helper macros
dnl 
 
dnl TODO: add README and (c) notice here

dnl TODO: fix "mutual exclusive" stuff
dnl TODO: MariaDB/Percona awareness
dnl TODO: support srcdir/builddir split
dnl TODO: explicitly request mariabd native client


dnl check for a --with-mysql configure option and set up
dnl MYSQL_CONFIG and MYSLQ_VERSION variables for further use
dnl this must always be called before any other macro from this file
dnl with the exception of WITH_MYSQL_SRC()
dnl
dnl WITH_MYSQL()
dnl
AC_DEFUN([WITH_MYSQL], [ 
  AC_MSG_CHECKING(for mysql_config executable)

  MYSQL_CONFIG_ALTERNATIVES="mysql_config mariadb_config"

  # try to find the mysql_config script,
  # --with-mysql will either accept its path directly
  # or will treat it as the mysql install prefix and will 
  # search for the script in there
  # if no path is given at all we look for the script in
  # /usr/bin and /usr/local/mysql/bin
  AC_ARG_WITH(mysql, [  --with-mysql=PATH       path to mysql_config binary or mysql prefix dir], [
    if test $withval = "no"
    then
      MYSQL_CONFIG="no"
    else
      if test -x $withval -a -f $withval
      then
        MYSQL_CONFIG=$withval
        MYSQL_PREFIX=`dirname \`dirname $withval\``
      else
        for _MC in $MYSQL_CONFIG_ALTERNATIVES
        do 
          if test -x $withval/bin/$_MC -a -f $withval/bin/$_MC
          then 
            MYSQL_CONFIG=$withval/bin/$_MC
            MYSQL_PREFIX=$withval
 	    break
	  fi
        done
      fi
    fi
  ], [
    # implicit "yes", check in $PATH and in known default prefix, 
    # but only if source not already configured
    if test "x$MYSQL_SRCDIR" != "x"
    then
      MYSQL_CONFIG="no"
    elif MYSQL_CONFIG=`which mysql_config` 
    then      
      MYSQL_PREFIX=`dirname \`dirname $MYSQL_CONFIG\``
    else
      for _MC in $MYSQL_CONFIG_ALTERNATIVES
      do 
        if test -x $withval/bin/$_MC -a -f $withval/bin/$_MC
        then 
          MYSQL_CONFIG=$withval/bin/$_MC
          MYSQL_PREFIX=$withval
          break
        fi
      done      
    fi
  ])

  if test "x$MYSQL_CONFIG" = "x" 
  then
    AC_MSG_ERROR([not found])
  elif test "$MYSQL_CONFIG" = "no" 
  then
    MYSQL_CONFIG=""
    MYSQL_PREFIX=""
    AC_MSG_RESULT([no])
  else
    if test "x$MYSQL_SRCDIR" != "x"
    then
      AC_MSG_ERROR("--with-mysql can't be used together with --with-mysql-src")
    else
      # get installed version
      MYSQL_VERSION=`$MYSQL_CONFIG --version`

      MYSQL_CONFIG_INCLUDE=`$MYSQL_CONFIG --include`
      MYSQL_CONFIG_LIBS_R=`$MYSQL_CONFIG --libs_r`

      AC_MSG_RESULT($MYSQL_CONFIG)
    fi
  fi

  if test -x "$MYSQL_CONFIG"
  then
    MYSQL_FORK=mysql
    include_dir=`$MYSQL_CONFIG --variable=pkgincludedir`
    if grep -q ndb "$include_dir"/mysql_version.h
    then
      MYSQL_FORK=mysql_cluster
    elif grep -q MariaDB "$include_dir"/mysql_version.h
    then
      MYSQL_FORK=mariadb
    fi    
    dnl TODO identify more forks, esp. percona
  fi
])



dnl check for a --with-mysql-src configure option and set up
dnl MYSQL_CONFIG and MYSLQ_VERSION variables for further use
dnl this must always be called before any other macro from this file
dnl
dnl if you use this together with WITH_MYSQL you have to put this in front of it
dnl
dnl WITH_MYSQL_SRC()
dnl
AC_DEFUN([WITH_MYSQL_SRC], [ 
  AC_MSG_CHECKING(for mysql source directory)

  mysql_src_default=$1;
  if test "x$mysql_src_default" = "x" 
  then
    mysql_src_default="no"
  fi

  AC_DEFUN([help_string],[path to mysql sourcecode @<:@default=$1@:>@])

  AC_ARG_WITH(mysql-src, [AS_HELP_STRING([--with-mysql-src@<:@=PATH@:>@],help_string)], 
                         [  if test "x$MYSQL_CONFIG" != "x"
                            then
                              AC_MSG_ERROR([--with-mysql-src can't be used together with --with-mysql])
                            fi
                         ],[
                            with_mysql_src=$mysql_src_default
                         ])

  AC_MSG_RESULT($with_mysql_src)

  if test "$with_mysql_src" != "no" && test "x$with_mysql_src" != "x"
  then
    if test -f $with_mysql_src/include/mysql_version.h.in
    then
        if test -f $with_mysql_src/include/mysql_version.h
        then
            MYSQL_SRCDIR=`readlink -f $with_mysql_src`
            MYSQL_VERSION=`grep MYSQL_SERVER_VERSION $MYSQL_SRCDIR/include/mysql_version.h | sed -e's/"$//g' -e's/.*"//g'`
	    MYSQL_FORK=mysql
	    if grep -q ndb $MYSQL_SRCDIR/include/mysql_version.h
	    then
	      MYSQL_FORK=mysql_cluster
	    elif grep -q MariaDB $MYSQL_SRCDIR/include/mysql_version.h
	    then
	      MYSQL_FORK=mariadb
	    fi
	    dnl TODO identify more forks, esp. percona
        else
            AC_MSG_ERROR([$MYSQL_SRCDIR/not configured yet])
        fi
    else
        AC_MSG_ERROR([$with_mysql_src doesn't look like a mysql source dir])
    fi
  dnl else
    dnl AC_MSG_ERROR([no source dir given $withval , $with_mysql_src, $mysql_src_default])
  fi 

  if test "x$MYSQL_SRCDIR" != "x"
  then
    MYSQL_CONFIG_INCLUDE="-I$MYSQL_SRCDIR/include -I$MYSQL_SRCDIR  -I$MYSQL_SRCDIR/sql -I$MYSQL_SRCDIR/regex -I$MYSQL_SRCDIR/libmysqld"
    MYSQL_CONFIG_LIBS_R="-L$MYSQL_SRCDIR/libmysql_r/.libs -lmysqlclient_r -lz -lm"
  fi
])


dnl
dnl check for successfull mysql detection
dnl and register AC_SUBST variables
dnl
dnl MYSQL_SUBST()
dnl
AC_DEFUN([MYSQL_SUBST], [
  if test "x$MYSQL_VERSION" = "x" 
  then
    AC_MSG_ERROR([MySQL required but not found])
  fi
   
  # register replacement vars, these will be filled
  # with contant by the other macros 
  AC_SUBST([MYSQL_CFLAGS])
  AC_SUBST([MYSQL_CXXFLAGS])
  AC_SUBST([MYSQL_LDFLAGS])
  AC_SUBST([MYSQL_LIBS])
  AC_SUBST([MYSQL_PLUGIN_DIR])
  AC_SUBST([MYSQL_VERSION])
  AC_SUBST([MYSQL_FORK])
  AC_SUBST([MYSQL_SRCDIR])
])


dnl check if current MySQL version meets a version requirement
dnl and act accordingly
dnl
dnl MYSQL_CHECK_VERSION([requested_version],[yes_action],[no_action])
dnl 
AC_DEFUN([MYSQL_CHECK_VERSION], [
  AX_COMPARE_VERSION([$MYSQL_VERSION], [GE], [$1], [$2], [$3])
])



dnl check if current MySQL version meets a version requirement
dnl and bail out with an error message if not
dnl
dnl MYSQL_NEED_VERSION([need_version])
dnl 
AC_DEFUN([MYSQL_NEED_VERSION], [
  if test "x$MYSQL_FORK" = "x"
  then
    forkname=mysql
  else
    forkname=$MYSQL_FORK
  fi
  AC_MSG_CHECKING([$forkname version >= $1])
  MYSQL_CHECK_VERSION([$1], 
    [AC_MSG_RESULT([yes ($MYSQL_VERSION)])], 
    [AC_MSG_ERROR([no ($MYSQL_VERSION)])])
])


dnl check for embedded server library
dnl 
dnl MYSQL_NEED_EMBEDDED()
dnl
AC_DEFUN([MYSQL_NEED_EMBEDDED], [
  AC_MSG_CHECKING([for libmysqld embedded server library])
  
  if test "x$MYSQL_SRCDIR" = "x" 
  then
    true # TODO check install prefix libdir
  else
    if test -f $MYSQL_SRCDIR/libmysqld/libmysqld.a
    then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_ERROR([not found in $MYSQL_SRCDIR/libmysqld])
    fi
  fi
])


dnl check for a specific fork/product
dnl and bail out with an error message if not
dnl
dnl MYSQL_NEED_FORK([need_fork])
dnl 
AC_DEFUN([MYSQL_NEED_FORK], [
    AC_MSG_CHECKING([if mysql variant is $1])
    if test "$MYSQL_FORK" = "$1"
    then			        	  
      AC_MSG_RESULT([yes])
    else
      AC_MSG_ERROR([no, it's $MYSQL_FORK])
    fi
])



dnl check whether the installed server was compiled with libdbug
dnl
dnl TODO we also need to figure out whether we need to define
dnl SAFEMALLOC, maybe PEDANTIC_SAFEMALLOC and SAFE_MUTEX, too
dnl else we may run into errors like 
dnl
dnl   Can't open shared library '...' 
dnl   (errno: 22 undefined symbol: my_no_flags_free)
dnl
dnl on loading plugins
dnl
dnl MYSQL_DEBUG_SERVER()
dnl
AC_DEFUN([MYSQL_DEBUG_SERVER], [
  AC_MSG_CHECKING(for mysqld debug version)

  MYSQL_DBUG=unknown

  OLD_CFLAGS=$CFLAGS
  CFLAGS="$CFLAGS $MYSQL_CONFIG_INCLUDE"
  OLD_CXXFLAGS=$CXXFLAGS
  CXXFLAGS="$CXXFLAGS $MYSQL_CONFIG_INCLUDE"
  # check for DBUG_ON/OFF being defined in my_config.h
  AC_TRY_COMPILE(,[
#include "my_config.h"
#ifdef DBUG_ON
  int ok;
#else
#  ifdef DBUG_OFF
  int ok;
#  else
  choke me
#  endif
#endif
  ],AS_VAR_SET(MYSQL_DBUG, ["defined by header file"]),AS_VAR_SET(MYSQL_DBUG, unknown))
  CFLAGS=$OLD_CFLAGS
  CXXFLAGS=$OLD_CXXFLAGS


  if test "$MYSQL_DBUG" = "unknown"
  then
    # fallback: need to check mysqld binary itself
    # check $prefix/libexec, $prefix/sbin, $prefix/bin in that order
    for dir in libexec sbin bin
    do
      MYSQLD=$MYSQL_PREFIX/$dir/mysqld
      if test -f $MYSQLD -a -x $MYSQLD
      then
        if ($MYSQLD --help --verbose | grep -q -- "--debug")
        then
          AC_DEFINE([DBUG_ON], [1], [Use libdbug])
          MYSQL_DBUG=yes
        else
          AC_DEFINE([DBUG_OFF], [1], [Don't use libdbug])
          MYSQL_DBUG=no
        fi
        break;
      fi
    done
  fi

  if test "$MYSQL_DBUG" = "unknown"
  then
    # still unknown? make sure not to use it then
    AC_DEFINE([DBUG_OFF], [1], [Don't use libdbug])
    MYSQL_DBUG="unknown, assuming no"
  fi

  AC_MSG_RESULT($MYSQL_DBUG)
  # 
])



dnl set up variables for compilation of regular C API applications
dnl with optional embedded server
dnl 
dnl MYSQL_USE_CLIENT_API()
dnl
AC_DEFUN([MYSQL_USE_CLIENT_API], [
  # add regular MySQL C flags
  ADDFLAGS=$MYSQL_CONFIG_INCLUDE 

  MYSQL_CFLAGS="$MYSQL_CFLAGS $ADDFLAGS"    
  MYSQL_CXXFLAGS="$MYSQL_CXXFLAGS $ADDFLAGS"    

  # add linker flags for client lib
  AC_ARG_ENABLE([embedded-mysql], [  --enable-embedded-mysql enable the MySQL embedded server feature], 
    [MYSQL_EMBEDDED_LDFLAGS()],
    [MYSQL_LDFLAGS="$MYSQL_LDFLAGS $MYSQL_CONFIG_LIBS_R"])
])



dnl set up variables for compilation of regular C API applications
dnl with mandatory embedded server
dnl 
dnl MYSQL_USE_EMBEDDED_API()
dnl
AC_DEFUN([MYSQL_USE_EMBEDDED_API], [
  MYSQL_NEED_EMBEDDED()

  # add regular MySQL C flags
  ADDFLAGS="$MYSQL_CONFIG_INCLUDE"
  if test "x$MYSQL_CONFIG" != "x"
  then
    ADDFLAGS="$ADDFLAGS -I"`$MYSQL_CONFIG --variable=pkgincludedir`"/private"
  fi

  MYSQL_CFLAGS="$MYSQL_CFLAGS $ADDFLAGS"    
  MYSQL_CXXFLAGS="$MYSQL_CXXFLAGS $ADDFLAGS"    

  MYSQL_EMBEDDED_LDFLAGS()
])


dnl
AC_DEFUN([MYSQL_EMBEDDED_LDFLAGS], [
  if test "x$MYSQL_CONFIG" != "x"
  then
    MYSQL_LDFLAGS="$MYSQL_LDFLAGS -Wl,-rpath="`$MYSQL_CONFIG --variable=pkglibdir`" "`$MYSQL_CONFIG --libmysqld-libs`
  else
    MYSQL_LDFLAGS="-L$MYSQL_SRCDIR/libmysqld -lmysqld"
  fi

  AC_MSG_CHECKING([for missing libs])
  OLD_CFLAGS=$CFLAGS
  OLD_LIBS=$LIBS
  CFLAGS="$CFLAGS $MYSQL_CFLAGS"
  for MISSING_LIBS in " " "-lz" "-lssl" "-lz -lssl"
  do
    LIBS="$OLD_LIBS $MYSQL_LDFLAGS $MISSING_LIBS"
    AC_TRY_LINK([
#include <stdio.h>
#include <mysql.h>
    ],[ 
      mysql_server_init(0, NULL, NULL);
    ], [
      LINK_OK=yes
    ], [
      LINK_OK=no
    ])
    if test $LINK_OK = "yes"
    then
      MYSQL_LDFLAGS="$MYSQL_LDFLAGS $MISSING_LIBS"
      AC_MSG_RESULT([$MISSING_LIBS])
      break;
    fi
  done
  if test $LINK_OK = "no"
  then
    AC_MSG_ERROR([linking still fails])
  fi

  LIBS=$OLD_LIBS
  CFLAGS=$OLD_CFLAGS
])




dnl set up variables for compilation of NDBAPI applications
dnl 
dnl MYSQL_USE_NDB_API()
dnl
AC_DEFUN([MYSQL_USE_NDB_API], [
  MYSQL_USE_CLIENT_API()
  AC_PROG_CXX
  MYSQL_CHECK_VERSION([5.0.0],[  

    # mysql_config results need some post processing for now

    # the include pathes changed in 5.1.x due
    # to the pluggable storage engine clenups,
    # it also dependes on whether we build against
    # mysql source or installed headers
    if test "x$MYSQL_SRCDIR" = "x"
    then 
      IBASE=$MYSQL_CONFIG_INCLUDE
    else
      IBASE=$MYSQL_SRCDIR
    fi
    MYSQL_CHECK_VERSION([5.1.0], [
      IBASE="$IBASE/storage/ndb"
    ],[
      IBASE="$IBASE/ndb"
    ])
    if test "x$MYSQL_SRCDIR" != "x"
    then 
      IBASE="$MYSQL_SRCDIR/include"
    fi

    # add the ndbapi specifc include dirs
    ADDFLAGS="$ADDFLAGS $IBASE"
    ADDFLAGS="$ADDFLAGS $IBASE/ndbapi"
    ADDFLAGS="$ADDFLAGS $IBASE/mgmapi"

    MYSQL_CFLAGS="$MYSQL_CFLAGS $ADDFLAGS"
    MYSQL_CXXFLAGS="$MYSQL_CXXFLAGS $ADDFLAGS"

    # check for ndbapi header file NdbApi.hpp
    AC_LANG_PUSH(C++)
    OLD_CXXFLAGS=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $MYSQL_CXXFLAGS"
    AC_CHECK_HEADER([NdbApi.hpp],,[AC_ERROR(["Can't find NdbApi header files"])])
    CXXFLAGS=$OLD_CXXFLAGS
    AC_LANG_POP()

    # check for the ndbapi client library
    AC_LANG_PUSH(C++)
    OLD_LIBS=$LIBS
    LIBS="$LIBS $MYSQL_LIBS -lmysys -lmystrings"
    OLD_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS $MYSQL_LDFLAGS"
    AC_CHECK_LIB([ndbclient],[ndb_init],,[AC_ERROR(["Can't find NdbApi client lib"])]) 
    LIBS=$OLD_LIBS
    LDFLAGS=$OLD_LDFLAGS
    AC_LANG_POP()

    # add the ndbapi specific static libs
    MYSQL_LIBS="$MYSQL_LIBS -lndbclient -lmysys -lmystrings "    

  ],[
    AC_ERROR(["NdbApi needs at lest MySQL 5.0"])
  ])
])



dnl set up variables for compilation of UDF extensions
dnl 
dnl MYSQL_USE_UDF_API()
dnl
AC_DEFUN([MYSQL_USE_UDF_API], [
  # add regular MySQL C flags
  ADDFLAGS=$MYSQL_CONFIG_INCLUDE 

  MYSQL_CFLAGS="$MYSQL_CFLAGS $ADDFLAGS"    
  MYSQL_CXXFLAGS="$MYSQL_CXXFLAGS $ADDFLAGS"    

  MYSQL_DEBUG_SERVER()
])



dnl set up variables for compilation of plugins
dnl 
dnl MYSQL_USE_PLUGIN_API()
dnl
AC_DEFUN([MYSQL_USE_PLUGIN_API], [
  # plugin interface is only availabe starting with MySQL 5.1
  MYSQL_NEED_VERSION([5.1.0])

  # for plugins the recommended way to include plugin.h 
  # is <mysql/plugin.h>, not <plugin.h>, so we have to
  # strip the trailing /mysql from the include path
  # reported by mysql_config
  ADDFLAGS=`echo $MYSQL_CONFIG_INCLUDE | sed -e"s/\/mysql\$//g"` 

  MYSQL_CFLAGS="$ADDFLAGS $MYSQL_CONFIG_INCLUDE $MYSQL_CFLAGS -DMYSQL_DYNAMIC_PLUGIN"    

  MYSQL_CXXFLAGS="$ADDFLAGS $MYSQL_CONFIG_INCLUDE $MYSQL_CXXFLAGS -DMYSQL_DYNAMIC_PLUGIN"
  MYSQL_CXXFLAGS="$MYSQL_CXXFLAGS -fno-implicit-templates -fno-exceptions -fno-rtti"    

  MYSQL_DEBUG_SERVER()
])
