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

con_info_t ci = { nfalse, 0 };
extern conf_t conf;

/*
 * Immediately after connecting to a MySQL server, save the current
 * socket information for later comparison purposes
 */
static freturn_t
_nss_mysql_save_socket_info (void)
{
  DN ("_nss_mysql_save_socket_info")
  socklen_t local_size = sizeof (struct sockaddr);
  socklen_t remote_size = sizeof (struct sockaddr);
  int r;

  DENTER
  memset(&(ci.sock_info.local), 0, sizeof (ci.sock_info.local));
  r = getsockname (ci.link.net.fd, &(ci.sock_info.local), &local_size);
  if (r != RETURN_SUCCESS)
    DFRETURN (RETURN_FAILURE)

  memset(&(ci.sock_info.remote), 0, sizeof (ci.sock_info.remote));
  r = getpeername (ci.link.net.fd, &(ci.sock_info.remote), &remote_size);
  DFRETURN (r)
}

/*
 * Compare ORIG and CUR
 */
static nboolean
_nss_mysql_is_same_sockaddr (struct sockaddr orig, struct sockaddr cur)
{
  DN ("_nss_mysql_is_same_sockaddr")
  struct sockaddr_in orig_sin, cur_sin;
  sa_family_t family;

  DENTER
  family = *(sa_family_t *)&orig;
  switch (family)
    {
    case AF_INET:
        orig_sin = *(struct sockaddr_in *) &orig;
        cur_sin = *(struct sockaddr_in *) &cur;
        if (orig_sin.sin_port != cur_sin.sin_port)
          DBRETURN (nfalse)
        if (orig_sin.sin_addr.s_addr != cur_sin.sin_addr.s_addr)
          DBRETURN (nfalse)
        break;
    case AF_UNIX:
        if (memcmp (&orig, &cur, sizeof (struct sockaddr)) != 0)
          DBRETURN (nfalse)
        break;
    default:
        _nss_mysql_log (LOG_ERR, "%s: Unhandled family: %d", FUNCNAME, family);
        break;
    }
  DBRETURN (ntrue)
}

/*
 * Check to see what current socket info is and compare to the saved
 * socket info (from _nss_mysql_save_socket_info() above).  Return
 * NTRUE if the current socket and saved socket match.
 */
static nboolean
_nss_mysql_validate_socket (void)
{
  DN ("_nss_mysql_validate_socket")
  socklen_t local_size = sizeof (struct sockaddr);
  socklen_t remote_size = sizeof (struct sockaddr);
  struct sockaddr check;

  DENTER
  memset(&check, 0, sizeof (check));
  if (getpeername (ci.link.net.fd, &check, &remote_size) != RETURN_SUCCESS)
    DBRETURN (nfalse)
  if (_nss_mysql_is_same_sockaddr (ci.sock_info.remote, check) != ntrue)
    DBRETURN (nfalse)

  memset(&check, 0, sizeof (check));
  if (getsockname (ci.link.net.fd, &check, &local_size) != RETURN_SUCCESS)
    DBRETURN (nfalse)
  if (_nss_mysql_is_same_sockaddr (ci.sock_info.local, check) != ntrue)
    DBRETURN (nfalse)

  DBRETURN (ntrue)
}

NSS_STATUS
_nss_mysql_close_sql (MYSQL_RES **mresult, nboolean graceful)
{
  DN ("_nss_mysql_close_sql")
  DENTER
  _nss_mysql_close_result (mresult);
  if (graceful && ci.valid)
    mysql_close (&(ci.link));
  ci.valid = nfalse;
  DSRETURN (NSS_SUCCESS)
}

/*
 * Attempt to connect to specified server number
 */
