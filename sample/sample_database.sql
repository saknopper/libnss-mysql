#
# $Id$
#
# Use 'mysql -u root < sample_database.sql' to load this example into your
# MySQL server.
# This example will create a database called 'auth', add two tables called
# 'users' and 'groups', add a single entry to each, and create two MySQL
# users ('nss-user' and 'nss-root') with appropriate SELECT privs.
#
# With a properly-functioning libnss-mysql, you should be able to log into
# the system as 'cinergi' with a password of 'cinergi'
#
# This is intended as an *example* and is perhaps not the best use of
# datatypes, space/size, etc.  I (Ben Goodwin) have used this database
# with 186,000 entries, consuming about 25 meg of disk space (8-character
# username and gecos), resulting in 0.00 to 0.01 second lookups for the
# getpw* routines, and 2.5 seconds for full get*ent (IE finger w/out the
# '-m' switch) routines on an athlon 1333.
#

create database auth ;
use auth ;

CREATE TABLE groups (
  rowid int(11) NOT NULL auto_increment,
  name varchar(16) NOT NULL default '',
  password varchar(34) NOT NULL default '',
  gid int(11) NOT NULL default '5000',
  members varchar(255) NOT NULL default '',
  PRIMARY KEY  (rowid)
) TYPE=MyISAM;

INSERT INTO groups VALUES (1, 'foobaz', 'x', 5000, 'cinergi');

CREATE TABLE users (
  rowid int(11) NOT NULL auto_increment,
  username varchar(16) NOT NULL default '',
  uid int(11) NOT NULL default '5000',
  gid int(11) NOT NULL default '5000',
  gecos varchar(128) NOT NULL default '',
  homedir varchar(255) NOT NULL default '',
  shell varchar(64) NOT NULL default '',
  password varchar(34) NOT NULL default '',
  lstchg bigint(20) NOT NULL default '1',
  min bigint(20) NOT NULL default '0',
  max bigint(20) NOT NULL default '99999',
  warn bigint(20) NOT NULL default '0',
  inact bigint(20) NOT NULL default '0',
  expire bigint(20) NOT NULL default '-1',
  flag bigint(20) unsigned NOT NULL default '0',
  PRIMARY KEY  (rowid),
  UNIQUE KEY rowid (rowid),
  KEY username (username),
  KEY uid (uid)
) TYPE=MyISAM;

INSERT INTO users VALUES (1, 'cinergi', 5000, 5000, 'Ben Goodwin', '/home/cinergi', '/bin/bash', ENCRYPT('cinergi'), 1, 0, 99999, 0, 0, -1, 0);

GRANT USAGE ON *.* TO `nss-root`@`localhost` IDENTIFIED BY 'rootpass' ;
GRANT USAGE ON *.* TO `nss-user`@`localhost` IDENTIFIED BY 'userpass' ;

GRANT Select (`username`, `uid`, `gid`, `gecos`, `homedir`, `shell`, `password`, `lstchg`, `min`, `max`, `warn`, `inact`, `expire`, `flag`) ON `auth`.`users` TO 'nss-root'@'localhost' ;
GRANT Select (`name`, `password`, `gid`, `members`) ON `auth`.`groups` TO 'nss-root'@'localhost' ;

GRANT Select (`username`, `uid`, `gid`, `gecos`, `homedir`, `shell`) ON `auth`.`users` TO 'nss-user'@'localhost' ;
GRANT Select (`name`, `password`, `gid`, `members`) ON `auth`.`groups` TO 'nss-user'@'localhost' ;

