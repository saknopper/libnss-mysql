# $Id$

Debugging libnss-mysql
======================
    Serious problems will be sent to syslog, so check there first.

    If you're having problems with ProFTPD, check the FAQ.
    If you've just installed libnss-mysql but it's not working, make
    sure you've restarted any related daemons (IE ssh or nscd).  It's
    probably best to reboot the system unless you know all the daemons
    to restart.  Rebooting (or manually restarting just about everything)
    will help prevent strange problems.

    libnss-mysql has debug code available if you compile is by adding
    '--enable-debug' to the 'configure' line.  Then, you have a choice
    as to where debug information goes.  By default, it will go to the
    file '/tmp/libnss-mysql-debug.log'. Setting the environment variable
    'LIBNSS_MYSQL_DEBUG' will change where debug information goes.
        0 = /tmp/libnss-mysql-debug.log
        1 = stderr
        2 = syslog (prio = debug, same facility compiled with (default=auth))
    Note that setting the environment variable will ONLY affect commands
    spawned from the same process the variable was set in.
    DO NOT LEAVE DEBUG SUPPORT COMPILED IN WHEN YOU'RE DONE - it's 3 
    times slower and is a security hazard.  Make sure to remove the debug
    logfile when you're done, too.

    You can also trace your programs with 'strace' (Linux) or 'truss'
    (Solaris).  This is especially useful if you get *no* debugging
    information (IE because the library isn't loading).

    Before we go down that road:
      - Make sure you've added 'mysql' to /etc/nsswitch.conf.
      - Note that daemons, including nscd, may not see changes to
        nsswitch.conf unless they're restarted.
      - Shut off nscd until you get things working (/etc/init.d/nscd stop).

    Trace a simple request like 'id username'.  Here are command lines
    that will trace the heck out of everything.  They will take some
    time to complete.  The results will be in the file 'trace.out'
    Linux:
        strace -e read=all -e write=all -x -o trace.out id cinergi
    Solaris:
        truss -u '*' -u '\!libc' -wall -rall -vall -o trace.out id cinergi

    You'll want to verify that the library gets loaded, so search for
    'nss_mysql' and make sure you see that the system attempts to open it:
  ****************************************************************************
        open("/usr/lib/nss_mysql.so.1", O_RDONLY)       = 3
  ****************************************************************************
    Soon after that line you should see further attempts to open other
    libraries such as libmysqlclient (if you're using a shared MySQL library).
    Make SURE the return code is NOT -1 (it is '3' in the example line above) -
    otherwise the system isn't finding the libraries it needs.  After
    the libraries are opened, you should see the libnss-mysql.cfg file get
    opened and whatnot.  If you get this far, we know that the library is
    properly installed.
    You can also check to see what the actual MySQL queries are like.
    Search for 'S E L' or 'SEL' in the output.  Depending on your OS, you'll
    see something like:
  ****************************************************************************
  write(3, 0x00025220, 88)                        = 88
  T\0\0\003 S E L E C T   u s e r n a m e , ' x ' , u i d , g i d
  , g e c o s , h o m e d i r , s h e l l   F R O M   u s e r s
  W H E R E   u i d = ' 5 0 0 0 '   L I M I T   1
  ****************************************************************************
    or:
  ****************************************************************************
  write(3, "T\0\0\0\3SELECT username,\'x\',uid,gid"..., 88) = 88
  | 00000  54 00 00 00 03 53 45 4c  45 43 54 20 75 73 65 72  T....SEL ECT user |
  | 00010  6e 61 6d 65 2c 27 78 27  2c 75 69 64 2c 67 69 64  name,'x' ,uid,gid |
  | 00020  2c 67 65 63 6f 73 2c 68  6f 6d 65 64 69 72 2c 73  ,gecos,h omedir,s |
  | 00030  68 65 6c 6c 20 46 52 4f  4d 20 75 73 65 72 73 20  hell FRO M users  |
  | 00040  57 48 45 52 45 20 75 69  64 3d 27 35 30 30 30 27  WHERE ui d='5000' |
  | 00050  20 4c 49 4d 49 54 20 31                            LIMIT 1          |
  ****************************************************************************

    Reading a little past that point, you should be able to see the response
    to your query.

    You can, of couse, use a debugger like gdb or adb, but I won't get
    into that here - it can get pretty complex.  Feel free to ask for help
    if you're having problems.  I try to help anyone who asks.

