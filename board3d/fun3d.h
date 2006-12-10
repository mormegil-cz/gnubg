
#ifndef _FUN3D_H
#define _FUN3D_H

#include "types3d.h"
#include "gtkboard.h"
#include "analysis.h"

/* Setup functions */
void InitGL(BoardData *bd);

/* Drawing functions */
void drawBoard(BoardData *bd, BoardData3d *bd3d, renderdata *prd);
void SetupPerspVolume(BoardData* bd, BoardData3d* bd3d, renderdata* prd, int viewport[4]);
float getBoardWidth();
float getBoardHeight();
void calculateBackgroundSize(BoardData3d *bd3d, int viewport[4]);

void RestrictiveRender(BoardData *bd, BoardData3d *bd3d, renderdata *prd);
void RestrictiveDrawFrame(float pos[3], float width, float height, float depth);
void RestrictiveDraw(ClipBox* pCb, float pos[3], float width, float height, float depth);
void EnlargeCurrentToBox(ClipBox* pOtherCb);
void RestrictiveDrawFlag(BoardData* bd);

extern void getPiecePos(int point, int pos, int swap, float v[3]);

/* Graph functions*/
GtkWidget* StatGraph(GraphData* pgd);
void SetNumGames(GraphData* pgd, int numGames);
void AddGameData(GraphData* pgd, int game, statcontext *psc);
void TidyGraphData(GraphData* pgd);
GraphData *CreateGraphData();

/* Misc functions */
void SetupSimpleMatAlpha(Material* pMat, float r, float g, float b, float a);
void SetupSimpleMat(Material* pMat, float r, float g, float b);
void SetupMat(Material* pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb, int shin, float a);
void setMaterial(Material* pMat);
void SetColour3d(float r, float g, float b, float a);
float randRange(float range);
void setupPath(BoardData *bd, Path* p, float* pRotate, int fClockwise, int fromPoint, int fromDepth, int toPoint, int toDepth);
int movePath(Path* p, float d, float* pRotate, float v[3]);
int finishedPath(Path* p);
void getProjectedPieceDragPos(int x, int y, float pos[3]);
void updateMovingPieceOccPos(BoardData* bd, BoardData3d* bd3d);
void LoadTextureInfo(int FirstPass);
GList *GetTextureList(int type);
extern void FindTexture(TextureInfo** textureInfo, char* file);
extern void FindNamedTexture(TextureInfo** textureInfo, char* name);
int IsSet(int flags, int bit);
float Dist2d(float a, float b);
float ***Alloc3d(int x, int y, int z);
void Free3d(float ***array, int x, int y);
int LoadTexture(Texture* texture, const char* Filename);
void CheckOpenglError();

/* Functions for 3d board */
extern void InitGTK3d(int *argc, char ***argv);
extern void Init3d();
extern void InitBoard3d(BoardData *bd, BoardData3d *bd3d);
extern void freeEigthPoints(float ****boardPoints, int accuracy);
extern void SetupVisual();
extern void SetupViewingVolume3d(BoardData *bd, BoardData3d* bd3d, renderdata *prd);
extern void DisplayCorrectBoardType(BoardData* bd, BoardData3d* bd3d, renderdata* prd);
extern void CreateGLWidget(BoardData* bd);
extern int DoAcceleratedCheck(BoardData3d* bd3d, GtkWidget* pwParent);

extern void *CreatePreviewBoard3d(BoardData* bd, GdkPixmap *ppm);
extern void RollDice3d(BoardData *bd, BoardData3d* bd3d, renderdata *prd);
extern void AnimateMove3d(BoardData *bd, BoardData3d *bd3d);
extern void ShowFlag3d(BoardData *bd, BoardData3d *bd3d, renderdata *prd);
extern void StopIdle3d(BoardData* bd);
extern void preDraw3d(BoardData *bd, BoardData3d *bd3d, renderdata *prd);
extern void CloseBoard3d(BoardData* bd, BoardData3d* bd3d, renderdata* prd);
extern int BoardPoint3d(BoardData* bd, BoardData3d* bd3d, renderdata* prd, int x, int y, int point);
extern int MouseMove3d(BoardData *bd, BoardData3d *bd3d, renderdata* prd, int x, int y);
extern void RenderBoard3d(BoardData* bd, renderdata* prd, void *glpixmap, unsigned char* buf);
extern void Tidy3dObjects(BoardData3d* bd3d, renderdata *prd);
extern float TestPerformance3d(BoardData* bd);
extern void Set3dSettings(renderdata *prdnew, const renderdata *prd);
extern void CopySettings3d(BoardData* from, BoardData* to);
extern void MakeCurrent3d(BoardData3d *bd3d);
extern void GetTextures(BoardData3d* bd3d, renderdata *prd);
extern void ClearTextures(BoardData3d* bd3d);
extern void DeleteTextureList();

extern void PlaceMovingPieceRotation(BoardData *bd, BoardData3d *bd3d, int dest, int src);
extern void SetMovingPieceRotation(BoardData *bd, BoardData3d *bd3d, int pt);
extern void updateOccPos(BoardData* bd);
extern void updateDiceOccPos(BoardData* bd, BoardData3d* bd3d);
extern void updatePieceOccPos(BoardData* bd, BoardData3d* bd3d);
extern void updateHingeOccPos(BoardData3d* bd3d, int show3dHinges);
extern void updateFlagOccPos(BoardData* bd, BoardData3d* bd3d);

extern void RestrictiveRedraw();
extern void RestrictiveDrawPiece(int pos, int depth);
extern void RestrictiveStartMouseMove(int pos, int depth);
extern void RestrictiveEndMouseMove(int pos, int depth);
extern void RestrictiveDrawDice(BoardData* bd);
extern void RestrictiveDrawCube(BoardData* bd, int old_doubled, int old_cube_owner);
extern void RestrictiveDrawMoveIndicator(BoardData* bd);
extern void RestrictiveDrawBoardNumbers(BoardData3d *bd3d);

extern void setDicePos(BoardData *bd, BoardData3d *bd3d);
extern int DiceTooClose(BoardData3d *bd3d, renderdata *prd);

void shadowInit(BoardData3d *bd3d, renderdata *prd);
void shadowDisplay(void (*drawScene)(BoardData *, BoardData3d *, renderdata *), BoardData* bd);

/* font functions */
void glPrintPointNumbers(BoardData3d* bd3d, const char *text);
void glPrintCube(BoardData3d* bd3d, const char *text);
void glPrintNumbersRA(BoardData3d* bd3d, const char *text);
int BuildFont3d(BoardData3d* bd3d);
float GetFontHeight3d(OGLFont *font);

GtkWidget *GetDrawingArea3d(BoardData3d* bd3d);
extern int MaterialCompare(Material* pMat1, Material* pMat2);
extern char *MaterialGetTextureFilename(Material* pMat);
extern void TidyCurveAccuracy3d(BoardData3d* bd3d, int accuracy);
extern void DrawScene3d(BoardData3d* bd3d);
extern int Animating3d(BoardData3d* bd3d);

#endif
