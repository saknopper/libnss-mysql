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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <pthread.h>

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
#elif defined HAVE_NSS_COMMON_H
typedef nss_status_t NSS_STATUS;
#define NSS_ARGS(args)  ((nss_XbyY_args_t *)args)
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
#define MAX_SERVERS     2               /* Max # of configured SQL servers
                                           See nss_config.c before changing */
#define PADSIZE         64              /* malloc this much more for queries
                                           to allow for format expansion.
                                           Max username length ~ 1/2 this val */
#define OPENLOG_OPTIONS LOG_PID         /* flags to use when syslogging */
#define MAX_LOG_LEN     2000            /* Max length of syslog entry */

/* Use these as defaults until they're overriden via the config file */
#define DEF_FACIL       LOG_AUTH
#define DEF_PRIO        LOG_ERR
#define DEF_RETRY       30
#define DEF_DFLAGS      0

/* Debug flags; used in conf.global.debug_flags and the debug function */
#define D_MEMORY        0x0001          /* malloc, realloc, free, etc */
#define D_FUNCTION      0x0002          /* function enter/leave */
#define D_CONNECT       0x0004          /* MySQL connect/disconnect */
#define D_QUERY         0x0008          /* MySQL queries */
#define D_PARSE         0x0010          /* File & Query parsing */
#define D_FILE          0x0020          /* File opening & closing */
#define D_ALL           0x0040 - 1      /* All of the above */

/* Flags to pass to the close_sql function */
#define CLOSE_RESULT    0x0001      /* Nuke the result but leave link open */
#define CLOSE_LINK      0x0002      /* Nuke result AND link */
#define CLOSE_NOGRACE   0x0004      /* Ungracefully close everything */

#define PTRSIZE sizeof (void *)     /* My attempt at being portable */

#define FOFS(x,y) ((int)&(((x *)0)->y))     /* Get Field OfFSet */
typedef unsigned char _nss_mysql_byte;      /* for pointer arithmetic */

/* These should be used in almost every function for debugging purposes */
#define function_enter _nss_mysql_debug (FNAME, D_FUNCTION, "BEGIN");
#define function_leave _nss_mysql_debug (FNAME, D_FUNCTION, "LEAVE: -");
#define function_return(to_return)                                            \
  {                                                                           \
    if (to_return == NSS_TRYAGAIN)                                            \
      errno = ERANGE;                                                         \
    _nss_mysql_debug (FNAME, D_FUNCTION, "LEAVE: %d",  to_return);            \
    return to_return;                                                         \
  }

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

/*
 * Parse types for the 'lis' functions.  This is how I accomplish
 * loading data into a struct without referencing the structure's members
 */
typedef enum {
    FT_NONE,
    FT_INT,
    FT_LONG,
    FT_PCHAR,    /* char *  Pointer to char (unallocated) */
    FT_PPCHAR,   /* char ** Pointer to pointer to char (unallocated) */
    FT_UINT,
    FT_ULONG,
    FT_SYSLOG,   /* incoming string, convert to appropriate integer */
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
    nboolean    valid;
    time_t      last_attempt;
    nboolean    up;
} server_status_t;

typedef struct {
    char        *host;      /* SQL Server to connect to */
    unsigned int port;      /* SQL port to connect to */
    char        *socket;    /* SQL socket path to use */
    char        *username;  /* Username to connect as */
    char        *password;  /* Password to connect with */
    char        *database;  /* SQL Database to open */
    server_status_t status;     /* */
} sql_server_t;

typedef struct {
    sql_query_t     query;
    sql_server_t    server[MAX_SERVERS];
} sql_conf_t;

typedef struct {
    int             retry;              /* retry server #0 every x seconds */
    int             syslog_facility;    /* facility to log with */
    int             syslog_priority;    /* Log at this prio and higher */
    int             debug_flags;        /* if prio = debug, log only these */
} global_conf_t;

typedef struct {
    nboolean        valid;              /* Have we loaded config yet? */
    global_conf_t   global;             /* settings that apply everywhere */
    sql_conf_t      sql;                /* [server] sections */
} conf_t;

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
    MYSQL_RES       *result;
    MYSQL           link;
    socket_info_t   sock_info;      /* See above */
} con_info_t;

/* nss_main.c */
void _nss_mysql_debug(const char *function, int flags, char *fmt, ...);
void _nss_mysql_log (int priority, char *fmt, ...);
#ifdef HAVE_NSS_COMMON_H
NSS_STATUS _nss_mysql_default_destr (nss_backend_t *be, void *args);
#endif

/* nss_support.c */
NSS_STATUS _nss_mysql_init (void);
NSS_STATUS _nss_mysql_liswb (const char *val, void *structure, char *buffer,
                             size_t buflen, int *bufused, int fofs,
                             ftype_t type);

/* mysql.c */
NSS_STATUS _nss_mysql_connect_sql (void);
NSS_STATUS _nss_mysql_close_sql (int flags);
NSS_STATUS _nss_mysql_run_query(char *query);
NSS_STATUS _nss_mysql_load_result(void *result, char *buffer, size_t buflen,
                                  field_info_t *fields);
void _nss_mysql_reset_ent (void);
nboolean _nss_mysql_active_result (void);
NSS_STATUS _nss_mysql_escape_string (char *to, const char *from);

/* memory.c */
void _nss_mysql_free(void *ptr);
void *_nss_mysql_malloc(size_t size);
void *_nss_mysql_realloc (void *ptr, size_t size);

/* nss_config.c */
NSS_STATUS _nss_mysql_load_config (void);
void _nss_mysql_reset_config (void);

/* nss_structures.c */
extern field_info_t passwd_fields[];
extern field_info_t spwd_fields[];
extern field_info_t group_fields[];

/* lookup.c */
NSS_STATUS _nss_mysql_lookup_name (const char *name, int qofs,
                                   const char *caller);
NSS_STATUS _nss_mysql_lookup_number (unsigned int num, int qofs,
                                     const char *caller);
NSS_STATUS _nss_mysql_lookup_ent (int qofs, const char *caller);

