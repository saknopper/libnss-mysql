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
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

#define LOCK pthread_mutex_lock (&lock);
#define UNLOCK pthread_mutex_unlock (&lock);

state_t pwent_state = { NULL, 0, 0, 0, 0 };
state_t spent_state = { NULL, 0, 0, 0, 0 };
state_t grent_state = { NULL, 0, 0, 0, 0 };
state_t other_state = { NULL, 0, 0, 0, 0 };
conf_t  conf = { 0 };

#define GET(funcname, sname, argtype, restrict)                               \
  NSS_STATUS                                                                  \
  _nss_mysql_##funcname##_r (argtype arg, struct sname *result,               \
                             char *buffer, size_t buflen)                     \
  {                                                                           \
    int retVal, i;                                                            \
    int size;                                                                 \
                                                                              \
    function_enter;                                                           \
    if (restrict && geteuid () != 0)                                          \
      function_return (NSS_NOTFOUND);                                         \
    LOCK;                                                                     \
    if (_nss_mysql_init (&conf, &other_state) != NSS_SUCCESS)                 \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    for (i = 0; i < MAX_SERVERS; i++)                                         \
      {                                                                       \
        if (!conf.sql[i].query.funcname)                                      \
          continue;                                                           \
        size = strlen (conf.sql[i].query.funcname) + 1 + PADSIZE;             \
        other_state.query[i] = realloc (other_state.query[i], size);          \
        if (other_state.query[i] == NULL)                                     \
          {                                                                   \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
        snprintf (other_state.query[i], size, conf.sql[i].query.funcname,     \
                  arg);                                                       \
      }                                                                       \
    if (_nss_mysql_run_query (conf, &other_state) != NSS_SUCCESS)             \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    retVal = _nss_mysql_load_result (result, buffer, buflen, &other_state,    \
                                     sname##_fields, 0);                      \
    _nss_mysql_close_sql (&other_state, CLOSE_RESULT);                        \
    UNLOCK;                                                                   \
    function_return (retVal);                                                 \
  }

#define ENDENT(type)                                                          \
  NSS_STATUS                                                                  \
  _nss_mysql_end##type (void)                                                 \
  {                                                                           \
    function_enter;                                                           \
    LOCK;                                                                     \
    type##_state.idx = 0;                                                     \
    _nss_mysql_close_sql (&type##_state, CLOSE_RESULT);                       \
    UNLOCK;                                                                   \
    function_return (NSS_SUCCESS);                                            \
  }

#define SETENT(type)                                                          \
  NSS_STATUS                                                                  \
  _nss_mysql_set##type (void)                                                 \
  {                                                                           \
    function_enter;                                                           \
    LOCK;                                                                     \
    type##_state.idx = 0;                                                     \
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
                                                                              \
    function_enter;                                                           \
    LOCK;                                                                     \
    if (_nss_mysql_init (&conf, &type##_state) != NSS_SUCCESS)                \
      {                                                                       \
        UNLOCK;                                                               \
        function_return (NSS_UNAVAIL);                                        \
      }                                                                       \
    if (type##_state.mysql_result == NULL)                                    \
      {                                                                       \
        for (i = 0; i < MAX_SERVERS; i++)                                     \
          {                                                                   \
            if (!conf.sql[i].query.get##type)                                 \
              continue;                                                       \
            size = strlen (conf.sql[i].query.get##type) + 1 + PADSIZE;        \
            type##_state.query[i] = realloc (type##_state.query[i], size);    \
            if (type##_state.query[i] == NULL)                                \
              {                                                               \
                UNLOCK;                                                       \
                function_return (NSS_UNAVAIL);                                \
              }                                                               \
            strcpy (type##_state.query[i], conf.sql[i].query.get##type);      \
          }                                                                   \
        if (_nss_mysql_run_query (conf, &type##_state) != NSS_SUCCESS)        \
          {                                                                   \
            UNLOCK;                                                           \
            function_return (NSS_UNAVAIL);                                    \
          }                                                                   \
      }                                                                       \
    retVal = _nss_mysql_load_result (result, buffer, buflen, &type##_state,   \
                                     sname##_fields, type##_state.idx++);     \
    UNLOCK;                                                                   \
    function_return (retVal);                                                 \
  }

void
_nss_mysql_log_error (char *function, char *fmt, ...)
{
  va_list ap;
  char string[2000];

  va_start (ap, fmt);
  vsnprintf (string, 2000, fmt, ap);
  va_end (ap);
  _nss_mysql_debug (function, D_ERROR, "%s\n", string);
  openlog ("nss_mysql", 0, conf.global.syslog_facility);
  syslog (conf.global.syslog_priority, string);
  closelog ();
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

