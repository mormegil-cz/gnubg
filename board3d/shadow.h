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

extern void initOccluder(Occluder* Occ);
extern void freeOccluder(Occluder* Occ);
extern void copyOccluder(Occluder* fromOcc, Occluder* toOcc);

void moveToOcc(Occluder* pOcc);

void shadowInit(BoardData* bd);
void shadowDisplay(void (*drawScene)(BoardData*), BoardData* bd);
void draw_shadow_volume_extruded_edges(Occluder* Occluder, float light_position[4], int prim);
void draw_shadow_volume_edges(Occluder* Occluder);

extern void addClosedSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquareCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCubeCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addWonkyCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d, float s, int full);
extern void addCylinder(Occluder* pOcc, float x, float y, float z, float r, float d, int a);
extern void addHalfTube(Occluder* pOcc, float r, float h, int a);
extern void addDice(Occluder* pOcc, float size);
