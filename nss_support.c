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
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <shadow.h>
#include <grp.h>

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

NSS_STATUS 
_nss_mysql_load_passwd (void *result, char *buffer, size_t buflen,
                        MYSQL_RES *mresult)
{
  MYSQL_ROW row;
  int retVal;
  struct passwd *pw = (struct passwd *)result;
  size_t offsets[7];
  unsigned long *lengths;

  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    return (retVal);
  if (_nss_mysql_num_fields (mresult) != 7)
    return (NSS_UNAVAIL);

  lengths = _nss_mysql_fetch_lengths (mresult);
  offsets[0] = 0;
  offsets[1] = offsets[0] + lengths[0] + 1;
  offsets[4] = offsets[1] + lengths[1] + 1;
  offsets[5] = offsets[4] + lengths[4] + 1;
  offsets[6] = offsets[5] + lengths[5] + 1;
  if (offsets[6] + lengths[6] + 1 > buflen)
    {
      errno = ERANGE;
      return (NSS_TRYAGAIN);
    }

  memset (buffer, 0, buflen);
  pw->pw_name = memcpy (buffer + offsets[0], row[0], lengths[0]);
  pw->pw_passwd = memcpy (buffer + offsets[1], row[1], lengths[1]);
  pw->pw_uid = atoi (row[2]);
  pw->pw_gid = atoi (row[3]);
  pw->pw_gecos = memcpy (buffer + offsets[4], row[4], lengths[4]);
  pw->pw_dir = memcpy (buffer + offsets[5], row[5], lengths[5]);
  pw->pw_shell = memcpy (buffer + offsets[6], row[6], lengths[6]);
  return (NSS_SUCCESS);
}

NSS_STATUS 
_nss_mysql_load_shadow (void *result, char *buffer, size_t buflen,
                        MYSQL_RES *mresult)
{
  MYSQL_ROW row;
  int retVal;
  struct spwd *sp = (struct spwd *)result;
  size_t offsets[9];
  unsigned long *lengths;

  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    return (retVal);
  if (_nss_mysql_num_fields (mresult) != 9)
    return (NSS_UNAVAIL);

  lengths = _nss_mysql_fetch_lengths (mresult);
  offsets[0] = 0;
  offsets[1] = offsets[0] + lengths[0] + 1;
  if (offsets[1] + lengths[1] + 1 > buflen)
    {
      errno = ERANGE;
      return (NSS_TRYAGAIN);
    }

  memset (buffer, 0, buflen);
  sp->sp_namp = memcpy (buffer + offsets[0], row[0], lengths[0]);
  sp->sp_pwdp = memcpy (buffer + offsets[1], row[1], lengths[1]);
  sp->sp_lstchg = atol (row[2]);
  sp->sp_min = atol (row[3]);
  sp->sp_max = atol (row[4]);
  sp->sp_warn = atol (row[5]);
  sp->sp_inact = atol (row[6]);
  sp->sp_expire = atol (row[7]);
  sp->sp_flag = (unsigned long) atol (row[8]);
  return (NSS_SUCCESS);
}

