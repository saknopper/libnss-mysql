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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

con_info_t ci = { nfalse, 0, 0, 0, NULL };

/* Please see the file INTERNALS to help you understand why I do what
 * I do with BUFFER.
 * This function finds the next available space in BUFFER
 */
static char *
_nss_mysql_find_eol (char *buffer, int buflen)
{
  char *p = buffer + buflen;
  int cur = buflen;

  function_enter;
  while (cur >= 1)
    {
      if (*p == 0 && *(p - 1) != 0)
        {
          function_leave;
          return (p + 1);
        }
      p--;
      cur--;
    }
  if (cur == 0 && *p == 0)
    {
      function_leave;
      return (p);
    }

  _nss_mysql_debug (FNAME, D_PARSE, "eol not found");
  function_leave;
  return (NULL);
}

/* Number of comma- or space-separated tokens in BUFFER */
static NSS_STATUS
_nss_mysql_count_tokens (const char *buffer, int *count)
{
  char *p;
  char *token;
  int buflen;

  function_enter;
  buflen = strlen (buffer);
  if (buflen == 0)
    function_return (NSS_SUCCESS);

  /* strtok destroys the input, so copy it somewhere safe first */
  _nss_mysql_debug (FNAME, D_MEMORY, "p = malloc (%d)", buflen);
  if ((p = (char *) malloc (buflen + 1)) == NULL)
    {
      _nss_mysql_log (LOG_ALERT, "malloc of %d bytes failed", buflen + 1);
      function_return (NSS_UNAVAIL);
    }
  memcpy (p, buffer, strlen (buffer) + 1);

  *count = 0;
  token = strtok (p, ", ");
  while (token)
    {
      (*count)++;
      token = strtok (NULL, ", ");
    }
  _nss_mysql_debug (FNAME, D_MEMORY, "free (p)");
  free (p);
  _nss_mysql_debug (FNAME, D_PARSE, "Found %d tokens", *count);
  function_return (NSS_SUCCESS);
}

/* 'lis' = Load Into Structure.  'wb' = With Buffer
 * There's a different version of this (just 'lis' and not 'liswb')
 * in nss_config.c
 */
static NSS_STATUS
_nss_mysql_liswb (const char *val, void *structure, char *buffer,
                  size_t buflen, int fofs, ftype_t type)
{
  _nss_mysql_byte *b;
  int val_length = 0;
  char *sos;    /* Start of String(s) */

  function_enter;
  if (!val)
    {
      _nss_mysql_log (LOG_CRIT, "%s was passed a NULL VAL");
      function_return (NSS_UNAVAIL);
    }
  val_length = strlen (val);

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
      sos = _nss_mysql_find_eol (buffer, buflen);
      if (sos == NULL)
        {
          errno = ERANGE;
          _nss_mysql_debug (FNAME, D_MEMORY,
                            "Requesting more memory for BUFFER");
          function_return (NSS_TRYAGAIN);
        }

      if (buflen - (intptr_t) (sos - buffer) <= val_length)
        {
          errno = ERANGE;
          _nss_mysql_debug (FNAME, D_MEMORY,
                            "Requesting more memory for BUFFER");
          function_return (NSS_TRYAGAIN);
        }

      *(intptr_t *) (b + fofs) = (intptr_t) memcpy (sos, val, val_length);
      break;
    case FT_PPCHAR:
      {
        char *sop;           /* Start of Pointers */
        char *copy_of_val;
        char *token;
        int num_tokens, token_length;
        int token_num;       /* Current token number */
        int offset;          /* # bytes ahead of sos to place this string */

        if ((_nss_mysql_count_tokens (val, &num_tokens)) != NSS_SUCCESS)
          function_return (NSS_UNAVAIL);

        if (num_tokens == 0)
          function_return (NSS_SUCCESS);

        /* Pointer storage will start here */
        if ((sop = _nss_mysql_find_eol (buffer, buflen)) == NULL)
          {
            errno = ERANGE;
            _nss_mysql_debug (FNAME, D_MEMORY,
                              "Requesting more memory for BUFFER");
            function_return (NSS_TRYAGAIN);
          }

        /* Leave room for n+1 pointers (last must be NULL) */
        sos = sop + ((num_tokens + 1) * PTRSIZE);

        /* strtok destroys the input, so copy it somewhere safe first */
        _nss_mysql_debug (FNAME, D_MEMORY, "copy_of_val = malloc (%d)",
                          val_length + 1);
        if ((copy_of_val = (char *) malloc (val_length + 1)) == NULL)
          {
            _nss_mysql_log (LOG_ALERT, "malloc of %d bytes failed",
                            val_length + 1);
            function_return (NSS_UNAVAIL);
          }

        memcpy (copy_of_val, val, val_length + 1);

        token_num = offset = 0;
        token = strtok (copy_of_val, ", ");
        while (token)
          {
            token_length = strlen (token);
            if (buflen - (intptr_t) (sos - buffer) <= token_length + 1)
              {
                _nss_mysql_debug (FNAME, D_MEMORY, "free (copy_of_val)");
                free (copy_of_val);
                errno = ERANGE;
                _nss_mysql_debug (FNAME, D_MEMORY,
                                  "Requesting more memory for BUFFER");
                function_return (NSS_TRYAGAIN);
              }
            *(intptr_t *) (sop + (token_num * PTRSIZE)) =
              (intptr_t) memcpy (sos + offset, token, token_length + 1);
            token_num++;
            offset += token_length + 1;

            token = strtok (NULL, ", ");
          }
        _nss_mysql_debug (FNAME, D_MEMORY, "free (copy_of_val)");
        free (copy_of_val);
        /* Set structure pointer to point at start of pointers */
        *(intptr_t *) (b + fofs) = (intptr_t) sop;
      }
      break;
    default:
      _nss_mysql_log (LOG_ERR, "%s: Unhandled type: %d", FNAME, type);
      break;
    }
  function_return (NSS_SUCCESS);
}

