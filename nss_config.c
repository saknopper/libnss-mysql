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

typedef unsigned char nbyte;
#define FOFS(x,y) ((int)&(((x *)0)->y))     /* Get Field OFfSet */

/*
 * Map config keys to the struct offsets to load their values into
 */
static field_info_t server_fields[] =
{
    {"host",     FOFS (sql_server_t, host),     FT_PCHAR},
    {"port",     FOFS (sql_server_t, port),     FT_UINT},
    {"socket",   FOFS (sql_server_t, socket),   FT_PCHAR},
    {"username", FOFS (sql_server_t, username), FT_PCHAR},
    {"password", FOFS (sql_server_t, password), FT_PCHAR},
    {"database", FOFS (sql_server_t, database), FT_PCHAR},
    {"ssl",      FOFS (sql_server_t, ssl),      FT_UINT},
    {NULL}
};

static field_info_t query_fields[] =
{
    {"getpwnam",  FOFS (sql_query_t, getpwnam),  FT_PCHAR},
    {"getpwuid",  FOFS (sql_query_t, getpwuid),  FT_PCHAR},
    {"getspnam",  FOFS (sql_query_t, getspnam),  FT_PCHAR},
    {"getpwent",  FOFS (sql_query_t, getpwent),  FT_PCHAR},
    {"getspent",  FOFS (sql_query_t, getspent),  FT_PCHAR},
    {"getgrnam",  FOFS (sql_query_t, getgrnam),  FT_PCHAR},
    {"getgrgid",  FOFS (sql_query_t, getgrgid),  FT_PCHAR},
    {"getgrent",  FOFS (sql_query_t, getgrent),  FT_PCHAR},
    {"memsbygid", FOFS (sql_query_t, memsbygid), FT_PCHAR},
    {"gidsbymem", FOFS (sql_query_t, gidsbymem), FT_PCHAR},
    {NULL}
};

static field_info_t global_fields[]=
{
    {"retry",       FOFS (global_conf_t, retry),           FT_UINT},
    {NULL}
};

/*
 * The various sections (IE '[global]') of the config file
 */
typedef enum
{
  SECTION_INVALID = -1,
  SECTION_GLOBAL,

  /* 1 for each server; edit to match MAX_SERVERS; ALSO EDIT section_info[] */
  SECTION_PRIMARY,
  SECTION_SECONDARY,

  SECTION_QUERIES
} sections_t;

/* I'm overloading the 'purpose' of field_info_t here ... */
static field_info_t section_info[] =
{
    {"global",    0, SECTION_GLOBAL},

    /* 1 for each server; edit to match MAX_SERVERS; ALSO EDIT sections_t */
    {"primary",   0, SECTION_PRIMARY},
    {"secondary", 0, SECTION_SECONDARY},

    {"queries",   0, SECTION_QUERIES},
    {NULL}
};

/*
 * "translate" NAME to a number using FIELDS
 * IE translate "local7" to LOG_LOCAL7 using SYSLOG_NAMES above
 */
static int
_nss_mysql_name_to_id (field_info_t *fields, const char *name)
{
  field_info_t *f;

  for (f = fields; f->name; f++)
    {
      if (strcmp (name, f->name) == 0)
        return (f->type);
    }
  return (-1);
}

/*
 * Load Into Structure - Take KEY/VAL and, using FIELDS as a guide for
 * what goes where, load VAL into the appropriate spot that KEY points
 * to.  Basically a generic way to assign values of any type to the
 * right spot in a struct.
 */
static NSS_STATUS
_nss_mysql_lis (const char *key, const char *val, field_info_t *fields,
                void *structure)
{
  static const char FNAME[] = "_nss_mysql_lis";
  field_info_t *f;
  nbyte *b;
  int size;
  void *ptr;

  for (f = fields; f->name; f++)
    {
      if (strcmp (key, f->name) == 0)
        {
          b = (nbyte *) structure;
          switch (f->type)
            {
            case FT_PCHAR:
              size = strlen (val) + 1 + PADSIZE;
              /* Set 'ptr' to addr of string */
              (uintptr_t) ptr = *(uintptr_t *) (b + f->ofs);
              /* allocate/reallocate space for incoming string */
              if ((ptr = _nss_mysql_realloc (ptr, size)) == NULL)
                return (NSS_UNAVAIL);
              /* Set the pointer in structure to new pointer */
              *(uintptr_t *) (b + f->ofs) = (uintptr_t) ptr;
              /* Copy value into newly-alloc'ed area */
              memcpy (ptr, val, size);
              break;
            case FT_UINT:
              *(unsigned int *) (b + f->ofs) = (unsigned int) atoi (val);
              break;
            default:
              _nss_mysql_log (LOG_ERR, "%s: Unhandled type: %d", FNAME,
                              f->type);
              break;
            }
        }
    }
  return (NSS_SUCCESS);
}

/*
 * Return NTRUE if the current line starts with '[' and has a ']' in it
 */
static nboolean _nss_mysql_is_bracketed (char *line)
{
  if (line && *line == '[' && index (line, ']'))
    return ntrue;
  return nfalse;
}

/*
 * Load KEY and VAL with the next key/val pair (if any) from the file pointed
 * to by FH.  If a key/val pair is found, return NSS_SUCCESS, otherwise
 * NSS_NOTFOUND.
 */
static NSS_STATUS
_nss_mysql_next_key (FILE *fh, char *key, int key_size, char *val,
                     int val_size)
{
  char line[MAX_LINE_LEN];
  char *ccil;    /* Current Character In Line */
  char *cur_key;
  char *cur_val;
  long previous_fpos;

  memset (key, 0, key_size);
  memset (val, 0, val_size);

  while (1)
    {
      previous_fpos = ftell (fh);
      if ((fgets (line, sizeof (line), fh)) == NULL)
        break;
      if (_nss_mysql_is_bracketed (line))
        {
          fseek (fh, previous_fpos, SEEK_SET);
          break;
        }

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
      return (NSS_SUCCESS);
    }
  return (NSS_NOTFOUND);
}

