/*
 * renderprefs.h
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2003
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

#ifndef _RENDERPREFS_H_
#define _RENDERPREFS_H_

#ifndef _RENDER_H_
#include "render.h"
#endif

extern char *aszWoodName[];
extern renderdata rdAppearance;

extern void RenderPreferencesParam( renderdata *prd, char *szParam,
				   char *szValue );
extern char *RenderPreferencesCommand( renderdata *prd, char *sz );

#if USE_BOARD3D
char *WriteMaterial(Material* pMat);
#endif

#endif
