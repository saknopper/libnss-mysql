/* Copyright (C) 2002 Ben Goodwin
   This file is part of the nss-mysql library.
  
   The nss-mysql library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   The nss-mysql library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with the nss-mysql library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "lookup.h"
#include <grp.h>

extern conf_t conf;

MYSQL_RES *mresult_grent = NULL;

/*
 * getgrnam
 */
NSS_STATUS
_nss_mysql_getgrnam_r (const char *name, struct group *result, char *buffer,
                       size_t buflen, int *errnop)
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER

  retVal = _nss_mysql_lookup (BYNAME, name, 0, conf.sql.query.getgrnam,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_group, &mresult, __FUNCTION__);

  DSRETURN (retVal)
}

/*
 * getgrgid
 */
NSS_STATUS
_nss_mysql_getgrgid_r (uid_t uid, struct group *result, char *buffer,
                       size_t buflen, int *errnop)
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER

  retVal = _nss_mysql_lookup (BYNUM, NULL, uid, conf.sql.query.getgrgid,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_group, &mresult, __FUNCTION__);

  DSRETURN (retVal)
}

/*
 * endgrent
 */
ENDENT(grent)

/*
 * setgrent
 */
SETENT(grent)

/*
 * getgrent
 */
NSS_STATUS
_nss_mysql_getgrent_r (struct group *result, char *buffer, size_t buflen,
                       int *errnop)
{
  int retVal;

  DENTER

  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getgrent,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_group, &mresult_grent, __FUNCTION__);

  DSRETURN (retVal)
}

/*
 * initgroups/getgrmem
 */
NSS_STATUS
_nss_mysql_initgroups_dyn (const char *user, gid_t group, long int *start,
                           long int *size, gid_t **groupsp, long int limit,
                           int *errnop)
{
  int retVal;
  MYSQL_RES *mresult = NULL;
  group_info_t gi;

  DENTER
  gi.start = start;
  gi.size = size;
  gi.limit = limit;
  gi.groupsp = groupsp;
  gi.group = (long int) group;


  retVal = _nss_mysql_lookup (BYNAME, user, 0, conf.sql.query.gidsbymem,
                              false, &gi, NULL, 0, errnop,
                              _nss_mysql_load_gidsbymem, &mresult,
                              "initgroups");

  if (retVal != NSS_SUCCESS)
    DSRETURN (retVal)

  DSRETURN (NSS_SUCCESS)
}
