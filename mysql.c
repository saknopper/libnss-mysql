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
#include <netinet/in.h>

#ifndef HAVE_SOCKLEN_T
typedef size_t socklen_t;
#endif

con_info_t ci = { nfalse };
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
  family = ((struct sockaddr_in *)&ci.sock_info.local)->sin_family;
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
    {
      D ("%s: calling mysql_close()", FUNCNAME);
      mysql_close (&ci.link);
    }
  ci.valid = nfalse;
  DSRETURN (NSS_SUCCESS)
}

static void
_nss_mysql_set_options (sql_server_t *server, int *flags)
{
  DN ("_nss_mysql_set_options")

  DENTER
  if (server->options.ssl)
    {
      D ("%s: Setting CLIENT_SSL flag", FUNCNAME);
      *flags |= CLIENT_SSL;
    }
  D ("%s: Setting connect timeout to %d", FUNCNAME, server->options.timeout);
  mysql_options(&ci.link, MYSQL_OPT_CONNECT_TIMEOUT,
                (char *)&server->options.timeout);
  if (server->options.compress)
    {
      D ("%s: Setting compressed protocol", FUNCNAME);
      mysql_options(&ci.link, MYSQL_OPT_COMPRESS, 0);
    }
  if (server->options.initcmd && strlen (server->options.initcmd))
    {
      D ("%s: Setting init-command to '%s'", FUNCNAME, server->options.initcmd);
      mysql_options(&ci.link, MYSQL_INIT_COMMAND,
                    (char *)server->options.initcmd);
    }
  DEXIT
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
  static pid_t pid = -1;

  DENTER
  if (ci.valid == nfalse || conf.valid == nfalse)
    DBRETURN (nfalse)

  if (pid == -1)
    pid = getpid ();
  else if (pid == getppid ())
    {
      /* saved pid == ppid = we've forked; We MUST create a new connection */
      D ("%s: fork() detected", FUNCNAME);
      ci.valid = nfalse;
      pid = getpid ();
      DBRETURN (nfalse)
    }

  if (_nss_mysql_validate_socket () == nfalse)
    {
       /* Do *NOT* CLOSE_LINK - the socket is invalid! */
      D ("%s: invalid socket detected", FUNCNAME);
      _nss_mysql_close_sql (mresult, nfalse);
      ci.valid = nfalse;
      DBRETURN (nfalse)
    }

  DBRETURN (ntrue)
}

/*
 * Connect to a MySQL server.
 */
static NSS_STATUS
_nss_mysql_connect_sql (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_connect_sql")
  int retval;
  sql_server_t *server = &conf.sql.server;
  int flags = 0;

  DENTER

  if (_nss_mysql_check_existing_connection (mresult) == ntrue)
    {
      D ("%s: Using existing connection", FUNCNAME);
      DSRETURN (NSS_SUCCESS)
    }

  retval = _nss_mysql_load_config ();
  if (retval != NSS_SUCCESS)
    {
      _nss_mysql_log (LOG_ALERT, "Failed to load configuration");
      DSRETURN (NSS_UNAVAIL)
    }

  if (mysql_init (&ci.link) == NULL)
    {
      _nss_mysql_log (LOG_ALERT, "mysql_init() failed");
      DSRETURN (NSS_UNAVAIL)
    }

  _nss_mysql_set_options (server, &flags);
  D ("%s: Connecting to %s", FUNCNAME, server->host);
  if (mysql_real_connect (&ci.link, server->host, server->username,
                          server->password, server->database, server->port,
                          server->socket, flags))
    {
      if (_nss_mysql_save_socket_info () != RETURN_SUCCESS )
        {
          _nss_mysql_log (LOG_ALERT, "Unable to save socket info");
          _nss_mysql_close_sql (mresult, ntrue);
          DSRETURN (NSS_UNAVAIL)
        }
      ci.valid = ntrue;
      ci.link.reconnect = 0; /* Safety: We can't let MySQL assume socket is
                                still valid; see _nss_mysql_validate_socket */
      DSRETURN (NSS_SUCCESS)
    }
  _nss_mysql_log (LOG_ALERT, "Connection to server '%s' failed: %s",
                  server->host, mysql_error (&ci.link));
  DSRETURN (NSS_UNAVAIL)
}

void
_nss_mysql_close_result (MYSQL_RES **mresult)
{
  DN ("_nss_mysql_close_result")
  DENTER
  if (mresult && *mresult && ci.valid)
    {
      D ("%s, calling mysql_free_result()", FUNCNAME);
      mysql_free_result (*mresult);
    }
  if (mresult)
    *mresult = NULL;
  DEXIT
}

/*
 * Run MySQL query
 */
NSS_STATUS
_nss_mysql_run_query (char *query, MYSQL_RES **mresult, int *attempts)
{
  DN ("_nss_mysql_run_query")
  int retval;

  DENTER
  if (!query || !strlen (query))
    DSRETURN (NSS_NOTFOUND)

  D ("%s: Executing query: %s", FUNCNAME, query);

  retval = _nss_mysql_connect_sql (mresult);
  if (retval != NSS_SUCCESS)
    DSRETURN (retval);

  retval = mysql_query (&ci.link, query);
  if (retval != RETURN_SUCCESS)
    {
      --(*attempts);
      if (*attempts > 0)
        {
          _nss_mysql_log (LOG_ALERT,
                          "mysql_query failed: %s, trying again (%d)",
                          mysql_error (&ci.link), *attempts);
          DSRETURN (_nss_mysql_run_query (query, mresult, attempts));
        }
      else
        {
          _nss_mysql_log (LOG_ALERT, "mysql_query failed: %s",
                          mysql_error (&ci.link));
          DSRETURN (retval);
        }
    }

  if ((*mresult = mysql_store_result (&ci.link)) == NULL)
    {
      _nss_mysql_log (LOG_ALERT, "mysql_store_result failed: %s",
                      mysql_error (&ci.link));
      DSRETURN (NSS_UNAVAIL);
    }
  DSRETURN (NSS_SUCCESS)
}

NSS_STATUS
_nss_mysql_fetch_row (MYSQL_ROW *row, MYSQL_RES *mresult)
{
  DN ("_nss_mysql_fetch_row")

  DENTER
  if ((*row = mysql_fetch_row (mresult)) == NULL)
    {
      if (mysql_errno (&ci.link))
        {
          _nss_mysql_log (LOG_ALERT, "mysql_fetch_row() failed: %s",
                          mysql_error (&ci.link));
          DSRETURN (NSS_UNAVAIL)
        }
      else
        DSRETURN (NSS_NOTFOUND)
    }
  DSRETURN (NSS_SUCCESS)
}

NSS_STATUS
_nss_mysql_escape_string (char *to, const char *from, MYSQL_RES **mresult)
{
  DN ("_nss_mysql_escape_string")

  DENTER
  if (_nss_mysql_connect_sql (mresult) != NSS_SUCCESS)
    DSRETURN (NSS_UNAVAIL)
  mysql_real_escape_string (&ci.link, to, from, strlen(from));
  DSRETURN (NSS_SUCCESS)
}

