#
# Minimalist sample libnss-mysql database
#
# $Id$
#
# sample_database.sql revision      compatible libnss-mysql versions
# -------------------------------------------------------------------
# 1.1                               0.5+
#
# Use 'mysql -u root -p < sample_database.sql' to load this example into MySQL
#
# This particular example stores as little as possible in the MySQL database.
# The 'users' table only has username/uid/password in it.  The rest is
# 'hard coded' via the configuration files.
# The groups database remains unchanged from the main sample as it is
# already pretty minimal.  If you do not require MySQL group support,
# simply delete the 'groups' and 'grouplist' tables and do not configure
# /etc/nsswitch.conf with 'mysql' on the 'group' line

# This configuration suits something like an ISP where all users may have
# the same gid, home directory base, shell, etc.  You can really do
# some funky things inside the configuration files if you wanted to, such
# as define a different shell depending on the uid returned; that just
# takes a bit of SQL query magic...

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
  password varchar(34) NOT NULL default 'x',
  PRIMARY KEY  (uid),
  UNIQUE KEY username (username),
  KEY uid (uid)
) TYPE=MyISAM AUTO_INCREMENT=5000;

# The data ...
INSERT INTO users (username,password) VALUES ('cinergi', ENCRYPT('cinergi'));
INSERT INTO groups (name) VALUES ('foobaz');
INSERT INTO grouplist (gid,username) VALUES (5000,'cinergi');

# The permissions ...
GRANT USAGE ON *.* TO `nss-root`@`localhost` IDENTIFIED BY 'rootpass';
GRANT USAGE ON *.* TO `nss-user`@`localhost` IDENTIFIED BY 'userpass';

GRANT Select (`username`, `uid`, `password`)
             ON `auth`.`users`
             TO 'nss-root'@'localhost';
GRANT Select (`name`, `password`, `gid`)
             ON `auth`.`groups`
             TO 'nss-root'@'localhost';

GRANT Select (`username`, `uid`)
             ON `auth`.`users`
             TO 'nss-user'@'localhost';
GRANT Select (`name`, `password`, `gid`)
             ON `auth`.`groups`
             TO 'nss-user'@'localhost';

GRANT Select (`username`, `gid`)
             ON `auth`.`grouplist`
             TO 'nss-user'@'localhost';
GRANT Select (`username`, `gid`)
             ON `auth`.`grouplist`
             TO 'nss-root'@'localhost';

