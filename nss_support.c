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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

/* Number of comma- or space-separated tokens in BUFFER */
static NSS_STATUS
_nss_mysql_count_tokens (const char *buffer, int *count)
{
  char *p;
  char *token;
  int buflen;

  function_enter;
  *count = 0;
  buflen = strlen (buffer);
  if (buflen == 0)
    function_return (NSS_SUCCESS);

  /* strtok destroys the input, so copy it somewhere safe first */
  if ((p = (char *) xmalloc (buflen + 1)) == NULL)
    function_return (NSS_UNAVAIL);
  memcpy (p, buffer, strlen (buffer) + 1);

  for (token = strtok (p, ", "); token; (*count)++)
    token = strtok (NULL, ", ");
  xfree (p);
  _nss_mysql_debug (FNAME, D_PARSE, "Found %d tokens", *count);
  function_return (NSS_SUCCESS);
}

/*
 * 'lis' = Load Into Structure.  'wb' = With Buffer
 * There's a different version of this (just 'lis' and not 'liswb')
 * in nss_config.c
 */
NSS_STATUS
_nss_mysql_liswb (const char *val, void *structure, char *buffer,
                  size_t buflen, int *bufused, int fofs, ftype_t type)
{
  _nss_mysql_byte *b;
  int val_size = 0;

  function_enter;
  if (!val)
    {
      _nss_mysql_log (LOG_CRIT, "%s was passed a NULL VAL");
      function_return (NSS_UNAVAIL);
    }
  val_size = strlen (val) + 1;

  b = (_nss_mysql_byte *) structure;
  switch (type)
    {
    case FT_INT:
      *(int *) (b + fofs) = atoi (val);
      break;
    case FT_UINT:
      *(unsigned int *) (b + fofs) = (unsigned int) atoi (val);
      break;
    case FT_LONG:
      *(long *) (b + fofs) = atol (val);
      break;
    case FT_ULONG:
      *(unsigned long *) (b + fofs) = (unsigned long) atol (val);
      break;
    case FT_PCHAR:
      if (*bufused > buflen)
        {
          errno = ERANGE;
          _nss_mysql_debug (FNAME, D_MEMORY,
                            "Requesting more memory for BUFFER");
          function_return (NSS_TRYAGAIN);
        }
      *(intptr_t *) (b + fofs) = (intptr_t) memcpy (buffer, val, val_size);
      *bufused += val_size;
      break;
    case FT_PPCHAR:
      {
        char *s;    /* Strings start here */
        char *p;    /* Pointers start here */
        char *copy_of_val;
        char *token;
        int num_tokens, token_size;

        if ((_nss_mysql_count_tokens (val, &num_tokens)) != NSS_SUCCESS)
          function_return (NSS_UNAVAIL);

        if (num_tokens == 0)
          {
            *(intptr_t *) (b + fofs) = (intptr_t) buffer;
            function_return (NSS_SUCCESS);
          }

        /* Leave room for n+1 pointers (last must be NULL) */
        p = buffer;
        s = p + ((num_tokens + 1) * PTRSIZE);
        *bufused += (num_tokens + 1) * PTRSIZE;

        /* strtok destroys the input, so copy it somewhere safe first */
        if ((copy_of_val = (char *) xmalloc (val_size)) == NULL)
          function_return (NSS_UNAVAIL);

        memcpy (copy_of_val, val, val_size);

        token = strtok (copy_of_val, ", ");
        while (token)
          {
            token_size = strlen (token) + 1;
            *bufused += token_size;
            if (*bufused > buflen)
              {
                xfree (copy_of_val);
                errno = ERANGE;
                _nss_mysql_debug (FNAME, D_MEMORY,
                                  "Requesting more memory for BUFFER");
                function_return (NSS_TRYAGAIN);
              }
            *(intptr_t *) p = (intptr_t) memcpy (s, token, token_size);
            p += PTRSIZE;
            s += token_size;

            token = strtok (NULL, ", ");
          }
        xfree (copy_of_val);
        /* Set structure pointer to point at start of pointers */
        *(intptr_t *) (b + fofs) = (intptr_t) buffer;
      }
      break;
    default:
      _nss_mysql_log (LOG_ERR, "%s: Unhandled type: %d", FNAME, type);
      break;
    }
  function_return (NSS_SUCCESS);
}

NSS_STATUS
_nss_mysql_init (conf_t *conf)
{
  int to_return;

  function_enter;
  to_return = _nss_mysql_load_config (conf);
  if (to_return != NSS_SUCCESS)
    function_return (to_return);
  to_return = _nss_mysql_connect_sql (*conf);
  function_return (to_return);
}