// TODO
// Double-check all the buflen/strings_len math
// How to know if RESULT's connection ID changed (due to another routine
// needing to close it) - may be moot as query will fail anyway no?  Check...
static NSS_STATUS
_nss_mysql_load_memsbygid (void *result, char *buffer, size_t buflen,
                           MYSQL_RES *mresult)
{
  MYSQL_ROW row;
  int retVal;
  struct group *gr = (struct group *)result;
  char **members;
  unsigned long num_rows, i;
  unsigned long *lengths;
  size_t strings_offset;
  size_t strings_len;

  num_rows = _nss_mysql_num_rows (mresult);
  if (num_rows == 0)
    return (NSS_NOTFOUND);

  align (buffer, buflen, char *);
  members = (char **)buffer;
  strings_offset = (num_rows + 1) * sizeof (char *);
  strings_len = buflen - strings_offset;
  /* Allow room for NUM_ROWS + 1 pointers */
  if (strings_offset > buflen)
    {
      errno = ERANGE;
      return (NSS_TRYAGAIN);
    }

  /* Load the first one */
  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    return (retVal);
  lengths = _nss_mysql_fetch_lengths (mresult);
  if (lengths[0] + 1 > strings_len)
    {
      errno = ERANGE;
      return (NSS_TRYAGAIN);
    }
  members[0] = buffer + strings_offset;
  strncpy (members[0], row[0], lengths[0]);
  strings_len -= lengths[0] + 1;

  /* Load the rest */
  for (i = 1; i < num_rows; i++)
    {
      /* Set pointer to one byte after the last string loaded */
      members[i] = members[i - 1] + lengths[0] + 1;

      retVal = _nss_mysql_fetch_row (&row, mresult);
      if (retVal != NSS_SUCCESS)
        return (retVal);
      lengths = _nss_mysql_fetch_lengths (mresult);
      if (lengths[0] + 1 > strings_len)
        {
          errno = ERANGE;
          return (NSS_TRYAGAIN);
        }
      strncpy (members[i], row[0], lengths[0]);
      strings_len -= lengths[0] + 1;
    }

  /* Set the last pointer to NULL to terminate the pointer-list */
  members[num_rows] = NULL;

  /* Set gr->gr_mem to point to start of our pointer-list */
  (char *)gr->gr_mem = (uintptr_t)buffer;

  return (NSS_SUCCESS);
}

NSS_STATUS 
_nss_mysql_load_group (void *result, char *buffer, size_t buflen,
                       MYSQL_RES *mresult)
{
  static const char FNAME[] = "_nss_mysql_load_group";
  MYSQL_ROW row;
  MYSQL_RES *mresult_grmem = NULL;
  int retVal;
  struct group *gr = (struct group *)result;
  size_t offsets[4];
  unsigned long *lengths;

  retVal = _nss_mysql_fetch_row (&row, mresult);
  if (retVal != NSS_SUCCESS)
    return (retVal);
  if (_nss_mysql_num_fields (mresult) != 3)
    return (NSS_UNAVAIL);

  lengths = _nss_mysql_fetch_lengths (mresult);
  offsets[0] = 0;
  offsets[1] = offsets[0] + lengths[0] + 1;
  offsets[3] = offsets[1] + lengths[1] + 1;
  if (offsets[3] + 1 > buflen)
    {
      errno = ERANGE;
      return (NSS_TRYAGAIN);
    }

  memset (buffer, 0, buflen);
  gr->gr_name = memcpy (buffer + offsets[0], row[0], lengths[0]);
  gr->gr_passwd = memcpy (buffer + offsets[1], row[1], lengths[1]);
  gr->gr_gid = atoi (row[2]);

  retVal = _nss_mysql_lookup (BYNUM, NULL, gr->gr_gid,
                              &conf.sql.query.memsbygid, nfalse, result,
                              buffer + offsets[3], buflen - offsets[3],
                              _nss_mysql_load_memsbygid, &mresult_grmem, FNAME);

  return (retVal);
}

NSS_STATUS
_nss_mysql_load_gidsbymem (void *result, char *buffer, size_t buflen,
                           MYSQL_RES *mresult)
{
  MYSQL_ROW row;
  unsigned long num_rows, i;
  group_info_t *gi = (group_info_t *)result;
  gid_t *groups;
  int retVal;
  gid_t gid;

  num_rows = _nss_mysql_num_rows (mresult);

  // Nothing to load = success
  if (num_rows == 0)
    return (NSS_SUCCESS);

  // If we need more room and we're allowed to alloc it, alloc it
  if (num_rows + *gi->start > *gi->size)
    {
      long int newsize = *gi->size;

      if (gi->limit <= 0)                // Allocate as much as we need
        newsize = num_rows + *gi->start;
      else if (*gi->size != gi->limit)   // Allocate to limit
        newsize = gi->limit;

      if (newsize != *gi->size)         // If we've got more room, do it
        {
          gid_t *groups = *gi->groupsp;
          gid_t *newgroups;

          newgroups = _nss_mysql_realloc (groups, newsize * sizeof (*groups));
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
        return (retVal);
      gid = atoi (row[0]);
      if ((long int)gid != gi->group && (long int)gid != groups[0])
        groups[(*gi->start)++] = gid;
    }

  return (NSS_SUCCESS);
}
