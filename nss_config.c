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
#include <unistd.h>
#include <stdlib.h>

static field_info_t server_fields[] =
{
    {"host",     FOFS (sql_server_t, host),     FT_PCHAR},
    {"port",     FOFS (sql_server_t, port),     FT_UINT},
    {"socket",   FOFS (sql_server_t, socket),   FT_PCHAR},
    {"username", FOFS (sql_server_t, username), FT_PCHAR},
    {"password", FOFS (sql_server_t, password), FT_PCHAR},
    {"database", FOFS (sql_server_t, database), FT_PCHAR},
    {NULL}
};

static field_info_t query_fields[] =
{
    {"getpwnam", FOFS (sql_query_t, getpwnam),  FT_PCHAR},
    {"getpwuid", FOFS (sql_query_t, getpwuid),  FT_PCHAR},
    {"getspnam", FOFS (sql_query_t, getspnam),  FT_PCHAR},
    {"getpwent", FOFS (sql_query_t, getpwent),  FT_PCHAR},
    {"getspent", FOFS (sql_query_t, getspent),  FT_PCHAR},
    {"getgrnam", FOFS (sql_query_t, getgrnam),  FT_PCHAR},
    {"getgrgid", FOFS (sql_query_t, getgrgid),  FT_PCHAR},
    {"getgrent", FOFS (sql_query_t, getgrent),  FT_PCHAR},
    {NULL}
};

static field_info_t global_fields[]=
{
    {"retry",       FOFS (global_conf_t, retry),           FT_UINT},
    {"facility",    FOFS (global_conf_t, syslog_facility), FT_SYSLOG},
    {"priority",    FOFS (global_conf_t, syslog_priority), FT_SYSLOG},
    {"debug_flags", FOFS (global_conf_t, debug_flags),     FT_UINT},
    {NULL}
};

typedef enum
{
  SECTION_INVALID,
  SECTION_GLOBAL,
  SECTION_SERVER,
  SECTION_QUERIES
} sections_t;

/* I'm overloading the 'purpose' of field_info_t here ... */
static field_info_t section_info[] =
{
    {"global",   0, SECTION_GLOBAL},
    {"server",   0, SECTION_SERVER},
    {"queries",  0, SECTION_QUERIES},
    {NULL}
};

/* More overloading, whee! */
static field_info_t syslog_names[] =
{
    /* priorities */
    {"alert",    0, LOG_ALERT},
    {"crit",     0, LOG_CRIT},
    {"debug",    0, LOG_DEBUG},
    {"emerg",    0, LOG_EMERG},
    {"err",      0, LOG_ERR},
    {"info",     0, LOG_INFO},
    {"notice",   0, LOG_NOTICE},
    {"warn",     0, LOG_WARNING},
    /* facilities */
    {"auth",     0, LOG_AUTH},
#ifdef HAVE_LOG_AUTHPRIV
    {"authpriv", 0, LOG_AUTHPRIV},
#endif
    {"cron",     0, LOG_CRON},
    {"daemon",   0, LOG_DAEMON},
    {"ftp",      0, LOG_FTP},
    {"kern",     0, LOG_KERN},
    {"lpr",      0, LOG_LPR},
    {"mail",     0, LOG_MAIL},
    {"news",     0, LOG_NEWS},
    {"syslog",   0, LOG_SYSLOG},
    {"user",     0, LOG_USER},
    {"uucp",     0, LOG_UUCP},
    {"local0",   0, LOG_LOCAL0},
    {"local1",   0, LOG_LOCAL1},
    {"local2",   0, LOG_LOCAL2},
    {"local3",   0, LOG_LOCAL3},
    {"local4",   0, LOG_LOCAL4},
    {"local5",   0, LOG_LOCAL5},
    {"local6",   0, LOG_LOCAL6},
    {"local7",   0, LOG_LOCAL7},
    {NULL}
};

static int
_nss_mysql_syslog_name_to_id (const char *name)
{
  field_info_t *f;

  function_enter;
  for (f = syslog_names; f->name; f++)
    {
      if (strcmp (name, f->name) == 0)
        function_return (f->type);
    }
  function_return (-1);
}

/* 'lis' = Load Into Structure */
static NSS_STATUS
_nss_mysql_lis (const char *key, const char *val, field_info_t *fields,
                void *structure)
{
  field_info_t *f;
  _nss_mysql_byte *b;
  int size, tmp;
  void *ptr;

  function_enter;
  _nss_mysql_debug (FNAME, D_PARSE, "Load '%s' (%p)", key, structure);
  for (f = fields; f->name; f++)
    {
      if (strcmp (key, f->name) == 0)
        {
          b = (_nss_mysql_byte *) structure;
          switch (f->type)
            {
            case FT_PCHAR:
              size = strlen (val) + 1 + PADSIZE;
              /* Set 'ptr' to addr of string */
              (intptr_t) ptr = *(intptr_t *) (b + f->ofs);
              /* allocate/reallocate space for incoming string */
              if ((ptr = xrealloc (ptr, size)) == NULL)
                function_return (NSS_UNAVAIL);
              /* Set the pointer in structure to new pointer */
              *(intptr_t *) (b + f->ofs) = (intptr_t) ptr;
              /* Copy value into newly-alloc'ed area */
              memcpy (ptr, val, size);
              break;
            case FT_UINT:
              *(unsigned int *) (b + f->ofs) = (unsigned int) atoi (val);
              break;
            case FT_SYSLOG:
              tmp = _nss_mysql_syslog_name_to_id (val);
              if (tmp == -1)
                {
                  _nss_mysql_log (LOG_ERR, "Syslog value '%s' invalid", val);
                  break; 
                }
              *(unsigned int *) (b + f->ofs) = (unsigned int) tmp;
              break;
            default:
              _nss_mysql_log (LOG_ERR, "%s: Unhandled type: %d", FNAME,
                              f->type);
              break;
            }
        }
    }
  function_return (NSS_SUCCESS);
}

