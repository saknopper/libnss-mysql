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
#include <stdio.h>
#include <string.h>

extern conf_t conf;

static NSS_STATUS
_nss_mysql_build_query (lookup_t ltype, const char *name, unsigned int num,
                        char *qin, char **qout, MYSQL_RES **mresult,
                        const char *caller)
{
  DN ("_nss_mysql_build_query")
  char *clean_name;
  size_t qout_size;

  DENTER
  *qout = NULL;

  if (!qin || strlen (qin) == 0)
    {
     _nss_mysql_log (LOG_CRIT, "%s has no valid query in config", caller);
     DSRETURN (NSS_UNAVAIL)
    }

  qout_size = strlen (qin) + PADSIZE + 1;
  *qout = _nss_mysql_malloc (qout_size);
  if (*qout == NULL)
    DSRETURN (NSS_UNAVAIL)

  switch (ltype)
    {
    case BYNAME:
      if (strlen (name) == 0)
        {
          _nss_mysql_free (*qout);
          DSRETURN (NSS_NOTFOUND)
        }
      clean_name = _nss_mysql_malloc (strlen (name) * 2 + 1);
      if (clean_name == NULL)
        {
          _nss_mysql_free (*qout);
          DSRETURN (NSS_UNAVAIL)
        }
      if (_nss_mysql_escape_string (clean_name, name, mresult) != NSS_SUCCESS)
        {
          _nss_mysql_free (*qout);
          _nss_mysql_free (clean_name);
          DSRETURN (NSS_UNAVAIL)
        }
      snprintf (*qout, qout_size, qin, clean_name);
      _nss_mysql_free (clean_name);
      _nss_mysql_reset_ent (mresult);
      break;
    case BYNUM:
      snprintf (*qout, qout_size, qin, num);
      _nss_mysql_reset_ent (mresult);
      break;
    case BYNONE:
      if (!mresult || !*mresult)
        strcpy (*qout, qin);
      break;
    default:
      _nss_mysql_free (*qout);
      DSRETURN (NSS_UNAVAIL)
    }
  DSRETURN (NSS_SUCCESS)
}

NSS_STATUS
_nss_mysql_lookup (lookup_t ltype, const char *name, unsigned int num,
                   char **q, nboolean restricted, void *result,
                   char *buffer, size_t buflen, int *errnop,
                   NSS_STATUS (*load_func)(void *, char *, size_t,
                                           MYSQL_RES *, int *),
                   MYSQL_RES **mresult, const char *caller)
{
  DN ("_nss_mysql_lookup")
  char *query;
  int retVal;

  DENTER
  if (restricted == ntrue && geteuid () != 0)
    DSRETURN (NSS_NOTFOUND)

  if (_nss_mysql_init () != NSS_SUCCESS)
    DSRETURN (NSS_UNAVAIL)

  /* BYNONE indicates *ent; don't create/run the query since we already did */
  if (!(ltype == BYNONE && *mresult))
    {
      /* Create query string using config & args; QUERY is malloc'ed! */
      retVal = _nss_mysql_build_query (ltype, name, num, *q, &query, mresult,
                                       caller);
      if (retVal != NSS_SUCCESS)
        DSRETURN (retVal)
      retVal = _nss_mysql_run_query (query, mresult);
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

