#include "nss_mysql.h"
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

static field_info_t defaults_fields[] =
{
    {"host",     FOFS (sql_conf_t, server.host),     FT_PCHAR},
    {"port",     FOFS (sql_conf_t, server.port),     FT_UINT},
    {"socket",   FOFS (sql_conf_t, server.socket),   FT_PCHAR},
    {"username", FOFS (sql_conf_t, server.username), FT_PCHAR},
    {"password", FOFS (sql_conf_t, server.password), FT_PCHAR},
    {"database", FOFS (sql_conf_t, server.database), FT_PCHAR},
    {"getpwnam", FOFS (sql_conf_t, query.getpwnam),  FT_PCHAR},
    {"getpwuid", FOFS (sql_conf_t, query.getpwuid),  FT_PCHAR},
    {"getspnam", FOFS (sql_conf_t, query.getspnam),  FT_PCHAR},
    {"getpwent", FOFS (sql_conf_t, query.getpwent),  FT_PCHAR},
    {"getspent", FOFS (sql_conf_t, query.getspent),  FT_PCHAR},
    {"getgrnam", FOFS (sql_conf_t, query.getgrnam),  FT_PCHAR},
    {"getgrgid", FOFS (sql_conf_t, query.getgrgid),  FT_PCHAR},
    {"getgrent", FOFS (sql_conf_t, query.getgrent),  FT_PCHAR},
    {NULL}
};

static field_info_t global_fields[]=
{
    {"retry",           FOFS (global_conf_t, retry),           FT_UINT},
    {NULL}
};

typedef enum
{
  SECTION_INVALID,
  SECTION_GLOBAL,
  SECTION_DEFAULTS,
  SECTION_SERVER
} sections_t;

/* I'm overloading the 'purpose' of field_info_t here ... */
static field_info_t section_info[] =
{
    {"global",   0, SECTION_GLOBAL},
    {"defaults", 0, SECTION_DEFAULTS},
    {"server",   0, SECTION_SERVER},
    {NULL}
};

/* 'lis' = Load Into Structure */
static NSS_STATUS
_nss_mysql_lis (const char *key, const char *val, field_info_t *fields,
               void *structure)
{
  field_info_t *f;
  _nss_mysql_byte *b;
  int size;
  void *ptr;

  function_enter;
  _nss_mysql_debug (FNAME, D_PARSE, "Load '%s' (%p)\n", key, structure);
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
              _nss_mysql_debug (FNAME, D_MEMORY, "ptr = realloc (%p, %d)\n",
                                ptr, size);
              if ((ptr = realloc (ptr, size)) == NULL)
                {
                  _nss_mysql_log_error (FNAME, "realloc() failed");
                  function_return (NSS_UNAVAIL);
                }
              /* Set the pointer in structure to new pointer */
              *(intptr_t *) (b + f->ofs) = (intptr_t) ptr;
              /* Copy value into newly-alloc'ed area */
              memcpy (ptr, val, size);
              break;
            case FT_UINT:
              *(unsigned int *) (b + f->ofs) = (unsigned int) atoi (val);
              break;
            default:
	      _nss_mysql_log_error (FNAME, "BUG!  Unhandled type: %d\n",
                                f->type);
              break;
            }
        }
    }
  function_return (NSS_SUCCESS);
}

/* No debug here cuz that would be way too noisy */
static int _nss_mysql_is_bracketed(char *line)
{
  if (line && *line == '[' && index (line, ']'))
    return 1;
  return 0;
}

/*
 * Load key and val with the next key/val pair (if any) from the file pointed
 * to by fh.  If a key/val pair is found, return 0, otherwise 1.
 */
static NSS_STATUS
_nss_mysql_next_key (FILE *fh, char *key, int key_size, char *val, int val_size)
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
      previous_fpos = ftell(fh);
      if ((fgets (line, sizeof (line), fh)) == NULL)
        {
          _nss_mysql_debug (FNAME, D_PARSE, "EOF\n");
          break;
        }
      if (_nss_mysql_is_bracketed(line))
        {
          _nss_mysql_debug (FNAME, D_PARSE, "End of section\n");
          fseek(fh, previous_fpos, SEEK_SET);
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
      _nss_mysql_debug (FNAME, D_PARSE, "Found key = %s\n", key);
      function_return (NSS_SUCCESS);
    }
  function_return (NSS_NOTFOUND);
}

