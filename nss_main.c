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
    "$Id$";

#include "nss_mysql.h"
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <pwd.h>
#include <shadow.h>
#include <grp.h>
#include <pthread.h>
#include <stdarg.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK pthread_mutex_lock (&lock);
#define UNLOCK pthread_mutex_unlock (&lock);

conf_t  conf = {0, 0, {DEF_RETRY, DEF_FACIL, DEF_PRIO, DEF_DFLAGS} };

#define GET(funcname, sname, argtype, restrict)                               \
  NSS_STATUS                                                                  \
  _nss_mysql_##funcname##_r (argtype arg, struct sname *result,               \
                             char *buffer, size_t buflen)                     \
  {                                                                           \
    int retVal;                                                               \
    int size;                                                                 \
    char *query;                                                              \
                                                                              \
    function_enter;                                                           \
    if (restrict && geteuid () != 0)                                          \
      function_return (NSS_NOTFOUND);                                         \
    if (!result)                                                              \
      {                                                                       \
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL RESULT", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (!buffer)                                                              \
      {                                                                       \
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL BUFFER", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    LOCK;                                                                     \
    if (_nss_mysql_init (&conf) != NSS_SUCCESS)                               \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (!conf.sql.query.funcname)                                             \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    size = strlen (conf.sql.query.funcname) + 1 + PADSIZE;                    \
    query = xmalloc (size);                                                   \
    if (query == NULL)                                                        \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    snprintf (query, size, conf.sql.query.funcname, arg);                     \
    _nss_mysql_reset_ent ();                                                  \
    if (_nss_mysql_run_query (conf, query) != NSS_SUCCESS)                    \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    retVal = _nss_mysql_load_result (result, buffer, buflen, sname##_fields); \
    _nss_mysql_close_sql (CLOSE_RESULT);                                      \
    UNLOCK;                                                                   \
    function_return (retVal);                                                 \
  }

#define ENDENT(type)                                                          \
  NSS_STATUS                                                                  \
  _nss_mysql_end##type (void)                                                 \
  {                                                                           \
    function_enter;                                                           \
    LOCK;                                                                     \
    _nss_mysql_reset_ent ();                                                  \
    UNLOCK;                                                                   \
    function_return (NSS_SUCCESS);                                            \
  }

#define SETENT(type)                                                          \
  NSS_STATUS                                                                  \
  _nss_mysql_set##type (void)                                                 \
  {                                                                           \
    function_enter;                                                           \
    LOCK;                                                                     \
    _nss_mysql_reset_ent ();                                                  \
    UNLOCK;                                                                   \
    function_return (NSS_SUCCESS);                                            \
  }

#define GETENT(type, sname)                                                   \
  NSS_STATUS                                                                  \
  _nss_mysql_get##type##_r (struct sname *result, char *buffer,               \
                            size_t buflen)                                    \
  {                                                                           \
    int retVal;                                                               \
    int size;                                                                 \
    char *query;                                                              \
                                                                              \
    function_enter;                                                           \
    if (!result)                                                              \
      {                                                                       \
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL RESULT", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (!buffer)                                                              \
      {                                                                       \
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL BUFFER", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    LOCK;                                                                     \
    if (_nss_mysql_init (&conf) != NSS_SUCCESS)                               \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (_nss_mysql_active_result () == nfalse)                                \
      {                                                                       \
        if (!conf.sql.query.get##type)                                        \
          {                                                                   \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
        size = strlen (conf.sql.query.get##type) + 1;                         \
        query = xmalloc (size);                                               \
        if (query == NULL)                                                    \
          {                                                                   \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
        strcpy (query, conf.sql.query.get##type);                             \
                                                                              \
        if (_nss_mysql_run_query (conf, query) != NSS_SUCCESS)                \
          {                                                                   \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
      }                                                                       \
    retVal = _nss_mysql_load_result (result, buffer, buflen, sname##_fields); \
    UNLOCK;                                                                   \
    function_return (retVal);                                                 \
  }

/*
 * Do *NOT* change this to maintain persistent connection - it will fail
 * under certain circumstances where programs using this library open and
 * close their own file descriptors.  I save the MySQL socket information
 * for later comparison for the same reason.
 */
void
_nss_mysql_log (int priority, char *fmt, ...)
{
  va_list ap;

  if (priority > conf.global.syslog_priority)
    return;

  openlog (PACKAGE, OPENLOG_OPTIONS, conf.global.syslog_facility);
  va_start (ap, fmt);
  vsyslog (priority, fmt, ap);
  va_end (ap);
  closelog ();
}

void
_nss_mysql_debug (char *function, int flags, char *fmt, ...)
{
  va_list ap;
  char string[MAX_LOG_LEN];

  if (conf.global.syslog_priority < LOG_DEBUG)
    return;

  if (!(flags & conf.global.debug_flags))
    return;

  snprintf (string, MAX_LOG_LEN, "%s: ", function);
  va_start (ap, fmt);
  vsnprintf (string + strlen (string), MAX_LOG_LEN, fmt, ap);
  va_end (ap);
  _nss_mysql_log (LOG_DEBUG, "%s", string);
}

/* "Create" functions using the defines above ... */
GET (getpwnam, passwd, const char *, 0);
GET (getpwuid, passwd, uid_t, 0);
GET (getspnam, spwd, const char *, 1);
GET (getgrnam, group, const char *, 0);
GET (getgrgid, group, gid_t, 0);
ENDENT (pwent);
SETENT (pwent);
GETENT (pwent, passwd);
ENDENT (spent);
SETENT (spent);
GETENT (spent, spwd);
ENDENT (grent);
SETENT (grent);
GETENT (grent, group);