static freturn_t
_nss_mysql_save_socket_info (void)
{
  socklen_t sockaddr_size = sizeof (struct sockaddr);
  int r;

  function_enter;
  r = getsockname (ci.link.net.fd, &(ci.sock_info.local),
                   &sockaddr_size) ;
  if (r != RETURN_SUCCESS)
    function_return (RETURN_FAILURE);

  r = getpeername (ci.link.net.fd, &(ci.sock_info.remote),
                   &sockaddr_size) ;
  function_return (r);
}

static nboolean
_nss_mysql_is_same_sockaddr (struct sockaddr orig, struct sockaddr cur)
{
  struct sockaddr_in orig_sin, cur_sin;
  struct sockaddr_un orig_sun, cur_sun;
  sa_family_t family;

  function_enter;
  family = *(sa_family_t *)&orig;
  switch (family)
    {
    case AF_INET:
        orig_sin = *(struct sockaddr_in *) &orig;
        cur_sin = *(struct sockaddr_in *) &cur;
        if (orig_sin.sin_port != cur_sin.sin_port)
          function_return (nfalse);
        if (orig_sin.sin_addr.s_addr != cur_sin.sin_addr.s_addr)
          function_return (nfalse);
        break;
    case AF_UNIX:
        orig_sun = *(struct sockaddr_un *) &orig;
        cur_sun = *(struct sockaddr_un *) &cur;
        if (strcmp (orig_sun.sun_path, cur_sun.sun_path) != 0)
          function_return (nfalse);
        break;
    default:
        _nss_mysql_log (LOG_ERR, "%s: Unhandled family: %d", family, FNAME);
        break;
    }
  function_return (ntrue);
}

static nboolean
_nss_mysql_validate_socket (void)
{
  socklen_t sockaddr_size = sizeof (struct sockaddr);
  struct sockaddr check;
  int r;

  function_enter;
  r = getsockname (ci.link.net.fd, &check, &sockaddr_size);
  if (r != ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "getsockname() failed");
      function_return (nfalse);
    }
  r = _nss_mysql_is_same_sockaddr (ci.sock_info.local, check);
  if (r != ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "local socket mismatch");
      function_return (nfalse);
    }
  r = getpeername (ci.link.net.fd, &check, &sockaddr_size);
  if (r != ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "getpeername() failed");
      function_return (nfalse);
    }
  r = _nss_mysql_is_same_sockaddr (ci.sock_info.remote, check);
  if (r != ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "remote socket mismatch");
      function_return (nfalse);
    }

  function_return (ntrue);
}