static int
_nss_mysql_section_to_id (char *name)
{
  field_info_t *f;

  function_enter ;
  for (f = section_info; f->name; f++)
    {
      if (strcmp(name, f->name) == 0)
        function_return (f->type);
    }
  function_return (SECTION_INVALID);
}

static NSS_STATUS
_nss_mysql_get_section (FILE *fh, int *section)
{
  char line[MAX_LINE_LEN];
  char *p;

  function_enter ;
  while (fgets (line, sizeof (line), fh) != NULL)
    {
      if (_nss_mysql_is_bracketed(line))
        {
          p = index(line, ']');
          *p = '\0';
          p = line;
          p++;
          *section = _nss_mysql_section_to_id(p);
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

static void
_nss_mysql_init_defaults(conf_t *conf, int num)
{
  field_info_t *f;
  _nss_mysql_byte *s, *d;
  char *in, *out;
  int size;

  function_enter ;
  s = (_nss_mysql_byte *) &(conf->defaults);
  d = (_nss_mysql_byte *) &(conf->sql[num]);
  for (f = defaults_fields; f->name; f++)
    {
      switch (f->type)
        {
        case FT_UINT:
          _nss_mysql_debug(FNAME, D_PARSE,
                           "%s: copying\n", f->name);
          *(unsigned int *) (s + f->ofs) = *(unsigned int *) (d + f->ofs);
          break;
        case FT_PCHAR:
          (intptr_t) in = *(intptr_t *) (s + f->ofs);
          if (!in)
            continue;
          (intptr_t) out = *(intptr_t *) (d + f->ofs);
          _nss_mysql_debug(FNAME, D_PARSE,
                           "%s: copying from %p to %p\n", f->name, in, out);
          size = strlen (in) + 1 + PADSIZE;
          _nss_mysql_debug(FNAME, D_MEMORY,
                           "%s: out = realloc (%p %d)\n", f->name, out, size);
          if ((out = realloc (out, size)) == NULL)
            {
              _nss_mysql_log_error (FNAME,
                                    "%s: Unable to realloc (%p, %d)\n",
                                    f->name, out, size);
              continue;
            }
          *(intptr_t *) (d + f->ofs) = (intptr_t) out;
          memcpy (out, in, size);
          break;
        default:
	      _nss_mysql_log_error (FNAME, "BUG!  Unhandled type: %d\n",
                                f->type);
          break;
        }
    }
  function_leave;
}

static NSS_STATUS
_nss_mysql_load_config_file (char *file, conf_t *conf)
{
  FILE *fh;
  int section;
  int to_return;

  function_enter ;
  _nss_mysql_debug(FNAME, D_FILE, "Opening %s\n", file);
  if ((fh = fopen (file, "r")) == NULL)
    function_return (NSS_UNAVAIL);
  while ((_nss_mysql_get_section (fh, &section)) == NSS_SUCCESS)
    {
      _nss_mysql_debug(FNAME, D_PARSE, "Loading section type: %d\n",
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
        case SECTION_DEFAULTS:
          to_return = _nss_mysql_load_section (fh, &conf->defaults,
                                               defaults_fields);
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
              _nss_mysql_log_error(FNAME,
                                   "Too many servers defined.  Max = %d\n",
                                   MAX_SERVERS);
              fclose (fh);
              function_return (NSS_SUCCESS);
            }
          /* First copy defaults over */
          _nss_mysql_init_defaults(conf, conf->num_servers - 1);
          to_return =
            _nss_mysql_load_section (fh,
                                     &(conf->sql[conf->num_servers - 1]),
                                     defaults_fields);
          if (to_return != NSS_SUCCESS)
            {
              fclose (fh);
              function_return (to_return);
            }
          break;
        default:
          _nss_mysql_log_error (FNAME, "Unhandled section type: %d\n",
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

  function_enter ;
  if (conf->valid)
   function_return (NSS_SUCCESS);

  memset (conf, 0, sizeof (conf_t));
  conf->global.retry = 30;
  to_return = _nss_mysql_load_config_file (MAINCFG, conf);
  if (to_return != NSS_SUCCESS)
    {
      function_return (to_return);
    }
  if (geteuid() == 0)
    {
      conf->num_servers = 0;
      to_return = _nss_mysql_load_config_file (ROOTCFG, conf);
      if (to_return != NSS_SUCCESS)
        {
          function_return (to_return);
        }
    }
  conf->valid = 1;
  function_return (NSS_SUCCESS);
}

