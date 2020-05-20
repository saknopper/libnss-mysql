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
#else
#error I need nss.h!
#endif

#include <mysql.h>

#include <sys/socket.h>
#include <errno.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdlib.h> /* realloc(), free(), malloc(), atoi() */

#include <pthread.h>

#define NSS_SUCCESS     NSS_STATUS_SUCCESS
#define NSS_NOTFOUND    NSS_STATUS_NOTFOUND
#define NSS_UNAVAIL     NSS_STATUS_UNAVAIL
#define NSS_TRYAGAIN    NSS_STATUS_TRYAGAIN
typedef enum nss_status NSS_STATUS;

#define MAX_LINE_SIZE       1024        /* Max line length in config file */
#define MAX_QUERY_SIZE      2048        /* Max size of SQL query */
#define MAX_NAME_SIZE       128         /* Max username/groupname size */
#define MAX_KEY_SIZE        128         /* Max length of a key in cfg file */
#define MAX_VAL_SIZE        1024        /* Max length of a val in cfg file */
#define MAX_QUERY_ATTEMPTS  3           /* # of query retries */

/* Default initializers */
#define DEF_TIMEOUT         3

#ifdef DEBUG
void _nss_mysql_debug (char *fmt, ...);
static const char DEBUG_FILE[] = "/tmp/libnss-mysql-debug.log";
#define D _nss_mysql_debug
#define DENTER D ("%s: ENTER", __FUNCTION__);
#define DIRETURN(r) { D ("%s: EXIT (%d)", __FUNCTION__, r); return (r); }
#define DFRETURN(r)                                                         \
  {                                                                         \
    D ("%s: EXIT (%s)", __FUNCTION__, r == 0 ? "SUCCESS" : "FAIL");             \
    return (r);                                                             \
  }
#define DBRETURN(r)                                                         \
  {                                                                         \
    D ("%s: EXIT (%s)", __FUNCTION__, r == true ? "TRUE" : "FALSE");           \
    return (r);                                                             \
  }
#define DSRETURN(r)                                                         \
  {                                                                         \
    char *status;                                                           \
    switch (r)                                                              \
      {                                                                     \
      case NSS_SUCCESS:                                                     \
        status = "NSS_SUCCESS";                                             \
        break;                                                              \
      case NSS_NOTFOUND:                                                    \
        status = "NSS_NOTFOUND";                                            \
        break;                                                              \
      case NSS_UNAVAIL:                                                     \
        status = "NSS_UNAVAIL";                                             \
        break;                                                              \
      case NSS_TRYAGAIN:                                                    \
        status = "NSS_TRYAGAIN";                                            \
        break;                                                              \
      default:                                                              \
        status = "UNKNOWN";                                                 \
        break;                                                              \
      }                                                                     \
    D ("%s: EXIT (%s)", __FUNCTION__, status);                                  \
    return (r);                                                             \
  }
#define DPRETURN(r) { D ("%s: EXIT (%p)", __FUNCTION__, r); return (r); }
#define DRETURN { D ("%s: EXIT", __FUNCTION__); return; }
#define DEXIT D ("%s: EXIT", __FUNCTION__);
#else
#define D
#define DENTER
#define DIRETURN(r) return (r);
#define DPRETURN(r) return (r);
#define DFRETURN(r) return (r);
#define DBRETURN(r) return (r);
#define DSRETURN(r) return (r);
#define DRETURN return;
#define DEXIT
#endif

extern pthread_mutex_t lock;
#define LOCK pthread_mutex_lock (&lock)
#define UNLOCK pthread_mutex_unlock (&lock)

/*
 * Linux and Solaris handle buffer exhaustion differently.
 * Linux sets errno to ERANGE and returns TRYAGAIN, which results in
 * the NSS system trying with a buffer twice as big.
 * Solaris, however, doesn't seem to retry.  I've checked the Solaris 8
 * code for files/ldap/nisplus NSS and they all set NSS_ARGS(args)->erange
 * to 1 and return NOTFOUND.  Note that this macro sets *errnop to 1, but
 * it's not really errnop, it's erange - see the calling functions.
 * In fact, my tests reveal that if you return TRYAGAIN, Solaris will try
 * over and over, without increasing the buffer - AKA infinite (or long)
 * loop.
 */
#define EXHAUSTED_BUFFER                                                     \
  {                                                                          \
    *errnop = ERANGE;                                                        \
    DSRETURN (NSS_TRYAGAIN);                                                 \
  }

/*
 * It's SO damn confusing when functions use a return of 0 for success and
 * 1 for failure, especially amidst functions that have a boolean return
 * type.. so use this instead.  PLEASE.  I BEG YOU.
 */
