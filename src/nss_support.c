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
#include <string.h>     /* memset() */
#include <pwd.h>        /* struct passwd */
#ifdef HAVE_SHADOW_H
#include <shadow.h>     /* struct spwd */
#endif
#include <grp.h>        /* struct group */

extern conf_t conf;

/* Alignment code adapted from padl.com's nss_ldap */
#ifdef __GNUC__
#define alignof(ptr) __alignof__(ptr)
#else
#define alignof(ptr) (sizeof(char *))
#endif

#define align(ptr, blen, TYPE)                                               \
do {                                                                         \
  char *qtr = ptr;                                                           \
  ptr += alignof(TYPE) - 1;                                                  \
  ptr -= ((ptr - (char *)NULL) % alignof(TYPE));                             \
  blen -= (ptr - qtr);                                                       \
} while (0)

/* Password */
#if defined(__FreeBSD__)
typedef enum {
  ROW_PW_NAME,
  ROW_PW_PASSWD,
  ROW_PW_UID,
  ROW_PW_GID,
  ROW_PW_CHANGE,
  ROW_PW_CLASS,
  ROW_PW_GECOS,
  ROW_PW_DIR,
  ROW_PW_SHELL,
  ROW_PW_EXPIRE,
  NUM_PW_ELEMENTS
} pw_rows;
#elif defined (sun)
typedef enum {
  ROW_PW_NAME,
  ROW_PW_PASSWD,
  ROW_PW_UID,
  ROW_PW_GID,
  ROW_PW_AGE,
  ROW_PW_COMMENT,
  ROW_PW_GECOS,
  ROW_PW_DIR,
  ROW_PW_SHELL,
  NUM_PW_ELEMENTS
} pw_rows;
#else
typedef enum {
  ROW_PW_NAME,
  ROW_PW_PASSWD,
  ROW_PW_UID,
  ROW_PW_GID,
  ROW_PW_GECOS,
  ROW_PW_DIR,
  ROW_PW_SHELL,
  NUM_PW_ELEMENTS
} pw_rows;
#endif

/* Shadow */
typedef enum {
  ROW_SP_NAMP,
  ROW_SP_PWDP,
  ROW_SP_LSTCHG,
  ROW_SP_MIN,
  ROW_SP_MAX,
  ROW_SP_WARN,
  ROW_SP_INACT,
  ROW_SP_EXPIRE,
  ROW_SP_FLAG,
  NUM_SP_ELEMENTS
} sp_rows;

/* Group */
typedef enum {
  ROW_GR_NAME,
  ROW_GR_PASSWD,
  ROW_GR_GID,
  ROW_GR_MEM,
  NUM_GR_ELEMENTS
} gr_rows;

NSS_STATUS 
_nss_mysql_load_passwd (void *result, char *buffer, size_t buflen,
                        MYSQL_RES *mresult, int *errnop)
{
  DN ("_nss_mysql_load_passwd")
  MYSQL_ROW row;
  int retVal, i;
  struct passwd *pw = (struct passwd *)result;
  size_t offsets[NUM_PW_ELEMENTS];
  unsigned long *lengths;
  unsigned int num_fields;

  DENTER
  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    DSRETURN (retVal)
  num_fields = _nss_mysql_num_fields (mresult);
  if (num_fields != NUM_PW_ELEMENTS)
    {
      _nss_mysql_log (LOG_ALERT,
                      "mysql_fetch_row() found %u rows (expecting %u).",
                      num_fields, NUM_PW_ELEMENTS);
      DSRETURN (NSS_UNAVAIL)
    }

  /* Make sure we have enough room in 'buffer' for all our data */
  lengths = (unsigned long *) _nss_mysql_fetch_lengths (mresult);
  offsets[ROW_PW_NAME] = 0;
  for (i = 1; i < NUM_PW_ELEMENTS; i++)
    offsets[i] = offsets[i - 1] + lengths[i - 1] + 1;
  if (offsets[NUM_PW_ELEMENTS - 1] > buflen)
    EXHAUSTED_BUFFER;

  /* Clear out buffer and copy in data */
  memset (buffer, 0, buflen);
  pw->pw_name = memcpy (buffer + offsets[ROW_PW_NAME], row[ROW_PW_NAME],
                        lengths[ROW_PW_NAME]);
  pw->pw_passwd = memcpy (buffer + offsets[ROW_PW_PASSWD], row[ROW_PW_PASSWD],
                          lengths[ROW_PW_PASSWD]);
  pw->pw_uid = atoi (row[ROW_PW_UID]);
  pw->pw_gid = atoi (row[ROW_PW_GID]);
  pw->pw_gecos = memcpy (buffer + offsets[ROW_PW_GECOS], row[ROW_PW_GECOS],
                         lengths[ROW_PW_GECOS]);
  pw->pw_dir = memcpy (buffer + offsets[ROW_PW_DIR], row[ROW_PW_DIR],
                       lengths[ROW_PW_DIR]);
  pw->pw_shell = memcpy (buffer + offsets[ROW_PW_SHELL], row[ROW_PW_SHELL],
                         lengths[ROW_PW_SHELL]);
#if defined(__FreeBSD__)
  pw->pw_change = atoi (row[ROW_PW_CHANGE]);
  pw->pw_class = memcpy (buffer + offsets[ROW_PW_CLASS], row[ROW_PW_CLASS],
                         lengths[ROW_PW_CLASS]);
  pw->pw_change = atoi (row[ROW_PW_EXPIRE]);
#endif

#if defined(sun)
  pw->pw_age = memcpy (buffer + offsets[ROW_PW_AGE], row[ROW_PW_AGE],
                       lengths[ROW_PW_AGE]);
  pw->pw_comment = memcpy (buffer + offsets[ROW_PW_COMMENT],
                           row[ROW_PW_COMMENT], lengths[ROW_PW_COMMENT]);
#endif

  DSRETURN (NSS_SUCCESS)
}

