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

static const char rcsid[] =
    "$Id$ ";

#include "lookup.h"
#include <grp.h>

extern conf_t conf;

MYSQL_RES *mresult_grent = NULL;

/*
 * getgrnam
 */
NSS_STATUS
#ifdef HAVE_NSS_H
_nss_mysql_getgrnam_r (const char *name, struct group *result, char *buffer,
                       size_t buflen)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getgrnam_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;
  MYSQL_RES *mresult = NULL;

  function_enter;
  LOCK;
#ifdef HAVE_NSS_H
  retVal = _nss_mysql_lookup (BYNAME, name, 0, &conf.sql.query.getgrnam,
                              nfalse, result, buffer, buflen,
                              _nss_mysql_load_group, &mresult, FNAME);
#elif defined(HAVE_NSS_COMMON_H)
  retVal = _nss_mysql_lookup (BYNAME, NSS_ARGS(args)->key.name,
                              0, &conf.sql.query.getgrnam,
                              nfalse, NSS_ARGS(args)->buf.result,
                              NSS_ARGS(args)->buf.buffer,
                              NSS_ARGS(args)->buf.buflen,
                              _nss_mysql_load_group, &mresult, FNAME);
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
  function_return (retVal);
}

/*
 * getgrgid
 */
NSS_STATUS
#ifdef HAVE_NSS_H
_nss_mysql_getgrgid_r (uid_t uid, struct group *result, char *buffer,
                       size_t buflen)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getgrgid_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;
  MYSQL_RES *mresult = NULL;
#ifdef HAVE_NSS_COMMON_H
  uid_t uid = NSS_ARGS(args)->key.uid;
  struct group *result = NSS_ARGS(args)->buf.result;
  char *buffer = NSS_ARGS(args)->buf.buffer;
  size_t buflen = NSS_ARGS(args)->buf.buflen;
#endif

  function_enter;
  LOCK;

  retVal = _nss_mysql_lookup (BYNUM, NULL, uid, &conf.sql.query.getgrgid,
                              nfalse, result, buffer, buflen,
                              _nss_mysql_load_group, &mresult, FNAME);
#ifdef HAVE_NSS_COMMON_H
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
  function_return (retVal);
}

/*
 * endgrent
 */
ENDENT(grent);

/*
 * setgrent
 */
SETENT(grent);

/*
 * getgrent
 */
NSS_STATUS
#ifdef HAVE_NSS_H
_nss_mysql_getgrent_r (struct group *result, char *buffer, size_t buflen)
#elif defined(HAVE_NSS_COMMON_H)
_nss_mysql_getgrent_r (nss_backend_t *be, void *args)
#endif
{
  int retVal;
#ifdef HAVE_NSS_COMMON_H
  struct group *result = NSS_ARGS(args)->buf.result;
  char *buffer = NSS_ARGS(args)->buf.buffer;
  size_t buflen = NSS_ARGS(args)->buf.buflen;
#endif

  function_enter;
  LOCK;

  retVal = _nss_mysql_lookup (BYNONE, NULL, 0, &conf.sql.query.getgrent,
                              nfalse, result, buffer, buflen,
                              _nss_mysql_load_group, &mresult_grent, FNAME);
#ifdef HAVE_NSS_COMMON_H
  if (retVal == NSS_SUCCESS)
    NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;
#endif
  UNLOCK;
  function_return (retVal);
}

/*
 * initgroups/getgrmem
 */
#ifdef HAVE_NSS_H
NSS_STATUS
_nss_mysql_initgroups_dyn (const char *user, gid_t group, long int *start,
                           long int *size, gid_t **groupsp, long int limit)
#endif
#ifdef HAVE_NSS_COMMON_H
NSS_STATUS
_nss_mysql_getgrmem (nss_backend_t *be, void *args)
#endif
{
  int retVal;
  MYSQL_RES *mresult = NULL;
  group_info_t gi;
#ifdef HAVE_NSS_COMMON_H
  const char *user = ((struct nss_groupsbymem *)args)->username;
  long int size;
  long int start;
#endif

  function_enter;
  LOCK;

#ifdef HAVE_NSS_H
  gi.start = start;
  gi.size = size;
  gi.limit = limit;
  gi.groupsp = groupsp;
  gi.group = (long int) group;
#elif defined(HAVE_NSS_COMMON_H)
  start = ((struct nss_groupsbymem *)args)->numgids;
  gi.start = &start;
  gi.limit = ((struct nss_groupsbymem *)args)->maxgids;
  size = sizeof (gid_t) * gi.limit;
  gi.size = &size;
  gi.groupsp = &(((struct nss_groupsbymem *)args)->gid_array);
  gi.group = -1;
#endif

  retVal = _nss_mysql_lookup (BYNAME, user, 0, &conf.sql.query.gidsbymem,
                              nfalse, &gi, NULL, 0,
                              _nss_mysql_load_gidsbymem, &mresult, FNAME);

#ifdef HAVE_NSS_COMMON_H
  return NSS_NOTFOUND;
#else
  return NSS_SUCCESS;
#endif
}

#ifdef HAVE_NSS_COMMON_H
static nss_backend_op_t group_ops[] = {
    _nss_mysql_default_destr,       /* NSS_DBOP_DESTRUCTOR */
    _nss_mysql_endgrent,            /* NSS_DBOP_ENDENT */
    _nss_mysql_setgrent,            /* NSS_DBOP_SETENT */
    _nss_mysql_getgrent_r,          /* NSS_DBOP_GETENT */
    _nss_mysql_getgrnam_r,          /* NSS_DBOP_PASSWD_BYNAME */
    _nss_mysql_getgrgid_r,          /* NSS_DBOP_PASSWD_BYUID */
    _nss_mysql_getgrmem
};

CONSTR(group);
#endif
