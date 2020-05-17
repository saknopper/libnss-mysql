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
#ifdef HAVE_NSS_H
_nss_mysql_getpwnam_r (const char *name, struct passwd *result, char *buffer,
                       size_t buflen, int *errnop)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getpwnam_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER
  LOCK;
#ifdef HAVE_NSS_H
  retVal = _nss_mysql_lookup (BYNAME, name, 0, conf.sql.query.getpwnam,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_passwd, &mresult, __FUNCTION__);
#else
  retVal = _nss_mysql_lookup (BYNAME, NSS_ARGS(args)->key.name, 0,
                              conf.sql.query.getpwnam, false,
                              NSS_ARGS(args)->buf.result,
                              NSS_ARGS(args)->buf.buffer,
                              NSS_ARGS(args)->buf.buflen,
                              &NSS_ARGS(args)->erange,
                              _nss_mysql_load_passwd, &mresult, __FUNCTION__);
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
  DSRETURN (retVal)
}

/*
 * getpwuid
 */
NSS_STATUS
#ifdef HAVE_NSS_H
_nss_mysql_getpwuid_r (uid_t uid, struct passwd *result, char *buffer,
                       size_t buflen, int *errnop)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getpwuid_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  DENTER
  LOCK;
#ifdef HAVE_NSS_H
  retVal = _nss_mysql_lookup (BYNUM, NULL, uid, conf.sql.query.getpwuid,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_passwd, &mresult, __FUNCTION__);
#else
  retVal = _nss_mysql_lookup (BYNUM, NULL, NSS_ARGS(args)->key.uid,
                              conf.sql.query.getpwuid, false,
                              NSS_ARGS(args)->buf.result,
                              NSS_ARGS(args)->buf.buffer,
                              NSS_ARGS(args)->buf.buflen,
                              &NSS_ARGS(args)->erange,
                              _nss_mysql_load_passwd, &mresult, __FUNCTION__);
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
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
#ifdef HAVE_NSS_H
_nss_mysql_getpwent_r (struct passwd *result, char *buffer, size_t buflen,
                       int *errnop)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getpwent_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;

  DENTER
  LOCK;
#ifdef HAVE_NSS_H
  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getpwent,
                              false, result, buffer, buflen, errnop,
                              _nss_mysql_load_passwd, &mresult_pwent, __FUNCTION__);
#else
  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, conf.sql.query.getpwent,
                              false, NSS_ARGS(args)->buf.result,
                              NSS_ARGS(args)->buf.buffer,
                              NSS_ARGS(args)->buf.buflen,
                              &NSS_ARGS(args)->erange,
                              _nss_mysql_load_passwd, &mresult_pwent, __FUNCTION__);
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
  DSRETURN (retVal)
}

#ifdef HAVE_NSS_COMMON_H

static nss_backend_op_t passwd_ops[] = {
    _nss_mysql_default_destr,       /* NSS_DBOP_DESTRUCTOR */
    _nss_mysql_endpwent,            /* NSS_DBOP_ENDENT */
    _nss_mysql_setpwent,            /* NSS_DBOP_SETENT */
    _nss_mysql_getpwent_r,          /* NSS_DBOP_GETENT */
    _nss_mysql_getpwnam_r,          /* NSS_DBOP_PASSWD_BYNAME */
    _nss_mysql_getpwuid_r           /* NSS_DBOP_PASSWD_BYUID */
};

CONSTR(passwd)

#endif
