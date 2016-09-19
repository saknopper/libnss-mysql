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
#include <stdio.h>      /* fprintf() */
#include <stdarg.h>     /* va_start() */
#include <sys/stat.h>   /* umask() */

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t _nss_mysql_once_control = PTHREAD_ONCE_INIT;
static int _nss_mysql_locked_by_atfork = 0;

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
 * Before fork() processing begins, the prepare fork handler is called
 */
static void
_nss_mysql_atfork_prepare (void)
{
  DENTER

  if (pthread_mutex_trylock(&lock) == 0)
  {
    _nss_mysql_locked_by_atfork = 1;
  } else
  {
    D ("%s: error calling pthread_mutex_trylock() - lock failed", __FUNCTION__);
  }

  DEXIT
}

/*
 * The parent fork handler is called after fork() processing finishes
 * in the parent process
 */
static void
_nss_mysql_atfork_parent (void)
{
  DENTER
  if (_nss_mysql_locked_by_atfork)
    {
      _nss_mysql_locked_by_atfork = 0;
      UNLOCK;
    }
  DEXIT
}

/*
 * The child fork handler is called after fork() processing finishes in the
 * child process.
 */
static void
_nss_mysql_atfork_child (void)
{
  DENTER
  /* Don't close the link; just set it to invalid so we'll open a new one */
  _nss_mysql_close_sql (NULL, false);
  if (_nss_mysql_locked_by_atfork)
    {
      _nss_mysql_locked_by_atfork = 0;
      UNLOCK;
    }
  DEXIT
}

/* Setup pthread_atfork if current namespace contains pthreads. */
static void
_nss_mysql_pthread_once_init (void)
{
  DENTER

  if (pthread_atfork(_nss_mysql_atfork_prepare, _nss_mysql_atfork_parent,
                       _nss_mysql_atfork_child) != 0)
  {
    D ("%s: error calling pthread_atfork()", __FUNCTION__);
  }

  DEXIT
}

/*
 * Prevent the "dead store removal" problem present with stock memset()
 */
static void *
_nss_mysql_safe_memset (void *s, int c, size_t n)
{
  volatile char *p = s;

  DENTER
  if (p)
    {
      while (n--)
        *p++ = c;
    }
  DPRETURN (s)
}

/*
 * Make an attempt to close the link when the process exits
 * Set in _nss_mysql_init() below
 */
static void
_nss_mysql_atexit_handler (void)
{
  extern conf_t conf;

  DENTER
  _nss_mysql_close_sql (NULL, true);
  _nss_mysql_safe_memset (conf.sql.server.password, 0,
                          sizeof (conf.sql.server.password));
  DEXIT
}

/*
 * Setup pthread_once if it's available in the current namespace
 * Load config file(s)
 */
NSS_STATUS
_nss_mysql_init (void)
{
  static int atexit_isset = false;

  DENTER

  if (pthread_once(&_nss_mysql_once_control, _nss_mysql_pthread_once_init) != 0) {
    D ("%s: error calling pthread_once()", __FUNCTION__);
  }

  if (atexit_isset == false)
    {
      if (atexit(_nss_mysql_atexit_handler) == RETURN_SUCCESS)
        atexit_isset = true;
    }

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

#ifdef HAVE_NSS_COMMON_H
NSS_STATUS
_nss_mysql_default_destr (nss_backend_t *be, void *args)
{
  DENTER
  if (be)
    {
      free (be);
      be = NULL;
    }

  /* TODO: Close link and free memory !? */

  DSRETURN (NSS_SUCCESS)
}
#endif

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

	// TODO Implement this, called before dlopen() finishes

	DEXIT
}

void __attribute__ ((destructor)) _nss_mysql_destructor(void)
{
	DENTER

	// TODO Implement this, called before dlclose() finishes

	DEXIT
}