static NSS_STATUS
_nss_mysql_try_server (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_try_server")
  sql_server_t *server = &conf.sql.server[ci.server_num];
  int flags = 0;

  DENTER
#if MYSQL_VERSION_ID >= 32309
  if (server->ssl)
    flags |= CLIENT_SSL;
#endif

  D ("%s: Connecting to %s:%s@%s, %u %s %d", FUNCNAME, server->username,
                                             server->password, server->host,
                                             server->port, server->socket,
                                             flags);
  time (&server->status.last_attempt);
  server->status.up = nfalse;

#if MYSQL_VERSION_ID >= 32210
  mysql_options(&(ci.link), MYSQL_READ_DEFAULT_GROUP, PACKAGE);
#endif /* MYSQL_VERSION_ID */

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
          _nss_mysql_log (LOG_ALERT, "Unable to select database %s: %s",
                          server->database, mysql_error ((&ci.link)));
          _nss_mysql_close_sql (mresult, ntrue);
          DSRETURN (NSS_UNAVAIL)
        }
      if (_nss_mysql_save_socket_info () != RETURN_SUCCESS )
        {
          _nss_mysql_log (LOG_ALERT, "Unable to save socket info");
          _nss_mysql_close_sql (mresult, ntrue);
          DSRETURN (NSS_UNAVAIL)
        }
      ci.valid = ntrue;
      server->status.up = ntrue;
#if MYSQL_VERSION_ID >= 32118
      ci.link.reconnect = 0; /* Safety: We can't let MySQL assume socket is
                                still valid; see _nss_mysql_validate_socket */
#endif
      DSRETURN (NSS_SUCCESS)
    }
  _nss_mysql_log (LOG_ALERT, "Connection to server '%s' failed: %s",
                  server->host, mysql_error (&(ci.link)));
  DSRETURN (NSS_UNAVAIL)
}

/*
 * Validate existing connection.
 * This does NOT run mysql_ping because that function is
 * extraordinarily slow (~doubles time to fetch a query)
 */
static nboolean
_nss_mysql_check_existing_connection (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_check_existing_connection")
  static int euid = -1;
  static pid_t pid = -1;
  time_t curTime;

  DENTER
  if (ci.valid == nfalse)
    DBRETURN (nfalse)

  if (pid == -1)
    {
      pid = getpid ();
    }
  else if (pid == getppid ())
    {
      /* saved pid == ppid = we've forked; We MUST create a new connection */
      ci.valid = nfalse;
      pid = getpid ();
      DBRETURN (nfalse)
    }

  if (_nss_mysql_validate_socket () == nfalse)
    {
       /* Do *NOT* CLOSE_LINK - the socket is invalid! */
      _nss_mysql_close_sql (mresult, nfalse);
      ci.valid = nfalse;
      DBRETURN (nfalse)
    }
   /* Make sure euid hasn't changed, thus changing our access abilities */
  if (euid == -1)
    euid = geteuid ();
  else if (euid != geteuid ())
    {
      _nss_mysql_close_sql (mresult, ntrue);
      conf.valid = nfalse;
      (void) _nss_mysql_load_config ();
      euid = geteuid ();
      DBRETURN (nfalse)
    }

  /* Force reversion to primary if appropriate */
  if (mresult == NULL && ci.server_num != 0 &&
      conf.sql.server[0].status.last_attempt + conf.global.retry <=
        time (&curTime))
    {
      _nss_mysql_close_sql (mresult, ntrue);
      DBRETURN (nfalse)
    }
  DBRETURN (ntrue)
}

static NSS_STATUS
_nss_mysql_pick_server (void)
{
  DN ("_nss_mysql_pick_server")
  time_t curTime;
  int i;

  DENTER
  time (&curTime);
  for (i = 0; i < MAX_SERVERS; i++)
    {
      if (conf.sql.server[i].status.valid == nfalse)
        continue;
      if (conf.sql.server[i].status.up == ntrue)
        {
          ci.server_num = i;
          DSRETURN (NSS_SUCCESS)
        }
      else
        {
          if (conf.sql.server[i].status.last_attempt == 0 ||
              conf.sql.server[i].status.last_attempt + conf.global.retry <=
                curTime)
            {
              ci.server_num = i;
              DSRETURN (NSS_SUCCESS)
            }
        }
    }
  /* If we get this far, simply try server #0 (if it's "valid") */
  if (conf.sql.server[0].status.valid)
    {
      ci.server_num = 0;
      DSRETURN (NSS_SUCCESS)
    }
  else
    DSRETURN (NSS_UNAVAIL)
}

/*
 * Connect to a MySQL server.  Try all servers defined until a working
 * one is found.  If current server != primary server, attempt to revert
 * to primary if RETRY (config option) seconds has passed and we're not
 * in the middle of an active result set.
 * Set CI.VALID to ntrue if we manage to connect to a server.
 */
