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

/*
 * Global variables and miscellaneous support routines
 */

static const char rcsid[] =
    "$Id$";

#include "nss_mysql.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// conf_t  conf = {0, 0, {DEF_RETRY, DEF_FACIL, DEF_PRIO, DEF_DFLAGS} };
extern conf_t conf;

/*
 * Syslog a message at PRIORITY.
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

/*
 * Our debug routine.  FUNCTION should contain the calling function name.
 * FLAGS contains the type of debug message this is.  Sends the resulting
 * string to the log function above.
 */
void
_nss_mysql_debug (const char *function, int flags, char *fmt, ...)
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

#ifdef HAVE_NSS_COMMON_H
NSS_STATUS
_nss_mysql_default_destr (nss_backend_t *be, void *args)
{
  function_enter;
  _nss_mysql_free (be);
  /*
   * We MUST close the link as the NSS subsystem unloads the module and thus
   * all static variables are lost
   */
  _nss_mysql_close_sql (CLOSE_LINK);
  _nss_mysql_reset_config ();
  function_return (NSS_SUCCESS);

}
#endif