/*
 * Step through FH until a valid section delimeter is found
 */
static NSS_STATUS
_nss_mysql_get_section (FILE *fh, int *section)
{
  char line[MAX_LINE_LEN];
  char *p;

  while (fgets (line, sizeof (line), fh) != NULL)
    {
      if (_nss_mysql_is_bracketed (line) == ntrue)
        {
          p = index (line, ']');
          *p = '\0';
          p = line;
          p++;
          *section = _nss_mysql_name_to_id (section_info, p);
          if (*section != SECTION_INVALID)
              return (NSS_SUCCESS);
        }
    }
  return (NSS_NOTFOUND);
}

/*
 * Step through FH, loading key/val pairs, until EOF or a new section is
 * encountered
 */
static NSS_STATUS
_nss_mysql_load_section (FILE *fh, void *structure, field_info_t *fields)
{
  char key[MAX_KEY_LEN];
  char val[MAX_LINE_LEN];
  int to_return;

  while (_nss_mysql_next_key (fh, key, sizeof (key), val, sizeof (val))
          == NSS_SUCCESS)
    {
      to_return = _nss_mysql_lis (key, val, fields, structure);
      if (to_return != NSS_SUCCESS)
        return (to_return);
    }
  return (NSS_SUCCESS);
}

/*
 * Open FILE and load up the config data inside it
 */
static NSS_STATUS
_nss_mysql_load_config_file (char *file, nboolean eacces_is_fatal)
{
  static const char FNAME[] = "_nss_mysql_load_config_file";
  FILE *fh;
  int section;
  int to_return;
  int server_num;

  if ((fh = fopen (file, "r")) == NULL)
    {
      if (errno == EACCES && eacces_is_fatal == nfalse)
        return (NSS_SUCCESS);
      else
        return (NSS_UNAVAIL);
    }
  while ((_nss_mysql_get_section (fh, &section)) == NSS_SUCCESS)
    {
      switch (section)
        {
        case SECTION_GLOBAL:
          to_return = _nss_mysql_load_section (fh, &conf.global,
                                               global_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              return (to_return);
            }
            break;
        /* Add to list if you edit MAX_SERVERS/sections_t/etc */
        case SECTION_PRIMARY:
        case SECTION_SECONDARY:
          server_num = section - SECTION_PRIMARY;
          if (server_num >= MAX_SERVERS)
            {
              fclose (fh);
              return (NSS_UNAVAIL);
            }
          to_return =
            _nss_mysql_load_section (fh, &(conf.sql.server[server_num]),
                                     server_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              return (to_return);
            }
          break;
        case SECTION_QUERIES:
          to_return = _nss_mysql_load_section (fh, &conf.sql.query,
                                               query_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              return (to_return);
            }
          break;
        default:
          _nss_mysql_log (LOG_ERR, "%s: Unhandled section type: %d", FNAME,
                          section);
          break;
        }
    }
  fclose (fh);
  return (NSS_SUCCESS);
}

static nboolean
_nss_mysql_validate_servers (void)
{
  int i;
  nboolean is_valid = nfalse;


  for (i = 0; i < MAX_SERVERS; i++)
    {
      if (!conf.sql.server[i].host || !strlen (conf.sql.server[i].host))
        continue;
      if (!conf.sql.server[i].database || !strlen (conf.sql.server[i].database))
        continue;
      if ((conf.sql.server[i].port == 0) &&
          (!conf.sql.server[i].socket || !strlen (conf.sql.server[i].socket)))
          continue;

      conf.sql.server[i].status.valid = ntrue;
      is_valid = ntrue;
    }

  return (is_valid);
}

/*
 * Load main and, if appropriate, root configs.  Set some defaults.
 * Set CONF->VALID to NTRUE and return NSS_SUCCESS if all goes well.
 */
NSS_STATUS
_nss_mysql_load_config (void)
{
  int to_return;

  if (conf.valid == ntrue)
    return (NSS_SUCCESS);

  memset (&conf, 0, sizeof (conf));
  conf.global.retry = DEF_RETRY;
  to_return = _nss_mysql_load_config_file (MAINCFG, ntrue);
  if (to_return != NSS_SUCCESS)
    return (to_return);

  to_return = _nss_mysql_load_config_file (ROOTCFG, nfalse);
  if (to_return != NSS_SUCCESS)
    return (to_return);
  if (_nss_mysql_validate_servers () == nfalse)
    return (NSS_UNAVAIL);
  conf.valid = ntrue;
  return (NSS_SUCCESS);
}

/*
 * Free all memory related to conf and reset it back to the defaults
 */
void
_nss_mysql_reset_config (void)
{
  int i;
  field_info_t *f;

  for (i = 0; i < MAX_SERVERS; i++)
  {
    _nss_mysql_free (conf.sql.server[i].host);
    _nss_mysql_free (conf.sql.server[i].socket);
    _nss_mysql_free (conf.sql.server[i].username);
    _nss_mysql_free (conf.sql.server[i].password);
    _nss_mysql_free (conf.sql.server[i].database);
  }

  for (f = query_fields; f->name; f++)
    {
      /* I use variables for clarity here.  This is ugly */
      char *q;
      nbyte *b;
      b = (nbyte *)&conf.sql.query + f->ofs;
      (uintptr_t *)q = *(uintptr_t *)b;
      _nss_mysql_free (q);
    }

  memset (&conf, 0, sizeof (conf));
  conf.global.retry = DEF_RETRY;
}

