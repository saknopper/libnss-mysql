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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

extern conf_t conf;

NSS_STATUS
_nss_mysql_lookup_name (const char *name, int qofs, const char *caller)
{
  _nss_mysql_byte *q;
  char *clean_name;
  char *query;
  size_t qsize;
  int retVal;

  if (!name || strlen (name) == 0)
    function_return (NSS_UNAVAIL);

  if (_nss_mysql_init (&conf) != NSS_SUCCESS)
    function_return (NSS_UNAVAIL);

  (intptr_t *)q =  *(intptr_t *)((_nss_mysql_byte *)&conf.sql.query + qofs);
  if (!q || strlen ((char *)q) == 0)
    {
      _nss_mysql_log (LOG_CRIT, "%s has no valid query in config", caller);
      function_return (NSS_UNAVAIL);
    }

  qsize = strlen ((char *)q) + PADSIZE + 1;
  query = xmalloc (qsize);
  if (query == NULL)
    function_return (NSS_UNAVAIL);
  clean_name = xmalloc (strlen (name) * 2 + 1);
  if (clean_name == NULL)
    {
      xfree (query);
      function_return (NSS_UNAVAIL);
    }
  _nss_mysql_escape_string (clean_name, name);
  snprintf (query, qsize, (char *)q, clean_name);
  _nss_mysql_reset_ent ();
  retVal = _nss_mysql_run_query (conf, query);
  xfree (query);
  function_return (retVal);
}

NSS_STATUS
_nss_mysql_lookup_number (unsigned int num, int qofs, const char *caller)
{
  _nss_mysql_byte *q;
  char *query;
  size_t qsize;
  int retVal;

  if (_nss_mysql_init (&conf) != NSS_SUCCESS)
    function_return (NSS_UNAVAIL);

  (intptr_t *)q =  *(intptr_t *)((_nss_mysql_byte *)&conf.sql.query + qofs);
  if (!q || strlen ((char *)q) == 0)
    {
      _nss_mysql_log (LOG_CRIT, "%s has no valid query in config", caller);
      function_return (NSS_UNAVAIL);
    }

  qsize = strlen ((char *)q) + PADSIZE + 1;
  query = xmalloc (qsize);
  if (query == NULL)
    function_return (NSS_UNAVAIL);
  snprintf (query, qsize, (char *)q, num);
  _nss_mysql_reset_ent ();
  retVal = _nss_mysql_run_query (conf, query);
  xfree (query);
  function_return (retVal);
}

NSS_STATUS
_nss_mysql_lookup_ent (int qofs, const char *caller)
{
  _nss_mysql_byte *q;
  int retVal;

  if (_nss_mysql_init (&conf) != NSS_SUCCESS)
    function_return (NSS_UNAVAIL);

  (intptr_t *)q =  *(intptr_t *)((_nss_mysql_byte *)&conf.sql.query + qofs);
  if (!q || strlen ((char *)q) == 0)
    {
      _nss_mysql_log (LOG_CRIT, "%s has no valid query in config", caller);
      function_return (NSS_UNAVAIL);
    }

  retVal = _nss_mysql_run_query (conf, q);
  function_return (retVal);
}


