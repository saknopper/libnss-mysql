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

static const char rcsid[] =
    "$Id$ ";

#include "nss_mysql.h"
#include <stdio.h>      /* snprintf () */
#include <string.h>     /* strcpy () */

extern conf_t conf;

/*
 * Take query from config, such as "SELECT x FROM foo WHERE bar = '%s'"
 * and replace "%s" with appropriate data
 * ltype = BYNAME, BYNUM, or BYNONE
 * name = username if this is a BYNAME
 * num = uid/gid if this is a BYNUM
 * qin = query in config to use
 * qout = string to send to MySQL server (after format string processing)
 * mresult = Current MySQL result (if any)
 * caller = calling function's name
 */
static NSS_STATUS
_nss_mysql_build_query (lookup_t ltype, const char *name, unsigned int num,
                        char *qin, char **qout, MYSQL_RES **mresult,
                        const char *caller)
{
  DN ("_nss_mysql_build_query")
  char *clean_name;                     /* escaped version of name */
  size_t qout_size, clean_name_size;
  size_t name_len, qin_len;

  DENTER
  *qout = NULL;

  /* Verify existence of input query, lest we crash */
  if (!qin || (qin_len = strlen (qin)) == 0)
    {
     _nss_mysql_log (LOG_CRIT, "%s has no valid query in config", caller);
     DSRETURN (NSS_UNAVAIL)
    }

  switch (ltype)
    {
    case BYNAME: /* This lookup key is a name/string */
      D ("%s: BYNAME, name = '%s'", FUNCNAME, name);
      name_len = strlen (name);
      if (name_len == 0)
        DSRETURN (NSS_NOTFOUND)
      /* MySQL docs state escapping a string requires (original * 2) + 1 */
      clean_name_size = name_len * 2 + 1;
      clean_name = _nss_mysql_malloc (clean_name_size);
      if (clean_name == NULL)
        DSRETURN (NSS_UNAVAIL)
      if (_nss_mysql_escape_string (clean_name, name, mresult) != NSS_SUCCESS)
        {
          _nss_mysql_free (clean_name);
          DSRETURN (NSS_UNAVAIL)
        }
      /* final string size = query string + replaced text in format string */
      qout_size = qin_len + clean_name_size;
      *qout = _nss_mysql_malloc (qout_size);
      if (*qout == NULL)
        {
          _nss_mysql_free (clean_name);
          DSRETURN (NSS_UNAVAIL)
        }
      /* I won't bother checking return value as qout size is correct */
      snprintf (*qout, qout_size, qin, clean_name);
      _nss_mysql_free (clean_name);
      /* New lookup, reset any existing queries */
      _nss_mysql_reset_ent (mresult);
      break;
    case BYNUM: /* This lookup key is a uid/gid/number */
      D ("%s: BYNUM, num = '%u'", FUNCNAME, num);
      qout_size = qin_len + 10 + 1; /* 10 = unsigned 32 bit integer (%u) */
      *qout = _nss_mysql_malloc (qout_size);
      if (*qout == NULL)
        DSRETURN (NSS_UNAVAIL)
      /* I won't bother checking return value as qout size is correct */
      snprintf (*qout, qout_size, qin, num);
      /* New lookup, reset any existing queries */
      _nss_mysql_reset_ent (mresult);
      break;
    case BYNONE: /* This query has no key (e.g. a getpwent) */
      D ("%s: BYNONE creating initial query", FUNCNAME);
      *qout = _nss_mysql_malloc (qin_len + 1);
      if (*qout == NULL)
        DSRETURN (NSS_UNAVAIL)
      strcpy (*qout, qin);
      break;
    default:
      _nss_mysql_log (LOG_ERR, "%s: default case reached - should never happen",
                      FUNCNAME);
      _nss_mysql_free (*qout);
      DSRETURN (NSS_UNAVAIL)
    }
  DSRETURN (NSS_SUCCESS)
}

/*
 * The high-level function that does the actual lookup
 * Gets the query built and sent to the MySQL server, as well as
 * loads the result & buffer with the appropriate return data
 *
 * ltype = BYNAME, BYNUM, or BYNONE
 * name = name if BYNAME
 * num = uid/gid if BYNUM
 * q = unformatted query from config to use / build from
 * restricted = restrict this query to euid = 0?
 * result = NSS API result
 * buffer = NSS API buffer
 * buflen = NSS API length of buffer
 * errnop = NSS API errno pointer
 * load_func = pointer to function to load result/buffer with
 * mresult = current MySQL result set
 * caller = name of calling function
 */
NSS_STATUS
_nss_mysql_lookup (lookup_t ltype, const char *name, unsigned int num,
                   char **q, nboolean restricted, void *result,
                   char *buffer, size_t buflen, int *errnop,
                   NSS_STATUS (*load_func)(void *, char *, size_t,
                                           MYSQL_RES *, int *),
                   MYSQL_RES **mresult, const char *caller)
{
  DN ("_nss_mysql_lookup")
  char *query;                          /* Query to send to MySQL */
  int retVal;
  int attempts = MAX_QUERY_ATTEMPTS;    /* Attempt # (countdown) */
  static uid_t euid = -1;               /* Last known euid for change detect */
  uid_t cur_euid;                       /* CURRENT euid */

  DENTER
  cur_euid = geteuid ();
  D ("%s: restricted = %d, cur_euid = %u", FUNCNAME, restricted, cur_euid);
  if (restricted == ntrue && cur_euid != 0)
    DSRETURN (NSS_NOTFOUND)

   /* Make sure euid hasn't changed, thus changing our access abilities */
  if (euid == -1)
    euid = cur_euid;
  else if (euid != cur_euid)
    {
      D ("%s:, euid changed: %u -> %u", FUNCNAME, euid, cur_euid);
      /* Close MySQL and config to force reload of config and reconnect */
      _nss_mysql_close_sql (mresult, ntrue);
      conf.valid = nfalse;
      euid = cur_euid;
    }

  /* Required in case link & config were reset due to euid change */
  if (_nss_mysql_init () != NSS_SUCCESS)
    DSRETURN (NSS_UNAVAIL)

  /* BYNONE indicates *ent; don't create/run the query since we already did */
  if (!(ltype == BYNONE && mresult && *mresult))
    {
      /* Create query string using config & args; QUERY is malloc'ed! */
      retVal = _nss_mysql_build_query (ltype, name, num, *q, &query, mresult,
                                       caller);
      if (retVal != NSS_SUCCESS)
        DSRETURN (retVal)
      retVal = _nss_mysql_run_query (query, mresult, &attempts);
      _nss_mysql_free (query);
      if (retVal != NSS_SUCCESS)
        DSRETURN (retVal)
    }

  /* Take result of query and load RESULT & BUFFER */
  retVal = load_func (result, buffer, buflen, *mresult, errnop);

  /* BYNONE indicates *ent; don't kill the result here, endent does that */
  if (ltype != BYNONE)
    _nss_mysql_close_result (mresult);

  DSRETURN (retVal)
}

