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
 * MySQL-specific functions; ALL MySQL calls that any other source functions
 * need should come from here.
 */

static const char rcsid[] =
    "$Id$";

#include "nss_mysql.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <netinet/in.h>

#ifndef HAVE_SOCKLEN_T
typedef size_t socklen_t;
#endif

con_info_t ci = { nfalse, 0, NULL };
extern conf_t conf;

/*
 * Immediately after connecting to a MySQL server, save the current
 * socket information for later comparison purposes
 */
static freturn_t
_nss_mysql_save_socket_info (void)
{
  socklen_t local_size = sizeof (struct sockaddr);
  socklen_t remote_size = sizeof (struct sockaddr);
  int r;

  function_enter;
  memset(&(ci.sock_info.local), 0, sizeof (ci.sock_info.local));
  r = getsockname (ci.link.net.fd, &(ci.sock_info.local), &local_size);
  if (r != RETURN_SUCCESS)
    function_return (RETURN_FAILURE);

  memset(&(ci.sock_info.remote), 0, sizeof (ci.sock_info.remote));
  r = getpeername (ci.link.net.fd, &(ci.sock_info.remote), &remote_size);
  function_return (r);
}

/*
 * Compare ORIG and CUR
 */
static nboolean
_nss_mysql_is_same_sockaddr (struct sockaddr orig, struct sockaddr cur)
{
  struct sockaddr_in orig_sin, cur_sin;
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
        if (memcmp (&orig, &cur, sizeof (struct sockaddr)) != 0)
          function_return (nfalse);
        break;
    default:
        _nss_mysql_log (LOG_ERR, "%s: Unhandled family: %d", FNAME, family);
        break;
    }
  function_return (ntrue);
}

/*
 * Check to see what current socket info is and compare to the saved
 * socket info (from _nss_mysql_save_socket_info() above).  Return
 * NTRUE if the current socket and saved socket match.
 */
static nboolean
_nss_mysql_validate_socket (void)
{
  socklen_t local_size = sizeof (struct sockaddr);
  socklen_t remote_size = sizeof (struct sockaddr);
  struct sockaddr check;

  function_enter;

  memset(&check, 0, sizeof (check));
  if (getpeername (ci.link.net.fd, &check, &remote_size) != RETURN_SUCCESS)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "getpeername() failed");
      function_return (nfalse);
    }
  if (_nss_mysql_is_same_sockaddr (ci.sock_info.remote, check) != ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "remote socket mismatch");
      function_return (nfalse);
    }

  memset(&check, 0, sizeof (check));
  if (getsockname (ci.link.net.fd, &check, &local_size) != RETURN_SUCCESS)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "getsockname() failed");
      function_return (nfalse);
    }
  if (_nss_mysql_is_same_sockaddr (ci.sock_info.local, check) != ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "local socket mismatch");
      function_return (nfalse);
    }

  function_return (ntrue);
}

/*
 * Attempt to connect to specified server number
 */
static NSS_STATUS
_nss_mysql_try_server (void)
{
  sql_server_t *server = &conf.sql.server[ci.server_num];
  int flags = 0;
  function_enter;

#if MYSQL_VERSION_ID >= 32309
  if (server->ssl)
    flags |= CLIENT_SSL;
#endif

  _nss_mysql_debug (FNAME, D_CONNECT, "Connecting to server %s (flags = %d)",
                    server->host, flags);
  time (&server->status.last_attempt);
  server->status.up = nfalse;
#ifdef HAVE_MYSQL_REAL_CONNECT /* comes from mysql.h, NOT config.h! */
#if MYSQL_VERSION_ID >= 32200  /* ditto */
  if (mysql_real_connect (&(ci.link), server->host, server->username,
                          server->password, NULL, server->port,
                          server->socket, flags))
#else /* MYSQL_VERSION_ID < 32200 */
  if (mysql_real_connect (&(ci.link), server->host, server->username,
                          server->password, server->port, server->socket,
                          flags))
#endif /* MYSQL_VERSION_ID >= 32200 */
#else /* HAVE_MYSQL_REAL_CONNECT */
  if (mysql_connect (&(ci.link), server->host, server->username,
                     server->password))
#endif /* HAVE_MYSQL_REAL_CONNECT */
  
    {
      if (mysql_select_db (&(ci.link), server->database) != RETURN_SUCCESS)
        {
          _nss_mysql_log (LOG_EMERG, "Unable to select database %s: %s",
                          server->database, mysql_error ((&ci.link)));
          _nss_mysql_close_sql (CLOSE_LINK);
          function_return (NSS_UNAVAIL);
        }
      if (_nss_mysql_save_socket_info () != RETURN_SUCCESS )
        {
          _nss_mysql_log (LOG_EMERG, "Unable to save socket info");
          _nss_mysql_close_sql (CLOSE_LINK);
          function_return (NSS_UNAVAIL);
        }
      ci.valid = ntrue;
      server->status.up = ntrue;
      _nss_mysql_debug (FNAME, D_CONNECT, "Connected!");
      function_return (NSS_SUCCESS);
    }
  _nss_mysql_log (LOG_ALERT, "Connection to server '%s' failed: %s",
                  server->host, mysql_error (&(ci.link)));
  function_return (NSS_UNAVAIL);
}

