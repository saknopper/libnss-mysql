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
#include <nss_dbdefs.h>
#else
#error I need either nss.h or nss_common.h!
#endif

#include <mysql.h>

#include <sys/socket.h>
#include <errno.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <pthread.h>

#ifdef HAVE_NSS_H
#define NSS_SUCCESS     NSS_STATUS_SUCCESS
#define NSS_NOTFOUND    NSS_STATUS_NOTFOUND
#define NSS_UNAVAIL     NSS_STATUS_UNAVAIL
#define NSS_TRYAGAIN    NSS_STATUS_TRYAGAIN
typedef enum nss_status NSS_STATUS;
#elif defined HAVE_NSS_COMMON_H
typedef nss_status_t NSS_STATUS;
#define NSS_ARGS(args)  ((nss_XbyY_args_t *)args)
#endif

#define MAX_LINE_LEN    1024            /* Max line length in config file */
#define MAX_KEY_LEN     128             /* Max length of a key in config file */
#define MAX_SERVERS     2               /* Max # of configured SQL servers
                                           See nss_config.c before changing */
#define PADSIZE         64              /* malloc this much more for queries
                                           to allow for format expansion.
                                           Max username length ~ 1/2 this val */
#define OPENLOG_OPTIONS LOG_PID         /* Flags to use when syslogging */
#define MAX_LOG_LEN     2000            /* Max length of syslog entry */

/* Use these as defaults until they're overridden via the config file */
#define DEF_RETRY       30
#define DEF_TIMEOUT     3

#ifdef DEBUG
void _nss_mysql_debug (char *fmt, ...);
#define DEBUG_FILE "/tmp/libnss_mysql-debug.log"
#define D _nss_mysql_debug
#define DN(n) static const char FUNCNAME[] = n;
#define DENTER D ("%s: ENTER", FUNCNAME);
#define DIRETURN(r) { D ("%s: EXIT (%d)", FUNCNAME, r); return (r); }
#define DFRETURN(r)                                                         \
  {                                                                         \
    D ("%s: EXIT (%s)", FUNCNAME, r == 0 ? "SUCCESS" : "FAIL");             \
    return (r);                                                             \
  }
#define DBRETURN(r)                                                         \
  {                                                                         \
    D ("%s: EXIT (%s)", FUNCNAME, r == ntrue ? "TRUE" : "FALSE");           \
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
    D ("%s: EXIT (%s)", FUNCNAME, status);                                  \
    return (r);                                                             \
  }
#define DPRETURN(r) { D ("%s: EXIT (%p)", FUNCNAME, r); return (r); }
#define DEXIT D ("%s: EXIT", FUNCNAME);
#else
#define D
#define DN(n)
#define DENTER
#define DIRETURN(r) return (r);
#define DPRETURN(r) return (r);
#define DFRETURN(r) return (r);
#define DBRETURN(r) return (r);
#define DSRETURN(r) return (r);
#define DEXIT
#define FUNCNAME ""
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
#ifdef HAVE_NSS_H
#define EXHAUSTED_BUFFER                                                     \
  {                                                                          \
    *errnop = ERANGE;                                                        \
    DSRETURN (NSS_TRYAGAIN);                                                 \
  }
#else
#define EXHAUSTED_BUFFER                                                     \
  {                                                                          \
    if (errnop)                                                              \
      *errnop = 1;                                                           \
    DSRETURN (NSS_NOTFOUND);                                                 \
  }
#endif

/*
 * To the untrained eye, this looks like my version of a boolean.  It's
 * really my secret code for taking over the universe ...
 */
typedef enum {
    nfalse,
    ntrue
} nboolean;

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
    char        *getpwuid;
    char        *getpwnam;
    char        *getspnam;
    char        *getpwent;
    char        *getspent;
    char        *getgrnam;
    char        *getgrgid;
    char        *getgrent;
    char        *gidsbymem;     /* list of gids a username belongs to */
    char        *memsbygid;     /* list of members a gid has */
} sql_query_t;

typedef struct {
    nboolean    valid;          /* valid config for this server? */
    time_t      last_attempt;   /* Last time we tried this server */
    nboolean    up;             /* self explanatory I hope */
} server_status_t;

typedef struct {
    unsigned int timeout;        /* Connect timeout in seconds */
    unsigned int compress;       /* Use compressed MySQL protocol? */
    char         *initcmd;       /* Send to server at time of connect */
    unsigned int ssl;            /* Connect with CLIENT_SSL flag? */
} server_options_t;

typedef struct {
    char             *host;      /* SQL Server to connect to */
    unsigned int     port;       /* SQL port to connect to */
    char             *socket;    /* SQL socket path to use */
    char             *username;  /* Username to connect as */
    char             *password;  /* Password to connect with */
    char             *database;  /* SQL Database to open */
    server_options_t options;
    server_status_t  status;
} sql_server_t;

typedef struct {
    sql_query_t     query;
    sql_server_t    server[MAX_SERVERS];
} sql_conf_t;

typedef struct {
    int             retry;              /* retry server #0 every x seconds */
} global_conf_t;

typedef struct {
    nboolean        valid;              /* Have we loaded config yet? */
    global_conf_t   global;             /* settings that apply everywhere */
    sql_conf_t      sql;                /* [server] sections */
} conf_t;

#define CONF_INITIALIZER {0, {DEF_RETRY}}

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
    nboolean        valid;          /* Are we connected to a server? */
    int             server_num;     /* 0 .. MAX_SERVERS - 1 */
    MYSQL           link;
    socket_info_t   sock_info;      /* See above */
} con_info_t;

/* nss_main.c */
NSS_STATUS _nss_mysql_init (void);
void _nss_mysql_log (int priority, char *fmt, ...);
#ifdef HAVE_NSS_COMMON_H
NSS_STATUS _nss_mysql_default_destr (nss_backend_t *be, void *args);
#endif
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
NSS_STATUS _nss_mysql_close_sql (MYSQL_RES **mresult, nboolean graceful);
void _nss_mysql_close_result (MYSQL_RES **mresult);
NSS_STATUS _nss_mysql_run_query(char *query, MYSQL_RES **mresult);
NSS_STATUS _nss_mysql_fetch_row (MYSQL_ROW *row, MYSQL_RES *mresult);
NSS_STATUS _nss_mysql_escape_string (char *to, const char *from,
                                     MYSQL_RES **mresult);
#define _nss_mysql_num_rows(m) mysql_num_rows (m)
#define _nss_mysql_fetch_lengths(m) mysql_fetch_lengths (m)
#define _nss_mysql_num_fields(m) mysql_num_fields (m)

/* memory.c */
void _nss_mysql_free(void *ptr);
void *_nss_mysql_malloc(size_t size);
void *_nss_mysql_realloc (void *ptr, size_t size);

/* nss_config.c */
NSS_STATUS _nss_mysql_load_config (void);

/* lookup.c */
NSS_STATUS _nss_mysql_lookup (lookup_t ltype, const char *name,
                              unsigned int num, char **q, nboolean restricted,
                              void *result, char *buffer, size_t buflen,
                              int *errnop,
                              NSS_STATUS (*load_func)(void *, char *, size_t,
                                                      MYSQL_RES *, int *),
                              MYSQL_RES **mresult, const char *caller);

