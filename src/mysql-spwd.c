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
#ifdef HAVE_NSS_H
_nss_mysql_getspnam_r (const char *name, struct spwd *result, char *buffer,
                       size_t buflen, int *errnop)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getspnam_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER
  LOCK;
#ifdef HAVE_NSS_H
  retVal = _nss_mysql_lookup (BYNAME, name, 0, conf.sql.query.getspnam,
                              true, result, buffer, buflen, errnop,
                              _nss_mysql_load_shadow, &mresult, __FUNCTION__);
#else
  retVal = _nss_mysql_lookup (BYNAME, NSS_ARGS(args)->key.name, 0,
                              conf.sql.query.getspnam, true,
                              NSS_ARGS(args)->buf.result,
                              NSS_ARGS(args)->buf.buffer,
                              NSS_ARGS(args)->buf.buflen,
                              &NSS_ARGS(args)->erange,
                              _nss_mysql_load_shadow, &mresult, __FUNCTION__);
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
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
#ifdef HAVE_NSS_H
_nss_mysql_getspent_r (struct spwd *result, char *buffer, size_t buflen,
                       int *errnop)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getspent_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;

  DENTER
  LOCK;
#ifdef HAVE_NSS_H
  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getspent,
                              true, result, buffer, buflen, errnop,
                              _nss_mysql_load_shadow, &mresult_spent, __FUNCTION__);
#else
  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getspent,
                              true, NSS_ARGS(args)->buf.result,
                              NSS_ARGS(args)->buf.buffer,
                              NSS_ARGS(args)->buf.buflen,
                              &NSS_ARGS(args)->erange,
                              _nss_mysql_load_shadow, &mresult_spent, __FUNCTION__);
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
  DSRETURN (retVal)
}

#ifdef HAVE_NSS_COMMON_H

static nss_backend_op_t shadow_ops[] = {
    _nss_mysql_default_destr,       /* NSS_DBOP_DESTRUCTOR */
    _nss_mysql_endspent,            /* NSS_DBOP_ENDENT */
    _nss_mysql_setspent,            /* NSS_DBOP_SETENT */
    _nss_mysql_getspent_r,          /* NSS_DBOP_GETENT */
    _nss_mysql_getspnam_r           /* NSS_DBOP_SHADOW_BYNAME */
};

CONSTR(shadow)

#endif
#endif /* HAVE_SHADOW_H */
