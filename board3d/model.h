/* This program is free software; you can redistribute it and/or modify
* it under the terms of version 3 or later of the GNU General Public License as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _MODEL_H_
#define _MODEL_H_

/* Occulsion model */
typedef struct _OccModel
{
	GArray *planes;
	GArray *edges;
	GArray *points;
} OccModel;

typedef struct Occluder_T
{
	float invMat[4][4];
	float trans[3];
	float rot[3];
	int rotator;

	OccModel* handle;
	int show;
} Occluder;

extern void GenerateShadowVolume(const Occluder* pOcc, const float olight[4]);

extern void initOccluder(Occluder* Occ);
extern void freeOccluder(Occluder* Occ);
extern void copyOccluder(const Occluder* fromOcc, Occluder* toOcc);
extern void moveToOcc(const Occluder* pOcc);

extern void addClosedSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquareCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addWonkyCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d, float s, int full);
extern void addCylinder(Occluder* pOcc, float x, float y, float z, float r, float d, unsigned int a);
extern void addHalfTube(Occluder* pOcc, float r, float h, unsigned int a);
extern void addDice(Occluder* pOcc, float size);

#endif
