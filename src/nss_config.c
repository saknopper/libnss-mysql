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

conf_t  conf = CONF_INITIALIZER;

typedef struct {
    char    *name;
    char    **ptr;
} config_info_t;

/*
 * Load KEY and VAL with the next key/val pair (if any) from the file pointed
 * to by FH.  If a key/val pair is found, return NSS_SUCCESS, otherwise
 * NSS_NOTFOUND.
 */
static NSS_STATUS
_nss_mysql_next_key (FILE *fh, char *key, int key_size, char *val,
                     int val_size)
{
  DN ("_nss_mysql_next_key")
  char line[MAX_LINE_LEN];
  char *ccil;    /* Current Character In Line */
  char *cur_key;
  char *cur_val;
  long previous_fpos;

  DENTER
  memset (key, 0, key_size);
  memset (val, 0, val_size);

  while (1)
    {
      previous_fpos = ftell (fh);
      if ((fgets (line, sizeof (line), fh)) == NULL)
        break;

      if (line[0] == '#')
        continue;    /* skip comments */

      ccil = line;
      cur_key = ccil;
      ccil += strcspn (ccil, "\n\r \t");
      if (ccil == cur_key)
        continue;    /* skip empty lines */
      *ccil = '\0';
      /* found a key; attempt to nab value */
      ++ccil;
      ccil += strspn (ccil, "\n\r \t");
      cur_val = ccil;
      ccil += strcspn (ccil, "\n\r\t");
      if (ccil == cur_val)
        continue;    /* no value ... */
      *ccil = '\0';
      strncpy (key, cur_key, key_size);
      strncpy (val, cur_val, val_size);
      D ("%s: Found: %s -> %s", FUNCNAME, key, val);
      DSRETURN (NSS_SUCCESS)
    }
  DSRETURN (NSS_NOTFOUND)
}

/*
 * Open FILE and load up the config data inside it
 */
static NSS_STATUS
_nss_mysql_load_config_file (char *file, nboolean eacces_is_fatal)
{
  DN ("_nss_mysql_load_config_file")
  FILE *fh;
  int section;
  int to_return;
  char key[MAX_KEY_LEN];
  char val[MAX_LINE_LEN];
  size_t size;
  config_info_t *c;

  config_info_t config_fields[] =
  {
      {"host",      &conf.sql.server.host},
      {"port",      &conf.sql.server.port},
      {"socket",    &conf.sql.server.socket},
      {"username",  &conf.sql.server.username},
      {"password",  &conf.sql.server.password},
      {"database",  &conf.sql.server.database},
      {"timeout",   &conf.sql.server.options.timeout},
      {"compress",  &conf.sql.server.options.compress},
      {"initcmd",   &conf.sql.server.options.initcmd},
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
      if (errno == EACCES && eacces_is_fatal == nfalse)
        DSRETURN (NSS_SUCCESS)
      else
        DSRETURN (NSS_UNAVAIL)
    }
  D ("%s: Loading: %s", FUNCNAME, file);

  while (_nss_mysql_next_key (fh, key, MAX_KEY_LEN, val, MAX_LINE_LEN)
          == NSS_SUCCESS)
    {
      for (c = config_fields; c->name; c++)
        {
          if (strcmp (key, c->name) == 0)
            {
              size = strlen (val) + 1;
              if ((*c->ptr = _nss_mysql_realloc (*c->ptr, size)) == NULL)
                DSRETURN (NSS_UNAVAIL)
              memcpy (*c->ptr, val, size);
            }
        }
    }

  fclose (fh);
  DSRETURN (NSS_SUCCESS)
}

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
 * Load main and, if appropriate, root configs.  Set some defaults.
 * Set CONF->VALID to NTRUE and return NSS_SUCCESS if all goes well.
 */
NSS_STATUS
_nss_mysql_load_config (void)
{
  DN ("_nss_mysql_load_config")
  int to_return;

  DENTER
  if (conf.valid == ntrue)
    DSRETURN (NSS_SUCCESS)

  memset (&conf, 0, sizeof (conf));
  to_return = _nss_mysql_load_config_file (MAINCFG, ntrue);
  if (to_return != NSS_SUCCESS)
    DSRETURN (to_return)

  to_return = _nss_mysql_load_config_file (ROOTCFG, nfalse);
  if (to_return != NSS_SUCCESS)
    DSRETURN (to_return)
  if (_nss_mysql_validate_config () == nfalse)
    DSRETURN (NSS_UNAVAIL)
  conf.valid = ntrue;
  DSRETURN (NSS_SUCCESS)
}

