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
    "$Id$ ";

#include "lookup.h"
#include <pwd.h>

#ifdef HAVE_NSS_H
GET(getpwnam, passwd_fields, name, const char *, struct passwd *, 0);
GET(getpwuid, passwd_fields, number, uid_t, struct passwd *, 0);
ENDENT(pwent);
SETENT(pwent);
GETENT(getpwent, passwd_fields, struct passwd *, 0);
#endif

#ifdef HAVE_NSS_COMMON_H
GET(getpwnam, passwd_fields, name, key.name, struct passwd *, 0);
GET(getpwuid, passwd_fields, number, key.uid, struct passwd *, 0);
ENDENT(pwent);
SETENT(pwent);
GETENT(getpwent, passwd_fields, struct passwd *, 0);

static nss_backend_op_t passwd_ops[] = {
    _nss_mysql_default_destr,       /* NSS_DBOP_DESTRUCTOR */
    _nss_mysql_endpwent_r,          /* NSS_DBOP_ENDENT */
    _nss_mysql_setpwent_r,          /* NSS_DBOP_SETENT */
    _nss_mysql_getpwent_r,          /* NSS_DBOP_GETENT */
    _nss_mysql_getpwnam_r,          /* NSS_DBOP_PASSWD_BYNAME */
    _nss_mysql_getpwuid_r           /* NSS_DBOP_PASSWD_BYUID */
};

CONSTR(passwd);

#endif

