$Id$

Q: What is NSS?
Q: What is PAM?
Q: Which one do I need?  One?  Both?
A: NSS stands for NameService Switch.  NSS allows you to implement access to
   various data using any number of modules.  This means that when the
   operating system wants to look up the user "cinergi", it doesn't have
   to know how - it calls upon the NSS system to perform the task.  In turn,
   we can now configure NSS to look for users in traditional places like
   /etc/passwd, NIS, LDAP, and now (using this module), MySQL.  The NSS
   API is the backend for traditional UNIX user lookup routines like
   'getpwnam' - providing details such as username, uid, gid, gecos, shell,
   homedirectory, password, etc.  It does *NOT* provide for changing user
   details.  This is where PAM comes in handy.
   PAM stands for Pluggable Authentication Modules.  Like the name suggests,
   PAM allows you to implement authentication (and data manipulation) using
   any number of modules.  Note that this differs from NSS in that it ONLY
   provides authentication.  It does not allow you to do such things as
   "finger username", or create files owned by "username".  Unlike NSS,
   however, it can enable users to change their passwords using traditional
   methods like the 'passwd' command.
   The libnss_mysql library, like the name suggests, provides an NSS-based
   solution.  Whether you also need PAM depends upon whether you need
   to enable users to change their password using traditional methods (you
   could always script a passwd-like utility that performs MySQL commands).
   PAM also allows more fine-grained setup than NSS does; you can specify
   which programs use which authentication methods - IE your FTP daemon
   could authenticate off a different database than SSH does.  There are
   a few other things it can do, too.  Try 'man pam' for more information.
   Most needs should be met using the NSS library.  There are a few cases
   where it may not be enough.  There is one MySQL PAM module available
   at the moment.  I don't know if it can be made to work in conjunction 
   this library (I don't really see why not).  I may be writing my own
   module(s) in the future to address better integration as well as
   the Solaris PAM problem (See the file README).

Q: Do I need to edit any PAM configuration files?
A: Not likely.  See the above question.

Q: Can I get the system to automatically create a user's homedirectory?
A: Yes.  There's a PAM module, pam_mkhomedir, that allows just this.
   I know that on RedHat linux, you can simply add the following line to
   your /etc/pam.d/system-auth file:
     session optional /lib/security/pam_mkhomedir.so skel=/etc/skel umask=0022
   Note that systems running ssh in privilege-separation mode (default
   in RedHat 8) will *NOT* be able to create homedirectories when logging
   in via ssh.  You'll have to shut off priv-sep mode in /etc/ssh/sshd_config
   and restart ssh.  There's no other known workaround at this time.  Other
   programs that drop root privs before calling PAM/session (I've seen 'su'
   do this) will have similar troubles.

Q: Are other databases (IE hosts, netgroup, automount, aliases, etc)
   supported?
A: Not at this time.  I plan to support these in the future, however.

Q: I have a lot of open MySQL processes - why?
A: libnss-mysql maintains a persistant connection - it's the only sane
   way to implement this library without a separate daemon.  If you've got
   too many open processes, I recommend reducing the default (28800 seconds -
   8 hours) timeout in MySQL to something like 60 seconds.  You can do this by
   editing/creating /etc/my.cnf and adding the following:
     [mysqld]
     set-variable=wait_timeout=60

Q: Why isn't it working?
A: See the file 'DEBUGGING' provided with the distribution.

Q: Why doesn't ProFTPD see my accounts in the database?
A: You must set 'PersistentPasswd' to 'Off' in your proftpd configuration.
   You may also need to set your PAM config to use pam_unix.so.

Q: Why do I get the following message when I try to use 'passwd' on Solaris?
   "Supported configurations for passwd management are as follows" ...
A: Sun chose to write their unix PAM module to only allow a very restrictive
   configuration in /etc/nsswitch.conf.  You must now specify '-r files' on
   the 'passwd' command-line to manipulate the password file.  For example:
   passwd -r files username
   I know this sucks, so figuring out a better workaround is on my TODO list.

Q: Why do I get the following message when compiling on Solaris?

    Undefined                       first referenced
     symbol                             in file
    (some-symbol-here)                  /usr/local/lib/mysql/libmysqlclient.so
A: There are a number of reasons for this, but basically you either need to:
   a) change the linker you're using
   b) add a library to the link line
   - To change the linker, simply set the environment variable 'LD' to the
     full path to the linker you want to use before running 'configure.
     Usually you'll need to download and install the GNU 'ld' from the GNU
     binutils package.
   - To use the same linker but add the missing library, locate your libgcc.a
     file from your GCC installation, and set the environment variable
     'LDFLAGS' to the following before running 'configure':
     -L/directory/containing/libgcc.a -lgcc

Q: I'm getting segfaults on Solaris; truss indicates a crash shortly after
   libz can't be found.

A: If you're using Solaris 8+, this shouldn't be a problem as libz is included
   with the OS.  On earlier versions, you've probably installed it into
   /usr/local/lib or somewhere in /opt.  You need to make sure this directory
   is included in the linker search PRIOR to building libnss-mysql.  If libz
   is installed in /usr/local/lib, you'd need to do the following:
   sh -c "LDFLAGS=-R/usr/local/lib ./configure"

