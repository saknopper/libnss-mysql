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
    "$Id$";

#include "nss_mysql.h"
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <stdlib.h>

/* global var 'conf' contains data loaded from config files */
conf_t  conf = CONF_INITIALIZER;

/* maps config key to spot in 'conf' */
typedef struct {
    char    *name;  /* key string */
    char    **ptr;  /* where in 'conf' to load this string */
} config_info_t;

/*
 * Get the next key/value pair from an open file
 * return NSS_SUCCESS if a key/val pair is found
 * return NSS_NOTFOUND if EOF
 * Lines can begin (column 0) with a '#' for comments
 * key/val pairs must be in the following format
 *      key = val
 * Whitespace can be spaces or tabs
 * Keys must be one word (no whitespace)
 * Values can be anything except CR/LF
 * Line continuation is NOT supported
 *
 * fh = open file handle
 * key = config key loaded here
 * key_size = storage size of key
 * val = config value loaded here
 * val_size = storage size of val
 */
static NSS_STATUS
_nss_mysql_next_key (FILE *fh, char *key, int key_size, char *val,
                     int val_size)
{
  DN ("_nss_mysql_next_key")
  char line[MAX_LINE_LEN];
  char *ccil;        /* Current Character In Line */
  char *cur_key;
  char *cur_val;

  DENTER
  memset (key, 0, key_size);
  memset (val, 0, val_size);

  /* Loop through file until a key/val pair is found or EOF */
  while (1)
    {
      if ((fgets (line, sizeof (line), fh)) == NULL)
        break;      /* EOF - we're done here */

      if (line[0] == '#')
        continue;   /* skip comments */

      ccil = line;
      cur_key = ccil;
      /* anything resembling a key here? */
      ccil += strcspn (ccil, "\n\r \t");
      if (ccil == cur_key)
        continue;   /* Nope, nothing resembling a key here */
      *ccil = '\0'; /* Remove trailing whitespace from key */
      /* found a key; attempt to nab value */
      ++ccil;
      /* Skip past key now that we've loaded it */
      ccil += strspn (ccil, "\n\r \t");
      cur_val = ccil;
      /* anything resembling a value here? */
      ccil += strcspn (ccil, "\n\r");
      if (ccil == cur_val)
        continue;    /* Nope, nothing resembling a value here */
      *ccil = '\0';  /* Remove trailing whitespace from val */

      /* Now that we have key/val, copy it into provided function args */
      strncpy (key, cur_key, key_size);
      strncpy (val, cur_val, val_size);
      D ("%s: Found: %s -> %s", FUNCNAME, key, val);
      DSRETURN (NSS_SUCCESS)
    }
  DSRETURN (NSS_NOTFOUND)
}

/*
 * Load configuration data from file
 *
 * file = full path of file to load
 * eaccess_is_fatal = should "access denied" be a FATAL error?
 */
static NSS_STATUS
_nss_mysql_load_config_file (char *file, nboolean eacces_is_fatal)
{
  DN ("_nss_mysql_load_config_file")
  FILE *fh;
  char key[MAX_KEY_LEN];
  char val[MAX_LINE_LEN];
  size_t size;
  config_info_t *c;

  /* map config key to 'conf' location; must be NULL-terminated */
  config_info_t config_fields[] =
  {
      /* MySQL server configuration */
      {"host",      &conf.sql.server.host},
      {"port",      &conf.sql.server.port},
      {"socket",    &conf.sql.server.socket},
      {"username",  &conf.sql.server.username},
      {"password",  &conf.sql.server.password},
      {"database",  &conf.sql.server.database},
      {"timeout",   &conf.sql.server.options.timeout},
      {"compress",  &conf.sql.server.options.compress},
      {"initcmd",   &conf.sql.server.options.initcmd},

      /* MySQL queries to execute */
      {"getpwnam",  &conf.sql.query.getpwnam},
      {"getpwuid",  &conf.sql.query.getpwuid},
      {"getspnam",  &conf.sql.query.getspnam},
      {"getpwent",  &conf.sql.query.getpwent},
      {"getspent",  &conf.sql.query.getspent},
      {"getgrnam",  &conf.sql.query.getgrnam},
      {"getgrgid",  &conf.sql.query.getgrgid},
      {"getgrent",  &conf.sql.query.getgrent},
      {"memsbygid", &conf.sql.query.memsbygid},
      {"gidsbymem", &conf.sql.query.gidsbymem},

      {NULL}
  };

  DENTER
  if ((fh = fopen (file, "r")) == NULL)
    {
      /* don't return fatal error on EACCES unless we're supposed to */
      if (errno == EACCES && eacces_is_fatal == nfalse)
        DSRETURN (NSS_SUCCESS)
      else
        DSRETURN (NSS_UNAVAIL)
    }
  D ("%s: Loading: %s", FUNCNAME, file);

  /* Step through all key/val pairs available */
  while (_nss_mysql_next_key (fh, key, MAX_KEY_LEN, val, MAX_LINE_LEN)
          == NSS_SUCCESS)
    {
      /* Search for matching key */
      for (c = config_fields; c->name; c++)
        {
          if (strcmp (key, c->name) == 0)
            {
              size = strlen (val) + 1;
              if ((*c->ptr = _nss_mysql_realloc (*c->ptr, size)) == NULL)
                DSRETURN (NSS_UNAVAIL)
              /* load value into proper place in 'conf' */
              memcpy (*c->ptr, val, size);
            }
        }
    }

  fclose (fh);
  DSRETURN (NSS_SUCCESS)
}

/*
 * Sanity-check the loaded configuration data
 * Make sure we have at least a host and database defined
 */
static nboolean
_nss_mysql_validate_config (void)
{
  DN ("_nss_mysql_validate_config")

  DENTER
  if (!conf.sql.server.host || !strlen (conf.sql.server.host))
    DBRETURN (nfalse);
  if (!conf.sql.server.database || !strlen (conf.sql.server.database))
    DBRETURN (nfalse);

  DBRETURN (ntrue)
}

/*
 * Load our config files
 * Upon success, set conf.valid = ntrue
 */
NSS_STATUS
_nss_mysql_load_config (void)
{
  DN ("_nss_mysql_load_config")
  int to_return;

  DENTER
  /* Config is already loaded, don't do it again */
  if (conf.valid == ntrue)
    DSRETURN (NSS_SUCCESS)

  memset (&conf, 0, sizeof (conf));
  /* Load main (world-readable) config; error out if errno = EACCES */
  to_return = _nss_mysql_load_config_file (MAINCFG, ntrue);
  if (to_return != NSS_SUCCESS)
    DSRETURN (to_return)

  /* Load root (root-readable) config; don't error out if errno = EACCES */
  to_return = _nss_mysql_load_config_file (ROOTCFG, nfalse);
  if (to_return != NSS_SUCCESS)
    DSRETURN (to_return)

  /* double-check our config */
  if (_nss_mysql_validate_config () == nfalse)
    DSRETURN (NSS_UNAVAIL)

  /* Let the rest of the module know we've got a good config */
  conf.valid = ntrue;

  DSRETURN (NSS_SUCCESS)
}

