$Id$

*** In all cases, you should at *least* restart any daemons that look up
*** users, such as ssh, nscd, cron, etc.  It's best if you simply reboot
*** unless you know the complete list of daemons to restart.
*** On Linux, try (as root) "lsof /lib/libnss_mysql.so" 

1.4 -> 1.5
    * This is a maintenance release.  No changes are needed on your part.

1.3 -> 1.4
    * No changes are *required* but the following options have been removed
      from libnss-mysql.cfg in favor of using "my.cnf" options: timeout,
      compress, initcmd.  Specify them in either the [client] section or a
      new [libnss-mysql] section in my.cnf

1.2 -> 1.3
    * No changes are *required* but the [section] names in the config files
      are now meaningless - remove them to avoid additional parsing overhead

    * You may want to take advantage of line continuation in the configuration
      files.  See the samples for examples.

1.1 -> 1.2
    * This is a maintenance release.  No changes are needed on your part.

1.0 -> 1.1
    * This is a maintenance release.  No changes are needed on your part.
      If compiling from source, you should note that the new 'configure'
      option '--with-mysql=DIR' which replaces '--with-mysql-inc' and
      '--with-mysql-lib'

0.9 -> 1.0
    * If you're running anything other than Linux, you *MUST* update your
      /etc/libnss-mysql.cfg file!  Each operating system now has their own
      independant password fields.  You'll need to add some columns to your
      queries; Solaris adds 'age' and 'comment'.  This is the first version
      that supports FreeBSD so there's no upgrade concerns here.  See the
      sample configs for details on where to put the new columns.

0.8 -> 0.9
    * IMPORTANT: The filenames for the configuration files has changed.
      There's also been a slight change inside the configuration files.
      It's MANDATORY that you follow these upgrade procedures.

    * /etc/nss_mysql.cfg is now /etc/libnss-mysql.cfg

    * /etc/nss_mysql_root.cfg is now /etc/libnss-mysql-root.cfg

    * Only one server can be defined in your configuration files.
      Delete any [secondary] sections and rename your [primary] section
      to [server].  The [global] section no longer has any meaning.  Remove
      it and any entries (IE 'retry') it has.  See the sample configuration
      files for more information

    * A "make install" and RPM upgrades will NOT rename your existing
      configuration files.  They will install new default ones under
      their new names.  Overwrite them with your old files.

0.7a -> 0.8
    * The /etc/my.cnf section, [libnss_mysql], is no longer supported.
      Use the new options in /etc/nss_mysql.cfg instead (see below).

    * The "port" and "socket" options are no longer required options
      in /etc/nss_mysql.cfg.  Remove them from your config to use your
      MySQL client defaults.

    * New options in /etc/nss_mysql.cfg (that go in the [primary] and/or
      [secondary] sections):
      Option    Values  Default  Description
      -----------------------------------------------------------------
      ssl       0|1     0        Use SSL (MySQL 4.x only)?
      timeout   0+      3        Connect timeout in seconds
      compress  0|1     0        Use MySQL compressed protocol?
      initcmd   string  ""       Execute this statement at connect time

0.7 -> 0.7a
    This is a maintenance release.  No changes are needed on your part.

0.6 -> 0.7
    No changes are needed on your part.  Simply install the new version!

0.5 -> 0.6
    No changes are needed on your part.  Simply install the new version!

0.4 -> 0.5
    NOTE: This included a lot of redesign/rewriting.  It's very possible
          I broke something that used to work.  Please report bugs to me.

    Debugging support has been removed - for now.  The config option
    'debug_flags' is now ignored.
    The syslog config options, 'facility' and 'priority' have been removed.
    Specify the facility at build time using the --with-log-facility
    option to the 'configure' program.
    Not that it really eats that many CPU cycles, but I recommend
    deleting those 3 configuration options from your config file.

    Since this version supports initgroups, the design of the group
    database needs to be altered.  You *must* alter your database/data
    in order for groups to work.  If you're not using groups, then you can
    stop reading.
    * First, alter your database such that you have a way to associate
      gids with a username.  Check the sample database for the 'grouplist'
      table to see what I mean.
    * Second, populate this new table with the appropriate data.
    * Third, REMOVE the fourth column in your SELECT statements for the
      getgr* configuration entries (getgrnam, getgrgid, & getgrent)
    * Fourth, add two new configuration entries for 'memsbygid' and
      'gidsbymem'.  See the new sample configuration file for examples.
    * Fifth, when you're comfortable you've converted the data, you can
      remove the fourth column mentioned above from your database - it's no
      longer needed.
    Two select calls will be made when retrieving group information - one
    for the getgr* call, and another to fetch the list of usernames that
    are members of that group (using the memsbygid call).
    The gidsbymem call is used for initgroups support.  This allows the
    system to find out all the groups a user is a member of *much* more
    efficiently.  In fact, some programs ONLY do it this way, and weren't
    seeing the supplemental groups users were in - until now.

0.3 -> 0.4
    First, you should note that the code currently supports only TWO
    servers.  That's easily changed, and I'll probably up it to a
    default of THREE later, but 0.4 is released with support for TWO.
    That said, the [server] section in BOTH of your config files must
    be changed to either [primary] or [secondary] as appropriate.  See
    the sample config files if it's not clear.

0.2 -> 0.3
    Simply change any '%d' you have in your configs to '%u'