static nboolean _nss_mysql_is_bracketed (char *line)
{
  if (line && *line == '[' && index (line, ']'))
    return ntrue;
  return nfalse;
}

/*
 * Load key and val with the next key/val pair (if any) from the file pointed
 * to by fh.  If a key/val pair is found, return 0, otherwise 1.
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

  function_enter;
  memset (key, 0, key_size);
  memset (val, 0, val_size);

  while (1)
    {
      previous_fpos = ftell (fh);
      if ((fgets (line, sizeof (line), fh)) == NULL)
        {
          _nss_mysql_debug (FNAME, D_PARSE, "EOF");
          break;
        }
      if (_nss_mysql_is_bracketed (line))
        {
          _nss_mysql_debug (FNAME, D_PARSE, "End of section");
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
      _nss_mysql_debug (FNAME, D_PARSE, "Found key = %s", key);
      function_return (NSS_SUCCESS);
    }
  function_return (NSS_NOTFOUND);
}

static int
_nss_mysql_section_to_id (char *name)
{
  field_info_t *f;

  function_enter;
  for (f = section_info; f->name; f++)
    {
      if (strcmp (name, f->name) == 0)
        function_return (f->type);
    }
  function_return (SECTION_INVALID);
}

static NSS_STATUS
_nss_mysql_get_section (FILE *fh, int *section)
{
  char line[MAX_LINE_LEN];
  char *p;

  function_enter;
  while (fgets (line, sizeof (line), fh) != NULL)
    {
      if (_nss_mysql_is_bracketed (line) == ntrue)
        {
          p = index (line, ']');
          *p = '\0';
          p = line;
          p++;
          *section = _nss_mysql_section_to_id (p);
          if (*section != SECTION_INVALID)
              function_return (NSS_SUCCESS);
        }
    }
  function_return (NSS_NOTFOUND);
}

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
        function_return (to_return);
    }
  function_return (NSS_SUCCESS);
}

static NSS_STATUS
_nss_mysql_load_config_file (char *file, conf_t *conf)
{
  FILE *fh;
  int section;
  int to_return;

  function_enter;
  _nss_mysql_debug (FNAME, D_FILE, "Opening %s", file);
  if ((fh = fopen (file, "r")) == NULL)
    function_return (NSS_UNAVAIL);
  while ((_nss_mysql_get_section (fh, &section)) == NSS_SUCCESS)
    {
      _nss_mysql_debug (FNAME, D_PARSE, "Loading section type: %d",
                        section);
      switch (section)
        {
        case SECTION_GLOBAL:
          to_return = _nss_mysql_load_section (fh, &conf->global,
                                               global_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              function_return (to_return);
            }
            break;
        case SECTION_SERVER:
          conf->num_servers++;
          if (conf->num_servers >= MAX_SERVERS)
            {
              /* Let's not bomb because we have too many specified .. */
              _nss_mysql_log (LOG_ERR,
                              "Too many servers defined.  Max = %d",
                              MAX_SERVERS);
              fclose (fh);
              function_return (NSS_SUCCESS);
            }
          to_return =
            _nss_mysql_load_section (fh,
                                     &(conf->sql.server[conf->num_servers - 1]),
                                     server_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              function_return (to_return);
            }
          break;
        case SECTION_QUERIES:
          to_return = _nss_mysql_load_section (fh, &conf->sql.query,
                                               query_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              function_return (to_return);
            }
          break;
        default:
          _nss_mysql_log (LOG_ERR, "%s: Unhandled section type: %d", FNAME,
                          section);
          break;
        }
    }
  function_return (NSS_SUCCESS);
}

NSS_STATUS
_nss_mysql_load_config (conf_t *conf)
{
  int to_return;

  function_enter;
  if (conf->valid == ntrue)
    function_return (NSS_SUCCESS);

  memset (conf, 0, sizeof (conf_t));
  conf->global.retry = DEF_RETRY;
  conf->global.syslog_facility = DEF_FACIL;
  conf->global.syslog_priority = DEF_PRIO;
  conf->global.debug_flags = DEF_DFLAGS;
  to_return = _nss_mysql_load_config_file (MAINCFG, conf);
  if (to_return != NSS_SUCCESS)
    function_return (to_return);
  if (geteuid() == 0)
    {
      conf->num_servers = 0;
      to_return = _nss_mysql_load_config_file (ROOTCFG, conf);
      if (to_return != NSS_SUCCESS)
        function_return (to_return);
    }
  conf->valid = ntrue;
  function_return (NSS_SUCCESS);
}