#ifdef HAVE_SHADOW_H
NSS_STATUS 
_nss_mysql_load_shadow (void *result, char *buffer, size_t buflen,
                        MYSQL_RES *mresult, int *errnop)
{
  DN ("_nss_mysql_load_shadow")
  MYSQL_ROW row;
  int retVal;
  struct spwd *sp = (struct spwd *)result;
  size_t offsets[NUM_SP_ELEMENTS];
  unsigned long *lengths;
  unsigned int num_fields;

  DENTER
  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    DSRETURN (retVal)
  num_fields = _nss_mysql_num_fields (mresult);
  if (num_fields != NUM_SP_ELEMENTS)
    {
      _nss_mysql_log (LOG_ALERT,
                      "mysql_fetch_row() found %u rows (expecting %u).",
                      num_fields, NUM_SP_ELEMENTS);
      DSRETURN (NSS_UNAVAIL)
    }

  /* Make sure we have enough room in 'buffer' for all our data */
  lengths = (unsigned long *) _nss_mysql_fetch_lengths (mresult);
  offsets[ROW_SP_NAMP] = 0;
  offsets[ROW_SP_PWDP] = offsets[ROW_SP_NAMP] + lengths[ROW_SP_NAMP] + 1;
  if (offsets[ROW_SP_PWDP] + lengths[ROW_SP_PWDP] + 1 > buflen)
    EXHAUSTED_BUFFER;

  /* Clear out buffer and copy in data */
  memset (buffer, 0, buflen);
  sp->sp_namp = memcpy (buffer + offsets[ROW_SP_NAMP], row[ROW_SP_NAMP],
                        lengths[ROW_SP_NAMP]);
  sp->sp_pwdp = memcpy (buffer + offsets[ROW_SP_PWDP], row[ROW_SP_PWDP],
                        lengths[ROW_SP_PWDP]);
  sp->sp_lstchg = atol (row[ROW_SP_LSTCHG]);
  sp->sp_min = atol (row[ROW_SP_MIN]);
  sp->sp_max = atol (row[ROW_SP_MAX]);
  sp->sp_warn = atol (row[ROW_SP_WARN]);
  sp->sp_inact = atol (row[ROW_SP_INACT]);
  sp->sp_expire = atol (row[ROW_SP_EXPIRE]);
  sp->sp_flag = (unsigned long) atol (row[ROW_SP_FLAG]);
  DSRETURN (NSS_SUCCESS)
}
#endif

