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

#include "nss_mysql.h"

#ifdef HAVE_NSS_H

#define SETENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_set##type (void)                                              \
    {                                                                        \
      DN ("_nss_mysql_set" #type)                                            \
      DENTER                                                                 \
      LOCK;                                                                  \
      _nss_mysql_reset_ent (&mresult_##type);                                \
      UNLOCK;                                                                \
      DSRETURN (NSS_SUCCESS)                                                 \
    }

#define ENDENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_end##type (void)                                              \
    {                                                                        \
      DN ("_nss_mysql_end" #type)                                            \
      DENTER                                                                 \
      LOCK;                                                                  \
      _nss_mysql_reset_ent (&mresult_##type);                                \
      UNLOCK;                                                                \
      DSRETURN (NSS_SUCCESS)                                                 \
    }

#elif defined (HAVE_NSS_COMMON_H)

#define SETENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_set##type (nss_backend_t *be, void *args)                     \
    {                                                                        \
      DN ("_nss_mysql_set" #type)                                            \
      DENTER                                                                 \
      LOCK;                                                                  \
      _nss_mysql_reset_ent (&mresult_##type);                                \
      UNLOCK;                                                                \
      DSRETURN (NSS_SUCCESS)                                                 \
    }

#define ENDENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_end##type (nss_backend_t *be, void *args)                     \
    {                                                                        \
      DN ("_nss_mysql_end" #type)                                            \
      DENTER                                                                 \
      LOCK;                                                                  \
      _nss_mysql_reset_ent (&mresult_##type);                                \
      UNLOCK;                                                                \
      DSRETURN (NSS_SUCCESS)                                                 \
    }

#define CONSTR(type)                                                         \
    nss_backend_t *                                                          \
    _nss_mysql_##type##_constr (const char *db_name, const char *src_name,   \
                                const char *cfg_args)                        \
    {                                                                        \
      DN ("_nss_mysql_" #type "_constr")                                     \
      nss_backend_t *be;                                                     \
      DENTER                                                                 \
      be = (nss_backend_t *) malloc (sizeof (*be));                          \
      if (!be)                                                               \
        DPRETURN (NULL)                                                      \
      be->ops = type##_ops;                                                  \
      be->n_ops = sizeof (type##_ops) / sizeof (nss_backend_op_t);           \
      DPRETURN (be)                                                          \
    }
#endif