/*
 * Validate existing connection.
 * This does NOT run mysql_ping because that function is
 * extraordinarily slow (~doubles time to fetch a query)
 */
static nboolean
_nss_mysql_check_existing_connection (void)
{
  static int euid = -1;
  time_t curTime;

  function_enter;
  if (ci.valid == nfalse)
    function_return (nfalse);

  if (_nss_mysql_validate_socket () == nfalse)
    {
       _nss_mysql_debug (FNAME, D_CONNECT,
                         "Socket changed - forcing reconnect");
       /* Do *NOT* CLOSE_LINK - the socket is invalid! */
      _nss_mysql_close_sql (CLOSE_NOGRACE);
      ci.valid = nfalse;
      function_return (nfalse);
    }
   /* Make sure euid hasn't changed, thus changing our access abilities */
  if (euid == -1)
    euid = geteuid();
  else if (euid != geteuid())
    {
      _nss_mysql_debug (FNAME, D_CONNECT,
                        "euid changed - forcing reconnect");
      _nss_mysql_close_sql (CLOSE_LINK);
      euid = geteuid();
      function_return (nfalse);
    }

  /* Force reversion to primary if appropriate */
  if (ci.result == NULL && ci.server_num != 0 &&
      conf.sql.server[0].status.last_attempt + conf.global.retry <=
        time (&curTime))
    {
      _nss_mysql_debug (FNAME, D_CONNECT,
                        "Attempting to revert to primary server");
      _nss_mysql_close_sql (CLOSE_LINK);
      function_return (nfalse);
    }
  _nss_mysql_debug (FNAME, D_CONNECT, "Using existing link");
  function_return (ntrue);
}

NSS_STATUS
_nss_mysql_pick_server (void)
{
  time_t curTime;
  int i;

  function_enter;
  time (&curTime);
  for (i = 0; i < MAX_SERVERS; i++)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "# v u l: %d %d %d %d", i,
                        conf.sql.server[i].status.valid,
                        conf.sql.server[i].status.up,
                        conf.sql.server[i].status.last_attempt);
      if (conf.sql.server[i].status.valid == nfalse)
        continue;
      if (conf.sql.server[i].status.up == ntrue)
        {
          _nss_mysql_debug (FNAME, D_CONNECT, "Picked %d (up)", i);
          ci.server_num = i;
          function_return (NSS_SUCCESS);
        }
      else
        {
          if (conf.sql.server[i].status.last_attempt == 0 ||
              conf.sql.server[i].status.last_attempt + conf.global.retry <=
                curTime)
            {
              _nss_mysql_debug (FNAME, D_CONNECT, "Picked %d (time)", i);
              ci.server_num = i;
              function_return (NSS_SUCCESS);
            }
        }
    }
  _nss_mysql_debug (FNAME, D_CONNECT, "Found no available servers", i);
  function_return (NSS_UNAVAIL);
}

/*
 * Connect to a MySQL server.  Try all servers defined until a working
 * one is found.  If current server != primary server, attempt to revert
 * to primary if RETRY (config option) seconds has passed and we're not
 * in the middle of an active result set.
 * Set CI.VALID to ntrue if we manage to connect to a server.
 */
NSS_STATUS
_nss_mysql_connect_sql (void)
{
  function_enter;
  if (_nss_mysql_check_existing_connection () == ntrue)
    function_return (NSS_SUCCESS);

#ifdef HAVE_MYSQL_INIT
  if (mysql_init (&(ci.link)) == NULL)
    {
      _nss_mysql_log (LOG_EMERG, "mysql_init() failed");
      function_return (NSS_UNAVAIL);
    }
#endif /* HAVE_MYSQL_INIT */

#if MYSQL_VERSION_ID >= 32210
  mysql_options(&(ci.link), MYSQL_READ_DEFAULT_GROUP, PACKAGE);
#endif /* MYSQL_VERSION_ID */

  while (_nss_mysql_pick_server () == NSS_SUCCESS)
    {
      if (_nss_mysql_try_server () == NSS_SUCCESS)
        function_return (NSS_SUCCESS);
    }
  _nss_mysql_log (LOG_EMERG, "Unable to connect to any MySQL servers");
  function_return (NSS_UNAVAIL);
}

/*
 * If flags | CLOSE_RESULT, then finish fetching any remaining MySQL rows
 * and free the MySQL result stored in CI.
 * If flags | CLOSE_LINK, then CLOSE_RESULT and close the link, setting
 * CI.VALID to nfalse.
 */
