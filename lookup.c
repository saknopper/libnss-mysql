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

NSS_STATUS
_nss_mysql_build_query (lookup_t ltype, const char *name, unsigned int num,
                        char **qin, char **qout, MYSQL_RES **mresult,
                        const char *caller)
{
  char *clean_name;
  size_t qout_size;

  *qout = NULL;

  if (!*qin || strlen (*qin) == 0)
    {
     _nss_mysql_log (LOG_CRIT, "%s has no valid query in config", caller);
     return (NSS_UNAVAIL);
    }

  qout_size = strlen (*qin) + PADSIZE + 1;
  *qout = _nss_mysql_malloc (qout_size);
  if (*qout == NULL)
    return (NSS_UNAVAIL);

  switch (ltype)
    {
    case BYNAME:
      if (strlen (name) == 0)
        {
          _nss_mysql_free (*qout);
          return (NSS_NOTFOUND);
        }
      clean_name = _nss_mysql_malloc (strlen (name) * 2 + 1);
      if (clean_name == NULL)
        {
          _nss_mysql_free (*qout);
          return (NSS_UNAVAIL);
        }
      if (_nss_mysql_escape_string (clean_name, name, mresult) != NSS_SUCCESS)
        {
          _nss_mysql_free (*qout);
          _nss_mysql_free (clean_name);
          return (NSS_UNAVAIL);
        }
      snprintf (*qout, qout_size, *qin, clean_name);
      _nss_mysql_free (clean_name);
      _nss_mysql_reset_ent (mresult);
      break;
    case BYNUM:
      snprintf (*qout, qout_size, *qin, num);
      _nss_mysql_reset_ent (mresult);
      break;
    case BYNONE:
      if (!mresult || !*mresult)
        strcpy (*qout, *qin);
      break;
    default:
      _nss_mysql_free (*qout);
      return (NSS_UNAVAIL);
    }
  return (NSS_SUCCESS);
}

NSS_STATUS
_nss_mysql_lookup (lookup_t ltype, const char *name, unsigned int num,
                   char **q, nboolean restrict, void *result,
                   char *buffer, size_t buflen,
                   NSS_STATUS (*load_func)(void *, char *, size_t,
                                           MYSQL_RES *mresult),
                   MYSQL_RES **mresult, const char *caller)
{
  char *query;
  int retVal;
  nboolean run_query = ntrue;

  if (_nss_mysql_init () != NSS_SUCCESS)
    return (NSS_UNAVAIL);

  /* Create query string using config & args; QUERY is malloc'ed! */
  retVal = _nss_mysql_build_query (ltype, name, num, q, &query, mresult,
                                   caller);
  if (retVal != NSS_SUCCESS)
    return (retVal);

  /* BYNONE indicates *ent; don't run the query if we already did */
  if (ltype == BYNONE && *mresult)
    run_query = nfalse;

  /* Send just-build query string to MySQL server */
  if (run_query)
    retVal = _nss_mysql_run_query (query, mresult);
  else
    retVal = NSS_SUCCESS;

  _nss_mysql_free (query);
  if (retVal != NSS_SUCCESS)
    return (retVal);

  /* Take result of query and load RESULT & BUFFER */
  retVal = load_func (result, buffer, buflen, *mresult);

  /* BYNONE indicates *ent; don't kill the result here, endent does that */
  if (ltype != BYNONE)
    mysql_free_result (*mresult);

  return (retVal);
}

