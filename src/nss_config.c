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
#include <stdio.h>      /* fopen() */
#include <string.h>     /* strlen() */

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
 * Line continuation is supported if EOL is strictly a '\'
 *      (no trailing whitespace)
 *
 * The in-memory copy of LINE is modified by this routine
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
  char line[MAX_LINE_SIZE];
  char *ccil;                     /* Current Character In Line */
  char *cur;
  size_t size;
  nboolean fetch_key = ntrue;

  DENTER

  *val = *key = '\0';
  /* Loop through file until a key/val pair is found or EOF */
  while (fgets (line, MAX_LINE_SIZE, fh) != NULL)
    {
      if (*line == '#')
        continue;

      ccil = line;
      if (fetch_key)
        {
          cur = ccil;
          /* Get key - anything but CR/LF or whitespace */
          ccil += strcspn (ccil, "\n\r \t");
          if (ccil == cur)
            continue;             /* No key */
          *ccil = '\0';           /* Null-terminate the key */
          /* Key found, move on */
          ++ccil;
          /* Save the key */
          strncpy (key, cur, key_size);
          key[key_size - 1] = '\0'; /* strncpy doesn't guarantee null-term */
        }

      /* Skip leading whitespace */
      ccil += strspn (ccil, " \t");
      cur = ccil;
      /* Get value - anything but CR/LF */
      size = strcspn (ccil, "\n\r");
      if (!size && fetch_key)
        continue;                 /* No value */
      ccil += size;
      if (*(ccil - 1) == '\\')
        {
          fetch_key = nfalse;     /* Next line continues a value */
          *(ccil - 1) = '\0';     /* Remove the '\' */
          size--;
        }
      else
        {
          fetch_key = ntrue;      /* Next line starts with a key */
          *ccil = '\0';           /* Null-terminate the value */
        }
      /* Append what we just snarfed to VAL */
      strncat (val, cur, val_size - 1);
      val_size -= size;
      if (val_size <= 0)
        {
          _nss_mysql_log (LOG_ERR, "%s: Config value too long", FUNCNAME);
          DSRETURN (NSS_NOTFOUND)
        }

      if (!fetch_key)             /* Next line continues a value */
        continue;

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
_nss_mysql_load_config_file (char *file)
{
  DN ("_nss_mysql_load_config_file")
  FILE *fh;
  char key[MAX_KEY_SIZE];
  char val[MAX_VAL_SIZE];
  size_t size;
  config_info_t *c;

  /* map config key to 'conf' location; must be NULL-terminated */
  config_info_t config_fields[] =
  {
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

      {NULL}
  };

  DENTER
  if ((fh = fopen (file, "r")) == NULL)
    DRETURN;

  D ("%s: Loading: %s", FUNCNAME, file);

  /* Step through all key/val pairs available */
  while (_nss_mysql_next_key (fh, key, MAX_KEY_SIZE, val, MAX_VAL_SIZE)
          == NSS_SUCCESS)
    {
      /* Search for matching key */
      for (c = config_fields; c->name; c++)
        {
          if (strcmp (key, c->name) == 0)
            {
              size = strlen (val) + 1;
              if ((*c->ptr = _nss_mysql_realloc (*c->ptr, size)) == NULL)
                {
                  fclose (fh);
                  DRETURN
                }
              /* load value into proper place in 'conf' */
              memcpy (*c->ptr, val, size);
            }
        }
    }

  fclose (fh);
  DRETURN
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
  if (!conf.sql.server.host || !(*conf.sql.server.host))
    DBRETURN (nfalse);
  if (!conf.sql.server.database || !(*conf.sql.server.database))
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

  DENTER
  /* Config is already loaded, don't do it again */
  if (conf.valid == ntrue)
    DSRETURN (NSS_SUCCESS)

  memset (&conf, 0, sizeof (conf));
  /* Load main (world-readable) config */
  _nss_mysql_load_config_file (MAINCFG);

  /* Load root (root-readable) config */
  _nss_mysql_load_config_file (ROOTCFG);

  /* double-check our config */
  if (_nss_mysql_validate_config () == nfalse)
    DSRETURN (NSS_UNAVAIL)

  /* Let the rest of the module know we've got a good config */
  conf.valid = ntrue;

  DSRETURN (NSS_SUCCESS)
}

