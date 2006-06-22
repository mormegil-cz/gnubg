
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

extern void GenerateShadowVolume(Occluder* pOcc, float olight[4]);
extern void GenerateShadowEdges(Occluder* pOcc);

extern void initOccluder(Occluder* Occ);
extern void freeOccluder(Occluder* Occ);
extern void copyOccluder(Occluder* fromOcc, Occluder* toOcc);

void moveToOcc(Occluder* pOcc);

extern void addClosedSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquareCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCubeCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addWonkyCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d, float s, int full);
extern void addCylinder(Occluder* pOcc, float x, float y, float z, float r, float d, int a);
extern void addHalfTube(Occluder* pOcc, float r, float h, int a);
extern void addDice(Occluder* pOcc, float size);

#endif
