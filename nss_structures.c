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
#include <pwd.h>
#include <shadow.h>
#include <grp.h>

field_info_t passwd_fields[] =
{
  { "pw_name",   FOFS(struct passwd,pw_name),   FT_PCHAR },
  { "pw_passwd", FOFS(struct passwd,pw_passwd), FT_PCHAR },
  { "pw_uid",    FOFS(struct passwd,pw_uid),    FT_INT   },
  { "pw_gid",    FOFS(struct passwd,pw_gid),    FT_INT   },
  { "pw_gecos",  FOFS(struct passwd,pw_gecos),  FT_PCHAR },
  { "pw_dir",    FOFS(struct passwd,pw_dir),    FT_PCHAR },
  { "pw_shell",  FOFS(struct passwd,pw_shell),  FT_PCHAR },
  { NULL },
} ;

field_info_t spwd_fields[] =
{
  { "sp_namp",   FOFS(struct spwd,sp_namp),   FT_PCHAR },
  { "sp_pwdp",   FOFS(struct spwd,sp_pwdp),   FT_PCHAR },
  { "sp_lstchg", FOFS(struct spwd,sp_lstchg), FT_LONG  },
  { "sp_min",    FOFS(struct spwd,sp_min),    FT_LONG  },
  { "sp_max",    FOFS(struct spwd,sp_max),    FT_LONG  },
  { "sp_warn",   FOFS(struct spwd,sp_warn),   FT_LONG  },
  { "sp_inact",  FOFS(struct spwd,sp_inact),  FT_LONG  },
  { "sp_expire", FOFS(struct spwd,sp_expire), FT_LONG  },
  { "sp_flag",   FOFS(struct spwd,sp_flag),   FT_ULONG },
  { NULL }
} ;

field_info_t group_fields[] =
{
  { "gr_name",   FOFS(struct group,gr_name),   FT_PCHAR },
  { "gr_passwd", FOFS(struct group,gr_passwd), FT_PCHAR },
  { "gr_gid",    FOFS(struct group,gr_gid),    FT_INT   },
  { "gr_mem",    FOFS(struct group,gr_mem),    FT_PPCHAR},
  { NULL }
} ;

