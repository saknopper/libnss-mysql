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

  _nss_mysql_debug (FNAME, D_PARSE,
                    "eol not found, setting errno to ERANGE\n");
  errno = ERANGE;
  function_leave;
  return (NULL);
}

/* Number of comma- or space-separated tokens in BUFFER */
static NSS_STATUS
_nss_mysql_count_tokens (const char *buffer, int *count)
{
  char *p;
  char *token;
  int buflen = strlen (buffer);

  function_enter;

  *count = 0;
  if (!buffer)
    {
      _nss_mysql_log_error (FNAME, "NULL 'buffer'");
      function_return (NSS_UNAVAIL);
    }

  if (buflen == 0)
    function_return (NSS_SUCCESS);

  /* strtok destroys the input, so copy it somewhere safe first */
  _nss_mysql_debug (FNAME, D_MEMORY, "p = malloc (%d)\n", buflen);
  if ((p = (char *) malloc (buflen + 1)) == NULL)
    {
      _nss_mysql_log_error (FNAME, "malloc() failed");
      function_return (NSS_UNAVAIL);
    }
  memcpy (p, buffer, strlen (buffer) + 1);

  token = strtok (p, ", ");
  while (token)
    {
      (*count)++;
      token = strtok (NULL, ", ");
    }
  _nss_mysql_debug (FNAME, D_MEMORY, "free (p)\n");
  free (p);
  _nss_mysql_debug (FNAME, D_PARSE, "Found %d tokens\n", *count);
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
      _nss_mysql_log_error (FNAME, "Attempt to load NULL 'val'");
      function_return (NSS_UNAVAIL);
    }
  val_length = strlen (val);
  if (!structure)
    {
      _nss_mysql_log_error (FNAME, "BUG!  NULL 'structure'!");
      function_return (NSS_UNAVAIL);
    }

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
      if (!buffer)
        {
          _nss_mysql_log_error (FNAME, "BUG!  NULL 'buffer'!");
          function_return (NSS_UNAVAIL);
        }
      sos = _nss_mysql_find_eol (buffer, buflen);
      if (sos == NULL)
        function_return (NSS_TRYAGAIN);

      if (buflen - (intptr_t) (sos - buffer) <= val_length)
        function_return (NSS_TRYAGAIN);

      *(intptr_t *) (b + fofs) = (intptr_t) memcpy (sos, val, val_length);
      break;
    case FT_PPCHAR:
      if (!buffer)
        {
          _nss_mysql_log_error (FNAME, "BUG!  NULL 'buffer'!");
          function_return (NSS_UNAVAIL);
        }
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
          function_return (NSS_TRYAGAIN);

        /* Leave room for n+1 pointers (last must be NULL) */
        sos = sop + ((num_tokens + 1) * PTRSIZE);

        /* strtok destroys the input, so copy it somewhere safe first */
        _nss_mysql_debug (FNAME, D_MEMORY, "copy_of_val = malloc (%d)\n",
                          val_length + 1);
        if ((copy_of_val = (char *) malloc (val_length + 1)) == NULL)
          {
            _nss_mysql_log_error (FNAME, "malloc() failed");
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
                _nss_mysql_debug (FNAME, D_MEMORY, "free (copy_of_val)\n");
                free (copy_of_val);
                function_return (NSS_TRYAGAIN);
              }
            *(intptr_t *) (sop + (token_num * PTRSIZE)) =
              (intptr_t) memcpy (sos + offset, token, token_length + 1);
            token_num++;
            offset += token_length + 1;

            token = strtok (NULL, ", ");
          }
        _nss_mysql_debug (FNAME, D_MEMORY, "free (copy_of_val)\n");
        free (copy_of_val);
        /* Set structure pointer to point at start of pointers */
        *(intptr_t *) (b + fofs) = (intptr_t) sop;
      }
      break;
    default:
      _nss_mysql_log_error (FNAME, "BUG!  Unhandled type: %d\n", type);
      break;
    }
  function_return (NSS_SUCCESS);
}

static int
_nss_mysql_save_socket_info (state_t *state)
{
  socklen_t sockaddr_size = sizeof (struct sockaddr);
  int r;

  function_enter;
  r = getsockname (state->link.net.fd, &(state->sock_info.local),
                   &sockaddr_size) ;
  if (r != 0)
    function_return (1);

  r = getpeername (state->link.net.fd, &(state->sock_info.remote),
                   &sockaddr_size) ;
  if (r != 0)
    function_return (1);

  function_return (0);
}

static int
_nss_mysql_compare_sockaddr (struct sockaddr orig, struct sockaddr cur)
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
          function_return (1);
        if (orig_sin.sin_addr.s_addr != cur_sin.sin_addr.s_addr)
          function_return (1);
        break;
    case AF_UNIX:
        orig_sun = *(struct sockaddr_un *) &orig;
        cur_sun = *(struct sockaddr_un *) &cur;
        if (strcmp (orig_sun.sun_path, cur_sun.sun_path) != 0)
          function_return (0);
        break;
    default:
        _nss_mysql_log_error (FNAME, "BUG!  Unhandled family: %d\n",
                              family);
        break;
    }
  function_return (0);
}

