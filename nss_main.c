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
#include <stdarg.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

  openlog (PACKAGE, OPENLOG_OPTIONS, SYSLOG_FACILITY);
  va_start (ap, fmt);
  vsyslog (priority, fmt, ap);
  va_end (ap);
  closelog ();
}

#ifdef HAVE_NSS_COMMON_H
NSS_STATUS
_nss_mysql_default_destr (nss_backend_t *be, void *args)
{
  _nss_mysql_free (be);
  /* Closing link & freeing memory unnecessary due to link w/ '-z nodelete' */
  return (NSS_SUCCESS);

}
#endif