static NSS_STATUS
_nss_mysql_connect_sql (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_connect_sql")

  DENTER
  if (_nss_mysql_check_existing_connection (mresult) == ntrue)
    DSRETURN (NSS_SUCCESS)

  /* Because check_existing_connection can try to reload the config */
  if (conf.valid == nfalse)
    DSRETURN (NSS_UNAVAIL)

#ifdef HAVE_MYSQL_INIT
  if (mysql_init (&(ci.link)) == NULL)
    {
      _nss_mysql_log (LOG_ALERT, "mysql_init() failed");
      DSRETURN (NSS_UNAVAIL)
    }
#endif /* HAVE_MYSQL_INIT */

  while (_nss_mysql_pick_server () == NSS_SUCCESS)
    {
      if (_nss_mysql_try_server (mresult) == NSS_SUCCESS)
        DSRETURN (NSS_SUCCESS)
    }
  _nss_mysql_log (LOG_ALERT, "Unable to connect to any MySQL servers");
  DSRETURN (NSS_UNAVAIL)
}

void
_nss_mysql_close_result (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_close_result")
  DENTER
  if (mresult && *mresult && ci.valid)
    mysql_free_result (*mresult);
  if (mresult)
    *mresult = NULL;
  DEXIT
}

static void
_nss_mysql_fail_server (MYSQL_RES **mresult, int server_num)
{
  DN ("_nss_mysql_fail_server")

  DENTER
  _nss_mysql_close_sql (mresult, ntrue);
  conf.sql.server[server_num].status.up = nfalse;
  time (&conf.sql.server[server_num].status.last_attempt);
  DEXIT
}

/*
 * Run QUERY against an existing MySQL connection.  If the query fails,
 * try to run the query against all other defined MySQL servers (in case
 * the query failure is due to a server failure/loss).
 * Caller should guarantee that QUERY isn't null or empty
 */
NSS_STATUS
_nss_mysql_run_query (char *query, MYSQL_RES **mresult)
{
  DN ("_nss_mysql_run_query")

  DENTER
  if (!query || !strlen (query))
    DSRETURN (NSS_NOTFOUND)

  D ("%s: Executing query: %s", FUNCNAME, query);
  while (_nss_mysql_connect_sql (mresult) == NSS_SUCCESS)
    {
      if ((mysql_query (&(ci.link), query)) != RETURN_SUCCESS)
        {
          _nss_mysql_log (LOG_ALERT, "mysql_query failed: %s",
                          mysql_error (&(ci.link)));
          _nss_mysql_fail_server (mresult, ci.server_num);
          continue;
        }
      if ((*mresult = mysql_store_result (&(ci.link))) == NULL)
        {
          _nss_mysql_log (LOG_ALERT, "mysql_store_result failed: %s",
                          mysql_error (&(ci.link)));
          _nss_mysql_fail_server (mresult, ci.server_num);
          continue;
        }
      DSRETURN (NSS_SUCCESS)
    }
  _nss_mysql_log (LOG_ALERT, "Unable to perform query on any MySQL server");
  DSRETURN (NSS_UNAVAIL)
}

NSS_STATUS
_nss_mysql_fetch_row (MYSQL_ROW *row, MYSQL_RES *mresult)
{
  DN ("_nss_mysql_fetch_row")

  DENTER
  if ((*row = mysql_fetch_row (mresult)) == NULL)
    {
      if (mysql_errno (&(ci.link)))
        {
          _nss_mysql_log (LOG_ALERT, "mysql_fetch_row() failed: %s",
                          mysql_error (&(ci.link)));
          DSRETURN (NSS_UNAVAIL)
        }
      else
        {
          DSRETURN (NSS_NOTFOUND)
        }
    }
  DSRETURN (NSS_SUCCESS)
}

NSS_STATUS
_nss_mysql_escape_string (char *to, const char *from, MYSQL_RES **mresult)
{
  DN ("_nss_mysql_escape_string")

  DENTER
#if MYSQL_VERSION_ID >= 32300 /* comes from mysql.h, NOT config.h! */
  if (_nss_mysql_connect_sql (mresult) != NSS_SUCCESS)
    DSRETURN (NSS_UNAVAIL)
  mysql_real_escape_string (&(ci.link), to, from, strlen(from));
#else
  mysql_escape_string (to, from, strlen(from));
#endif
  DSRETURN (NSS_SUCCESS)
}