NSS_STATUS
_nss_mysql_close_sql (int flags)
{

  function_enter;
  if (flags & (CLOSE_RESULT | CLOSE_LINK) && ci.valid == ntrue && ci.result)
    {
      _nss_mysql_debug (FNAME, D_CONNECT | D_MEMORY, "Freeing result");
      /* mysql_use_result requires fetching all rows */
      while (mysql_fetch_row (ci.result) != NULL) {}
      mysql_free_result (ci.result);
      ci.result = NULL;
    }
  if (flags & CLOSE_LINK && ci.valid == ntrue)
    {
      _nss_mysql_debug (FNAME, D_CONNECT, "Closing link");
      mysql_close (&(ci.link));
      ci.valid = nfalse;
    }
  if (flags & CLOSE_NOGRACE)
    {
      /* 
       * This leaks memory.  But if something stomped on our connection,
       * we don't have much choice ...
       */
      _nss_mysql_debug (FNAME, D_CONNECT,
                        "Ungracefully closing link & result");
      ci.valid = nfalse;
      ci.result = NULL;
    }
  function_return (NSS_SUCCESS);
}

void
_nss_mysql_fail_server (int server_num)
{
  function_enter;
  _nss_mysql_close_sql (CLOSE_LINK);
  conf.sql.server[server_num].status.up = nfalse;
  time (&conf.sql.server[server_num].status.last_attempt);
  function_leave;
}

/*
 * Run QUERY against an existing MySQL connection.  If the query fails,
 * try to run the query against all other defined MySQL servers (in case
 * the query failure is due to a server failure/loss).
 * Caller should guarantee that QUERY isn't null or empty
 */
NSS_STATUS
_nss_mysql_run_query (char *query)
{
  function_enter;

  while (_nss_mysql_connect_sql () == NSS_SUCCESS)
    {
      _nss_mysql_debug (FNAME, D_QUERY, "Executing query on server #%d: %s",
                        ci.server_num, query);
      if ((mysql_query (&(ci.link), query)) != RETURN_SUCCESS)
        {
          _nss_mysql_log (LOG_ALERT, "mysql_query failed: %s",
                          mysql_error (&(ci.link)));
          _nss_mysql_fail_server (ci.server_num);
          continue;
        }
      if ((ci.result = mysql_use_result (&(ci.link))) == NULL)
        {
          _nss_mysql_log (LOG_ALERT, "mysql_use_result failed: %s",
                          mysql_error (&(ci.link)));
          _nss_mysql_fail_server (ci.server_num);
          continue;
        }
      function_return (NSS_SUCCESS);
    }
  _nss_mysql_log (LOG_EMERG, "Unable to perform query on any MySQL server");
  function_return (NSS_UNAVAIL);
}

/*
 * Fetch next row from a MySQL result set and load the data into
 * RESULT using FIELDS as a guide for what goes where
 */
NSS_STATUS
_nss_mysql_load_result (void *result, char *buffer, size_t buflen,
                        field_info_t *fields)
{
  MYSQL_ROW row;
  int to_return;
  field_info_t *f;
  int i;
  char *p;
  int bufused;

  function_enter;
  if ((row = mysql_fetch_row (ci.result)) == NULL)
    {
      if (mysql_errno (&(ci.link)))
        {
          _nss_mysql_log (LOG_ALERT, "mysql_fetch_row() failed: %s",
                          mysql_error (&(ci.link)));
          function_return (NSS_UNAVAIL);
        }
      else
        {
          _nss_mysql_debug (FNAME, D_QUERY, "NOTFOUND in mysql_fetch_row()");
          function_return (NSS_NOTFOUND);
        }
    }

  memset (buffer, 0, buflen);
  bufused = 0;
  for (f = fields, i = 0; f->name; f++, i++)
    {
      p = buffer + bufused;
      _nss_mysql_debug (FNAME, D_PARSE, "Loading: %s = '%s' (%d %d %d)",
                        f->name, row[i], i, f->ofs, f->type);
      to_return = _nss_mysql_liswb (row[i], result, p, buflen, &bufused,
                                    f->ofs, f->type);
      if (to_return != NSS_SUCCESS)
        function_return (to_return);
    }
  function_return (NSS_SUCCESS);
}

/*
 * SET/END ent's call this.  While the definition of endent is to close
 * the file, I see no reason to actually do that - just clear the
 * current result set.
 */
void
_nss_mysql_reset_ent (void)
{
  function_enter;
  _nss_mysql_close_sql (CLOSE_RESULT);
  function_leave;
}

/*
 * Returns NTRUE if there's a currently-active result set
 */
nboolean
_nss_mysql_active_result (void)
{
  function_enter;
  function_return (ci.result != NULL);
}


NSS_STATUS
_nss_mysql_escape_string (char *to, const char *from)
{
  function_enter;
#if MYSQL_VERSION_ID >= 32300 /* comes from mysql.h, NOT config.h! */
  if (_nss_mysql_connect_sql () != NSS_SUCCESS)
    function_return (NSS_UNAVAIL);
  mysql_real_escape_string (&(ci.link), to, from, strlen(from));
#else
  mysql_escape_string (to, from, strlen(from));
#endif
  function_return (NSS_SUCCESS);
}

