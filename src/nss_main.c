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
#include <stdlib.h>
#include <stdarg.h>

/* 
 * GNU source only defines RTLD_DEFAULT if __USE_GNU is set
 */
#ifndef __USE_GNU
#define __USE_GNU
#include <dlfcn.h>
#undef __USE_GNU
#endif

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t _nss_mysql_once_control = {PTHREAD_ONCE_INIT};
static int _nss_mysql_locked_by_atfork = 0;

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
  char msg[1000];
  FILE *fh;
  char *env;
  int type = 0;
  mode_t old_mask;

  va_start (ap, fmt);
  vsnprintf (msg, 1000, fmt, ap);
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
 * Before fork() processing begins, the prepare fork handler is called
 */
static void
_nss_mysql_atfork_prepare (void)
{
  DN ("_nss_mysql_atfork_prepare")
  int (*trylock)();

  DENTER
  trylock = (int (*)(int))dlsym (RTLD_DEFAULT, "pthread_mutex_trylock");
  if (trylock)
    if ((*trylock) (&lock) == 0)
      _nss_mysql_locked_by_atfork = 1;
  DEXIT
}

/*
 * The parent fork handler is called after fork() processing finishes
 * in the parent process
 */
static void
_nss_mysql_atfork_parent (void)
{
  DN ("_nss_mysql_atfork_parent")
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
  DN ("_nss_mysql_atfork_child")
  DENTER
  /* Don't close the link; just set it to invalid so we'll open a new one */
  _nss_mysql_close_sql (NULL, nfalse);
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
  DN ("_nss_mysql_atfork_once_init")
  int (*pthread_atfork)();

  DENTER
  pthread_atfork = (int (*)(int))dlsym (RTLD_DEFAULT, "pthread_atfork");
  if (pthread_atfork)
    (*pthread_atfork) (_nss_mysql_atfork_prepare, _nss_mysql_atfork_parent,
                       _nss_mysql_atfork_child);
  DEXIT
}

/*
 * Make an attempt to close the link when the process exits
 * Set in _nss_mysql_init() below
 */
static void
_nss_mysql_atexit_handler (void)
{
  DN ("_nss_mysql_atexit_handler")

  DENTER
  _nss_mysql_close_sql (NULL, ntrue);
  DEXIT
}

/*
 * Setup pthread_once if it's available in the current namespace
 * Load config file(s)
 */
NSS_STATUS
_nss_mysql_init (void)
{
  DN ("_nss_mysql_init")
  int (*pthread_once)();
  static int atexit_isset = nfalse;

  DENTER
  pthread_once = (int (*)(int))dlsym (RTLD_DEFAULT, "pthread_once");
  if (pthread_once)
    (*pthread_once) (&_nss_mysql_once_control, _nss_mysql_pthread_once_init);
  if (atexit_isset == nfalse)
    {
      if (atexit(_nss_mysql_atexit_handler) == RETURN_SUCCESS)
        atexit_isset = ntrue;
    }
  UNLOCK;
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
  DN ("_nss_mysql_log")
  va_list ap;
  char msg[1000];

  va_start (ap, fmt);
  vsnprintf (msg, 1000, fmt, ap);
  syslog (priority, "%s: %s", PACKAGE, msg);
  va_end (ap);
}

#ifdef HAVE_NSS_COMMON_H
NSS_STATUS
_nss_mysql_default_destr (nss_backend_t *be, void *args)
{
  DN ("_nss_mysql_default_destr")
  DENTER
  _nss_mysql_free (be);
  /* Closing link & freeing memory unnecessary due to link w/ '-znodelete' */
  DSRETURN (NSS_SUCCESS)

}
#endif

/*
 * SET/END ent's call this.  While the definition of endent is to close
 * the file, I see no reason to actually do that - just clear the
 * current result set.
 */
void
_nss_mysql_reset_ent (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_reset_ent")
  DENTER
  _nss_mysql_close_result (mresult);
  DEXIT
}

