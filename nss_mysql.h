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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_NSS_H
#include <nss.h>
#elif defined HAVE_NSS_COMMON_H
#include <nss_common.h>
#else
#error I need either nss.h or nss_common.h!
#endif
#ifdef HAVE_MYSQL_MYSQL_H
#include <mysql/mysql.h>
#elif defined HAVE_MYSQL_H
#include <mysql.h>
#endif
#include <sys/socket.h>

#ifdef HAVE___FUNC__
#define FNAME __func__
#else
#define FNAME "UNKNOWN"
#endif

#ifdef HAVE_NSS_H
#define NSS_SUCCESS     NSS_STATUS_SUCCESS
#define NSS_NOTFOUND    NSS_STATUS_NOTFOUND
#define NSS_UNAVAIL     NSS_STATUS_UNAVAIL
#define NSS_TRYAGAIN    NSS_STATUS_TRYAGAIN
typedef enum nss_status NSS_STATUS;
#elif defined HAVE_NSSWITCH_H
#else
typedef enum
{
  NSS_SUCCESS,
  NSS_NOTFOUND,
  NSS_UNAVAIL,
  NSS_TRYAGAIN
} NSS_STATUS;
#endif

#define MAX_LINE_LEN    1024            /* Max line length in config file */
#define MAX_KEY_LEN     128             /* Max length of a key in config file */
#define MAX_SERVERS     3               /* Max # of configured SQL servers */
#define PADSIZE         64              /* malloc this much more for queries
                                           to allow for format expansion */
#define SYSLOG_FACILITY LOG_AUTHPRIV
#define SYSLOG_PRIORITY LOG_ALERT

#define FOFS(x,y) ((int)&(((x *)0)->y)) /* Get field offset */
#define FSIZ(x,y) (sizeof(((x *)0)->y)) /* Get field size */

#define D_ERROR         0x0001
#define D_MEMORY        0x0002
#define D_FUNCTION      0x0004
#define D_CONNECT       0x0008
#define D_QUERY         0x0010
#define D_PARSE         0x0020
#define D_FILE          0x0040

#define CLOSE_RESULT    0x0001
#define CLOSE_LINK      0x0002
#define CLOSE_ALL       (CLOSE_RESULT | CLOSE_LINK)

#define PTRSIZE sizeof (void *)

#define function_enter _nss_mysql_debug (FNAME, D_FUNCTION, "BEGIN\n");
#define function_leave _nss_mysql_debug (FNAME, D_FUNCTION, "LEAVE: -\n");
#define function_return(to_return)                                            \
  {                                                                           \
    if (to_return == NSS_TRYAGAIN)                                            \
      errno = ERANGE;                                                         \
    _nss_mysql_debug (FNAME, D_FUNCTION, "LEAVE: %d\n",  to_return);          \
    return to_return;                                                         \
  }

typedef unsigned char _nss_mysql_byte;        /* for pointer arithmetic */

/* Parse types */
typedef enum {
    FT_NONE,
    FT_INT,
    FT_LONG,
    FT_PCHAR,    /* char *  Pointer to char (unallocated) */
    FT_PPCHAR,   /* char ** Pointer to pointer to char (unallocated) */
    FT_UINT,
    FT_ULONG,
} ftype_t;

typedef struct {
    char    *name;
    int     ofs;
    ftype_t type;
} field_info_t;

/* Sql queries to execute for ... */
 typedef struct {
    char        *getpwuid;
    char        *getpwnam;
    char        *getspnam;
    char        *getpwent;
    char        *getspent;
    char        *getgrnam;
    char        *getgrgid;
    char        *getgrent;
} sql_query_t;

typedef struct {
    char        *host;      /* SQL Server to connect to */
    unsigned int port;      /* SQL port to connect to */
    char        *socket;    /* SQL socket path to use */
    char        *username;  /* Username to connect as */
    char        *password;  /* Password to connect with */
    char        *database;  /* SQL Database to open */
} sql_server_t;

typedef struct {
    sql_query_t     query;
    sql_server_t    server;
} sql_conf_t;

typedef struct {
    int             retry;
} global_conf_t;

typedef struct {
    int             valid;
    int             num_servers;
    global_conf_t   global;
    sql_conf_t      defaults;
    sql_conf_t      sql[MAX_SERVERS];
} conf_t;

typedef struct {
    struct sockaddr local;
    struct sockaddr remote;
} socket_info_t;

/* Used for static-variable state retention */
typedef struct {
    MYSQL_RES       *mysql_result;  /* Current result set */
    int             server_num;     /* Server # currently connected to */
    int             idx;            /* result set index (get/set/end*ent) */
    time_t          last_attempt;   /* last time we tried primary server */
    int             valid_link;     /* is LINK below valid? */
    MYSQL           link;           /* Current MySQL link info */
    socket_info_t   sock_info;      /* store get[sock|peer]name results here */
    char            *query[MAX_SERVERS];
} state_t;

void _nss_mysql_debug(char *function, int flags, char *fmt, ...);
NSS_STATUS _nss_mysql_close_sql (state_t *state, int flags);
NSS_STATUS _nss_mysql_init(conf_t *conf, state_t *state);
NSS_STATUS _nss_mysql_run_query(conf_t conf, state_t *state);
NSS_STATUS _nss_mysql_load_result(void *result, char *buffer,
                                       size_t buflen, state_t *state,
                                       field_info_t *fields, int idx);
void _nss_mysql_log_error (char *function, char *fmt, ...);
NSS_STATUS _nss_mysql_load_config (conf_t *conf);

extern field_info_t passwd_fields[];
extern field_info_t spwd_fields[];
extern field_info_t group_fields[];