typedef enum {
    RETURN_SUCCESS,
    RETURN_FAILURE
} freturn_t;

typedef enum {
    BYNONE,
    BYNAME,
    BYNUM
} lookup_t;

typedef struct {
    gid_t       **groupsp;
    long int    group;
    long int    *start;
    long int    *size;
    long int    limit;
} group_info_t;

/* Sql queries to execute for ... */
typedef struct {
    char        getpwuid[MAX_VAL_SIZE];
    char        getpwnam[MAX_VAL_SIZE];
    char        getspnam[MAX_VAL_SIZE];
    char        getpwent[MAX_VAL_SIZE];
    char        getspent[MAX_VAL_SIZE];
    char        getgrnam[MAX_VAL_SIZE];
    char        getgrgid[MAX_VAL_SIZE];
    char        getgrent[MAX_VAL_SIZE];
    char        gidsbymem[MAX_VAL_SIZE];       /* list of gids a username belongs to */
    char        memsbygid[MAX_VAL_SIZE];       /* list of members a gid has */
} sql_query_t;

typedef struct {
    char        host[MAX_VAL_SIZE];            /* SQL Server to connect to */
    char        port[MAX_VAL_SIZE];            /* SQL port to connect to */
    char        socket[MAX_VAL_SIZE];          /* SQL socket path to use */
    char        username[MAX_VAL_SIZE];        /* Username to connect as */
    char        password[MAX_VAL_SIZE];        /* Password to connect with */
    char        database[MAX_VAL_SIZE];        /* SQL Database to open */
} sql_server_t;

typedef struct {
    sql_query_t     query;
    sql_server_t    server;
} sql_conf_t;

typedef struct {
    bool            valid;              /* Have we loaded config yet? */
    sql_conf_t      sql;                /* [server] section */
} conf_t;

#define CONF_INITIALIZER {0}

/*
 * As soon as a MySQL link is established, save the results of
 * getsockname and getpeername here so we can make sure our
 * socket hasn't been mutilated by an outside program.
 */
typedef struct {
    struct sockaddr local;              /* getsockname */
    struct sockaddr remote;             /* getpeername */
} socket_info_t;

/* All information regarding existing MySQL link */
typedef struct {
    bool            valid;          /* Are we connected to a server? */
    MYSQL           link;
    socket_info_t   sock_info;      /* See above */
} con_info_t;

/* nss_main.c */
NSS_STATUS _nss_mysql_init (void);
void _nss_mysql_log (int priority, char *fmt, ...);
void _nss_mysql_reset_ent (MYSQL_RES **mresult);

/* nss_support.c */
NSS_STATUS _nss_mysql_load_passwd (void *result, char *buffer, size_t buflen,
                                   MYSQL_RES *mresult, int *errnop);
NSS_STATUS _nss_mysql_load_shadow (void *result, char *buffer, size_t buflen,
                                   MYSQL_RES *mresult, int *errnop);
NSS_STATUS _nss_mysql_load_group (void *result, char *buffer, size_t buflen,
                                  MYSQL_RES *mresult, int *errnop);
NSS_STATUS _nss_mysql_load_gidsbymem (void *result, char *buffer, size_t buflen,
                                      MYSQL_RES *mresult, int *errnop);

/* mysql.c */
NSS_STATUS _nss_mysql_close_sql (MYSQL_RES **mresult, bool graceful);
void _nss_mysql_close_result (MYSQL_RES **mresult);
NSS_STATUS _nss_mysql_run_query (char *query, MYSQL_RES **mresult,
                                 int *attempts);
NSS_STATUS _nss_mysql_fetch_row (MYSQL_ROW *row, MYSQL_RES *mresult);
NSS_STATUS _nss_mysql_escape_string (char *to, const char *from,
                                     MYSQL_RES **mresult);
#define _nss_mysql_num_rows(m) mysql_num_rows (m)
#define _nss_mysql_fetch_lengths(m) mysql_fetch_lengths (m)
#define _nss_mysql_num_fields(m) mysql_num_fields (m)

/* nss_config.c */
NSS_STATUS _nss_mysql_load_config (void);

/* lookup.c */
NSS_STATUS _nss_mysql_lookup (lookup_t ltype, const char *name,
                              unsigned int num, char *q, bool restricted,
                              void *result, char *buffer, size_t buflen,
                              int *errnop,
                              NSS_STATUS (*load_func)(void *, char *, size_t,
                                                      MYSQL_RES *, int *),
                              MYSQL_RES **mresult, const char *caller);