static int
_nss_mysql_validate_socket (state_t *state)
{
  socklen_t sockaddr_size = sizeof (struct sockaddr);
  struct sockaddr check;
  int r;

  function_enter;
  r = getsockname (state->link.net.fd, &check, &sockaddr_size);
  if (r != 0)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "getsockname() failed\n");
      function_return (1);
    }
  r = _nss_mysql_compare_sockaddr (state->sock_info.local, check);
  if (r != 0)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "local socket mismatch\n");
      function_return (1);
    }
  r = getpeername (state->link.net.fd, &check, &sockaddr_size);
  if (r != 0)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "getpeername() failed\n");
      function_return (1);
    }
  r = _nss_mysql_compare_sockaddr (state->sock_info.remote, check);
  if (r != 0)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "remote socket mismatch\n");
      function_return (1);
    }

  function_return (0);
}

static NSS_STATUS
_nss_mysql_try_server (int num, conf_t conf, state_t *state)
{
  sql_server_t server;

  server = conf.sql[num].server;
  _nss_mysql_debug (FNAME, D_CONNECT,
                    "Connecting to server number %d (%s)\n",
                    num, server.host);
#ifdef HAVE_MYSQL_REAL_CONNECT /* comes from mysql.h, NOT config.h! */
#if MYSQL_VERSION_ID >= 32200  /* ditto */
  if (mysql_real_connect (&(state->link), server.host, server.username,
                          server.password, NULL, server.port,
                          server.socket, 0))
#else /* MYSQL_VERSION_ID < 32200 */
  if (mysql_real_connect (&(state->link), server.host, server.username,
                          server.password, server.port, server.socket, 0))
#endif /* MYSQL_VERSION_ID >= 32200 */
#else /* HAVE_MYSQL_REAL_CONNECT */
  if (mysql_connect (&(state->link), server.host, server.username,
                     server.password))
#endif /* HAVE_MYSQL_REAL_CONNECT */
  
    {
      if (mysql_select_db (&(state->link), server.database) != 0)
        {
          _nss_mysql_log_error (FNAME, "Unable to select database");
          _nss_mysql_close_sql (state, CLOSE_ALL);
          return NSS_UNAVAIL;
        }
      if (_nss_mysql_save_socket_info (state) != 0)
        {
          _nss_mysql_log_error (FNAME, "Unable to save socket info");
          _nss_mysql_close_sql (state, CLOSE_ALL);
          return NSS_UNAVAIL;
        }
      state->server_num = num;
      state->valid_link = 1;
      function_return (NSS_SUCCESS);
    }
  _nss_mysql_log_error (FNAME, "Connection to server number %d failed: %s",
                        num, mysql_error (&(state->link)));
  return NSS_UNAVAIL;
}

static NSS_STATUS
_nss_mysql_connect_sql (conf_t conf, state_t *state)
{
  int i;
  time_t curTime;

  function_enter;
  if (state->valid_link)
    {
      if (_nss_mysql_validate_socket (state) == 0)
        {
          if (state->mysql_result || state->server_num == 0
              || state->last_attempt + conf.global.retry > time (&curTime))
            {
#ifdef HAVE_MYSQL_PING
              if (mysql_ping (&(state->link)) == 0)
                {
#endif /* HAVE_MYSQL_PING */
                  _nss_mysql_debug (FNAME, D_CONNECT,
                                    "Using existing link\n");
                  function_return (NSS_SUCCESS);
#ifdef HAVE_MYSQL_PING
                }
              else
                {
                  _nss_mysql_debug (FNAME, D_CONNECT,
                                    "Existing link failed: %s\n",
                                    mysql_error (&(state->link)));
                  _nss_mysql_close_sql (state, CLOSE_ALL);
                }
#endif /* HAVE_MYSQL_PING */
            }
          else
            {
              _nss_mysql_debug (FNAME, D_CONNECT,
                                "Attempting to revert to primary server\n");
              _nss_mysql_close_sql (state, CLOSE_ALL);
              state->server_num = 0;
            }
        }
      else
        {
          _nss_mysql_debug (FNAME, D_CONNECT,
                            "Socket changed - forcing reconnect\n");
          _nss_mysql_close_sql (state, CLOSE_RESULT);
          state->valid_link = 0;
        }
    }

#ifdef HAVE_MYSQL_INIT
  if (mysql_init (&(state->link)) == NULL)
    {
      _nss_mysql_log_error (FNAME, "mysql_init() failed");
      function_return (NSS_UNAVAIL);
    }
#endif /* HAVE_MYSQL_INIT */

  time (&(state->last_attempt));
  for (i = state->server_num; i < conf.num_servers; i++)
    {
      if (_nss_mysql_try_server (i, conf, state) == NSS_SUCCESS)
        function_return (NSS_SUCCESS);
    }
  _nss_mysql_log_error (FNAME, "Exhausted all servers!");
  function_return (NSS_UNAVAIL);
}


