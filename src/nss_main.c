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

#include "nss_mysql.h"
#include <stdio.h>      /* fprintf() */
#include <stdarg.h>     /* va_start() */
#include <string.h>     /* explicit_bzero() */
#include <sys/stat.h>   /* umask() */

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t _nss_mysql_once_control = PTHREAD_ONCE_INIT;

#define MAX_MSG_SIZE 1024

/*
 * Debugs to either a file, stderr, or syslog, depending on the environ var
 * LIBNSS_MYSQL_DEBUG
 * none or 0 = file (DEBUG_FILE, defined in nss_mysql.h)
 *         1 = stderr
 *         2 = syslog (facility defined at configure, priority DEBUG)
 */
#ifdef DEBUG
void
_nss_mysql_debug (char *fmt, ...)
{
  va_list ap;
  char msg[MAX_MSG_SIZE];
  FILE *fh;
  char *env;
  int type = 0;
  mode_t old_mask;

  va_start (ap, fmt);
  vsnprintf (msg, MAX_MSG_SIZE, fmt, ap);
  env = getenv ("LIBNSS_MYSQL_DEBUG");
  if (env)
    type = atoi (env);

  if (type <= 1)
    {
      if (type == 0)
        {
          old_mask = umask (000);
          fh = fopen (DEBUG_FILE, "a");
          umask (old_mask);
        }
      else
        fh = stderr;
      if (fh)
        {
          fprintf (fh, "[%d]: %s\n", getpid(), msg);
          if (type == 0)
            fclose (fh);
        }
    }
  else
    {
      _nss_mysql_log (LOG_DEBUG, "%s", msg);
    }
  va_end (ap);
}
#endif

/*
 *                          THREADING NOTES
 *
 * libnss-mysql is *not* to be linked against any threading library.
 * Instead, check for pthread functions in the current namespace
 * by using dlsym() and RTLD_DEFAULT.  This way we don't litter every
 * application with a thread library (since libnss-mysql would be linked
 * against it and so many applications would dlopen libnss-mysql).
 * Applications often misbehave when a thread environment is unexpected
 *
 * libnss-mysql is *not* to be linked against the threaded version
 * of MySQL for the same reasons.  libnss-mysql is not coded in such
 * a way that a threaded MySQL library is needed, anyway.
 *
 * libnss-mysql locks just prior to calling _nss_mysql_lookup() and
 * unlocks when done with it and all data handled by it.  Since the lock
 * occurs so early on and lasts so long, libnss-mysql doesn't perform
 * that well in a heavily threaded application (e.g. a threaded radius
 * server).  Use of "nscd" is highly recommended.
 */

/*
 * Load config file(s)
 */
NSS_STATUS
_nss_mysql_init (void)
{
  DENTER

  DSRETURN (_nss_mysql_load_config ())
}

/*
 * Syslog a message at PRIORITY.
 * Do *NOT* openlog/closelog as you'll mess up calling programs that
 * are syslogging their own stuff.
 */
void
_nss_mysql_log (int priority, char *fmt, ...)
{
  va_list ap;
  char msg[MAX_MSG_SIZE];

  va_start (ap, fmt);
  vsnprintf (msg, MAX_MSG_SIZE, fmt, ap);
  syslog (priority, "%s: %s", PACKAGE, msg);
  va_end (ap);
}

/*
 * SET/END ent's call this.  While the definition of endent is to close
 * the "file", we need to keep the MySQL link persistent, so we just
 * clear any lingering MySQL result set.
 */
void
_nss_mysql_reset_ent (MYSQL_RES **mresult)
{
  DENTER
  _nss_mysql_close_result (mresult);
  DEXIT
}

void __attribute__ ((constructor)) _nss_mysql_constructor(void)
{
	DENTER
	D ("%s: loading %s", __FUNCTION__, PACKAGE_STRING);

	DEXIT
}

void __attribute__ ((destructor)) _nss_mysql_destructor(void)
{
	DENTER

	extern conf_t conf;
	_nss_mysql_close_sql (NULL, true);
	explicit_bzero(conf.sql.server.password, sizeof (conf.sql.server.password));

	DEXIT
}
