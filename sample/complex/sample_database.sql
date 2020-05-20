#
# Complex sample libnss-mysql database
#
# $Id$
#
# sample_database.sql revision      compatible libnss-mysql versions
# -------------------------------------------------------------------
# 1.1                               0.2+
#
# Use 'mysql -u root -p < sample_database.sql' to load this example into MySQL
#

# This particular example is actually a modified version of the database
# in use at the ISP I originally developed this for.  It is designed this
# way in order to be able to define one "person" and have that one "person"
# have multiple "services" (accounts) without duplicating the "person"
# over and over again.  Services are linked to customers via the cust_num
# field.  The "service_defs" table enables you to define different
# homedir and shell structures depending on the service ("product" in
# their "services" entry) they purchased.  This was useful to separate
# dialup users and email-only users as I configured cistron radius to
# disallow users with a certain shell.
# Note that I do not make use of the shadow 'expire' field - instead, when
# a user 'expires' the stop showing up in the system, as if deleted.
# Setting the 'suspend' field for a user will do the same thing.  I designed
# it this way because, back when I implemented all of this, a lot of
# software did not pay attention to the shadow expire field.

create database auth;
use auth;

# The tables ...
CREATE TABLE `customer` (
  cust_num int(11) NOT NULL auto_increment,
  first_name varchar(25) NOT NULL default '',
  last_name varchar(25) NOT NULL default '',
  middle_initial char(1) default NULL,
  company varchar(50) default NULL,
  address_one varchar(35) default NULL,
  address_two varchar(35) default NULL,
  city varchar(25) default NULL,
  state char(2) default NULL,
  zip varchar(10) default NULL,
  home_phone varchar(20) default NULL,
  work_phone varchar(20) default NULL,
  notes text,
  signupdate date NOT NULL default '0000-00-00',
  PRIMARY KEY  (cust_num)
);

CREATE TABLE `service_defs` (
  name varchar(25) NOT NULL default '',
  shell varchar(255) NOT NULL default '/bin/date',
  homedir varchar(255) NOT NULL default '/tmp',
  PRIMARY KEY  (name)
);

CREATE TABLE `services` (
  cust_num int(11) NOT NULL default '0',
  username varchar(16) NOT NULL default '',
  uid int(11) NOT NULL auto_increment,
  gid int(11) NOT NULL default '100',
  password varchar(16) NOT NULL default '',
  product varchar(25) default NULL,
  created date NOT NULL default '0000-00-00',
  expire date NOT NULL default '0000-00-00',
  suspended set('Y','N') NOT NULL default 'N',
  notes text,
  PRIMARY KEY  (uid),
  UNIQUE KEY username (username),
  KEY cust_num (cust_num)
) AUTO_INCREMENT=5000;

# The data ...
INSERT INTO `customer` (first_name,last_name,middle_initial)
    VALUES ('Benjamin','Goodwin','C');
INSERT INTO `services` (cust_num,username,password,product,created,expire)
    VALUES (LAST_INSERT_ID(),'cinergi',ENCRYPT('cinergi'),'Basic Dialup',NOW(),
            DATE_ADD(NOW(), INTERVAL 1 YEAR));
INSERT INTO `service_defs` (name)
    VALUES ('Basic Dialup');

# The permissions ...
GRANT USAGE ON *.* TO `nss-root`@`localhost` IDENTIFIED BY 'rootpass';
GRANT USAGE ON *.* TO `nss-user`@`localhost` IDENTIFIED BY 'userpass';

GRANT Select (`cust_num`, `uid`, `gid`, `password`, `product`, `expire`,
              `suspended`, `username`)
    ON `auth`.`services`
    TO 'nss-root'@'localhost';
GRANT Select (`cust_num`, `first_name`, `last_name`, `middle_initial`)
    ON `auth`.`customer`
    TO 'nss-root'@'localhost';
GRANT Select (`name`,`shell`,`homedir`)
    ON `auth`.`service_defs`
    TO 'nss-root'@'localhost';

GRANT Select (`cust_num`, `uid`, `gid`, `product`, `expire`, `suspended`,
              `username`)
    ON `auth`.`services`
    TO 'nss-user'@'localhost';
GRANT Select (`cust_num`, `first_name`, `last_name`, `middle_initial`)
    ON `auth`.`customer`
    TO 'nss-user'@'localhost';
GRANT Select (`name`,`shell`,`homedir`)
    ON `auth`.`service_defs`
    TO 'nss-user'@'localhost';

