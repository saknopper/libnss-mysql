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
#define GET(fname, fields, ltype, argtype, restype, restrict)                \
    NSS_STATUS                                                               \
    _nss_mysql_##fname##_r (argtype arg, restype result, char *buffer,       \
                           size_t buflen)                                    \
    {                                                                        \
      int retVal;                                                            \
      function_enter;                                                        \
      if (restrict && geteuid() != 0)                                        \
        function_return (NSS_NOTFOUND)                                       \
      LOCK;                                                                  \
      retVal = _nss_mysql_lookup_##ltype (arg,                               \
                                          FOFS (sql_query_t, fname),         \
                                          FNAME);                            \
      if (retVal != NSS_SUCCESS)                                             \
        {                                                                    \
          UNLOCK;                                                            \
          function_return (retVal);                                          \
        }                                                                    \
      retVal = _nss_mysql_load_result (result, buffer, buflen,               \
                                       fields);                              \
      _nss_mysql_close_sql (CLOSE_RESULT);                                   \
      UNLOCK;                                                                \
      function_return (retVal);                                              \
    }

#define GETENT(fname, fields, restype, restrict)                             \
    NSS_STATUS                                                               \
    _nss_mysql_##fname##_r (restype *result, char *buffer,                   \
                           size_t buflen)                                    \
    {                                                                        \
      int retVal;                                                            \
      function_enter;                                                        \
      if (restrict && geteuid() != 0)                                        \
        function_return (NSS_NOTFOUND)                                       \
      LOCK;                                                                  \
      if (_nss_mysql_active_result () == nfalse)                             \
        {                                                                    \
          retVal = _nss_mysql_lookup_ent (FOFS (sql_query_t, fname), FNAME); \
          if (retVal != NSS_SUCCESS)                                         \
            {                                                                \
              UNLOCK;                                                        \
              function_return (retVal);                                      \
            }                                                                \
        }                                                                    \
      retVal = _nss_mysql_load_result (result, buffer, buflen, fields);      \
      UNLOCK;                                                                \
      function_return (retVal);                                              \
    }

#define ENDENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_end##type (void)                                              \
    {                                                                        \
      function_enter;                                                        \
      LOCK;                                                                  \
      _nss_mysql_reset_ent ();                                               \
      UNLOCK;                                                                \
      function_return (NSS_SUCCESS);                                         \
    }                                                                        \


#define SETENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_set##type (void)                                              \
    {                                                                        \
      function_enter;                                                        \
      LOCK;                                                                  \
      _nss_mysql_reset_ent ();                                               \
      UNLOCK;                                                                \
      function_return (NSS_SUCCESS);                                         \
    }

#endif

#ifdef HAVE_NSS_COMMON_H
#define GET(fname, fields, ltype, arg, restype)                              \
    NSS_STATUS                                                               \
    _nss_mysql_##fname##_r (nss_backend_t *be, void *args)                   \
    {                                                                        \
      int retVal;                                                            \
      function_enter;                                                        \
      LOCK;                                                                  \
      retVal = _nss_mysql_lookup_##ltype (NSS_ARGS(args)->arg,               \
                                          FOFS (sql_query_t, fname),         \
                                          FNAME);                            \
      if (retVal != NSS_SUCCESS)                                             \
        {                                                                    \
          UNLOCK;                                                            \
          function_return (retVal);                                          \
        }                                                                    \
      retVal = _nss_mysql_load_result (NSS_ARGS(args)->buf.result,           \
                                       NSS_ARGS(args)->buf.buffer,           \
                                       NSS_ARGS(args)->buf.buflen,           \
                                       fields);                              \
      if (retVal == NSS_SUCCESS)                                             \
        NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;              \
      UNLOCK;                                                                \
      function_return (retVal);                                              \
    }

#define GETENT(fname, fields, restype)                                       \
    NSS_STATUS                                                               \
    _nss_mysql_##fname##_r (nss_backend_t *be, void *args)                   \
    {                                                                        \
      int retVal;                                                            \
      function_enter;                                                        \
      LOCK;                                                                  \
      if (_nss_mysql_active_result () == nfalse)                             \
        {                                                                    \
          retVal = _nss_mysql_lookup_ent (FOFS (sql_query_t, fname), FNAME); \
          if (retVal != NSS_SUCCESS)                                         \
            {                                                                \
              UNLOCK;                                                        \
              function_return (retVal);                                      \
            }                                                                \
        }                                                                    \
      retVal = _nss_mysql_load_result (NSS_ARGS(args)->buf.result,           \
                                       NSS_ARGS(args)->buf.buffer,           \
                                       NSS_ARGS(args)->buf.buflen,           \
                                       fields);                              \
      if (retVal == NSS_SUCCESS)                                             \
        NSS_ARGS(args)->returnval = NSS_ARGS(args)->buf.result;              \
      UNLOCK;                                                                \
      function_return (retVal);                                              \
    }

#define ENDENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_end##type##_r (nss_backend_t *be, void *args)                 \
    {                                                                        \
      function_enter;                                                        \
      LOCK;                                                                  \
      _nss_mysql_reset_ent ();                                               \
      UNLOCK;                                                                \
      function_return (NSS_SUCCESS);                                         \
    }                                                                        \


#define SETENT(type)                                                         \
    NSS_STATUS                                                               \
    _nss_mysql_set##type##_r (nss_backend_t *be, void *args)                 \
    {                                                                        \
      function_enter;                                                        \
      LOCK;                                                                  \
      _nss_mysql_reset_ent ();                                               \
      UNLOCK;                                                                \
      function_return (NSS_SUCCESS);                                         \
    }

#define CONSTR(type)                                                         \
    nss_backend_t *                                                          \
    _nss_mysql_##type##_constr (const char *db_name, const char *src_name,   \
                                const char *cfg_args)                        \
    {                                                                        \
      nss_backend_t *be;                                                     \
      function_enter;                                                        \
      be = (nss_backend_t *) xmalloc (sizeof (*be));                         \
      be->ops = type##_ops;                                                  \
      be->n_ops = sizeof (type##_ops) / sizeof (nss_backend_op_t);           \
      function_leave;                                                        \
      return (be);                                                           \
    }                                                                        \

#endif

