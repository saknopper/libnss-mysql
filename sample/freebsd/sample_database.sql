#
# $Id$
#
#                THIS DATABASE IS INTENDED FOR FreeBSD
#
# Use 'mysql -u root -p < sample_database.sql' to load this example into your
# MySQL server.
# This example will:
#   1) create a database called 'auth'
#   2) add three tables: 'users', 'groups' and 'grouplist'
#   3) add some data to each table
#   4) create two MySQL users ('nss-user' and 'nss-root') with appropriate
#      SELECT privs.
#
# With a properly-functioning libnss-mysql, you should be able to log into
# the system as 'cinergi' with a password of 'cinergi'.  'cinergi' should be
# a member of the group 'foobaz' as well.
#
# This is intended as an *example* and is perhaps not the best use of
# datatypes, space/size, data normalization, etc.
#

create database auth;
use auth;

# The tables ...
CREATE TABLE groups (
  name varchar(16) NOT NULL default '',
  password varchar(34) NOT NULL default 'x',
  gid int(11) NOT NULL auto_increment,
  PRIMARY KEY  (gid)
) TYPE=MyISAM AUTO_INCREMENT=5000;

CREATE TABLE grouplist (
  rowid int(11) NOT NULL auto_increment,
  gid int(11) NOT NULL default '0',
  username char(16) NOT NULL default '',
  PRIMARY KEY  (rowid)
) TYPE=MyISAM;

CREATE TABLE users (
  username varchar(16) NOT NULL default '',
  uid int(11) NOT NULL auto_increment,
  gid int(11) NOT NULL default '5000',
  pwchange int(11) NOT NULL default '0',
  class varchar(64) NOT NULL default '',
  gecos varchar(128) NOT NULL default '',
  homedir varchar(255) NOT NULL default '',
  shell varchar(64) NOT NULL default '/bin/sh',
  password varchar(34) NOT NULL default 'x',
  expire bigint(20) NOT NULL default '0',
  PRIMARY KEY  (uid),
  KEY username (username),
  KEY uid (uid)
) TYPE=MyISAM AUTO_INCREMENT=5000;

# The data ...
INSERT INTO users (username,gecos,homedir,password)
    VALUES ('cinergi', 'Ben Goodwin', '/home/cinergi', ENCRYPT('cinergi'));
INSERT INTO groups (name)
    VALUES ('foobaz');
INSERT INTO grouplist (gid,username)
    VALUES (5000,'cinergi');

# The permissions ...
GRANT USAGE ON *.* TO `nss-root`@`localhost` IDENTIFIED BY 'rootpass';
GRANT USAGE ON *.* TO `nss-user`@`localhost` IDENTIFIED BY 'userpass';

GRANT Select (`username`, `uid`, `gid`, `gecos`, `homedir`, `shell`,
              `pwchange`,`class`,`expire`)
             ON `auth`.`users`
             TO 'nss-user'@'localhost';
GRANT Select (`username`, `uid`, `gid`, `gecos`, `homedir`, `shell`, `password`,
              `pwchange`,`class`,`expire`)
             ON `auth`.`users`
             TO 'nss-root'@'localhost';

GRANT Select (`name`, `password`, `gid`)
             ON `auth`.`groups`
             TO 'nss-user'@'localhost';
GRANT Select (`name`, `password`, `gid`)
             ON `auth`.`groups`
             TO 'nss-root'@'localhost';

GRANT Select (`username`, `gid`)
             ON `auth`.`grouplist`
             TO 'nss-user'@'localhost';
GRANT Select (`username`, `gid`)
             ON `auth`.`grouplist`
             TO 'nss-root'@'localhost';

