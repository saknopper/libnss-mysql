Official website: http://libnss-mysql.sourceforge.net/

MySQL for NSS systems
=====================
Supported Operating Systems:
    o Linux
    o Solaris (Sparc or Intel) (2.6, 7, 8. 9 without PAM)
    o FreeBSD (5.1+)

Supported MySQL Versions:
    o MySQL 3.23.9 - 4.1.x

Supported Compilers:
    o GCC (2.95.2, 3.x)
    o Sun Forte Developer 7 C 5.4

NOTE: Solaris 9 is not fully unsupported.  Sun's PAM modules have become
      so restrictive that you can't even properly authenticate via PAM
      anymore if there's a "mysql" in /etc/nsswitch.conf.  System routines
      like getpwnam will still work, so you can work around it some by
      compiling your apps with PAM turned *off* - however logging in via
      system programs such as telnet will fail.  I'll be addressing this
      when I can; unfortunately it requires a PAM module that I'd need to
      write.  It's on my TODO list ...

Prerequisites
=============
    o Installing from source:
      o A functional compile environment (system headers, C compiler, ...)
      o MySQL client library & header files (local)
      o MySQL server (local or remote)

    o Installing from RPM:
      o MySQL server (local or remote)

    o Installing from Solaris Package
      o MySQL server (local or remote)

    o Installing from FreeBSD Port
      o MySQL client library (local)
      o MySQL server (local or remote)

The details
===========
    o If installing from source:
      o ./configure
      o make
      o make install
      If your MySQL installation is based in strange directory, use
      the --with-mysql=DIR option of ./configure to specify.  For example,
      "./configure --with-mysql=/usr2"

    o Edit /etc/libnss-mysql.cfg and /etc/libnss-mysql-root.cfg.

    o Add data to MySQL. The default configs will work well with the sample
      sql database in sample/sample_database.sql.  Read that file for more
      details on the sample database.

    o Edit (or create) /etc/nsswitch.conf such that it contains at least the
      following:
        passwd: files mysql
        shadow: files mysql
        group:  files mysql

      Do not enter the 'shadow' line on any system except Linux.
      If you don't want groups from MySQL, simply don't include 'mysql' in 
      in the 'group' line.

64-bit Support (SPARC V9/Solaris 7+)
====================================
You need to produce BOTH a 32-bit and 64-bit library, so get the 32-bit
one working first.  32-bit programs (most still are) will use the 32-bit
library, and 64-bit programs will use the 64-bit version.

You must have a set of 32-bit libraries, and another set of 64-bit
libraries: MySQL, libc, libm, etc ... Solaris tends to put 64-bit
libraries (and binaries) in a subdirectory named 'sparcv9' - so 64-bit
versions of libc, libm, etc, can be found in /usr/lib/sparcv9.

    o Build a 64-bit MySQL library and install it in a DIFFERENT place than
      your 32-bit MySQL library - IE /usr/local/lib/sparcv9/mysql
    o Build a 64-bit libnss-mysql:
      o Set CFLAGS settings in your environment to:
            gcc:      "-m64"
            Sun's CC: "-xarch=v9"
      o Re-run 'configure', pointing it at your 64-bit MySQL library
        (using --with-mysql=...)
      o Edit Makefile and change 'libdir' to be '/usr/lib/sparcv9'
      o Compile and install
    o Test it by using programs such as 'ls' and 'ps' from the /usr/bin/sparcv9
      directory.

At some point in the future, 64-bit support will be integrated.

Debugging
=========
    See the file DEBUGGING

$Id$