static NSS_STATUS
_nss_mysql_try_server (int num, conf_t conf)
{
  sql_server_t server;

  function_enter;
  server = conf.sql[num].server;
  _nss_mysql_debug (FNAME, D_CONNECT,
                    "Connecting to server number %d (%s)",
                    num, server.host);
#ifdef HAVE_MYSQL_REAL_CONNECT /* comes from mysql.h, NOT config.h! */
#if MYSQL_VERSION_ID >= 32200  /* ditto */
  if (mysql_real_connect (&(ci.link), server.host, server.username,
                          server.password, NULL, server.port,
                          server.socket, 0))
#else /* MYSQL_VERSION_ID < 32200 */
  if (mysql_real_connect (&(ci.link), server.host, server.username,
                          server.password, server.port, server.socket, 0))
#endif /* MYSQL_VERSION_ID >= 32200 */
#else /* HAVE_MYSQL_REAL_CONNECT */
  if (mysql_connect (&(ci.link), server.host, server.username,
                     server.password))
#endif /* HAVE_MYSQL_REAL_CONNECT */
  
    {
      if (mysql_select_db (&(ci.link), server.database) != RETURN_SUCCESS)
        {
          _nss_mysql_log (LOG_EMERG, "Unable to select database %s: %s",
                          server.database, mysql_error ((&ci.link)));
          _nss_mysql_close_sql (CLOSE_LINK);
          function_return (NSS_UNAVAIL);
        }
      if (_nss_mysql_save_socket_info () != RETURN_SUCCESS )
        {
          _nss_mysql_log (LOG_EMERG, "Unable to save socket info");
          _nss_mysql_close_sql (CLOSE_LINK);
          function_return (NSS_UNAVAIL);
        }
      ci.server_num = num;
      ci.valid = ntrue;
      function_return (NSS_SUCCESS);
    }
  _nss_mysql_log (LOG_ALERT, "Connection to server number %d failed: %s",
                  num, mysql_error (&(ci.link)));
  function_return (NSS_UNAVAIL);
}

static NSS_STATUS
_nss_mysql_connect_sql (conf_t conf)
{
  int i;
  time_t curTime;

  function_enter;
  if (ci.valid == ntrue)
    {
      if (_nss_mysql_validate_socket () == ntrue)
        {
          if (ci.result || ci.server_num == 0
              || ci.last_attempt + conf.global.retry > time (&curTime))
            {
#ifdef HAVE_MYSQL_PING
              if (mysql_ping (&(ci.link)) == RETURN_SUCCESS)
                {
#endif /* HAVE_MYSQL_PING */
                  _nss_mysql_debug (FNAME, D_CONNECT, "Using existing link");
                  function_return (NSS_SUCCESS);
#ifdef HAVE_MYSQL_PING
                }
              else
                {
                  _nss_mysql_debug (FNAME, D_CONNECT,
                                    "Existing link failed: %s",
                                    mysql_error (&(ci.link)));
                  _nss_mysql_close_sql (CLOSE_LINK);
                }
#endif /* HAVE_MYSQL_PING */
            }
          else
            {
              _nss_mysql_debug (FNAME, D_CONNECT,
                                "Attempting to revert to primary server");
              _nss_mysql_close_sql (CLOSE_LINK);
              ci.server_num = 0;
            }
        }
      else
        {
          _nss_mysql_debug (FNAME, D_CONNECT,
                            "Socket changed - forcing reconnect");
          _nss_mysql_close_sql (CLOSE_RESULT);
        }
    }

#ifdef HAVE_MYSQL_INIT
  if (mysql_init (&(ci.link)) == NULL)
    {
      _nss_mysql_log (LOG_EMERG, "mysql_init() failed");
      function_return (NSS_UNAVAIL);
    }
#endif /* HAVE_MYSQL_INIT */

  time (&(ci.last_attempt));
  for (i = ci.server_num; i < conf.num_servers; i++)
    {
      if (_nss_mysql_try_server (i, conf) == NSS_SUCCESS)
        function_return (NSS_SUCCESS);
    }
  _nss_mysql_log (LOG_EMERG, "Unable to connect to any MySQL servers");
  function_return (NSS_UNAVAIL);
}


