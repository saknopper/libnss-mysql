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
#include <grp.h>

#ifdef HAVE_NSS_H
GET(getgrnam, group_fields, name, const char *, struct group *, 0);
GET(getgrgid, group_fields, number, uid_t, struct group *, 0);
ENDENT(grent);
SETENT(grent);
GETENT(getgrent, group_fields, struct group *, 0);
#endif

#ifdef HAVE_NSS_COMMON_H
GET(getgrnam, group_fields, name, key.name, struct group *, 0);
GET(getgrgid, group_fields, number, key.uid, struct group *, 0);
ENDENT(grent);
SETENT(grent);
GETENT(getgrent, group_fields, struct group *, 0);

static nss_backend_op_t group_ops[] = {
    _nss_mysql_default_destr,       /* NSS_DBOP_DESTRUCTOR */
    _nss_mysql_endgrent_r,          /* NSS_DBOP_ENDENT */
    _nss_mysql_setgrent_r,          /* NSS_DBOP_SETENT */
    _nss_mysql_getgrent_r,          /* NSS_DBOP_GETENT */
    _nss_mysql_getgrnam_r,          /* NSS_DBOP_GROUP_BYNAME */
    _nss_mysql_getgrgid_r           /* NSS_DBOP_GROUP_BYUID */
};

CONSTR(group);

#endif

