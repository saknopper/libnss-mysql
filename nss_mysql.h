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
#define OPENLOG_OPTIONS LOG_PID         /* flags to use when syslogging */
#define MAX_LOG_LEN     2000            /* Max length of syslog entry */

/* Use these as defaults until they're overriden via the config file */
#define DEF_RETRY       30

extern pthread_mutex_t lock;
#define LOCK pthread_mutex_lock (&lock);
#define UNLOCK pthread_mutex_unlock (&lock);

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

/*
 * Parse types for _nss_mysql_lis.  This is how I accomplish
 * loading data into a struct without referencing the structure's members
 */
typedef enum {
    FT_NONE,
    FT_PCHAR,    /* char *  Pointer to char (unallocated) */
    FT_UINT,
} ftype_t;

/*
 * Mostly used to use a string to describe where in a structure something
 * goes.  I overload it's purpose in a couple places though
 */
typedef struct {
    char    *name;
    int     ofs;
    int     type;
} field_info_t;

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
    nboolean    valid;      /* valid config for this server? */
    time_t      last_attempt; /* Last time we tried this server */
    nboolean    up;         /* self explanatory I hope */
} server_status_t;

typedef struct {
    char        *host;      /* SQL Server to connect to */
    unsigned int port;      /* SQL port to connect to */
    char        *socket;    /* SQL socket path to use */
    char        *username;  /* Username to connect as */
    char        *password;  /* Password to connect with */
    char        *database;  /* SQL Database to open */
    unsigned int ssl;       /* Connect with CLIENT_SSL flag? */
    server_status_t status;
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
void _nss_mysql_log (int priority, char *fmt, ...);
#ifdef HAVE_NSS_COMMON_H
NSS_STATUS _nss_mysql_default_destr (nss_backend_t *be, void *args);
#endif

/* nss_support.c */
NSS_STATUS _nss_mysql_init (void);
NSS_STATUS _nss_mysql_load_passwd (void *result, char *buffer, size_t buflen,
                                   MYSQL_RES *mresult);
NSS_STATUS _nss_mysql_load_shadow (void *result, char *buffer, size_t buflen,
                                   MYSQL_RES *mresult);
NSS_STATUS _nss_mysql_load_group (void *result, char *buffer, size_t buflen,
                                  MYSQL_RES *mresult);
NSS_STATUS _nss_mysql_load_gidsbymem (void *result, char *buffer, size_t buflen,
                                      MYSQL_RES *mresult);

/* mysql.c */
NSS_STATUS _nss_mysql_close_sql (MYSQL_RES **mresult, nboolean graceful);
NSS_STATUS _nss_mysql_run_query(char *query, MYSQL_RES **mresult);
NSS_STATUS _nss_mysql_fetch_row (MYSQL_ROW *row, MYSQL_RES *mresult);
my_ulonglong _nss_mysql_num_rows (MYSQL_RES *mresult);
unsigned long * _nss_mysql_fetch_lengths (MYSQL_RES *mresult);
unsigned int _nss_mysql_num_fields (MYSQL_RES *mresult);
void _nss_mysql_reset_ent (MYSQL_RES **mresult);
NSS_STATUS _nss_mysql_escape_string (char *to, const char *from,
                                     MYSQL_RES **mresult);

/* memory.c */
void _nss_mysql_free(void *ptr);
void *_nss_mysql_malloc(size_t size);
void *_nss_mysql_realloc (void *ptr, size_t size);

/* nss_config.c */
NSS_STATUS _nss_mysql_load_config (void);
void _nss_mysql_reset_config (void);

/* lookup.c */
NSS_STATUS _nss_mysql_lookup (lookup_t ltype, const char *name,
                              unsigned int num, char **q, nboolean restrict,
                              void *result, char *buffer, size_t buflen,
                              NSS_STATUS (*load_func)(void *, char *, size_t,
                                                      MYSQL_RES *mresult),
                              MYSQL_RES **mresult, const char *caller);

NSS_STATUS _nss_mysql_build_query (lookup_t ltype, const char *name,
                                   unsigned int num, char **qin, char **qout,
                                   MYSQL_RES **mresult, const char *caller);

