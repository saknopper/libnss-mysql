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
#include <errno.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

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

/* Use these as defaults until they're overriden via the config file */
#define DEF_FACIL       LOG_AUTH
#define DEF_PRIO        LOG_ALERT
#define DEF_RETRY       30
#define DEF_DFLAGS      D_ALL

/* Debug flags; used in conf.global.debug_flags and the debug function */
#define D_MEMORY        0x0001
#define D_FUNCTION      0x0002
#define D_CONNECT       0x0004
#define D_QUERY         0x0008
#define D_PARSE         0x0010
#define D_FILE          0x0020
#define D_ALL           0x0040 - 1

/* Flags to pass to the close_sql function */
#define CLOSE_RESULT    0x0001      /* Nuke the result but leave link open */
#define CLOSE_LINK      0x0002      /* Nuke result AND link */

#define PTRSIZE sizeof (void *)     /* My attempt at being portable */

#define FOFS(x,y) ((int)&(((x *)0)->y)) /* Get Field OfFSet */
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

typedef unsigned char _nss_mysql_byte;        /* for pointer arithmetic */

/* To the untrained eye, this looks like my version of a boolean.  It's
   really my secret code for taking over the universe ... */
typedef enum {
    nfalse,
    ntrue
} nboolean;

/* It's SO damn confusing when functions use a return of 0 for success and
   1 for failure, especially amidst functions that have a boolean return
   type.. so use this instead.  PLEASE.  I BEG YOU. */
typedef enum {
    RETURN_SUCCESS,
    RETURN_FAILURE
} freturn_t;

/* Parse types for the 'lis' functions.  This is how I accomplish
   loading data into a struct without referencing the structure's members */
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

/* Mostly used to use a string to describe where in a structure something
   goes.  I overload it's purpose in a couple places though */
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
    int             retry;              /* retry server #0 every x seconds */
    int             syslog_facility;    /* facility to log with */
    int             syslog_priority;    /* Log at this prio and higher */
    int             debug_flags;        /* if prio = debug, log only these */
} global_conf_t;

typedef struct {
    nboolean        valid;              /* Have we loaded config yet? */
    int             num_servers;        /* 1 .. MAX_SERVERS */
    global_conf_t   global;             /* settings that apply everywhere */
    sql_conf_t      defaults;           /* [defaults] config section */
    sql_conf_t      sql[MAX_SERVERS];   /* [server] sections */
} conf_t;

/* As soon as a MySQL link is established, save the results of
   getsockname and getpeername here so we can make sure our
   socket hasn't been mutilated by an outside program. */
typedef struct {
    struct sockaddr local;              /* getsockname */
    struct sockaddr remote;             /* getpeername */
} socket_info_t;

/* All information regarding existing MySQL link */
typedef struct {
    nboolean        valid;          /* Are we connected to a server? */
    time_t          last_attempt;   /* Last time we tried server #0 */
    int             server_num;     /* 0 .. MAX_SERVERS - 1 */
    int             idx;            /* 0 .. # - For *ent routines */
    MYSQL_RES       *result;
    MYSQL           link;
    socket_info_t   sock_info;      /* See above */
} con_info_t;

/* nss_mysql.c */
void _nss_mysql_debug(char *function, int flags, char *fmt, ...);
void _nss_mysql_log (int priority, char *fmt, ...);

/* nss_support.c */
NSS_STATUS _nss_mysql_close_sql (int flags);
NSS_STATUS _nss_mysql_init(conf_t *conf);
NSS_STATUS _nss_mysql_run_query(conf_t conf, char **query_list);
NSS_STATUS _nss_mysql_load_result(void *result, char *buffer, size_t buflen,
                                  field_info_t *fields);
void _nss_mysql_reset_ent (void);
nboolean _nss_mysql_active_result (void);
void _nss_mysql_inc_ent (void);

/* memory.c */
void xfree(void *ptr);
void *xmalloc(size_t size);
void *xrealloc (void *ptr, size_t size);

/* nss_config.c */
NSS_STATUS _nss_mysql_load_config (conf_t *conf);

/* nss_structures.c */
extern field_info_t passwd_fields[];
extern field_info_t spwd_fields[];
extern field_info_t group_fields[];

