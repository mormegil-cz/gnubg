
#include "mylist.h"

typedef struct _OccModel
{
	myList planes;
	myList edges;
	myList points;
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

typedef enum _OcculderType {
	OCC_BOARD, OCC_CUBE, OCC_DICE1, OCC_DICE2, OCC_FLAG, OCC_HINGE1, OCC_HINGE2, OCC_PIECE
} OcculderType;
#define LAST_PIECE (OCC_PIECE + 29)

#define NUM_OCC (LAST_PIECE + 1)
extern Occluder Occluders[NUM_OCC];

extern void initOccluder(Occluder* Occ);
extern void freeOccluder(Occluder* Occ);
extern void copyOccluder(Occluder* fromOcc, Occluder* toOcc);

void moveToOcc(Occluder* pOcc);

void shadowInit();
void shadowDisplay(void (*drawScene)(void*), void* arg);
void draw_shadow_volume_extruded_edges(Occluder* Occluder, int prim);
void draw_shadow_volume_edges(Occluder* Occluder);

extern float (*light_position)[4];
extern float dim;
extern void addClosedSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addSquareCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addCubeCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d);
extern void addWonkyCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d, float s, int full);
extern void addCylinder(Occluder* pOcc, float x, float y, float z, float r, float d, int a);
extern void addHalfTube(Occluder* pOcc, float r, float h, int a);
extern void addDice(Occluder* pOcc, float size);
