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
#include <stdlib.h>
#include <stdarg.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK pthread_mutex_lock (&lock);
#define UNLOCK pthread_mutex_unlock (&lock);

conf_t  conf = { 0, 0, { DEF_RETRY, DEF_FACIL, DEF_PRIO, DEF_DFLAGS} };

#define _nss_mysql_free_query_list                                            \
    {                                                                         \
      int i;                                                                  \
      for (i = 0; i < MAX_SERVERS; i++)                                       \
        {                                                                     \
          if (query_list[i])                                                  \
            {                                                                 \
              free (query_list[i]);                                           \
              query_list[i] = NULL;                                           \
            }                                                                 \
        }                                                                     \
    }

#define GET(funcname, sname, argtype, restrict)                               \
  NSS_STATUS                                                                  \
  _nss_mysql_##funcname##_r (argtype arg, struct sname *result,               \
                             char *buffer, size_t buflen)                     \
  {                                                                           \
    int retVal, i;                                                            \
    int size;                                                                 \
    char *query_list[MAX_SERVERS];                                            \
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
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL buffer", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    LOCK;                                                                     \
    memset (query_list, 0, sizeof (query_list));                              \
    if (_nss_mysql_init (&conf) != NSS_SUCCESS)                               \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    for (i = 0; i < MAX_SERVERS; i++)                                         \
      {                                                                       \
        if (!conf.sql[i].query.funcname)                                      \
          continue;                                                           \
        size = strlen (conf.sql[i].query.funcname) + 1 + PADSIZE;             \
        query_list[i] = malloc (size);                                        \
        if (query_list[i] == NULL)                                            \
          {                                                                   \
            _nss_mysql_free_query_list;                                       \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
        snprintf (query_list[i], size, conf.sql[i].query.funcname, arg);      \
      }                                                                       \
    _nss_mysql_reset_ent ();                                                  \
    if (_nss_mysql_run_query (conf, query_list) != NSS_SUCCESS)               \
      {                                                                       \
        _nss_mysql_free_query_list;                                           \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    retVal = _nss_mysql_load_result (result, buffer, buflen, sname##_fields); \
    _nss_mysql_close_sql (CLOSE_RESULT);                                      \
    _nss_mysql_free_query_list;                                               \
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
    int i, size;                                                              \
    char *query_list[MAX_SERVERS];                                            \
                                                                              \
    function_enter;                                                           \
    if (!result)                                                              \
      {                                                                       \
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL RESULT", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (!buffer)                                                              \
      {                                                                       \
        _nss_mysql_log (LOG_CRIT, "%s was passed a NULL buffer", FNAME);      \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    LOCK;                                                                     \
    memset (query_list, 0, sizeof (query_list));                              \
    if (_nss_mysql_init (&conf) != NSS_SUCCESS)                               \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (_nss_mysql_active_result () == nfalse)                                \
      {                                                                       \
        for (i = 0; i < MAX_SERVERS; i++)                                     \
          {                                                                   \
            if (!conf.sql[i].query.get##type)                                 \
              continue;                                                       \
            size = strlen (conf.sql[i].query.get##type) + 1 + PADSIZE;        \
            query_list[i] = malloc (size);                                    \
            if (query_list[i] == NULL)                                        \
              {                                                               \
                _nss_mysql_free_query_list;                                   \
                UNLOCK;                                                       \
                function_return (NSS_UNAVAIL);                                \
              }                                                               \
            strcpy (query_list[i], conf.sql[i].query.get##type);              \
          }                                                                   \
        if (_nss_mysql_run_query (conf, query_list) != NSS_SUCCESS)           \
          {                                                                   \
            _nss_mysql_free_query_list;                                       \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
      }                                                                       \
    retVal = _nss_mysql_load_result (result, buffer, buflen, sname##_fields); \
    _nss_mysql_inc_ent ();                                                    \
    _nss_mysql_free_query_list;                                               \
    UNLOCK;                                                                   \
    function_return (retVal);                                                 \
  }

void
_nss_mysql_log (int priority, char *fmt, ...)
{
  va_list ap;
  static char *string = NULL;

  if (priority > conf.global.syslog_priority)
    return;

  if (string == NULL)
    string = malloc (2000);
  if (string == NULL)
    return;
  va_start (ap, fmt);
  vsnprintf (string, 2000, fmt, ap);
  va_end (ap);
  openlog ("nss_mysql", 0, conf.global.syslog_facility);
  syslog (priority, string);
  closelog ();
}

void
_nss_mysql_debug (char *function, int flags, char *fmt, ...)
{
  va_list ap;
  static char *string;

  if (conf.global.syslog_priority < LOG_DEBUG)
    return;

  if (!(flags & conf.global.debug_flags))
    return;

  if (string == NULL)
    string = malloc (2000);
  if (string == NULL)
    return;
  snprintf (string, 2000, "%s: ", function);
  va_start (ap, fmt);
  vsnprintf (string + strlen (string), 2000, fmt, ap);
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

