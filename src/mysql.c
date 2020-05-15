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
#include <string.h>     /* strlen() */
#include <netinet/in.h> /* struct sockaddr_in */

#ifndef HAVE_SOCKLEN_T
typedef size_t socklen_t;
#endif

con_info_t ci = { false };
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
static bool
_nss_mysql_is_same_sockaddr (struct sockaddr orig, struct sockaddr cur)
{
  DENTER
  switch (((struct sockaddr_in *)&ci.sock_info.local)->sin_family)
    {
    case AF_INET:
        if ((*(struct sockaddr_in *) &orig).sin_port != 
            (*(struct sockaddr_in *) &cur).sin_port)
          DBRETURN (false)
        if ((*(struct sockaddr_in *) &orig).sin_addr.s_addr != 
            (*(struct sockaddr_in *) &cur).sin_addr.s_addr)
          DBRETURN (false)
        break;
    case AF_UNIX:
        if (memcmp (&orig, &cur, sizeof (struct sockaddr)) != 0)
          DBRETURN (false)
        break;
    default:
        _nss_mysql_log (LOG_ERR, "%s: Unhandled sin_family", __FUNCTION__);
        DBRETURN (false)
        break;
    }
  DBRETURN (true)
}

/*
 * Check to see what current socket info is and compare to the saved
 * socket info (from _nss_mysql_save_socket_info() above).  Return
 * true if the current socket and saved socket match.
 */
static bool
_nss_mysql_validate_socket (void)
{
  socklen_t local_size = sizeof (struct sockaddr);
  socklen_t remote_size = sizeof (struct sockaddr);
  struct sockaddr check;

  DENTER
  memset(&check, 0, sizeof (check));
  if (getpeername (ci.link.net.fd, &check, &remote_size) != RETURN_SUCCESS)
    DBRETURN (false)
  if (_nss_mysql_is_same_sockaddr (ci.sock_info.remote, check) != true)
    DBRETURN (false)

  memset(&check, 0, sizeof (check));
  if (getsockname (ci.link.net.fd, &check, &local_size) != RETURN_SUCCESS)
    DBRETURN (false)
  if (_nss_mysql_is_same_sockaddr (ci.sock_info.local, check) != true)
    DBRETURN (false)

  DBRETURN (true)
}

NSS_STATUS
_nss_mysql_close_sql (MYSQL_RES **mresult, bool graceful)
{
  DENTER
  _nss_mysql_close_result (mresult);
  if (graceful && ci.valid)
    {
      D ("%s: calling mysql_close()", __FUNCTION__);
      mysql_close (&ci.link);
    }
  ci.valid = false;
  DSRETURN (NSS_SUCCESS)
}

static void
_nss_mysql_set_options (sql_server_t *server)
{
  const unsigned int def_timeout = DEF_TIMEOUT;

  DENTER

  mysql_options(&ci.link, MYSQL_OPT_CONNECT_TIMEOUT,
                (const char *) &def_timeout);
  mysql_options(&ci.link, MYSQL_READ_DEFAULT_GROUP, "libnss-mysql");
#if MYSQL_VERSION_ID >= 50013
  const bool reconnect = 1;
  mysql_options(&ci.link, MYSQL_OPT_RECONNECT, &reconnect);
#else
  ci.link.reconnect = (my_bool) 1;
#endif

  DEXIT
}

/*
 * Validate existing connection.
 * This does NOT run mysql_ping because that function is
 * extraordinarily slow (~doubles time to fetch a query)
 */
static bool
_nss_mysql_check_existing_connection (MYSQL_RES **mresult)
{
  static pid_t pid = -1;

  DENTER
  if (ci.valid == false || conf.valid == false)
    DBRETURN (false)

  if (pid == -1)
    pid = getpid ();
  else if (pid == getppid ())
    {
      /* saved pid == ppid = we've forked; We MUST create a new connection */
      D ("%s: fork() detected", __FUNCTION__);
      ci.valid = false;
      pid = getpid ();
      DBRETURN (false)
    }

  if (_nss_mysql_validate_socket () == false)
    {
       /* Do *NOT* CLOSE_LINK - the socket is invalid! */
      D ("%s: invalid socket detected", __FUNCTION__);
      _nss_mysql_close_sql (mresult, false);
      ci.valid = false;
      DBRETURN (false)
    }

  DBRETURN (true)
}

/*
 * Connect to a MySQL server.
 */
static NSS_STATUS
_nss_mysql_connect_sql (MYSQL_RES **mresult)
{
  int retval;
  sql_server_t *server = &conf.sql.server;
  unsigned int port;

  DENTER

  if (_nss_mysql_check_existing_connection (mresult) == true)
    {
      D ("%s: Using existing connection", __FUNCTION__);
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

  _nss_mysql_set_options (server);
  D ("%s: Connecting to %s", __FUNCTION__, server->host);
  if (server->port)
    port = atoi (server->port);
  else
    port = 0;
  if (mysql_real_connect (&ci.link, server->host,
                          server->username[0] ? server->username : NULL,
                          server->password[0] ? server->password : NULL,
                          server->database, port,
                          server->socket[0] ? server->socket : NULL,
                          0))
    {
      if (_nss_mysql_save_socket_info () != RETURN_SUCCESS )
        {
          _nss_mysql_log (LOG_ALERT, "Unable to save socket info");
          _nss_mysql_close_sql (mresult, true);
          DSRETURN (NSS_UNAVAIL)
        }
      ci.valid = true;
      /* Safety: We can't let MySQL assume socket is still valid; see _nss_mysql_validate_socket */
#if MYSQL_VERSION_ID >= 50013
      const bool reconnect = 0;
      mysql_options(&ci.link, MYSQL_OPT_RECONNECT, &reconnect);
#else
      ci.link.reconnect = (my_bool) 0;
#endif

      DSRETURN (NSS_SUCCESS)
    }
  _nss_mysql_log (LOG_ALERT, "Connection to server '%s' failed: %s",
                  server->host, mysql_error (&ci.link));
  DSRETURN (NSS_UNAVAIL)
}

void
_nss_mysql_close_result (MYSQL_RES **mresult)
{
  DENTER
  if (mresult && *mresult && ci.valid)
    {
      D ("%s, calling mysql_free_result()", __FUNCTION__);
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
  int retval;

  DENTER
  if (!query)
    DSRETURN (NSS_NOTFOUND)

  D ("%s: Executing query: %s", __FUNCTION__, query);

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
          DSRETURN (NSS_UNAVAIL);
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
  DENTER
  if (_nss_mysql_connect_sql (mresult) != NSS_SUCCESS)
    DSRETURN (NSS_UNAVAIL)
  mysql_real_escape_string (&ci.link, to, from, strlen(from));
  DSRETURN (NSS_SUCCESS)
}
