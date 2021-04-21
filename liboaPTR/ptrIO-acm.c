/*****************************************************************************
 *
 * ptrIO-acm.c -- PTR IO routines (acm)
 *
 * Copyright 2016,2017,2021 James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <oa_common.h>

#include <openastro/util.h>
#include <openastro/timer.h>

#include "oaptrprivate.h"
#include "ptr.h"


int
_ptrWrite ( int fd, const char* buffer, int len )
{
  return 0;
}


int
_ptrRead ( int fd, char* buffer, int maxlen )
{
  oaLogError ( OA_LOG_TIMER, "%s: Not yet implemented", __func__ );
  return 0;
}