/*
 * Exported functions
 */
void
_nss_mysql_debug (char *function, int flags, char *fmt, ...)
{
#ifdef DEBUG_FILE
  FILE *fh;
  va_list ap;
  char *string;
  char *env;
  int userFlags;

  if ((env = getenv ("DEBUG_NSS")) == NULL)
    return;
  userFlags = atoi (env);
  if (!(flags & userFlags))
    return;
  if ((fh = fopen (DEBUG_FILE, "a")) == NULL)
    return;
  string = malloc (2000);
  snprintf (string, 2000, "%30s: ", function);
  va_start (ap, fmt);
  vsnprintf (string + 32, 2000, fmt, ap);
  va_end (ap);
  fprintf (fh, string);
  free (string);
  fclose (fh);
#endif /* DEBUG_FILE */
}

NSS_STATUS
_nss_mysql_close_sql (state_t *state, int flags)
{

  function_enter;
  if (flags & CLOSE_RESULT && state->mysql_result)
    {
      _nss_mysql_debug (FNAME, D_CONNECT | D_MEMORY, "Freeing result\n");
      mysql_free_result (state->mysql_result);
      state->mysql_result = NULL;
    }
  if (flags & CLOSE_LINK && state->valid_link)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "Closing link\n");
      mysql_close (&(state->link));
      state->valid_link = 0;
    }
  function_return (NSS_SUCCESS);
}

NSS_STATUS
_nss_mysql_init (conf_t *conf, state_t *state)
{
  int to_return;

  function_enter;
  to_return = _nss_mysql_load_config (conf);
  if (to_return != NSS_SUCCESS)
    function_return (to_return);
  to_return = _nss_mysql_connect_sql (*conf, state);
  function_return (to_return);
}

NSS_STATUS
_nss_mysql_run_query (conf_t conf, state_t *state)
{
  char *q;
  int attempts;

  function_enter;
  attempts = 0;

  while (attempts < conf.num_servers)
    {
      attempts++;
      _nss_mysql_debug (FNAME, D_QUERY, "Query attempt #%d\n", attempts);
      state->server_num = (state->server_num + attempts - 1)
                          % conf.num_servers;
      if (_nss_mysql_connect_sql (conf, state) != NSS_SUCCESS)
          continue;
      q = state->query[state->server_num];
      if (!q || !strlen (q))
        {
          _nss_mysql_log_error (FNAME,
                                "Server #%d is missing a query in config",
                                state->server_num);
          continue;
        }
      _nss_mysql_debug (FNAME, D_QUERY,
                        "Executing query on server #%d: %s\n",
             state->server_num, q);
      if ((mysql_query (&(state->link), q)) != 0)
        {
          _nss_mysql_log_error (FNAME, "Query failed: %s",
                                mysql_error (&(state->link)));
          _nss_mysql_close_sql (state, CLOSE_ALL);
        }
      if ((state->mysql_result = mysql_store_result (&(state->link))) == NULL)
        {
          _nss_mysql_log_error (FNAME, "store_result failed: %s",
                                mysql_error (&(state->link)));
          _nss_mysql_close_sql (state, CLOSE_ALL);
          continue;
        }
      _nss_mysql_debug (FNAME, D_QUERY, "Rows returned: %d\n",
                        mysql_num_rows (state->mysql_result));
      function_return (NSS_SUCCESS);
    }
  _nss_mysql_log_error (FNAME, "No valid SQL servers!");
  function_return (NSS_UNAVAIL);
}

NSS_STATUS
_nss_mysql_load_result (void *result, char *buffer, size_t buflen,
                        state_t *state, field_info_t *fields, int idx)
{
  MYSQL_ROW row;
  int to_return;
  field_info_t *f;
  int i;
  int num_rows;

  function_enter;
  num_rows = mysql_num_rows (state->mysql_result);
  _nss_mysql_debug (FNAME, D_QUERY, "num = %d, idx = %d\n", num_rows, idx);

  if (num_rows == 0)
    function_return (NSS_NOTFOUND);

  if (idx > num_rows - 1)
    function_return (NSS_NOTFOUND);

  mysql_data_seek (state->mysql_result, idx);

  if ((row = mysql_fetch_row (state->mysql_result)) == NULL)
    {
      _nss_mysql_log_error (FNAME, "mysql_fetch_row() failed: %s",
                            mysql_error (&(state->link)));
      function_return (NSS_UNAVAIL);
    }

  memset (buffer, 0, buflen);
  for (f = fields, i = 0; f->name; f++, i++)
    {
      _nss_mysql_debug (FNAME, D_PARSE, "Loading: %s = '%s' (%d %d %d)\n",
                        f->name, row[i], i, f->ofs, f->type);
      to_return = _nss_mysql_liswb (row[i], result, buffer, buflen,
                                    f->ofs, f->type);
      if (to_return != NSS_SUCCESS)
        function_return (to_return);
    }
  function_return (NSS_SUCCESS);
}

