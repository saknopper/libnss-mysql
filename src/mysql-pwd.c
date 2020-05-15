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
#include <pwd.h>

extern conf_t conf;

MYSQL_RES *mresult_pwent = NULL;

/*
 * getpwnam
 */
NSS_STATUS
_nss_mysql_getpwnam_r (const char *name, struct passwd *result, char *buffer,
                       size_t buflen, int *errnop)
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER

  retVal = _nss_mysql_lookup (BYNAME, name, 0, conf.sql.query.getpwnam,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_passwd, &mresult, __FUNCTION__);

  DSRETURN (retVal)
}

/*
 * getpwuid
 */
NSS_STATUS
_nss_mysql_getpwuid_r (uid_t uid, struct passwd *result, char *buffer,
                       size_t buflen, int *errnop)
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER

  retVal = _nss_mysql_lookup (BYNUM, NULL, uid, conf.sql.query.getpwuid,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_passwd, &mresult, __FUNCTION__);

  DSRETURN (retVal)
}

/*
 * endpwent
 */
ENDENT(pwent)

/*
 * setpwent
 */
SETENT(pwent)

/*
 * getpwent
 */
NSS_STATUS
_nss_mysql_getpwent_r (struct passwd *result, char *buffer, size_t buflen,
                       int *errnop)
{
  int retVal;

  DENTER

  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getpwent,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_passwd, &mresult_pwent, __FUNCTION__);

  DSRETURN (retVal)
}