static NSS_STATUS
_nss_mysql_load_memsbygid (void *result, char *buffer, size_t buflen,
                           MYSQL_RES *mresult, int *errnop)
{
  DN ("_nss_mysql_load_memsbygid")
  MYSQL_ROW row;
  int retVal;
  struct group *gr = (struct group *)result;
  char **members;
  unsigned long num_rows, i;
  unsigned long *lengths;
  size_t strings_offset;

  DENTER
  num_rows = _nss_mysql_num_rows (mresult);
  align (buffer, buflen, char *);

  /* Return empty/NULL group list if no members */
  if (num_rows == 0)
    {
      gr->gr_mem = (char **) (uintptr_t)buffer;
      DSRETURN (NSS_SUCCESS)
    }

  members = (char **)buffer;
  strings_offset = (num_rows + 1) * sizeof (char *);
  /* Allow room for NUM_ROWS + 1 pointers */
  if (strings_offset > buflen)
    EXHAUSTED_BUFFER;

  buflen -= strings_offset;

  /* Load the first one */
  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    DSRETURN (retVal)
  lengths = (unsigned long *) _nss_mysql_fetch_lengths (mresult);
  if (lengths[0] + 1 > buflen)
    EXHAUSTED_BUFFER;

  members[0] = buffer + strings_offset;
  strncpy (members[0], row[0], lengths[0]);
  buflen -= lengths[0] + 1;

  /* Load the rest */
  for (i = 1; i < num_rows; i++)
    {
      /* Set pointer to one byte after the last string loaded */
      members[i] = members[i - 1] + lengths[0] + 1;

      retVal = _nss_mysql_fetch_row (&row, mresult);
      if (retVal != NSS_SUCCESS)
        DSRETURN (retVal)
      lengths = (unsigned long *) _nss_mysql_fetch_lengths (mresult);
      if (lengths[0] + 1 > buflen)
        EXHAUSTED_BUFFER;
      strncpy (members[i], row[0], lengths[0]);
      buflen -= lengths[0] + 1;
    }

  /* Set the last pointer to NULL to terminate the pointer-list */
  members[num_rows] = NULL;

  /* Set gr->gr_mem to point to start of our pointer-list */
  gr->gr_mem = (char **) (uintptr_t)buffer;

  DSRETURN (NSS_SUCCESS)
}

NSS_STATUS 
_nss_mysql_load_group (void *result, char *buffer, size_t buflen,
                       MYSQL_RES *mresult, int *errnop)
{
  DN ("_nss_mysql_load_group")
  MYSQL_ROW row;
  MYSQL_RES *mresult_grmem = NULL;
  int retVal;
  struct group *gr = (struct group *)result;
  size_t offsets[NUM_GR_ELEMENTS];
  unsigned long *lengths;
  unsigned int num_fields;

  DENTER
  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    DSRETURN (retVal)
  num_fields = _nss_mysql_num_fields (mresult);
  if (num_fields != NUM_GR_ELEMENTS - 1) /* gr_mem not part of this query */
    {
      _nss_mysql_log (LOG_ALERT,
                      "mysql_fetch_row() found %u rows (expecting %u).",
                      num_fields, NUM_GR_ELEMENTS - 1);
      DSRETURN (NSS_UNAVAIL)
    }

  /*
   * Make sure we have enough room in 'buffer' for all our data
   * (not including gr_mem - that's dealt with later
   */
  lengths = (unsigned long *) _nss_mysql_fetch_lengths (mresult);
  offsets[ROW_GR_NAME] = 0;
  offsets[ROW_GR_PASSWD] = offsets[ROW_GR_NAME] + lengths[ROW_GR_NAME] + 1;
  offsets[ROW_GR_MEM] = offsets[ROW_GR_PASSWD] + lengths[ROW_GR_PASSWD] + 1;
  if (offsets[ROW_GR_MEM] + 1 > buflen)
    EXHAUSTED_BUFFER;

  /* Clear out buffer and copy in data (except gr_mem) */
  memset (buffer, 0, buflen);
  gr->gr_name = memcpy (buffer + offsets[ROW_GR_NAME], row[ROW_GR_NAME],
                        lengths[ROW_GR_NAME]);
  gr->gr_passwd = memcpy (buffer + offsets[ROW_GR_PASSWD], row[ROW_GR_PASSWD],
                          lengths[ROW_GR_PASSWD]);
  gr->gr_gid = atoi (row[ROW_GR_GID]);

  /* Load gr_mem */
  retVal = _nss_mysql_lookup (BYNUM, NULL, gr->gr_gid,
                              conf.sql.query.memsbygid, nfalse, result,
                              buffer + offsets[ROW_GR_MEM],
                              buflen - offsets[ROW_GR_MEM],
                              errnop, _nss_mysql_load_memsbygid,
                              &mresult_grmem, FUNCNAME);

  DSRETURN (retVal)
}

