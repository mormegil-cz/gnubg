/*
* shadow.h
* by Jon Kinsey, 2003
*
* 3d shadow functions
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of version 2 of the GNU General Public License as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
* $Id$
*/

#ifndef _SHADOW_H_
#define _SHADOW_H_

#include "inc3d.h"
#include "gtkboard.h"

void shadowInit(BoardData3d *bd3d, renderdata *prd);
void shadowDisplay(void (*drawScene)(BoardData *, BoardData3d *, renderdata *), BoardData* bd);

#endif