/*
 * Exported functions
 */

NSS_STATUS
_nss_mysql_close_sql (int flags)
{

  function_enter;
  if (flags & (CLOSE_RESULT | CLOSE_LINK) && ci.result)
    {
      _nss_mysql_debug (FNAME, D_CONNECT | D_MEMORY, "Freeing result");
      mysql_free_result (ci.result);
      ci.result = NULL;
    }
  if (flags & CLOSE_LINK && ci.valid == ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "Closing link");
      mysql_close (&(ci.link));
      ci.valid = nfalse;
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

NSS_STATUS
_nss_mysql_run_query (conf_t conf, char **query_list)
{
  char *q;
  int attempts;

  function_enter;
  attempts = 0;

  while (attempts < conf.num_servers)
    {
      attempts++;
      _nss_mysql_debug (FNAME, D_QUERY, "Query attempt #%d", attempts);
      ci.server_num = (ci.server_num + attempts - 1)
                          % conf.num_servers;
      if (_nss_mysql_connect_sql (conf) != NSS_SUCCESS)
          continue;
      q = query_list[ci.server_num];
      if (!q || !strlen (q))
        {
          _nss_mysql_log (LOG_ALERT,
                          "Server #%d is missing a query in config",
                          ci.server_num);
          continue;
        }
      _nss_mysql_debug (FNAME, D_QUERY,
                        "Executing query on server #%d: %s",
             ci.server_num, q);
      if ((mysql_query (&(ci.link), q)) != RETURN_SUCCESS)
        {
          _nss_mysql_log (LOG_ALERT, "Query failed: %s",
                          mysql_error (&(ci.link)));
          _nss_mysql_close_sql (CLOSE_LINK);
        }
      if ((ci.result = mysql_store_result (&(ci.link))) == NULL)
        {
          _nss_mysql_log (LOG_ALERT, "store_result failed: %s",
                          mysql_error (&(ci.link)));
          _nss_mysql_close_sql (CLOSE_LINK);
          continue;
        }
      _nss_mysql_debug (FNAME, D_QUERY, "Rows returned: %d",
                        mysql_num_rows (ci.result));
      function_return (NSS_SUCCESS);
    }
  _nss_mysql_log (LOG_EMERG, "Unable to perform query on any MySQL server");
  function_return (NSS_UNAVAIL);
}

NSS_STATUS
_nss_mysql_load_result (void *result, char *buffer, size_t buflen,
                        field_info_t *fields)
{
  MYSQL_ROW row;
  int to_return;
  field_info_t *f;
  int i;
  int num_rows;

  function_enter;
  num_rows = mysql_num_rows (ci.result);
  _nss_mysql_debug (FNAME, D_QUERY, "num = %d, idx = %d", num_rows, ci.idx);

  if (num_rows == 0)
    function_return (NSS_NOTFOUND);

  if (ci.idx > num_rows - 1)
    function_return (NSS_NOTFOUND);

  mysql_data_seek (ci.result, ci.idx);

  if ((row = mysql_fetch_row (ci.result)) == NULL)
    {
      _nss_mysql_log (LOG_ALERT, "mysql_fetch_row() failed: %s",
                      mysql_error (&(ci.link)));
      function_return (NSS_UNAVAIL);
    }

  memset (buffer, 0, buflen);
  for (f = fields, i = 0; f->name; f++, i++)
    {
      _nss_mysql_debug (FNAME, D_PARSE, "Loading: %s = '%s' (%d %d %d)",
                        f->name, row[i], i, f->ofs, f->type);
      to_return = _nss_mysql_liswb (row[i], result, buffer, buflen,
                                    f->ofs, f->type);
      if (to_return != NSS_SUCCESS)
        function_return (to_return);
    }
  function_return (NSS_SUCCESS);
}

void
_nss_mysql_reset_ent (void)
{
  function_enter;
  _nss_mysql_close_sql (CLOSE_RESULT);
  ci.idx = 0;
  function_leave;
}

nboolean
_nss_mysql_active_result (void)
{
  function_enter;
  if (ci.result == NULL)
    function_return (nfalse);
  function_return (ntrue);
}

void
_nss_mysql_inc_ent (void)
{
  function_enter;
  ci.idx++;
  function_leave;
}