NSS_STATUS
_nss_mysql_load_gidsbymem (void *result, char *buffer, size_t buflen,
                           MYSQL_RES *mresult, int *errnop)
{
  DN ("_nss_mysql_load_gidsbymem")
  MYSQL_ROW row;
  unsigned long num_rows, i;
  group_info_t *gi = (group_info_t *)result;
  gid_t *groups;
  int retVal;
  gid_t gid;

  DENTER
  num_rows = _nss_mysql_num_rows (mresult);

  /* Nothing to load = success */
  if (num_rows == 0)
    DSRETURN (NSS_SUCCESS)

  /* If we need more room and we're allowed to alloc it, alloc it */
  if (num_rows + *gi->start > *gi->size)
    {
      long int newsize = *gi->size;

      if (gi->limit <= 0)                /* Allocate as much as we need */
        newsize = num_rows + *gi->start;
      else if (*gi->size != gi->limit)   /* Allocate to limit */
        newsize = gi->limit;

      if (newsize != *gi->size)          /* If we've got more room, do it */
        {
          gid_t *groups = *gi->groupsp;
          gid_t *newgroups;

          newgroups = realloc (groups, newsize * sizeof (*groups));
          if (newgroups != NULL)
            {
              *gi->groupsp = groups = newgroups;
              *gi->size = newsize;
            }
        }
    }

  groups = *gi->groupsp;
  for (i = *gi->start; i < *gi->size; i++)
    {
      retVal = _nss_mysql_fetch_row (&row, mresult);
      if (retVal != NSS_SUCCESS)
        DSRETURN (retVal)
      gid = atoi (row[0]);
      if ((long int)gid != gi->group && (long int)gid != groups[0])
        groups[(*gi->start)++] = gid;
    }

  DSRETURN (NSS_SUCCESS)
}

/* Thanks to Clement Laforet for most of this */
#if defined(__FreeBSD__)

NSS_METHOD_PROTOTYPE(__nss_compat_getpwnam_r);
NSS_METHOD_PROTOTYPE(__nss_compat_getpwuid_r);
NSS_METHOD_PROTOTYPE(__nss_compat_getpwent_r);
NSS_METHOD_PROTOTYPE(__nss_compat_setpwent);
NSS_METHOD_PROTOTYPE(__nss_compat_endpwent);
NSS_METHOD_PROTOTYPE(__nss_compat_getgrnam_r);
NSS_METHOD_PROTOTYPE(__nss_compat_getgrgid_r);
NSS_METHOD_PROTOTYPE(__nss_compat_getgrent_r);
NSS_METHOD_PROTOTYPE(__nss_compat_setgrent);
NSS_METHOD_PROTOTYPE(__nss_compat_endgrent);

NSS_STATUS _nss_mysql_getpwnam_r (const char *, struct passwd *, char *,
                                  size_t, int *);
NSS_STATUS _nss_mysql_getpwuid_r (uid_t, struct passwd *, char *,
                                  size_t, int *);
NSS_STATUS _nss_mysql_getpwent_r (struct passwd *, char *, size_t, int *);
NSS_STATUS _nss_mysql_setpwent (void);
NSS_STATUS _nss_mysql_endpwent (void);

NSS_STATUS _nss_mysql_getgrnam_r (const char *, struct group *, char *,
                                  size_t, int *);
NSS_STATUS _nss_mysql_getgrgid_r (gid_t, struct group *, char *,
                                  size_t, int *);
NSS_STATUS _nss_mysql_getgrent_r (struct group *, char *, size_t, int *);
NSS_STATUS _nss_mysql_setgrent (void);
NSS_STATUS _nss_mysql_endgrent (void);

static ns_mtab methods[] = {
    { NSDB_PASSWD, "getpwnam_r", __nss_compat_getpwnam_r, _nss_mysql_getpwnam_r },
    { NSDB_PASSWD, "getpwuid_r", __nss_compat_getpwuid_r, _nss_mysql_getpwuid_r },
    { NSDB_PASSWD, "getpwent_r", __nss_compat_getpwent_r, _nss_mysql_getpwent_r },
    { NSDB_PASSWD, "endpwent",   __nss_compat_setpwent,   _nss_mysql_setpwent },
    { NSDB_PASSWD, "setpwent",   __nss_compat_endpwent,   _nss_mysql_endpwent },
    { NSDB_GROUP,  "getgrnam_r", __nss_compat_getgrnam_r, _nss_mysql_getgrnam_r },
    { NSDB_GROUP,  "getgrgid_r", __nss_compat_getgrgid_r, _nss_mysql_getgrgid_r },
    { NSDB_GROUP,  "getgrent_r", __nss_compat_getgrent_r, _nss_mysql_getgrent_r },
    { NSDB_GROUP,  "endgrent",   __nss_compat_setgrent,   _nss_mysql_setgrent },
    { NSDB_GROUP,  "setgrent",   __nss_compat_endgrent,   _nss_mysql_endgrent },
};

ns_mtab *
nss_module_register (const char *name, unsigned int *size,
                     nss_module_unregister_fn *unregister)
{
    *size = sizeof (methods) / sizeof (methods[0]);
    *unregister = NULL;
    return (methods);
}

#endif /* defined(__FreeBSD__) */

