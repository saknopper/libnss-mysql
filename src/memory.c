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
 * Safe memory functions
 */

static const char rcsid[] =
    "$Id$";

#include "nss_mysql.h"
#include <stdlib.h>     /* free() */

void
_nss_mysql_free (void *ptr)
{
  DN ("_nss_mysql_free")

  DENTER
  if (ptr)
    {
      D ("%s: free (%p)", FUNCNAME, ptr);
      free (ptr);
    }
  ptr = NULL;
  DEXIT
}

void *
_nss_mysql_malloc (size_t size)
{
  DN ("_nss_mysql_malloc")
  static void *ptr;

  DENTER
  D ("%s: malloc (%u)", FUNCNAME, size);
  ptr = malloc (size);
  if (ptr == NULL)
    _nss_mysql_log (LOG_ALERT, "malloc of %d bytes failed", size);
  DPRETURN (ptr)
}

void *
_nss_mysql_realloc (void *ptr, size_t size)
{
  DN ("_nss_mysql_realloc")

  DENTER
  D ("%s: realloc (%p, %u)", FUNCNAME, ptr, size);
  ptr = realloc (ptr, size);
  if (ptr == NULL)
    _nss_mysql_log (LOG_ALERT, "realloc of %d bytes failed", size);
  DPRETURN (ptr)
}

/*
 * Prevent the "dead store removal" problem present with stock memset()
 */
void *
_nss_mysql_safe_memset (void *s, int c, size_t n)
{
  DN ("_nss_mysql_realloc")
  volatile char *p = s;

  DENTER
  while (n--)
    *p++ = c;
  DPRETURN (s)
}

