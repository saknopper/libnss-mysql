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
#ifdef HAVE_SHADOW_H
#include <shadow.h>

extern conf_t conf;

MYSQL_RES *mresult_spent = NULL;

/*
 * getspnam
 */
NSS_STATUS
_nss_mysql_getspnam_r (const char *name, struct spwd *result, char *buffer,
                       size_t buflen, int *errnop)
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER
  LOCK;
  retVal = _nss_mysql_lookup (BYNAME, name, 0, conf.sql.query.getspnam,
                              true, result, buffer, buflen, errnop,
                              _nss_mysql_load_shadow, &mresult, __FUNCTION__);
  UNLOCK;
  DSRETURN (retVal)
}

/*
 * endspent
 */
ENDENT(spent)

/*
 * setspent
 */
SETENT(spent)

/*
 * getspent
 */
NSS_STATUS
_nss_mysql_getspent_r (struct spwd *result, char *buffer, size_t buflen,
                       int *errnop)
{
  int retVal;

  DENTER
  LOCK;
  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getspent,
                              true, result, buffer, buflen, errnop,
                              _nss_mysql_load_shadow, &mresult_spent, __FUNCTION__);
  UNLOCK;
  DSRETURN (retVal)
}

#endif /* HAVE_SHADOW_H */
