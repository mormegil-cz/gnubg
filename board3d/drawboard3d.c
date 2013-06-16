/*
 * drawboard3d.c
 * by Jon Kinsey, 2003
 *
 * 3d board drawing code
 *
 *
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#include "config.h"
#include "inc3d.h"
#include "boardpos.h"
#ifdef WIN32
#include "wglbuffer.h"
#endif
#include "gtklocdefs.h"

/* Used to calculate correct viewarea for board/fov angles */
typedef struct _viewArea {
    float top;
    float bottom;
    float width;
} viewArea;

/* My logcube - more than 32 then return 0 (show 64) */
static int
LogCube(int n)
{
    int i = 0;
    while (n > (1 << i))
        i++;

    return i < 6 ? i : 0;
}

/* All the board element sizes - based on base_unit size */

/*lint --e{834} ignore potentially confusing operators - caused by lots of board sizes calculations */

/* Widths */

/* Side edge width of bearoff trays */
#define EDGE_WIDTH base_unit

#define TRAY_WIDTH (EDGE_WIDTH * 2.0f + PIECE_HOLE)
#define BOARD_WIDTH (PIECE_HOLE * 6.0f)
#define BAR_WIDTH (PIECE_HOLE * 1.7f)
#define DICE_AREA_CLICK_WIDTH BOARD_WIDTH

#define TAKI_WIDTH .9f

#define TOTAL_WIDTH ((TRAY_WIDTH + BOARD_WIDTH) * 2.0f + BAR_WIDTH)

/* Heights */

/* Bottom + top edge */
#define EDGE_HEIGHT (base_unit * 1.5f)

#define POINT_HEIGHT (PIECE_HOLE * 6)
#define TRAY_HEIGHT (EDGE_HEIGHT + POINT_HEIGHT)
#define MID_SIDE_GAP_HEIGHT (base_unit * 3.5f)
#define DICE_AREA_HEIGHT MID_SIDE_GAP_HEIGHT
/* Vertical gap between pieces */
#define PIECE_GAP_HEIGHT (base_unit / 5.0f)

#define TOTAL_HEIGHT (TRAY_HEIGHT * 2.0f + MID_SIDE_GAP_HEIGHT)

/* Depths */

#define EDGE_DEPTH (base_unit * 1.95f)
#define BASE_DEPTH base_unit

/* Other objects */

#define BOARD_FILLET (base_unit / 3.0f)

#define DOUBLECUBE_SIZE (PIECE_HOLE * .9f)

/* Dice animation step size */
#define DICE_STEP_SIZE0 (base_unit * 1.3f)
#define DICE_STEP_SIZE1 (base_unit * 1.7f)

#define HINGE_GAP (base_unit / 12.0f)
#define HINGE_WIDTH (base_unit / 2.0f)
#define HINGE_HEIGHT (base_unit * 7.0f)

#undef ARROW_SIZE
#define ARROW_SIZE (EDGE_HEIGHT * .8f)

#define FLAG_HEIGHT (base_unit * 3)
#define FLAG_WIDTH (FLAG_HEIGHT * 1.4f)
#define FLAG_WAG (FLAG_HEIGHT * .3f)
#define FLAGPOLE_WIDTH (base_unit * .2f)
#define FLAGPOLE_HEIGHT (FLAG_HEIGHT * 2.05f)

/* Slight offset from surface - avoid using unless necessary */
#define LIFT_OFF (base_unit / 50.0f)

float
getBoardWidth(void)
{
    return TOTAL_WIDTH;
}

float
getBoardHeight(void)
{
    return TOTAL_HEIGHT;
}

float
getDiceSize(const renderdata * prd)
{
    return prd->diceSize * base_unit;
}

extern Flag3d flag;

static void
TidyShadows(BoardData3d * bd3d)
{
    freeOccluder(&bd3d->Occluders[OCC_BOARD]);
    freeOccluder(&bd3d->Occluders[OCC_CUBE]);
    freeOccluder(&bd3d->Occluders[OCC_DICE1]);
    freeOccluder(&bd3d->Occluders[OCC_FLAG]);
    freeOccluder(&bd3d->Occluders[OCC_HINGE1]);
    freeOccluder(&bd3d->Occluders[OCC_PIECE]);
}

void
Tidy3dObjects(BoardData3d * bd3d, const renderdata * prd)
{
    bd3d->shadowsInitialised = FALSE;

    glDeleteLists(bd3d->pieceList, 1);
    glDeleteLists(bd3d->diceList, 1);
    glDeleteLists(bd3d->piecePickList, 1);
    glDeleteLists(bd3d->DCList, 1);

    FreeNumberFont(bd3d->numberFont);
    FreeNumberFont(bd3d->cubeFont);

    gluDeleteQuadric(bd3d->qobjTex);
    gluDeleteQuadric(bd3d->qobj);

    if (flag.flagNurb != NULL)
        gluDeleteNurbsRenderer(flag.flagNurb);

    if (bd3d->boardPoints)
        freeEigthPoints(&bd3d->boardPoints, prd->curveAccuracy);

    TidyShadows(bd3d);
    ClearTextures(bd3d);
    DeleteTextureList();
}

static void
preDrawPiece0(const renderdata * prd, int display)
{
    unsigned int i, j;
    float angle, angle2, step;

    float latitude;

    float new_radius;
    float radius = PIECE_HOLE / 2.0f;
    float discradius = radius * 0.8f;
    float lip = radius - discradius;
    float height = PIECE_DEPTH - 2 * lip;
    float ***p;
    float ***n;

    step = (2 * (float) G_PI) / prd->curveAccuracy;

    /* Draw top/bottom of piece */
    if (display) {
        circleTex(discradius, PIECE_DEPTH, prd->curveAccuracy, prd->ChequerMat[0].pTexture);
        if (prd->ChequerMat[0].pTexture && prd->pieceTextureType == PTT_TOP)
            glDisable(GL_TEXTURE_2D);

        circleRevTex(discradius, 0.f, prd->curveAccuracy, prd->ChequerMat[0].pTexture);
    } else {
        circleSloped(radius, 0.f, PIECE_DEPTH, prd->curveAccuracy);
        return;
    }
    /* Draw side of piece */
    glPushMatrix();
    glTranslatef(0.f, 0.f, lip);
    cylinder(radius, height, prd->curveAccuracy, prd->ChequerMat[0].pTexture);
    glPopMatrix();

    /* Draw edges of piece */
    p = Alloc3d(prd->curveAccuracy + 1, prd->curveAccuracy / 4 + 1, 3);
    n = Alloc3d(prd->curveAccuracy + 1, prd->curveAccuracy / 4 + 1, 3);

    angle2 = 0;
    for (j = 0; j <= prd->curveAccuracy / 4; j++) {
        latitude = sinf(angle2) * lip;
        new_radius = Dist2d(lip, latitude);

        angle = 0;
        for (i = 0; i < prd->curveAccuracy; i++) {
            n[i][j][0] = sinf(angle) * new_radius;
            p[i][j][0] = sinf(angle) * (discradius + new_radius);
            n[i][j][1] = cosf(angle) * new_radius;
            p[i][j][1] = cosf(angle) * (discradius + new_radius);
            p[i][j][2] = latitude + lip + height;
            n[i][j][2] = latitude;

            angle += step;
        }
        p[i][j][0] = p[0][j][0];
        p[i][j][1] = p[0][j][1];
        p[i][j][2] = p[0][j][2];
        n[i][j][0] = n[0][j][0];
        n[i][j][1] = n[0][j][1];
        n[i][j][2] = n[0][j][2];

        angle2 += step;
    }

    for (j = 0; j < prd->curveAccuracy / 4; j++) {
        glBegin(GL_QUAD_STRIP);
        for (i = 0; i < prd->curveAccuracy + 1; i++) {
            glNormal3f((n[i][j][0]) / lip, (n[i][j][1]) / lip, n[i][j][2] / lip);
            if (prd->ChequerMat[0].pTexture)
                glTexCoord2f((p[i][j][0] + discradius) / (discradius * 2),
                             (p[i][j][1] + discradius) / (discradius * 2));
            glVertex3f(p[i][j][0], p[i][j][1], p[i][j][2]);

            glNormal3f((n[i][j + 1][0]) / lip, (n[i][j + 1][1]) / lip, n[i][j + 1][2] / lip);
            if (prd->ChequerMat[0].pTexture)
                glTexCoord2f((p[i][j + 1][0] + discradius) / (discradius * 2),
                             (p[i][j + 1][1] + discradius) / (discradius * 2));
            glVertex3f(p[i][j + 1][0], p[i][j + 1][1], p[i][j + 1][2]);
        }
        glEnd();

        glBegin(GL_QUAD_STRIP);
        for (i = 0; i < prd->curveAccuracy + 1; i++) {
            glNormal3f((n[i][j + 1][0]) / lip, (n[i][j + 1][1]) / lip, n[i][j + 1][2] / lip);
            if (prd->ChequerMat[0].pTexture)
                glTexCoord2f((p[i][j + 1][0] + discradius) / (discradius * 2),
                             (p[i][j + 1][1] + discradius) / (discradius * 2));
            glVertex3f(p[i][j + 1][0], p[i][j + 1][1], PIECE_DEPTH - p[i][j + 1][2]);

            glNormal3f((n[i][j][0]) / lip, (n[i][j][1]) / lip, n[i][j][2] / lip);
            if (prd->ChequerMat[0].pTexture)
                glTexCoord2f((p[i][j][0] + discradius) / (discradius * 2),
                             (p[i][j][1] + discradius) / (discradius * 2));
            glVertex3f(p[i][j][0], p[i][j][1], PIECE_DEPTH - p[i][j][2]);
        }
        glEnd();
    }

    Free3d(p, prd->curveAccuracy + 1, prd->curveAccuracy / 4 + 1);
    Free3d(n, prd->curveAccuracy + 1, prd->curveAccuracy / 4 + 1);

    /* Anti-alias piece edges */
    glLineWidth(1.f);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    circleOutlineOutward(radius, PIECE_DEPTH - lip, prd->curveAccuracy);
    circleOutlineOutward(radius, lip, prd->curveAccuracy);

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
    glDepthMask(GL_TRUE);

    if (prd->ChequerMat[0].pTexture && prd->pieceTextureType == PTT_TOP)
        glEnable(GL_TEXTURE_2D);        /* Re-enable texturing */
}

static void
preDrawPiece1(const renderdata * prd, int display)
{
    float pieceRad = PIECE_HOLE / 2.0f;

    /* Draw top/bottom of piece */
    if (display) {
        circleTex(pieceRad, PIECE_DEPTH, prd->curveAccuracy, prd->ChequerMat[0].pTexture);

        if (prd->ChequerMat[0].pTexture && prd->pieceTextureType == PTT_TOP)
            glDisable(GL_TEXTURE_2D);

        circleRevTex(pieceRad, 0.f, prd->curveAccuracy, prd->ChequerMat[0].pTexture);
    } else {
        circleSloped(pieceRad, 0.f, PIECE_DEPTH, prd->curveAccuracy);
        return;
    }

    /* Edge of piece */
    cylinder(pieceRad, PIECE_DEPTH, prd->curveAccuracy, prd->ChequerMat[0].pTexture);

    /* Anti-alias piece edges */
    glLineWidth(1.f);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    circleOutlineOutward(pieceRad, PIECE_DEPTH, prd->curveAccuracy);
    circleOutlineOutward(pieceRad, 0.f, prd->curveAccuracy);

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
    glDepthMask(GL_TRUE);

    if (prd->ChequerMat[0].pTexture && prd->pieceTextureType == PTT_TOP)
        glEnable(GL_TEXTURE_2D);        /* Re-enable texturing */
}

static void
preRenderPiece(GLuint pieceList, const renderdata * prd, int display)
{
    glNewList(pieceList, GL_COMPILE);

    switch (prd->pieceType) {
    case PT_ROUNDED:
        preDrawPiece0(prd, display);
        break;
    case PT_FLAT:
        preDrawPiece1(prd, display);
        break;
    default:
        g_print("Error: Unhandled piece type\n");
    }

    glEndList();
}

static void
preDrawPiece(BoardData3d * bd3d, renderdata * prd)
{
    unsigned int temp;
    if (bd3d->pieceList) {
        glDeleteLists(bd3d->pieceList, 1);
        glDeleteLists(bd3d->piecePickList, 1);
    }

    bd3d->pieceList = glGenLists(1);
    bd3d->piecePickList = glGenLists(1);
    preRenderPiece(bd3d->pieceList, prd, TRUE);

    /* Simplified piece for picking */
    temp = prd->curveAccuracy;
    prd->curveAccuracy = 10;
    preRenderPiece(bd3d->piecePickList, prd, FALSE);
    prd->curveAccuracy = temp;
}

static void
UnitNormal(float x, float y, float z)
{
    /* Calculate the length of the vector */
    float length = sqrtf((x * x) + (y * y) + (z * z));

    /* Dividing each element by the length will result in a unit normal vector */
    glNormal3f(x / length, y / length, z / length);
}

static void
renderDice(const renderdata * prd)
{
    unsigned int ns, lns;
    unsigned int i, j;
    int c;
    float lat_angle;
    float lat_step;
    float radius;
    float angle, step = 0;
    float size = getDiceSize(prd);

    unsigned int corner_steps = (prd->curveAccuracy / 4) + 1;
    float ***corner_points = Alloc3d(corner_steps, corner_steps, 3);

    radius = size / 2.0f;
    step = (2 * (float) G_PI) / prd->curveAccuracy;

    glPushMatrix();

    /* Draw 6 faces */
    for (c = 0; c < 6; c++) {
        circle(radius, radius, prd->curveAccuracy);

        if (c % 2 == 0)
            glRotatef(-90.f, 0.f, 1.f, 0.f);
        else
            glRotatef(90.f, 1.f, 0.f, 0.f);
    }

    lat_angle = 0;
    lns = (prd->curveAccuracy / 4);
    lat_step = ((float) G_PI / 2) / lns;

    /* Calculate corner points */
    for (i = 0; i < lns + 1; i++) {
        ns = (prd->curveAccuracy / 4) - i;
        if (ns > 0)
            step = ((float) G_PI / 2 - lat_angle) / (ns);
        angle = 0;

        for (j = 0; j <= ns; j++) {
            corner_points[i][j][0] = cosf(lat_angle) * radius;
            corner_points[i][j][1] = cosf(angle) * radius;
            corner_points[i][j][2] = sinf(angle + lat_angle) * radius;

            angle += step;
        }
        lat_angle += lat_step;
    }

    /* Draw 8 corners */
    for (c = 0; c < 8; c++) {
        glPushMatrix();

        glRotatef((float) c * 90, 0.f, 0.f, 1.f);

        for (i = 0; i < prd->curveAccuracy / 4; i++) {
            ns = (prd->curveAccuracy / 4) - (i + 1);

            glBegin(GL_TRIANGLE_STRIP);
            UnitNormal(corner_points[i][0][0], corner_points[i][0][1], corner_points[i][0][2]);
            glVertex3f(corner_points[i][0][0], corner_points[i][0][1], corner_points[i][0][2]);
            for (j = 0; j <= ns; j++) {
                UnitNormal(corner_points[i + 1][j][0], corner_points[i + 1][j][1], corner_points[i + 1][j][2]);
                glVertex3f(corner_points[i + 1][j][0], corner_points[i + 1][j][1], corner_points[i + 1][j][2]);
                UnitNormal(corner_points[i][j + 1][0], corner_points[i][j + 1][1], corner_points[i][j + 1][2]);
                glVertex3f(corner_points[i][j + 1][0], corner_points[i][j + 1][1], corner_points[i][j + 1][2]);
            }
            glEnd();
        }

        glPopMatrix();
        if (c == 3)
            glRotatef(180.f, 1.f, 0.f, 0.f);
    }

    /* Anti-alias dice edges */
    glLineWidth(1.f);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    for (c = 0; c < 6; c++) {
        circleOutline(radius, radius + LIFT_OFF, prd->curveAccuracy);

        if (c % 2 == 0)
            glRotatef(-90.f, 0.f, 1.f, 0.f);
        else
            glRotatef(90.f, 1.f, 0.f, 0.f);
    }
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
    glDepthMask(GL_TRUE);

    glPopMatrix();

    Free3d(corner_points, corner_steps, corner_steps);
}

static void
renderCube(const renderdata * prd, float size)
{
    int i, c;
    float ***corner_points;
    float radius = size / 7.0f;
    float ds = (size * 5.0f / 7.0f);
    float hds = (ds / 2);

    glPushMatrix();

    /* Draw 6 faces */
    for (c = 0; c < 6; c++) {
        glPushMatrix();
        glTranslatef(0.f, 0.f, hds + radius);

        glNormal3f(0.f, 0.f, 1.f);

        glBegin(GL_QUADS);
        glVertex3f(-hds, -hds, 0.f);
        glVertex3f(hds, -hds, 0.f);
        glVertex3f(hds, hds, 0.f);
        glVertex3f(-hds, hds, 0.f);
        glEnd();

        /* Draw 12 edges */
        for (i = 0; i < 2; i++) {
            glPushMatrix();
            glRotatef((float) i * 90, 0.f, 0.f, 1.f);

            glTranslatef(hds, -hds, -radius);
            QuarterCylinder(radius, ds, prd->curveAccuracy, 0);
            glPopMatrix();
        }
        glPopMatrix();
        if (c % 2 == 0)
            glRotatef(-90.f, 0.f, 1.f, 0.f);
        else
            glRotatef(90.f, 1.f, 0.f, 0.f);
    }

    calculateEigthPoints(&corner_points, radius, prd->curveAccuracy);

    /* Draw 8 corners */
    for (c = 0; c < 8; c++) {
        glPushMatrix();
        glTranslatef(0.f, 0.f, hds + radius);

        glRotatef((float) c * 90, 0.f, 0.f, 1.f);

        glTranslatef(hds, -hds, -radius);
        glRotatef(-90.f, 0.f, 0.f, 1.f);

        drawCornerEigth(corner_points, radius, prd->curveAccuracy);

        glPopMatrix();
        if (c == 3)
            glRotatef(180.f, 1.f, 0.f, 0.f);
    }
    glPopMatrix();

    freeEigthPoints(&corner_points, prd->curveAccuracy);
}

static void
preDrawDice(BoardData3d * bd3d, const renderdata * prd)
{
    if (bd3d->diceList)
        glDeleteLists(bd3d->diceList, 1);

    bd3d->diceList = glGenLists(1);
    glNewList(bd3d->diceList, GL_COMPILE);
    renderDice(prd);
    glEndList();

    if (bd3d->DCList)
        glDeleteLists(bd3d->DCList, 1);

    bd3d->DCList = glGenLists(1);
    glNewList(bd3d->DCList, GL_COMPILE);
    renderCube(prd, DOUBLECUBE_SIZE);
    glEndList();
}

static void
getDoubleCubePos(const BoardData * bd, float v[3])
{
    v[2] = BASE_DEPTH + DOUBLECUBE_SIZE / 2.0f; /* Cube on board most of time */

    if (bd->doubled != 0) {
        v[0] = TRAY_WIDTH + BOARD_WIDTH / 2;
        if (bd->doubled != 1)
            v[0] = TOTAL_WIDTH - v[0];

        v[1] = TOTAL_HEIGHT / 2.0f;
    } else {
        if (fClockwise)
            v[0] = TOTAL_WIDTH - TRAY_WIDTH / 2.0f;
        else
            v[0] = TRAY_WIDTH / 2.0f;

        switch (bd->cube_owner) {
        case 0:
            v[1] = TOTAL_HEIGHT / 2.0f;
            v[2] = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE / 2.0f;
            break;
        case -1:
            v[1] = EDGE_HEIGHT + DOUBLECUBE_SIZE / 2.0f;
            break;
        case 1:
            v[1] = (TOTAL_HEIGHT - EDGE_HEIGHT) - DOUBLECUBE_SIZE / 2.0f;
            break;
        default:
            v[1] = 0;           /* error */
        }
    }
}

static void
moveToDoubleCubePos(const BoardData * bd)
{
    float v[3];
    getDoubleCubePos(bd, v);
    glTranslatef(v[0], v[1], v[2]);
}

NTH_STATIC void
drawDCNumbers(const BoardData * bd, const diceTest * dt)
{
    int c, nice;
    float radius = DOUBLECUBE_SIZE / 7.0f;
    float ds = (DOUBLECUBE_SIZE * 5.0f / 7.0f);
    float hds = (ds / 2);
    float depth = hds + radius;

    const char *sides[] = { "4", "16", "32", "64", "8", "2" };
    int side;

    glLineWidth(.5f);
    glPushMatrix();
    for (c = 0; c < 6; c++) {
        if (c < 3)
            side = c;
        else
            side = 8 - c;
        /* Nicer top numbers */
        nice = (side == dt->top);

        /* Don't draw bottom number or back number (unless cube at bottom) */
        if (side != dt->bottom && !(side == dt->side[0] && bd->cube_owner != -1)) {
            if (nice)
                glDisable(GL_DEPTH_TEST);

            glPushMatrix();
            glTranslatef(0.f, 0.f, depth + (nice ? 0 : LIFT_OFF));

            glPrintCube(bd->bd3d->cubeFont, sides[side]);

            glPopMatrix();
            if (nice)
                glEnable(GL_DEPTH_TEST);
        }
        if (c % 2 == 0)
            glRotatef(-90.f, 0.f, 1.f, 0.f);
        else
            glRotatef(90.f, 1.f, 0.f, 0.f);
    }
    glPopMatrix();
}

static void
DrawDCNumbers(const BoardData * bd)
{
    diceTest dt;
    int extraRot = 0;
    int rotDC[6][3] = { {1, 0, 0}, {2, 0, 3}, {0, 0, 0}, {0, 3, 1}, {0, 1, 0}, {3, 0, 3} };

    int cubeIndex;
    /* Rotate to correct number + rotation */
    if (!bd->doubled) {
        cubeIndex = LogCube(bd->cube);
        extraRot = bd->cube_owner + 1;
    } else {
        cubeIndex = LogCube(bd->cube * 2);      /* Show offered cube value */
        extraRot = bd->turn + 1;
    }

    glRotatef((rotDC[cubeIndex][2] + extraRot) * 90.0f, 0.f, 0.f, 1.f);
    glRotatef(rotDC[cubeIndex][0] * 90.0f, 1.f, 0.f, 0.f);
    glRotatef(rotDC[cubeIndex][1] * 90.0f, 0.f, 1.f, 0.f);

    initDT(&dt, rotDC[cubeIndex][0], rotDC[cubeIndex][1], rotDC[cubeIndex][2] + extraRot);

    setMaterial(&bd->rd->CubeNumberMat);
    glNormal3f(0.f, 0.f, 1.f);

    drawDCNumbers(bd, &dt);
}

NTH_STATIC void
drawDC(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    glPushMatrix();
    moveToDoubleCubePos(bd);

    setMaterial(&prd->CubeMat);
    glCallList(bd3d->DCList);

    DrawDCNumbers(bd);

    glPopMatrix();
}

/* Define position of dots on dice */
static int dots1[] = { 2, 2, 0 };
static int dots2[] = { 1, 1, 3, 3, 0 };
static int dots3[] = { 1, 3, 2, 2, 3, 1, 0 };
static int dots4[] = { 1, 1, 1, 3, 3, 1, 3, 3, 0 };
static int dots5[] = { 1, 1, 1, 3, 2, 2, 3, 1, 3, 3, 0 };
static int dots6[] = { 1, 1, 1, 3, 2, 1, 2, 3, 3, 1, 3, 3, 0 };
static int *dots[6] = { dots1, dots2, dots3, dots4, dots5, dots6 };
static int dot_pos[] = { 0, 20, 50, 80 };       /* percentages across face */

static void
drawDots(const BoardData3d * bd3d, float diceSize, float dotOffset, const diceTest * dt, int showFront)
{
    int dot;
    int c, nd;
    int *dp;
    float radius;
    float ds = (diceSize * 5.0f / 7.0f);
    float hds = (ds / 2);
    float x, y;
    float dotSize = diceSize / 10.0f;
    /* Remove specular effects */
    float zero[4] = { 0, 0, 0, 0 };
    glMaterialfv(GL_FRONT, GL_SPECULAR, zero);

    radius = diceSize / 7.0f;

    glPushMatrix();
    for (c = 0; c < 6; c++) {
        if (c < 3)
            dot = c;
        else
            dot = 8 - c;

        /* Make sure top dot looks nice */
        nd = !bd3d->shakingDice && (dot == dt->top);

        if (bd3d->shakingDice || (showFront && dot != dt->bottom && dot != dt->side[0])
            || (!showFront && dot != dt->top && dot != dt->side[2])) {
            if (nd)
                glDisable(GL_DEPTH_TEST);
            glPushMatrix();
            glTranslatef(0.f, 0.f, hds + radius);

            glNormal3f(0.f, 0.f, 1.f);

            /* Show all the dots for this number */
            dp = dots[dot];
            do {
                x = (dot_pos[dp[0]] * ds) / 100;
                y = (dot_pos[dp[1]] * ds) / 100;

                glPushMatrix();
                glTranslatef(x - hds, y - hds, 0.f);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, bd3d->dotTexture);

                glBegin(GL_QUADS);
                glTexCoord2f(0.f, 1.f);
                glVertex3f(dotSize, dotSize, dotOffset);
                glTexCoord2f(1.f, 1.f);
                glVertex3f(-dotSize, dotSize, dotOffset);
                glTexCoord2f(1.f, 0.f);
                glVertex3f(-dotSize, -dotSize, dotOffset);
                glTexCoord2f(0.f, 0.f);
                glVertex3f(dotSize, -dotSize, dotOffset);
                glEnd();

                glPopMatrix();

                dp += 2;
            } while (*dp);

            glPopMatrix();
            if (nd)
                glEnable(GL_DEPTH_TEST);
        }

        if (c % 2 == 0)
            glRotatef(-90.f, 0.f, 1.f, 0.f);
        else
            glRotatef(90.f, 1.f, 0.f, 0.f);
    }
    glPopMatrix();
}

static void
getDicePos(const BoardData * bd, int num, float v[3])
{
    float size = getDiceSize(bd->rd);
    if (bd->diceShown == DICE_BELOW_BOARD) {    /* Show below board */
        v[0] = size * 1.5f;
        v[1] = -size / 2.0f;
        v[2] = size / 2.0f;

        if (bd->turn == 1)
            v[0] += TOTAL_WIDTH - size * 4;
        if (num == 1)
            v[0] += size;       /* Place 2nd dice by 1st */
    } else {
        v[0] = bd->bd3d->dicePos[num][0];
        if (bd->turn == 1)
            v[0] = TOTAL_WIDTH - v[0];  /* Dice on right side */

        v[1] = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f + bd->bd3d->dicePos[num][1];
        v[2] = BASE_DEPTH + LIFT_OFF + size / 2.0f;
    }
}

static void
moveToDicePos(const BoardData * bd, int num)
{
    float v[3];
    getDicePos(bd, num, v);
    glTranslatef(v[0], v[1], v[2]);

    if (bd->diceShown == DICE_ON_BOARD) {       /* Spin dice to required rotation if on board */
        glRotatef(bd->bd3d->dicePos[num][2], 0.f, 0.f, 1.f);
    }
}

NTH_STATIC void
drawDice(const BoardData * bd, int num)
{
    unsigned int value;
    int rotDice[6][2] = { {0, 0}, {0, 1}, {3, 0}, {1, 0}, {0, 3}, {2, 0} };
    int diceCol = (bd->turn == 1);
    int z;
    float diceSize = getDiceSize(bd->rd);
    diceTest dt;
    Material *pDiceMat = &bd->rd->DiceMat[diceCol];
    Material whiteMat;
    SetupSimpleMat(&whiteMat, 1.f, 1.f, 1.f);

    value = bd->diceRoll[num];

    /* During program startup value may be zero, if so don't draw */
    if (!value)
        return;
    value--;                    /* Zero based for array access */

    /* Get dice rotation */
    if (bd->diceShown == DICE_BELOW_BOARD)
        z = 0;
    else
        z = ((int) bd->bd3d->dicePos[num][2] + 45) / 90;

    /* Orientate dice correctly */
    glRotatef(90.0f * rotDice[value][0], 1.f, 0.f, 0.f);
    glRotatef(90.0f * rotDice[value][1], 0.f, 1.f, 0.f);

    /* DT = dice test, use to work out which way up the dice is */
    initDT(&dt, rotDice[value][0], rotDice[value][1], z);

    if (pDiceMat->alphaBlend) { /* Draw back of dice separately */
        glCullFace(GL_FRONT);
        glEnable(GL_BLEND);

        /* Draw dice */
        setMaterial(pDiceMat);
        glCallList(bd->bd3d->diceList);

        /* Place back dots inside dice */
        setMaterial(&bd->rd->DiceDotMat[diceCol]);
        glEnable(GL_BLEND);     /* NB. Disabled in diceList */
        glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        drawDots(bd->bd3d, diceSize, -LIFT_OFF, &dt, 0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glCullFace(GL_BACK);
    }
    /* Draw dice */
    setMaterial(&bd->rd->DiceMat[diceCol]);
    glCallList(bd->bd3d->diceList);

    /* Draw (front) dots */
    glEnable(GL_BLEND);
    /* First blank out space for dots */
    setMaterial(&whiteMat);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    drawDots(bd->bd3d, diceSize, LIFT_OFF, &dt, 1);

    /* Now fill space with coloured dots */
    setMaterial(&bd->rd->DiceDotMat[diceCol]);
    glBlendFunc(GL_ONE, GL_ONE);
    drawDots(bd->bd3d, diceSize, LIFT_OFF, &dt, 1);

    /* Restore blending defaults */
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void
getPiecePos(unsigned int point, unsigned int pos, float v[3])
{
    if (point == 0 || point == 25) {    /* bars */
        v[0] = TOTAL_WIDTH / 2.0f;
        v[1] = TOTAL_HEIGHT / 2.0f;
        v[2] = BASE_DEPTH + EDGE_DEPTH + (int) ((pos - 1) / 3) * PIECE_DEPTH;
        pos = ((pos - 1) % 3) + 1;

        if (point == 25) {
            v[1] += DOUBLECUBE_SIZE / 2.0f + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos + .5f);
        } else {
            v[1] -= DOUBLECUBE_SIZE / 2.0f + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos + .5f);
        }
        v[1] -= PIECE_HOLE / 2.0f;
    } else if (point >= 26) {   /* homes */
        v[2] = BASE_DEPTH;
        if (fClockwise)
            v[0] = TRAY_WIDTH / 2.0f;
        else
            v[0] = TOTAL_WIDTH - TRAY_WIDTH / 2.0f;

        if (point == 26)
            v[1] = EDGE_HEIGHT + (PIECE_DEPTH * 1.2f * (pos - 1));      /* 1.3 gives a gap between pieces */
        else
            v[1] = TOTAL_HEIGHT - EDGE_HEIGHT - PIECE_DEPTH - (PIECE_DEPTH * 1.2f * (pos - 1));
    } else {
        v[2] = BASE_DEPTH + (int) ((pos - 1) / 5) * PIECE_DEPTH;

        if (point < 13) {
            if (fClockwise)
                point = 13 - point;

            if (pos > 10)
                pos -= 10;

            if (pos > 5)
                v[1] = EDGE_HEIGHT + (PIECE_HOLE / 2.0f) + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos - 5 - 1);
            else
                v[1] = EDGE_HEIGHT + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos - 1);

            v[0] = TRAY_WIDTH + PIECE_HOLE * (12 - point);
            if (point < 7)
                v[0] += BAR_WIDTH;
        } else {
            if (fClockwise)
                point = (24 + 13) - point;

            if (pos > 10)
                pos -= 10;

            if (pos > 5)
                v[1] =
                    TOTAL_HEIGHT - EDGE_HEIGHT - (PIECE_HOLE / 2.0f) - PIECE_HOLE - (PIECE_HOLE +
                                                                                     PIECE_GAP_HEIGHT) * (pos - 5 - 1);
            else
                v[1] = TOTAL_HEIGHT - EDGE_HEIGHT - PIECE_HOLE - (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos - 1);

            v[0] = TRAY_WIDTH + PIECE_HOLE * (point - 13);
            if (point > 18)
                v[0] += BAR_WIDTH;
        }
        v[0] += PIECE_HOLE / 2.0f;
    }
    v[2] += LIFT_OFF * 2;

    /* Move to centre of piece */
    if (point < 26) {
        v[1] += PIECE_HOLE / 2.0f;
    } else {                    /* Home pieces are sideways */
        if (point == 27)
            v[1] += PIECE_DEPTH;
        v[2] += PIECE_HOLE / 2.0f;
    }
}

static void
renderSpecialPieces(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    if (bd->drag_point >= 0) {
        glPushMatrix();
        glTranslatef(bd3d->dragPos[0], bd3d->dragPos[1], bd3d->dragPos[2]);
        glRotatef((float) bd3d->movingPieceRotation, 0.f, 0.f, 1.f);
        setMaterial(&prd->ChequerMat[(bd->drag_colour == 1) ? 1 : 0]);
        glCallList(bd3d->pieceList);
        glPopMatrix();
    }

    if (bd3d->moving) {
        glPushMatrix();
        glTranslatef(bd3d->movingPos[0], bd3d->movingPos[1], bd3d->movingPos[2]);
        if (bd3d->rotateMovingPiece > 0)
            glRotatef(-90 * bd3d->rotateMovingPiece * bd->turn, 1.f, 0.f, 0.f);
        glRotatef((float) bd3d->movingPieceRotation, 0.f, 0.f, 1.f);
        setMaterial(&prd->ChequerMat[(bd->turn == 1) ? 1 : 0]);
        glCallList(bd3d->pieceList);
        glPopMatrix();
    }
}

static void
drawSpecialPieces(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{                               /* Draw animated or dragged pieces */
    int blend = (prd->ChequerMat[0].alphaBlend) || (prd->ChequerMat[1].alphaBlend);

    if (blend) {                /* Draw back of piece separately */
        glCullFace(GL_FRONT);
        glEnable(GL_BLEND);
        renderSpecialPieces(bd, bd3d, prd);
        glCullFace(GL_BACK);
        glEnable(GL_BLEND);
    }
    renderSpecialPieces(bd, bd3d, prd);

    if (blend)
        glDisable(GL_BLEND);
}

static void
drawPieceSimplified(GLuint UNUSED(pieceList), const BoardData3d * UNUSED(bd3d), unsigned int point, unsigned int pos)
{
    float v[3];
    glPushMatrix();

    getPiecePos(point, pos, v);
    drawBox(BOX_ALL, v[0] - PIECE_HOLE / 2.f, v[1] - PIECE_HOLE / 2.f, v[2] - PIECE_DEPTH / 2.f, PIECE_HOLE, PIECE_HOLE,
            PIECE_DEPTH, NULL);

    glPopMatrix();
}

static void
drawPiece(GLuint pieceList, const BoardData3d * bd3d, unsigned int point, unsigned int pos, int rotate)
{
    float v[3];
    glPushMatrix();

    getPiecePos(point, pos, v);
    glTranslatef(v[0], v[1], v[2]);

    /* Home pieces are sideways */
    if (point == 26)
        glRotatef(-90.f, 1.f, 0.f, 0.f);
    if (point == 27)
        glRotatef(90.f, 1.f, 0.f, 0.f);

    if (rotate)
        glRotatef((float) bd3d->pieceRotation[point][pos - 1], 0.f, 0.f, 1.f);
    glCallList(pieceList);

    glPopMatrix();
}

NTH_STATIC void
drawPieces(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    unsigned int i;
    unsigned int j;
    int blend = (prd->ChequerMat[0].alphaBlend) || (prd->ChequerMat[1].alphaBlend);

    if (blend) {                /* Draw back of piece separately */
        glCullFace(GL_FRONT);

        setMaterial(&prd->ChequerMat[0]);
        for (i = 0; i < 28; i++) {
            if (bd->points[i] < 0) {
                unsigned int numPieces = (unsigned int) (-bd->points[i]);
                for (j = 1; j <= numPieces; j++) {
                    glEnable(GL_BLEND);
                    drawPiece(bd3d->pieceList, bd3d, i, j, TRUE);
                }
            }
        }
        setMaterial(&prd->ChequerMat[1]);
        for (i = 0; i < 28; i++) {
            if (bd->points[i] > 0) {
                unsigned int numPieces = (unsigned int) bd->points[i];
                for (j = 1; j <= numPieces; j++) {
                    glEnable(GL_BLEND);
                    drawPiece(bd3d->pieceList, bd3d, i, j, TRUE);
                }
            }
        }
        glCullFace(GL_BACK);
    }

    setMaterial(&prd->ChequerMat[0]);
    for (i = 0; i < 28; i++) {
        if (bd->points[i] < 0) {
            unsigned int numPieces = (unsigned int) (-bd->points[i]);
            for (j = 1; j <= numPieces; j++) {
                if (blend)
                    glEnable(GL_BLEND);
                drawPiece(bd3d->pieceList, bd3d, i, j, TRUE);
            }
        }
    }
    setMaterial(&prd->ChequerMat[1]);
    for (i = 0; i < 28; i++) {
        if (bd->points[i] > 0) {
            unsigned int numPieces = (unsigned int) bd->points[i];
            for (j = 1; j <= numPieces; j++) {
                if (blend)
                    glEnable(GL_BLEND);
                drawPiece(bd3d->pieceList, bd3d, i, j, TRUE);
            }
        }
    }
    if (blend)
        glDisable(GL_BLEND);

    if (bd->DragTargetHelp) {   /* highlight target points */
        glPolygonMode(GL_FRONT, GL_LINE);
        SetColour3d(0.f, 1.f, 0.f, 0.f);        /* Nice bright green... */

        for (i = 0; i <= 3; i++) {
            int target = bd->iTargetHelpPoints[i];
            if (target != -1) { /* Make sure texturing is disabled */
                if (prd->ChequerMat[0].pTexture)
                    glDisable(GL_TEXTURE_2D);
                drawPiece(bd3d->pieceList, bd3d, (unsigned int) target, Abs(bd->points[target]) + 1, TRUE);
            }
        }
        glPolygonMode(GL_FRONT, GL_FILL);
    }
}

static void
DrawNumbers(const BoardData * bd, int sides)
{
    int i;
    char num[3];
    float x;
    float textHeight = GetFontHeight3d(bd->bd3d->numberFont);
    int n;

    glPushMatrix();
    glTranslatef(0.f, (EDGE_HEIGHT - textHeight) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
    x = TRAY_WIDTH - PIECE_HOLE / 2.0f;

    for (i = 0; i < 12; i++) {
        x += PIECE_HOLE;
        if (i == 6)
            x += BAR_WIDTH;

        if ((i < 6 && (sides & 1)) || (i >= 6 && (sides & 2))) {
            glPushMatrix();
            glTranslatef(x, 0.f, 0.f);
            if (!fClockwise)
                n = 12 - i;
            else
                n = i + 1;

            if (bd->turn == -1 && bd->rd->fDynamicLabels)
                n = 25 - n;

            sprintf(num, "%d", n);
            glPrintPointNumbers(bd->bd3d->numberFont, num);
            glPopMatrix();
        }
    }

    glPopMatrix();
    glPushMatrix();
    glTranslatef(0.f, TOTAL_HEIGHT - textHeight - (EDGE_HEIGHT - textHeight) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
    x = TRAY_WIDTH - PIECE_HOLE / 2.0f;

    for (i = 0; i < 12; i++) {
        x += PIECE_HOLE;
        if (i == 6)
            x += BAR_WIDTH;
        if ((i < 6 && (sides & 1)) || (i >= 6 && (sides & 2))) {
            glPushMatrix();
            glTranslatef(x, 0.f, 0.f);
            if (!fClockwise)
                n = 13 + i;
            else
                n = 24 - i;

            if (bd->turn == -1 && bd->rd->fDynamicLabels)
                n = 25 - n;

            sprintf(num, "%d", n);
            glPrintPointNumbers(bd->bd3d->numberFont, num);
            glPopMatrix();
        }
    }
    glPopMatrix();
}

static void
drawNumbers(const BoardData * bd)
{
    /* No need to depth test as on top of board (depth test could cause alias problems too) */
    glDisable(GL_DEPTH_TEST);
    /* Draw inside then anti-aliased outline of numbers */
    setMaterial(&bd->rd->PointNumberMat);
    glNormal3f(0.f, 0.f, 1.f);

    glLineWidth(1.f);
    DrawNumbers(bd, 1);
    DrawNumbers(bd, 2);
    glEnable(GL_DEPTH_TEST);
}

static void
drawPoint(const renderdata * prd, float tuv, unsigned int i, int p, int outline)
{                               /* Draw point with correct texture co-ords */
    float w = PIECE_HOLE;
    float h = POINT_HEIGHT;
    float x, y;

    if (p) {
        x = TRAY_WIDTH - EDGE_WIDTH + PIECE_HOLE * i;
        y = -LIFT_OFF;
    } else {
        x = TRAY_WIDTH - EDGE_WIDTH + BOARD_WIDTH - (PIECE_HOLE * i);
        y = TOTAL_HEIGHT - EDGE_HEIGHT * 2 + LIFT_OFF;
        w = -w;
        h = -h;
    }

    glPushMatrix();
    if (prd->bgInTrays)
        glTranslatef(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH);
    else {
        x -= TRAY_WIDTH - EDGE_WIDTH;
        glTranslatef(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH);
    }

    if (prd->roundedPoints) {   /* Draw rounded point ends */
        float xoff;

        w = w * TAKI_WIDTH;
        y += w / 2.0f;
        h -= w / 2.0f;

        if (p)
            xoff = x + (PIECE_HOLE / 2.0f);
        else
            xoff = x - (PIECE_HOLE / 2.0f);

        /* Draw rounded semi-circle end of point (with correct texture co-ords) */
        {
            unsigned int j;
            float angle, step;
            float radius = w / 2.0f;

            step = (2 * (float) G_PI) / prd->curveAccuracy;
            angle = -step * (int) (prd->curveAccuracy / 4);
            glNormal3f(0.f, 0.f, 1.f);
            glBegin(outline ? GL_LINE_STRIP : GL_TRIANGLE_FAN);
            glTexCoord2f(xoff * tuv, y * tuv);

            glVertex3f(xoff, y, 0.f);
            for (j = 0; j <= prd->curveAccuracy / 2; j++) {
                glTexCoord2f((xoff + sinf(angle) * radius) * tuv, (y + cosf(angle) * radius) * tuv);
                glVertex3f(xoff + sinf(angle) * radius, y + cosf(angle) * radius, 0.f);
                angle -= step;
            }
            glEnd();
        }
        /* Move rest of point in slighlty */
        if (p)
            x -= -((PIECE_HOLE * (1 - TAKI_WIDTH)) / 2.0f);
        else
            x -= ((PIECE_HOLE * (1 - TAKI_WIDTH)) / 2.0f);
    }

    glBegin(outline ? GL_LINE_STRIP : GL_TRIANGLES);
    glNormal3f(0.f, 0.f, 1.f);
    glTexCoord2f((x + w) * tuv, y * tuv);
    glVertex3f(x + w, y, 0.f);
    glTexCoord2f((x + w / 2) * tuv, (y + h) * tuv);
    glVertex3f(x + w / 2, y + h, 0.f);
    glTexCoord2f(x * tuv, y * tuv);
    glVertex3f(x, y, 0.f);
    glEnd();

    glPopMatrix();
}

static void
drawPoints(const renderdata * prd)
{
    /* texture unit value */
    float tuv;

    /* Don't worry about depth testing (but set depth values) */
    glDepthFunc(GL_ALWAYS);

    setMaterial(&prd->BaseMat);
    if (prd->bgInTrays)
        drawChequeredRect(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BOARD_WIDTH + TRAY_WIDTH - EDGE_WIDTH,
                          TOTAL_HEIGHT - EDGE_HEIGHT * 2, prd->acrossCheq, prd->downCheq, prd->BaseMat.pTexture);
    else
        drawChequeredRect(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2,
                          prd->acrossCheq, prd->downCheq, prd->BaseMat.pTexture);

    /* Ignore depth values when drawing points */
    glDepthMask(GL_FALSE);

    if (prd->PointMat[0].pTexture)
        tuv = (TEXTURE_SCALE) / prd->PointMat[0].pTexture->width;
    else
        tuv = 0;
    setMaterial(&prd->PointMat[0]);
    drawPoint(prd, tuv, 0, 0, 0);
    drawPoint(prd, tuv, 0, 1, 0);
    drawPoint(prd, tuv, 2, 0, 0);
    drawPoint(prd, tuv, 2, 1, 0);
    drawPoint(prd, tuv, 4, 0, 0);
    drawPoint(prd, tuv, 4, 1, 0);

    glLineWidth(1.f);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);

    drawPoint(prd, tuv, 0, 0, 1);
    drawPoint(prd, tuv, 0, 1, 1);
    drawPoint(prd, tuv, 2, 0, 1);
    drawPoint(prd, tuv, 2, 1, 1);
    drawPoint(prd, tuv, 4, 0, 1);
    drawPoint(prd, tuv, 4, 1, 1);

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);

    if (prd->PointMat[1].pTexture)
        tuv = (TEXTURE_SCALE) / prd->PointMat[1].pTexture->width;
    else
        tuv = 0;
    setMaterial(&prd->PointMat[1]);
    drawPoint(prd, tuv, 1, 0, 0);
    drawPoint(prd, tuv, 1, 1, 0);
    drawPoint(prd, tuv, 3, 0, 0);
    drawPoint(prd, tuv, 3, 1, 0);
    drawPoint(prd, tuv, 5, 0, 0);
    drawPoint(prd, tuv, 5, 1, 0);

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);

    drawPoint(prd, tuv, 1, 0, 1);
    drawPoint(prd, tuv, 1, 1, 1);
    drawPoint(prd, tuv, 3, 0, 1);
    drawPoint(prd, tuv, 3, 1, 1);
    drawPoint(prd, tuv, 5, 0, 1);
    drawPoint(prd, tuv, 5, 1, 1);

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);

    /* Restore depth buffer settings */
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
}

static void
drawBase(const renderdata * prd, int sides)
{
    if (sides & 1)
        drawPoints(prd);

    if (sides & 2) {
        /* Rotate right board around */
        glPushMatrix();
        glTranslatef(TOTAL_WIDTH, TOTAL_HEIGHT, 0.f);
        glRotatef(180.f, 0.f, 0.f, 1.f);
        drawPoints(prd);
        glPopMatrix();
    }
}

static void
drawHinge(const BoardData3d * bd3d, const renderdata * prd, float height)
{
    setMaterial(&prd->HingeMat);

    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glScalef(1.f, HINGE_SEGMENTS, 1.f);
    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
    glTranslatef((TOTAL_WIDTH) / 2.0f, height, BASE_DEPTH + EDGE_DEPTH);
    glRotatef(-90.f, 1.f, 0.f, 0.f);
    gluCylinder(bd3d->qobjTex, (double) HINGE_WIDTH, (double) HINGE_WIDTH, (double) HINGE_HEIGHT,
                (GLint) prd->curveAccuracy, 1);

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glRotatef(180.f, 1.f, 0.f, 0.f);
    gluDisk(bd3d->qobjTex, 0., (double) HINGE_WIDTH, (GLint) prd->curveAccuracy, 1);

    glPopMatrix();
}

NTH_STATIC void
tidyEdges(const renderdata * prd)
{                               /* Anti-alias board edges */
    setMaterial(&prd->BoxMat);

    glLineWidth(1.f);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glDepthMask(GL_FALSE);

    glNormal3f(0.f, 0.f, 1.f);

    glBegin(GL_LINES);
    if (prd->roundedEdges) {
        /* bar */
        glNormal3f(-1.f, 0.f, 0.f);
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        glNormal3f(1.f, 0.f, 0.f);
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT,
                   BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        /* left bear off tray */
        glNormal3f(-1.f, 0.f, 0.f);
        glVertex3f(0.f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(0.f, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        /* right bear off tray */
        glNormal3f(1.f, 0.f, 0.f);
        glVertex3f(TOTAL_WIDTH, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TOTAL_WIDTH, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT,
                   BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT,
                   BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

    } else {
        /* bar */
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        /* left bear off tray */
        glVertex3f(0.f, 0.f, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(0.f, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        /* right bear off tray */
        glVertex3f(TOTAL_WIDTH, 0.f, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
        glVertex3f(TOTAL_WIDTH - TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
    }

    /* inner edges (sides) */
    glNormal3f(1.f, 0.f, 0.f);
    glVertex3f(EDGE_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glVertex3f(EDGE_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glVertex3f(TRAY_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glVertex3f(TRAY_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);

    glNormal3f(-1.f, 0.f, 0.f);
    glVertex3f(TOTAL_WIDTH - EDGE_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glVertex3f(TOTAL_WIDTH - EDGE_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

static void
getMoveIndicatorPos(const BoardData * bd, float pos[3])
{
    if (!fClockwise)
        pos[0] = TOTAL_WIDTH - TRAY_WIDTH + ARROW_SIZE / 2.0f;
    else
        pos[0] = TRAY_WIDTH - ARROW_SIZE / 2.0f;

    if (bd->turn == 1)
        pos[1] = EDGE_HEIGHT / 2.0f;
    else
        pos[1] = TOTAL_HEIGHT - EDGE_HEIGHT / 2.0f;

    pos[2] = BASE_DEPTH + EDGE_DEPTH;
}

static void
showMoveIndicator(const BoardData * bd)
{
    float pos[3];
    /* ARROW_UNIT used to draw sub-bits of arrow */
#define ARROW_UNIT (ARROW_SIZE / 4.0f)

    setMaterial(&bd->rd->ChequerMat[(bd->turn == 1) ? 1 : 0]);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glNormal3f(0.f, 0.f, 1.f);

    glPushMatrix();
    getMoveIndicatorPos(bd, pos);
    glTranslatef(pos[0], pos[1], pos[2]);
    if (fClockwise)
        glRotatef(180.f, 0.f, 0.f, 1.f);

    glBegin(GL_QUADS);
    glVertex2f(-ARROW_UNIT * 2, -ARROW_UNIT);
    glVertex2f(LIFT_OFF, -ARROW_UNIT);
    glVertex2f(LIFT_OFF, ARROW_UNIT);
    glVertex2f(-ARROW_UNIT * 2, ARROW_UNIT);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex2f(0.f, ARROW_UNIT * 2);
    glVertex2f(0.f, -ARROW_UNIT * 2);
    glVertex2f(ARROW_UNIT * 2, 0.f);
    glEnd();

    /* Outline arrow */
    SetColour3d(0.f, 0.f, 0.f, 1.f);    /* Black outline */

    glLineWidth(.5f);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);

    glBegin(GL_LINE_LOOP);
    glVertex2f(-ARROW_UNIT * 2, -ARROW_UNIT);
    glVertex2f(-ARROW_UNIT * 2, ARROW_UNIT);
    glVertex2f(0.f, ARROW_UNIT);
    glVertex2f(0.f, ARROW_UNIT * 2);
    glVertex2f(ARROW_UNIT * 2, 0.f);
    glVertex2f(0.f, -ARROW_UNIT * 2);
    glVertex2f(0.f, -ARROW_UNIT);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);

    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

static void
ClearScreen(const renderdata * prd)
{
    glClearColor(prd->BackGroundMat.ambientColour[0], prd->BackGroundMat.ambientColour[1],
                 prd->BackGroundMat.ambientColour[2], 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void
RotateClosingBoard(const BoardData3d * bd3d, const renderdata * prd)
{
    float rotAngle = 90;
    float trans = getBoardHeight() * .4f;
    float zoom = .2f;

    glPopMatrix();

    ClearScreen(prd);

    if ((bd3d->State == BOARD_OPENING) || (bd3d->State == BOARD_CLOSING)) {
        rotAngle *= bd3d->perOpen;
        trans *= bd3d->perOpen;
        zoom *= bd3d->perOpen;
    }
    glPushMatrix();
    glTranslatef(0.f, -trans, zoom);
    glTranslatef(getBoardWidth() / 2.0f, getBoardHeight() / 2.0f, 0.f);
    glRotatef(rotAngle, 0.f, 0.f, 1.f);
    glTranslatef(-getBoardWidth() / 2.0f, -getBoardHeight() / 2.0f, 0.f);
}

/* Macros to make texture specification easier */
#define M_X(x, y, z) if (tuv) glTexCoord2f((z) * tuv, (y) * tuv); glVertex3f(x, y, z);
#define M_Z(x, y, z) if (tuv) glTexCoord2f((x) * tuv, (y) * tuv); glVertex3f(x, y, z);

#define DrawBottom(x, y, z, w, h)\
	glNormal3f(0.f, -1.f, 0.f);\
	glBegin(GL_QUADS);\
	if (tuv) glTexCoord2f((x) * tuv, ((y) + BOARD_FILLET - curveTextOff - (h)) * tuv);\
	glVertex3f(x, y, z);\
	if (tuv) glTexCoord2f(((x) + (w)) * tuv, ((y) + BOARD_FILLET - curveTextOff - (h)) * tuv);\
	glVertex3f((x) + (w), y, z);\
	if (tuv) glTexCoord2f(((x) + (w)) * tuv, ((y) + BOARD_FILLET - curveTextOff) * tuv);\
	glVertex3f((x) + (w), y, z + (h));\
	if (tuv) glTexCoord2f((x) * tuv, ((y) + BOARD_FILLET - curveTextOff) * tuv);\
	glVertex3f(x, y, z + (h));\
	glEnd();

#define DrawTop(x, y, z, w, h)\
	glNormal3f(0.f, 1.f, 0.f);\
	glBegin(GL_QUADS);\
	if (tuv) glTexCoord2f((x) * tuv, ((y) - (BOARD_FILLET - curveTextOff) + (h)) * tuv);\
	glVertex3f(x, y, z);\
	if (tuv) glTexCoord2f((x) * tuv, ((y) - (BOARD_FILLET - curveTextOff)) * tuv);\
	glVertex3f(x, y, z + (h));\
	if (tuv) glTexCoord2f(((x) + (w)) * tuv, ((y) - (BOARD_FILLET - curveTextOff)) * tuv);\
	glVertex3f((x) + (w), y, z + (h));\
	if (tuv) glTexCoord2f(((x) + (w)) * tuv, ((y) - (BOARD_FILLET - curveTextOff) + (h)) * tuv);\
	glVertex3f((x) + (w), y, z);\
	glEnd();

#define DrawLeft(x, y, z, w, h)\
	glNormal3f(-1.f, 0.f, 0.f);\
	glBegin(GL_QUADS);\
	if (tuv) glTexCoord2f(((x) + BOARD_FILLET - curveTextOff - (h)) * tuv, (y) * tuv);\
	glVertex3f(x, y, z);\
	if (tuv) glTexCoord2f(((x) + BOARD_FILLET - curveTextOff) * tuv, (y) * tuv);\
	glVertex3f(x, y, (z) + (h));\
	if (tuv) glTexCoord2f(((x) + BOARD_FILLET - curveTextOff) * tuv, ((y) + (w)) * tuv);\
	glVertex3f(x, (y) + (w), (z) + (h));\
	if (tuv) glTexCoord2f(((x) + BOARD_FILLET - curveTextOff - (h)) * tuv, ((y) + (w)) * tuv);\
	glVertex3f(x, (y) + (w), z);\
	glEnd();

#define DrawRight(x, y, z, w, h)\
	glNormal3f(1.f, 0.f, 0.f);\
	glBegin(GL_QUADS);\
	if (tuv) glTexCoord2f(((x) - (BOARD_FILLET - curveTextOff) + (h)) * tuv, (y) * tuv);\
	glVertex3f(x, y, z);\
	if (tuv) glTexCoord2f(((x) - (BOARD_FILLET - curveTextOff) + (h)) * tuv, ((y) + (w)) * tuv);\
	glVertex3f(x, (y) + (w), z);\
	if (tuv) glTexCoord2f(((x) - (BOARD_FILLET - curveTextOff)) * tuv, ((y) + (w)) * tuv);\
	glVertex3f(x, (y) + (w), z + (h));\
	if (tuv) glTexCoord2f(((x) - (BOARD_FILLET - curveTextOff)) * tuv, (y) * tuv);\
	glVertex3f(x, y, z + (h));\
	glEnd();

#define TextureOffset(s, t) if (tuv)\
{\
	glMatrixMode(GL_TEXTURE);\
	glPushMatrix();\
	glTranslatef(s, t, 0.f);\
	glMatrixMode(GL_MODELVIEW);\
}

#define TextureReset if (tuv)\
{\
	glMatrixMode(GL_TEXTURE);\
	glPopMatrix();\
	glMatrixMode(GL_MODELVIEW);\
}

/* texture unit value */
static float tuv;

static void
InsideFillet(float x, float y, float z, float w, float h, float radius, unsigned int accuracy, const Texture * texture,
             float curveTextOff)
{
    /* Left */
    DrawRight(x + BOARD_FILLET, y + BOARD_FILLET, BASE_DEPTH, h, EDGE_DEPTH - BOARD_FILLET);
    /* Top */
    DrawBottom(x + BOARD_FILLET, y + h + BOARD_FILLET, BASE_DEPTH, w, EDGE_DEPTH - BOARD_FILLET + LIFT_OFF);
    /* Bottom */
    DrawTop(x + BOARD_FILLET, y + BOARD_FILLET, BASE_DEPTH, w, EDGE_DEPTH - BOARD_FILLET)
        /* Right */
        DrawLeft(x + w + BOARD_FILLET, y + BOARD_FILLET, BASE_DEPTH + LIFT_OFF, h, EDGE_DEPTH - BOARD_FILLET);

    if (tuv) {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glTranslatef((x + curveTextOff) * tuv, (y + h + radius * 2) * tuv, 0.f);
        glRotatef(-90.f, 0.f, 0.f, 1.f);
        glMatrixMode(GL_MODELVIEW);
    }
    glPushMatrix();
    glTranslatef(x, y + h + radius * 2, z - radius);
    glRotatef(-180.f, 1.f, 0.f, 0.f);
    glRotatef(90.f, 0.f, 1.f, 0.f);
    QuarterCylinderSplayed(radius, h + radius * 2, accuracy, texture);
    glPopMatrix();
    TextureReset TextureOffset(x * tuv, (y + curveTextOff) * tuv);
    glPushMatrix();
    glTranslatef(x, y, z - radius);
    glRotatef(-90.f, 1.f, 0.f, 0.f);
    glRotatef(-90.f, 0.f, 0.f, 1.f);
    QuarterCylinderSplayed(radius, w + radius * 2, accuracy, texture);
    glPopMatrix();
    TextureReset if (tuv) {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glTranslatef((x + w + radius * 2 - curveTextOff) * tuv, y * tuv, 0.f);
        glRotatef(90.f, 0.f, 0.f, 1.f);
        glMatrixMode(GL_MODELVIEW);
    }
    glPushMatrix();
    glTranslatef(x + w + radius * 2, y, z - radius);
    glRotatef(-90.f, 0.f, 1.f, 0.f);
    QuarterCylinderSplayed(radius, h + radius * 2, accuracy, texture);
    glPopMatrix();
    TextureReset TextureOffset((x + radius) * tuv, (y + h + radius * 2) * tuv);
    glPushMatrix();
    glTranslatef(x + radius, y + h + radius * 2, z - radius);
    glRotatef(-90.f, 0.f, 0.f, 1.f);
    QuarterCylinderSplayedRev(radius, w, accuracy, texture);
    glPopMatrix();
TextureReset}

NTH_STATIC void
drawTable(const BoardData3d * bd3d, const renderdata * prd)
{
    float st, ct, dInc, curveTextOff = 0;
    tuv = 0;

    if (bd3d->State != BOARD_OPEN)
        RotateClosingBoard(bd3d, prd);

    /* Draw background */
    setMaterial(&prd->BackGroundMat);

    /* Set depth and default pixel values (as background covers entire screen) */
    glDepthFunc(GL_ALWAYS);
    drawRect(bd3d->backGroundPos[0], bd3d->backGroundPos[1], 0.f, bd3d->backGroundSize[0], bd3d->backGroundSize[1],
             prd->BackGroundMat.pTexture);
    glDepthFunc(GL_LEQUAL);

    /* Right side of board */
    if (!prd->fHinges3d)
        drawBase(prd, 1 | 2);
    else
        drawBase(prd, 2);
    setMaterial(&prd->BoxMat);

    if (!prd->bgInTrays)
        drawSplitRect(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE + LIFT_OFF,
                      TOTAL_HEIGHT - EDGE_HEIGHT * 2, prd->BoxMat.pTexture);

    if (prd->roundedEdges) {
        if (prd->BoxMat.pTexture) {
            tuv = (TEXTURE_SCALE) / prd->BoxMat.pTexture->width;
            st = sinf((2 * G_PI) / prd->curveAccuracy) * BOARD_FILLET;
            ct = (cosf((2 * G_PI) / prd->curveAccuracy) - 1) * BOARD_FILLET;
            dInc = sqrtf(st * st + ct * ct);
            curveTextOff = (int) (prd->curveAccuracy / 4) * dInc;
        }

        /* Right edge */
        glBegin(GL_QUADS);
        /* Front Face */
        glNormal3f(0.f, 0.f, 1.f);

        M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - BOARD_FILLET + LIFT_OFF, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);

        M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        glEnd();

        /* Right face */
        DrawRight(TOTAL_WIDTH, BOARD_FILLET, 0.f, TOTAL_HEIGHT - BOARD_FILLET * 2,
                  BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        if (tuv) {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glTranslatef((TOTAL_WIDTH - BOARD_FILLET) * tuv,
                         (BOARD_FILLET - curveTextOff - (BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET)) * tuv, 0.f);
            glRotatef(90.f, 0.f, 0.f, 1.f);
            glMatrixMode(GL_MODELVIEW);
        }
        glPushMatrix();
        glTranslatef(TOTAL_WIDTH - BOARD_FILLET, BOARD_FILLET, 0.f);
        glRotatef(90.f, 1.f, 0.f, 0.f);
        QuarterCylinder(BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET, prd->curveAccuracy, prd->BoxMat.pTexture);
        glPopMatrix();
        TextureReset if (tuv) {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glTranslatef((TOTAL_WIDTH - BOARD_FILLET) * tuv, (TOTAL_HEIGHT - (BOARD_FILLET - curveTextOff)) * tuv, 0.f);
            glRotatef(90.f, 0.f, 0.f, 1.f);
            glMatrixMode(GL_MODELVIEW);
        }
        glPushMatrix();
        glTranslatef(TOTAL_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(-90.f, 1.f, 0.f, 0.f);
        QuarterCylinder(BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET, prd->curveAccuracy, prd->BoxMat.pTexture);
        glPopMatrix();
        TextureReset if (tuv) {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glTranslatef((TOTAL_WIDTH - BOARD_FILLET + curveTextOff) * tuv, (TOTAL_HEIGHT - BOARD_FILLET) * tuv, 0.f);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            glMatrixMode(GL_MODELVIEW);
        }
        glPushMatrix();
        glTranslatef(TOTAL_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(-180.f, 1.f, 0.f, 0.f);
        glRotatef(90.f, 0.f, 1.f, 0.f);
        QuarterCylinder(BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET * 2, prd->curveAccuracy, prd->BoxMat.pTexture);
        glPopMatrix();
        TextureReset glPushMatrix();
        glTranslatef(TOTAL_WIDTH - BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(-90.f, 0.f, 0.f, 1.f);
        drawCornerEigth(bd3d->boardPoints, BOARD_FILLET, prd->curveAccuracy);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(TOTAL_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        drawCornerEigth(bd3d->boardPoints, BOARD_FILLET, prd->curveAccuracy);
        glPopMatrix();

        /* Top + bottom edges */
        if (!prd->fHinges3d) {
            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(EDGE_WIDTH - BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH / 2.0f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH / 2.0f, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(EDGE_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);

            M_Z(TOTAL_WIDTH / 2.0f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH / 2.0f, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            glEnd();

            DrawBottom(BOARD_FILLET, 0.f, 0.f, TOTAL_WIDTH - BOARD_FILLET * 2, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(EDGE_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH / 2.0f, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(EDGE_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);

            M_Z(TOTAL_WIDTH / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH / 2.0f, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            glEnd();
            /* Top Face */
            DrawTop(BOARD_FILLET, TOTAL_HEIGHT, 0.f, TOTAL_WIDTH - BOARD_FILLET * 2,
                    BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

            TextureOffset(BOARD_FILLET * tuv, BOARD_FILLET * tuv);
            glPushMatrix();
            glTranslatef(BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            QuarterCylinder(BOARD_FILLET, TOTAL_WIDTH - BOARD_FILLET * 2, prd->curveAccuracy, prd->BoxMat.pTexture);
            glPopMatrix();
            TextureReset TextureOffset(BOARD_FILLET * tuv, (TOTAL_HEIGHT - (BOARD_FILLET - curveTextOff)) * tuv);
            glPushMatrix();
            glTranslatef(BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
            glRotatef(-90.f, 1.f, 0.f, 0.f);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            QuarterCylinder(BOARD_FILLET, TOTAL_WIDTH - BOARD_FILLET * 2, prd->curveAccuracy, prd->BoxMat.pTexture);
            glPopMatrix();
        TextureReset} else {
            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z((TOTAL_WIDTH + HINGE_GAP) / 2.0f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z((TOTAL_WIDTH + HINGE_GAP) / 2.0f, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            glEnd();

            DrawBottom((TOTAL_WIDTH + HINGE_GAP) / 2.0f, 0.f, 0.f, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET,
                       BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z((TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z((TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            glEnd();
            /* Top Face */
            DrawTop((TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT, 0.f,
                    (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET)

                glBegin(GL_QUADS);

            if (bd3d->State != BOARD_OPEN) {
                /* Cover up back when closing */
                glNormal3f(-1.f, 0.f, 0.f);
                M_X(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, 0.f, 0.f);
                M_X(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, 0.f, BASE_DEPTH + EDGE_DEPTH);
                M_X(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
                M_X(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT, 0.f);
            }
            glEnd();

            TextureOffset(((TOTAL_WIDTH + HINGE_GAP) / 2.0f) * tuv, BOARD_FILLET * tuv);
            glPushMatrix();
            glTranslatef((TOTAL_WIDTH + HINGE_GAP) / 2.0f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            QuarterCylinder(BOARD_FILLET, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET, prd->curveAccuracy,
                            prd->BoxMat.pTexture);
            glPopMatrix();
            TextureReset
                TextureOffset(((TOTAL_WIDTH + HINGE_GAP) / 2.0f) * tuv,
                              (TOTAL_HEIGHT - (BOARD_FILLET - curveTextOff)) * tuv);
            glPushMatrix();
            glTranslatef((TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - BOARD_FILLET,
                         BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
            glRotatef(-90.f, 1.f, 0.f, 0.f);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            QuarterCylinder(BOARD_FILLET, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET, prd->curveAccuracy,
                            prd->BoxMat.pTexture);
            glPopMatrix();
        TextureReset}
        /* Bar */
        if (!prd->fHinges3d) {
            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);

            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF,
                TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            glEnd();
        } else {
            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, EDGE_HEIGHT - BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);

            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET + LIFT_OFF,
                TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            glEnd();
        }
        /* Bear-off edge */
        glBegin(GL_QUADS);
        /* Front Face */
        glNormal3f(0.f, 0.f, 1.f);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + BOARD_FILLET - LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);

        M_Z(TOTAL_WIDTH - TRAY_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        glEnd();

        glBegin(GL_QUADS);
        /* Front Face */
        glNormal3f(0.f, 0.f, 1.f);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - LIFT_OFF - BOARD_FILLET, TRAY_HEIGHT + BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, TRAY_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - EDGE_WIDTH + BOARD_FILLET, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - LIFT_OFF - BOARD_FILLET,
            TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        glEnd();

        InsideFillet(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET,
                     BASE_DEPTH + EDGE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, BOARD_FILLET,
                     prd->curveAccuracy, prd->BoxMat.pTexture, curveTextOff);
        InsideFillet(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET,
                     TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH,
                     TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, BOARD_FILLET, prd->curveAccuracy,
                     prd->BoxMat.pTexture, curveTextOff);

        InsideFillet(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET,
                     BASE_DEPTH + EDGE_DEPTH, BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, BOARD_FILLET,
                     prd->curveAccuracy, prd->BoxMat.pTexture, curveTextOff);
    } else {
        /* Right edge */
        drawBox(BOX_SPLITTOP, TOTAL_WIDTH - EDGE_WIDTH, 0.f, 0.f, EDGE_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH,
                prd->BoxMat.pTexture);

        /* Top + bottom edges */
        if (!prd->fHinges3d) {
            drawBox(BOX_NOSIDES | BOX_SPLITWIDTH, EDGE_WIDTH, 0.f, 0.f, TOTAL_WIDTH - EDGE_WIDTH * 2, EDGE_HEIGHT,
                    BASE_DEPTH + EDGE_DEPTH, prd->BoxMat.pTexture);
            drawBox(BOX_NOSIDES | BOX_SPLITWIDTH, EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, 0.f,
                    TOTAL_WIDTH - EDGE_WIDTH * 2, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, prd->BoxMat.pTexture);
        } else {
            drawBox(BOX_ALL, (TOTAL_WIDTH + HINGE_GAP) / 2.0f, 0.f, 0.f, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH,
                    EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, prd->BoxMat.pTexture);
            drawBox(BOX_ALL, (TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT, 0.f,
                    (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH,
                    prd->BoxMat.pTexture);
        }

        /* Bar */
        if (!prd->fHinges3d)
            drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BAR_WIDTH,
                    TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH, prd->BoxMat.pTexture);
        else
            drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, EDGE_HEIGHT,
                    0.f, (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT * 2, BASE_DEPTH + EDGE_DEPTH,
                    prd->BoxMat.pTexture);

        /* Bear-off edge */
        drawBox(BOX_NOENDS | BOX_SPLITTOP, TOTAL_WIDTH - TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH, EDGE_WIDTH,
                TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH, prd->BoxMat.pTexture);
        drawBox(BOX_NOSIDES, TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - LIFT_OFF, TRAY_HEIGHT, BASE_DEPTH,
                TRAY_WIDTH - EDGE_WIDTH * 2 + LIFT_OFF * 2, MID_SIDE_GAP_HEIGHT, EDGE_DEPTH, prd->BoxMat.pTexture);
    }

    /* Left side of board */
    glPushMatrix();

    if (bd3d->State != BOARD_OPEN) {
        float boardAngle = 180;
        if ((bd3d->State == BOARD_OPENING) || (bd3d->State == BOARD_CLOSING))
            boardAngle *= bd3d->perOpen;

        glTranslatef(TOTAL_WIDTH / 2.0f, 0.f, BASE_DEPTH + EDGE_DEPTH);
        glRotatef(boardAngle, 0.f, 1.f, 0.f);
        glTranslatef(-TOTAL_WIDTH / 2.0f, 0.f, -(BASE_DEPTH + EDGE_DEPTH));
    }

    if (prd->fHinges3d)
        drawBase(prd, 1);

    setMaterial(&prd->BoxMat);

    if (!prd->bgInTrays)
        drawSplitRect(EDGE_WIDTH - LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE, TOTAL_HEIGHT - EDGE_HEIGHT * 2,
                      prd->BoxMat.pTexture);

    if (bd3d->State != BOARD_OPEN) {    /* Back of board */
        float logoSize = (TOTAL_WIDTH * .3f) / 2.0f;
        drawRect(TOTAL_WIDTH / 2.0f, 0.f, 0.f, -(TOTAL_WIDTH / 2.0f), TOTAL_HEIGHT, prd->BoxMat.pTexture);
        /* logo */
        glPushMatrix();
        glTranslatef(TOTAL_WIDTH / 4.0f, TOTAL_HEIGHT / 2.0f, -LIFT_OFF);
        glRotatef(90.f, 0.f, 0.f, 1.f);
        setMaterial(&bd3d->logoMat);
        glNormal3f(0.f, 0.f, 1.f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.f, 0.f);
        glVertex3f(logoSize, -logoSize, 0.f);
        glTexCoord2f(1.f, 0.f);
        glVertex3f(-logoSize, -logoSize, 0.f);
        glTexCoord2f(1.f, 1.f);
        glVertex3f(-logoSize, logoSize, 0.f);
        glTexCoord2f(0.f, 1.f);
        glVertex3f(logoSize, logoSize, 0.f);
        glEnd();
        glPopMatrix();
        setMaterial(&prd->BoxMat);
    }

    if (prd->roundedEdges) {
        /* Left edge */
        glBegin(GL_QUADS);
        /* Front Face */
        glNormal3f(0.f, 0.f, 1.f);
        M_Z(BOARD_FILLET - LIFT_OFF, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);

        M_Z(BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(EDGE_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        glEnd();
        /* Left Face */
        DrawLeft(0.f, BOARD_FILLET, 0.f, TOTAL_HEIGHT - BOARD_FILLET * 2, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

        if (tuv) {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glTranslatef(BOARD_FILLET * tuv, (BOARD_FILLET - curveTextOff) * tuv, 0.f);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            glMatrixMode(GL_MODELVIEW);
        }
        glPushMatrix();
        glTranslatef(BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(-90.f, 1.f, 0.f, 0.f);
        glRotatef(180.f, 0.f, 1.f, 0.f);
        QuarterCylinder(BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET, prd->curveAccuracy, prd->BoxMat.pTexture);
        glPopMatrix();
        TextureReset if (tuv) {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glTranslatef((BOARD_FILLET - curveTextOff) * tuv, (TOTAL_HEIGHT - (BOARD_FILLET - curveTextOff)) * tuv,
                         0.f);
            glRotatef(90.f, 0.f, 0.f, 1.f);
            glMatrixMode(GL_MODELVIEW);
        }
        glPushMatrix();
        glTranslatef(BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(-90.f, 1.f, 0.f, 0.f);
        glRotatef(-90.f, 0.f, 1.f, 0.f);
        QuarterCylinder(BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET, prd->curveAccuracy, prd->BoxMat.pTexture);
        glPopMatrix();
        TextureReset if (tuv) {
            glMatrixMode(GL_TEXTURE);
            glPushMatrix();
            glTranslatef((BOARD_FILLET - curveTextOff) * tuv, BOARD_FILLET * tuv, 0.f);
            glRotatef(90.f, 0.f, 0.f, 1.f);
        }
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(-90.f, 0.f, 1.f, 0.f);
        QuarterCylinder(BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET * 2, prd->curveAccuracy, prd->BoxMat.pTexture);
        glPopMatrix();
        TextureReset glPushMatrix();
        glTranslatef(BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(180.f, 0.f, 0.f, 1.f);
        drawCornerEigth(bd3d->boardPoints, BOARD_FILLET, prd->curveAccuracy);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
        glRotatef(90.f, 0.f, 0.f, 1.f);
        drawCornerEigth(bd3d->boardPoints, BOARD_FILLET, prd->curveAccuracy);
        glPopMatrix();

        if (prd->fHinges3d) {   /* Top + bottom edges and bar */
            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(EDGE_WIDTH - BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z((TOTAL_WIDTH - HINGE_GAP) / 2.0f, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z((TOTAL_WIDTH - HINGE_GAP) / 2.0f, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(EDGE_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            glEnd();

            DrawBottom(BOARD_FILLET, 0.f, 0.f, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET,
                       BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);

            glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(EDGE_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z((TOTAL_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z((TOTAL_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            M_Z(EDGE_WIDTH - BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
            /* Top Face */
            glEnd();

            DrawTop(BOARD_FILLET, TOTAL_HEIGHT, 0.f, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET,
                    BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET)

                glBegin(GL_QUADS);
            if (bd3d->State != BOARD_OPEN) {
                /* Cover up back when closing */
                glNormal3f(1.f, 0.f, 0.f);
                M_X((TOTAL_WIDTH - HINGE_GAP) / 2.0f, 0.f, 0.f);
                M_X((TOTAL_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT, 0.f);
                M_X((TOTAL_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
                M_X((TOTAL_WIDTH - HINGE_GAP) / 2.0f, 0.f, BASE_DEPTH + EDGE_DEPTH);
            }
            glEnd();

            TextureOffset(BOARD_FILLET * tuv, BOARD_FILLET * tuv);
            glPushMatrix();
            glTranslatef(BOARD_FILLET, BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            QuarterCylinder(BOARD_FILLET, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET, prd->curveAccuracy,
                            prd->BoxMat.pTexture);
            glPopMatrix();
            TextureReset TextureOffset(BOARD_FILLET * tuv, (TOTAL_HEIGHT - (BOARD_FILLET - curveTextOff)) * tuv);
            glPushMatrix();
            glTranslatef(BOARD_FILLET, TOTAL_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH - BOARD_FILLET);
            glRotatef(-90.f, 1.f, 0.f, 0.f);
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            QuarterCylinder(BOARD_FILLET, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - BOARD_FILLET, prd->curveAccuracy,
                            prd->BoxMat.pTexture);
            glPopMatrix();
            TextureReset glBegin(GL_QUADS);
            /* Front Face */
            glNormal3f(0.f, 0.f, 1.f);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH - HINGE_GAP) / 2.0f, EDGE_HEIGHT - BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);

            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT / 2.0f,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            M_Z(TRAY_WIDTH + BOARD_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
                BASE_DEPTH + EDGE_DEPTH);
            glEnd();
        }

        /* Bear-off edge */
        glBegin(GL_QUADS);
        /* Front Face */
        glNormal3f(0.f, 0.f, 1.f);
        M_Z(TRAY_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - BOARD_FILLET + LIFT_OFF, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);

        M_Z(TRAY_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT / 2.0f, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - BOARD_FILLET + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - EDGE_WIDTH + BOARD_FILLET - LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT + BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        glEnd();

        glBegin(GL_QUADS);
        /* Front Face */
        glNormal3f(0.f, 0.f, 1.f);
        M_Z(EDGE_WIDTH - LIFT_OFF - BOARD_FILLET, TRAY_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - EDGE_WIDTH + BOARD_FILLET + LIFT_OFF * 2, TRAY_HEIGHT + BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH);
        M_Z(TRAY_WIDTH - EDGE_WIDTH + BOARD_FILLET + LIFT_OFF * 2, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        M_Z(EDGE_WIDTH - LIFT_OFF - BOARD_FILLET, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET,
            BASE_DEPTH + EDGE_DEPTH);
        glEnd();

        InsideFillet(EDGE_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH,
                     TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, BOARD_FILLET, prd->curveAccuracy,
                     prd->BoxMat.pTexture, curveTextOff);
        InsideFillet(EDGE_WIDTH - BOARD_FILLET, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET,
                     BASE_DEPTH + EDGE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, BOARD_FILLET,
                     prd->curveAccuracy, prd->BoxMat.pTexture, curveTextOff);

        InsideFillet(TRAY_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH + EDGE_DEPTH, BOARD_WIDTH,
                     TOTAL_HEIGHT - EDGE_HEIGHT * 2, BOARD_FILLET, prd->curveAccuracy, prd->BoxMat.pTexture,
                     curveTextOff);
    } else {
        /* Left edge */
        drawBox(BOX_SPLITTOP, 0.f, 0.f, 0.f, EDGE_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH, prd->BoxMat.pTexture);

        if (prd->fHinges3d) {   /* Top + bottom edges and bar */
            drawBox(BOX_ALL, EDGE_WIDTH, 0.f, 0.f, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH, EDGE_HEIGHT,
                    BASE_DEPTH + EDGE_DEPTH, prd->BoxMat.pTexture);
            drawBox(BOX_ALL, EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, 0.f, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH,
                    EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, prd->BoxMat.pTexture);
        }

        if (prd->fHinges3d)
            drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, 0.f,
                    (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT * 2, BASE_DEPTH + EDGE_DEPTH,
                    prd->BoxMat.pTexture);

        /* Bear-off edge */
        drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, EDGE_WIDTH,
                TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH, prd->BoxMat.pTexture);
        drawBox(BOX_NOSIDES, EDGE_WIDTH - LIFT_OFF, TRAY_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2 + LIFT_OFF * 2,
                MID_SIDE_GAP_HEIGHT, EDGE_DEPTH, prd->BoxMat.pTexture);
    }

    if (prd->fHinges3d) {
        drawHinge(bd3d, prd, ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT) / 2.0f);
        drawHinge(bd3d, prd, ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT + TOTAL_HEIGHT) / 2.0f);

        if (bd3d->State == BOARD_OPEN) {        /* Shadow in gap between boards */
            setMaterial(&bd3d->gapColour);
            drawRect((TOTAL_WIDTH - HINGE_GAP * 1.5f) / 2.0f, 0.f, LIFT_OFF, HINGE_GAP * 2, TOTAL_HEIGHT + LIFT_OFF, 0);
        }
    }

    glPopMatrix();
}

static void
GrayScaleCol(float *pCols)
{
    float gs = (pCols[0] + pCols[1] + pCols[2]) / 2.5f; /* Slightly lighter gray */
    pCols[0] = pCols[1] = pCols[2] = gs;
}

static void
GrayScale3D(Material * pMat)
{
    GrayScaleCol(pMat->ambientColour);
    GrayScaleCol(pMat->diffuseColour);
    GrayScaleCol(pMat->specularColour);
}

static void
drawTableGrayed(const BoardData3d * bd3d, renderdata tmp)
{

    GrayScale3D(&tmp.BaseMat);
    GrayScale3D(&tmp.PointMat[0]);
    GrayScale3D(&tmp.PointMat[1]);
    GrayScale3D(&tmp.BoxMat);

    drawTable(bd3d, &tmp);

}

static int
DiceShowing(const BoardData * bd)
{
    return ((bd->diceShown == DICE_ON_BOARD && bd->diceRoll[0]) ||
            (bd->rd->fDiceArea && bd->diceShown == DICE_BELOW_BOARD));
}

NTH_STATIC void
drawPickAreas(const BoardData * bd, void *UNUSED(data))
{                               /* Draw main board areas to see where user clicked */
    renderdata *prd = bd->rd;
    float barHeight;

    /* boards */
    glLoadName(fClockwise ? 2 : 1);
    drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE * 6, POINT_HEIGHT, 0);

    glLoadName(fClockwise ? 1 : 2);
    drawRect(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE * 6, POINT_HEIGHT, 0);

    glLoadName(fClockwise ? 4 : 3);
    drawRect(TRAY_WIDTH + BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH, -PIECE_HOLE * 6, -POINT_HEIGHT, 0);

    glLoadName(fClockwise ? 3 : 4);
    drawRect(TOTAL_WIDTH - TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH, -PIECE_HOLE * 6, -POINT_HEIGHT, 0);

    /* bars + homes */
    barHeight = (PIECE_HOLE + PIECE_GAP_HEIGHT) * 4;
    glLoadName(0);
    drawRect(TOTAL_WIDTH / 2.0f - PIECE_HOLE / 2.0f,
             TOTAL_HEIGHT / 2.0f - (DOUBLECUBE_SIZE / 2.0f + barHeight + PIECE_HOLE / 2.0f), BASE_DEPTH + EDGE_DEPTH,
             PIECE_HOLE, barHeight, 0);
    glLoadName(25);
    drawRect(TOTAL_WIDTH / 2.0f - PIECE_HOLE / 2.0f, TOTAL_HEIGHT / 2.0f + (DOUBLECUBE_SIZE / 2.0f + PIECE_HOLE / 2.0f),
             BASE_DEPTH + EDGE_DEPTH, PIECE_HOLE, barHeight, 0);

    /* Home/unused trays depend on direction */
    glLoadName((!fClockwise) ? 26 : POINT_UNUSED0);
    drawRect(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2,
             TRAY_HEIGHT - EDGE_HEIGHT, 0);
    glLoadName((fClockwise) ? 26 : POINT_UNUSED0);
    drawRect(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, 0);

    glLoadName((!fClockwise) ? 27 : POINT_UNUSED1);
    drawRect(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT, BASE_DEPTH,
             TRAY_WIDTH - EDGE_WIDTH * 2, POINT_HEIGHT, 0);
    glLoadName((fClockwise) ? 27 : POINT_UNUSED1);
    drawRect(EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2,
             POINT_HEIGHT, 0);

    /* Dice areas */
    glLoadName(POINT_LEFT);
    drawRect(TRAY_WIDTH, (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f, BASE_DEPTH, DICE_AREA_CLICK_WIDTH, DICE_AREA_HEIGHT,
             0);
    glLoadName(POINT_RIGHT);
    drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f, BASE_DEPTH,
             DICE_AREA_CLICK_WIDTH, DICE_AREA_HEIGHT, 0);

    /* Dice */
    if (DiceShowing(bd)) {
        glLoadName(POINT_DICE);
        glPushMatrix();
        moveToDicePos(bd, 0);
        drawCube(getDiceSize(prd) * .95f);
        glPopMatrix();
        glPushMatrix();
        moveToDicePos(bd, 1);
        drawCube(getDiceSize(prd) * .95f);
        glPopMatrix();
    }

    /* Double Cube */
    glPushMatrix();
    glLoadName(POINT_CUBE);
    moveToDoubleCubePos(bd);
    drawCube(DOUBLECUBE_SIZE * .95f);
    glPopMatrix();
}

NTH_STATIC void
drawPickBoard(const BoardData * bd, void *data)
{                               /* Draw points and piece objects for selected board */
    BoardData3d *bd3d = bd->bd3d;
    int selectedBoard = GPOINTER_TO_INT(data);
    unsigned int i, j;

    unsigned int firstPoint = (selectedBoard - 1) * 6;
    /* pieces */
    for (i = firstPoint + 1; i <= firstPoint + 6; i++) {
        glLoadName(i);
        for (j = 1; j <= Abs(bd->points[i]); j++)
            drawPieceSimplified(bd3d->piecePickList, bd3d, i, j);
    }
    /* points */
    if (fClockwise) {
        int SwapBoard[4] = { 2, 1, 4, 3 };
        selectedBoard = SwapBoard[selectedBoard - 1];
    }
    for (i = 0; i < 6; i++) {
        switch (selectedBoard) {
        case 1:
            glLoadName(fClockwise ? i + 7 : 6 - i);
            drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * i, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE,
                     POINT_HEIGHT, 0);
            break;
        case 2:
            glLoadName(fClockwise ? i + 1 : 12 - i);
            drawRect(TRAY_WIDTH + PIECE_HOLE * i, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE, POINT_HEIGHT, 0);
            break;
        case 3:
            glLoadName(fClockwise ? i + 19 : 18 - i);
            drawRect(TRAY_WIDTH + BOARD_WIDTH - (PIECE_HOLE * i), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH, -PIECE_HOLE,
                     -POINT_HEIGHT, 0);
            break;
        case 4:
            glLoadName(fClockwise ? i + 13 : 24 - i);
            drawRect(TOTAL_WIDTH - TRAY_WIDTH - (PIECE_HOLE * i), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH, -PIECE_HOLE,
                     -POINT_HEIGHT, 0);
            break;
        }
    }
}

static void
DrawPoint(const BoardData * bd, int point)
{
    BoardData3d *bd3d = bd->bd3d;
    unsigned int j;
    glLoadName(point);
    for (j = 1; j <= Abs(bd->points[point]); j++)
        drawPiece(bd3d->piecePickList, bd3d, point, j, FALSE);

    if (fClockwise) {
        point--;
        if (point < 6)
            drawRect(TRAY_WIDTH + PIECE_HOLE * point, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE, POINT_HEIGHT, 0);
        else if (point < 12)
            drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * (point - 6), EDGE_HEIGHT, BASE_DEPTH,
                     PIECE_HOLE, POINT_HEIGHT, 0);
        else if (point < 18)
            drawRect(TOTAL_WIDTH - TRAY_WIDTH - (PIECE_HOLE * (point - 12)), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH,
                     -PIECE_HOLE, -POINT_HEIGHT, 0);
        else if (point < 24)
            drawRect(TRAY_WIDTH + BOARD_WIDTH - (PIECE_HOLE * (point - 18)), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH,
                     -PIECE_HOLE, -POINT_HEIGHT, 0);
    } else {
        if (point <= 6)
            drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * (6 - point), EDGE_HEIGHT, BASE_DEPTH,
                     PIECE_HOLE, POINT_HEIGHT, 0);
        else if (point <= 12)
            drawRect(TRAY_WIDTH + PIECE_HOLE * (12 - point), EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE, POINT_HEIGHT, 0);
        else if (point <= 18)
            drawRect(TRAY_WIDTH + BOARD_WIDTH - (PIECE_HOLE * (18 - point)), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH,
                     -PIECE_HOLE, -POINT_HEIGHT, 0);
        else if (point <= 24)
            drawRect(TOTAL_WIDTH - TRAY_WIDTH - (PIECE_HOLE * (24 - point)), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH,
                     -PIECE_HOLE, -POINT_HEIGHT, 0);
    }
}

NTH_STATIC void
drawPickPoint(const BoardData * bd, void *data)
{                               /* Draw all the objects on the board to see if any have been selected */
    int pointA = (GPOINTER_TO_INT(data)) / 100;
    int pointB = (GPOINTER_TO_INT(data)) - pointA * 100;

    /* pieces on both points */
    DrawPoint(bd, pointA);
    DrawPoint(bd, pointB);
}

static int
NearestHit(int hits, const GLuint * ptr)
{
    if (hits == 1) {            /* Only one hit - just return this one */
        return (int) ptr[3];
    } else {                    /* Find the highest/closest object */
        int i, sel = -1;
        GLuint names;
        float minDepth = FLT_MAX, depth;

        for (i = 0; i < hits; i++) {    /* for each hit */
            names = *ptr++;
            depth = (float) *ptr++ / 0x7fffffff;
            ptr++;              /* Skip max depth value */
            /* Ignore clicks on the board base as other objects must be closer */
            if (*ptr >= POINT_DICE && !(*ptr == POINT_LEFT || *ptr == POINT_RIGHT) && depth < minDepth) {
                minDepth = depth;
                sel = (int) *ptr;
            }
            ptr += names;
        }
        return sel;
    }
}

static void
getProjectedPos(int x, int y, float atDepth, float pos[3])
{                               /* Work out where point (x, y) projects to at depth atDepth */
    GLint viewport[4];
    GLdouble mvmatrix[16], projmatrix[16];
    double nearX, nearY, nearZ, farX, farY, farZ, zRange;

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

    if ((gluUnProject((GLdouble) x, (GLdouble) y, 0.0, mvmatrix, projmatrix, viewport, &nearX, &nearY, &nearZ) ==
         GL_FALSE)
        || (gluUnProject((GLdouble) x, (GLdouble) y, 1.0, mvmatrix, projmatrix, viewport, &farX, &farY, &farZ) ==
            GL_FALSE)) {
        /* Maybe a g_assert_not_reached() would be appropriate here
         * Don't leave output parameters undefined anyway */
        pos[0] = pos[1] = pos[2] = 0.0f;
        g_print("gluUnProject failed!\n");
        return;
    }

    zRange = (fabs(nearZ) - atDepth) / (fabs(farZ) + fabs(nearZ));
    pos[0] = (float) (nearX - (-farX + nearX) * zRange);
    pos[1] = (float) (nearY - (-farY + nearY) * zRange);
    pos[2] = (float) (nearZ - (-farZ + nearZ) * zRange);
}

void
getProjectedPieceDragPos(int x, int y, float pos[3])
{
    getProjectedPos(x, y, BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE + LIFT_OFF * 3, pos);
}

void
calculateBackgroundSize(BoardData3d * bd3d, const GLint viewport[4])
{
    float pos1[3], pos2[3], pos3[3];

    getProjectedPos(0, viewport[1] + viewport[3], 0.f, pos1);
    getProjectedPos(viewport[2], viewport[1] + viewport[3], 0.f, pos2);
    getProjectedPos(0, viewport[1], 0.f, pos3);

    bd3d->backGroundPos[0] = pos1[0];
    bd3d->backGroundPos[1] = pos3[1];
    bd3d->backGroundSize[0] = pos2[0] - pos1[0];
    bd3d->backGroundSize[1] = pos1[1] - pos3[1];
}

static void
getFlagPos(const BoardData * bd, float v[3])
{
    v[0] = (TRAY_WIDTH + BOARD_WIDTH) / 2.0f;
    v[1] = TOTAL_HEIGHT / 2.0f;
    v[2] = BASE_DEPTH + EDGE_DEPTH;

    if (bd->turn == -1)         /* Move to other side of board */
        v[0] += BOARD_WIDTH + BAR_WIDTH;
}

void
waveFlag(float wag)
{
    int i, j;

    /* wave the flag by rotating Z coords though a sine wave */
    for (i = 1; i < S_NUMPOINTS; i++)
        for (j = 0; j < T_NUMPOINTS; j++)
            flag.ctlpoints[i][j][2] = sinf((GLfloat) i + wag) * FLAG_WAG;
}

NTH_STATIC void
drawFlagPick(const BoardData * bd, void *UNUSED(data))
{
    BoardData3d *bd3d = bd->bd3d;
    renderdata *prd = bd->rd;
    int s;
    float v[3];

    waveFlag(bd3d->flagWaved);

    glLoadName(POINT_RESIGN);

    glPushMatrix();

    getFlagPos(bd, v);
    glTranslatef(v[0], v[1], v[2]);

    /* Draw flag surface (approximation) */
    glBegin(GL_QUAD_STRIP);
    for (s = 0; s < S_NUMPOINTS; s++) {
        glVertex3fv(flag.ctlpoints[s][1]);
        glVertex3fv(flag.ctlpoints[s][0]);
    }
    glEnd();

    /* Draw flag pole */
    glTranslatef(0.f, -FLAG_HEIGHT, 0.f);

    glRotatef(-90.f, 1.f, 0.f, 0.f);
    gluCylinder(bd3d->qobj, (double) FLAGPOLE_WIDTH, (double) FLAGPOLE_WIDTH, (double) FLAGPOLE_HEIGHT,
                (GLint) prd->curveAccuracy, 1);

    circleRev(FLAGPOLE_WIDTH, 0.f, prd->curveAccuracy);
    circleRev(FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, prd->curveAccuracy);

    glPopMatrix();
}

NTH_STATIC void
drawPointPick(const BoardData * UNUSED(bd), void *data)
{                               /* Draw sub parts of point to work out which part of point clicked */
    unsigned int point = GPOINTER_TO_UINT(data);
    unsigned int i;
    float pos[3];

    if (point <= 25) {          /* Split first point into 2 parts for zero and one selection */
#define SPLIT_PERCENT .40f
#define SPLIT_HEIGHT (PIECE_HOLE * SPLIT_PERCENT)

        getPiecePos(point, 1, pos);

        glLoadName(0);
        if (point > 12)
            drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] + (PIECE_HOLE / 2.0f) + PIECE_GAP_HEIGHT - SPLIT_HEIGHT,
                     pos[2], PIECE_HOLE, SPLIT_HEIGHT, 0);
        else
            drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE, SPLIT_HEIGHT, 0);

        glLoadName(1);
        if (point > 12)
            drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE,
                     PIECE_HOLE + PIECE_GAP_HEIGHT - SPLIT_HEIGHT, 0);
        else
            drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f) + SPLIT_HEIGHT, pos[2], PIECE_HOLE,
                     PIECE_HOLE + PIECE_GAP_HEIGHT - SPLIT_HEIGHT, 0);

        for (i = 2; i < 6; i++) {
            /* Only 3 places for bar */
            if ((point == 0 || point == 25) && i == 4)
                break;
            getPiecePos(point, i, pos);
            glLoadName(i);
            drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE,
                     PIECE_HOLE + PIECE_GAP_HEIGHT, 0);
        }
        /* extra bit */
        /*              glLoadName(i);
         * if (point > 12)
         * {
         * pos[1] = pos[1] - (PIECE_HOLE / 2.0f);
         * drawRect(pos[0] - (PIECE_HOLE / 2.0f), TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT, pos[2], PIECE_HOLE, pos[1] - (TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT), 0);
         * }
         * else
         * {
         * pos[1] = pos[1] + (PIECE_HOLE / 2.0f) + PIECE_GAP_HEIGHT;
         * drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1], pos[2], PIECE_HOLE, EDGE_HEIGHT + POINT_HEIGHT - pos[1], 0);
         * }
         */
    }
}

/* 20 allows for 5 hit records (more than enough) */
#define BUFSIZE 20
GLuint selectBuf[BUFSIZE];

typedef void (*PickDrawFun) (const BoardData * bd, void *data);

static int
PickDraw(int x, int y, PickDrawFun drawFun, const BoardData * bd, void *data)
{                               /* Identify if anything is below point (x,y) */
    BoardData3d *bd3d = bd->bd3d;
    GLint hits;
    GLint viewport[4];

    glSelectBuffer(BUFSIZE, selectBuf);
    glRenderMode(GL_SELECT);
    glInitNames();
    glPushName(0);

    glGetIntegerv(GL_VIEWPORT, viewport);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    glLoadIdentity();
    gluPickMatrix((double) x, (double) y, 1.0, 1.0, viewport);

    /* Setup projection matrix - using saved values */
    if (bd->rd->planView)
        glOrtho(-bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, 0.0, 5.0);
    else
        glFrustum(-bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(bd3d->modelMatrix);

    drawFun(bd, data);

    glPopName();

    hits = glRenderMode(GL_RENDER);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    return hits;
}

int
BoardPoint3d(const BoardData * bd, int x, int y)
{
    int hits;
    if (bd->resigned) {         /* Flag showing - just pick this */
        hits = PickDraw(x, y, drawFlagPick, bd, NULL);
        return NearestHit(hits, (GLuint *) selectBuf);
    } else {
        int picked, firstPoint, secondPoint;
        hits = PickDraw(x, y, drawPickAreas, bd, NULL);
        picked = NearestHit(hits, (GLuint *) selectBuf);
        if (picked <= 0 || picked > 24)
            return picked;      /* Not a point so actual hit */
        g_assert(picked >= 1 && picked <= 4);   /* one of 4 boards */

        /* Work out which point in board was clicked */
        hits = PickDraw(x, y, drawPickBoard, bd, GINT_TO_POINTER(picked));
        firstPoint = (int) selectBuf[3];
        if (hits == 1)
            return firstPoint;  /* Board point clicked */

        secondPoint = (int) selectBuf[selectBuf[0] + 6];
        if (firstPoint == secondPoint)
            return firstPoint;  /* Chequer clicked over point (common) */

        /* Could be either chequer or point - work out which one */
        hits = PickDraw(x, y, drawPickPoint, bd, GINT_TO_POINTER(firstPoint * 100 + secondPoint));
        return NearestHit(hits, (GLuint *) selectBuf);
    }
}

int
BoardSubPoint3d(const BoardData * bd, int x, int y, guint point)
{
    int hits = PickDraw(x, y, drawPointPick, bd, GUINT_TO_POINTER(point));
    return NearestHit(hits, (GLuint *) selectBuf);
}

void
setupPath(const BoardData * bd, Path * p, float *pRotate, unsigned int fromPoint, unsigned int fromDepth,
          unsigned int toPoint, unsigned int toDepth)
{                               /* Work out the animation path for a moving piece */
    float point[3];
    float w, h, xDist, yDist;
    float xDiff, yDiff;
    float bar1Dist, bar1Ratio;
    float bar2Dist, bar2Ratio;
    float start[3], end[3], obj1, obj2, objHeight;
    unsigned int fromBoard = (fromPoint + 5) / 6;
    unsigned int toBoard = (toPoint - 1) / 6 + 1;
    int yDir = 0;

    /* First work out were piece has to move */
    /* Get start and end points */
    getPiecePos(fromPoint, fromDepth, start);
    getPiecePos(toPoint, toDepth, end);

    /* Only rotate piece going home */
    *pRotate = -1;

    /* Swap boards if displaying other way around */
    if (fClockwise) {
        unsigned int swapBoard[] = { 0, 2, 1, 4, 3, 5 };
        fromBoard = swapBoard[fromBoard];
        toBoard = swapBoard[toBoard];
    }

    /* Work out what obstacle needs to be avoided */
    if ((fromBoard == 2 || fromBoard == 3) && (toBoard == 1 || toBoard == 4)) { /* left side to right side */
        obj1 = TRAY_WIDTH + BOARD_WIDTH;
        obj2 = obj1 + BAR_WIDTH;
        objHeight = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE;
    } else if ((fromBoard == 1 || fromBoard == 4) && (toBoard == 2 || toBoard == 3)) {  /* right side to left side */
        obj2 = TRAY_WIDTH + BOARD_WIDTH;
        obj1 = obj2 + BAR_WIDTH;
        objHeight = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE;
    } else if ((fromBoard == 1 && toBoard == 4) || (fromBoard == 2 && toBoard == 3)) {  /* up same side */
        obj1 = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f;
        obj2 = (TOTAL_HEIGHT + DICE_AREA_HEIGHT) / 2.0f;
        objHeight = BASE_DEPTH + getDiceSize(bd->rd);
        yDir = 1;
    } else if ((fromBoard == 4 && toBoard == 1) || (fromBoard == 3 && toBoard == 2)) {  /* down same side */
        obj1 = (TOTAL_HEIGHT + DICE_AREA_HEIGHT) / 2.0f;
        obj2 = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f;
        objHeight = BASE_DEPTH + getDiceSize(bd->rd);
        yDir = 1;
    } else if (fromBoard == toBoard) {
        if (fromBoard <= 2) {
            if (fClockwise) {
                fromPoint = 13 - fromPoint;
                toPoint = 13 - toPoint;
            }

            if (fromPoint < toPoint)
                toPoint--;
            else
                fromPoint--;

            fromPoint = 12 - fromPoint;
            toPoint = 12 - toPoint;
        } else {
            if (fClockwise) {
                fromPoint = 24 + 13 - fromPoint;
                toPoint = 24 + 13 - toPoint;
            }

            if (fromPoint < toPoint)
                fromPoint++;
            else
                toPoint++;
            fromPoint = fromPoint - 13;
            toPoint = toPoint - 13;
        }
        obj1 = TRAY_WIDTH + PIECE_HOLE * fromPoint;
        obj2 = TRAY_WIDTH + PIECE_HOLE * toPoint;
        if ((fromBoard == 1) || (fromBoard == 4)) {
            obj1 += BAR_WIDTH;
            obj2 += BAR_WIDTH;
        }

        objHeight = BASE_DEPTH + PIECE_DEPTH * 3;
    } else {
        if (fromPoint == 0 || fromPoint == 25) {        /* Move from bar */
            if (!fClockwise) {
                obj1 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH;
                if (fromPoint == 0) {
                    obj2 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH;
                    if (toPoint > 20)
                        obj2 += PIECE_HOLE * (toPoint - 20);
                } else {
                    obj2 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH;
                    if (toPoint < 5)
                        obj2 += PIECE_HOLE * (5 - toPoint);
                }
            } else {
                obj1 = TRAY_WIDTH + BOARD_WIDTH;
                if (fromPoint == 0) {
                    obj2 = TRAY_WIDTH + PIECE_HOLE * (25 - toPoint);
                    if (toPoint > 19)
                        obj2 += PIECE_HOLE;
                } else {
                    obj2 = TRAY_WIDTH + PIECE_HOLE * toPoint;
                    if (toPoint < 6)
                        obj2 += PIECE_HOLE;
                }
            }
            objHeight = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE;
        } else {                /* Move home */
            if (!fClockwise) {
                if (toPoint == 26)
                    obj1 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * (7 - fromPoint);
                else            /* (toPoint == 27) */
                    obj1 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * (fromPoint - 18);
            } else {
                if (toPoint == 26)
                    obj1 = TRAY_WIDTH + PIECE_HOLE * (fromPoint - 1);
                else            /* (toPoint == 27) */
                    obj1 = TRAY_WIDTH + PIECE_HOLE * (24 - fromPoint);
            }

            if (!fClockwise)
                obj2 = TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH;
            else
                obj2 = TRAY_WIDTH - EDGE_WIDTH;

            *pRotate = 0;
            objHeight = BASE_DEPTH + EDGE_DEPTH + getDiceSize(bd->rd);
        }
    }
    if ((fromBoard == 3 && toBoard == 4) || (fromBoard == 4 && toBoard == 3)) { /* Special case when moving along top of board */
        if ((bd->cube_owner != 1) && (fromDepth <= 2) && (toDepth <= 2))
            objHeight = BASE_DEPTH + EDGE_DEPTH + PIECE_DEPTH;
    }

    /* Now setup path object */
    /* Start point */
    initPath(p, start);

    /* obstacle height */
    point[2] = objHeight;

    if (yDir) {                 /* barriers are along y axis */
        xDiff = end[0] - start[0];
        yDiff = end[1] - start[1];
        bar1Dist = obj1 - start[1];
        bar1Ratio = bar1Dist / yDiff;
        bar2Dist = obj2 - start[1];
        bar2Ratio = bar2Dist / yDiff;

        /* 2nd point - moved up from 1st one */
        /* Work out whether 2nd point is further away than height required */
        xDist = xDiff * bar1Ratio;
        yDist = obj1 - start[1];
        h = objHeight - start[2];
        w = sqrtf(xDist * xDist + yDist * yDist);

        if (h > w) {
            point[0] = start[0] + xDiff * bar1Ratio;
            point[1] = obj1;
        } else {
            point[0] = start[0] + xDist * (h / w);
            point[1] = start[1] + yDist * (h / w);
        }
        addPathSegment(p, PATH_CURVE_9TO12, point);

        /* Work out whether 3rd point is further away than height required */
        yDist = end[1] - obj2;
        xDist = xDiff * (yDist / yDiff);
        w = sqrtf(xDist * xDist + yDist * yDist);
        h = objHeight - end[2];

        /* 3rd point - moved along from 2nd one */
        if (h > w) {
            point[0] = start[0] + xDiff * bar2Ratio;
            point[1] = obj2;
        } else {
            point[0] = end[0] - xDist * (h / w);
            point[1] = end[1] - yDist * (h / w);
        }
        addPathSegment(p, PATH_LINE, point);
    } else {                    /* barriers are along x axis */
        xDiff = end[0] - start[0];
        yDiff = end[1] - start[1];
        bar1Dist = obj1 - start[0];
        bar1Ratio = bar1Dist / xDiff;
        bar2Dist = obj2 - start[0];
        bar2Ratio = bar2Dist / xDiff;

        /* Work out whether 2nd point is further away than height required */
        xDist = obj1 - start[0];
        yDist = yDiff * bar1Ratio;
        h = objHeight - start[2];
        w = sqrtf(xDist * xDist + yDist * yDist);

        /* 2nd point - moved up from 1st one */
        if (h > w) {
            point[0] = obj1;
            point[1] = start[1] + yDiff * bar1Ratio;
        } else {
            point[0] = start[0] + xDist * (h / w);
            point[1] = start[1] + yDist * (h / w);
        }
        addPathSegment(p, PATH_CURVE_9TO12, point);

        /* Work out whether 3rd point is further away than height required */
        xDist = end[0] - obj2;
        yDist = yDiff * (xDist / xDiff);
        w = sqrtf(xDist * xDist + yDist * yDist);
        h = objHeight - end[2];

        /* 3rd point - moved along from 2nd one */
        if (h > w) {
            point[0] = obj2;
            point[1] = start[1] + yDiff * bar2Ratio;
        } else {
            point[0] = end[0] - xDist * (h / w);
            point[1] = end[1] - yDist * (h / w);
        }
        addPathSegment(p, PATH_LINE, point);
    }
    /* End point */
    addPathSegment(p, PATH_CURVE_12TO3, end);
}

static void
setupDicePath(int num, float stepSize, Path * p, const BoardData * bd)
{
    float point[3];
    getDicePos(bd, num, point);

    point[0] -= stepSize * 5;
    initPath(p, point);

    point[0] += stepSize * 2;
    addPathSegment(p, PATH_PARABOLA_12TO3, point);

    point[0] += stepSize * 2;
    addPathSegment(p, PATH_PARABOLA, point);

    point[0] += stepSize;
    addPathSegment(p, PATH_PARABOLA, point);
}

static void
randomDiceRotation(const Path * p, DiceRotation * diceRotation, int dir)
{
    /* Dice rotation range values */
#define XROT_MIN 1
#define XROT_RANGE 1.5f

#define YROT_MIN -.5f
#define YROT_RANGE 1.f

    diceRotation->xRotFactor = XROT_MIN + randRange(XROT_RANGE);
    diceRotation->xRotStart = 1 - (p->pts[p->numSegments][0] - p->pts[0][0]) * diceRotation->xRotFactor;
    diceRotation->xRot = diceRotation->xRotStart * 360;

    diceRotation->yRotFactor = YROT_MIN + randRange(YROT_RANGE);
    diceRotation->yRotStart = 1 - (p->pts[p->numSegments][0] - p->pts[0][0]) * diceRotation->yRotFactor;
    diceRotation->yRot = diceRotation->yRotStart * 360;

    if (dir == -1) {
        diceRotation->xRotFactor = -diceRotation->xRotFactor;
        diceRotation->yRotFactor = -diceRotation->yRotFactor;
    }
}

void
setupDicePaths(const BoardData * bd, Path dicePaths[2], float diceMovingPos[2][3], DiceRotation diceRotation[2])
{
    int firstDie = (bd->turn == 1);
    int secondDie = !firstDie;
    int dir = (bd->turn == 1) ? -1 : 1;

    setupDicePath(firstDie, dir * DICE_STEP_SIZE0, &dicePaths[firstDie], bd);
    setupDicePath(secondDie, dir * DICE_STEP_SIZE1, &dicePaths[secondDie], bd);

    randomDiceRotation(&dicePaths[firstDie], &diceRotation[firstDie], dir);
    randomDiceRotation(&dicePaths[secondDie], &diceRotation[secondDie], dir);

    copyPoint(diceMovingPos[0], dicePaths[0].pts[0]);
    copyPoint(diceMovingPos[1], dicePaths[1].pts[0]);
}

int
DiceTooClose(const BoardData3d * bd3d, const renderdata * prd)
{
    float dist;
    float orgX[2];
    int firstDie = bd3d->dicePos[0][0] > bd3d->dicePos[1][0];
    int secondDie = !firstDie;

    orgX[0] = bd3d->dicePos[firstDie][0] - DICE_STEP_SIZE0 * 5;
    orgX[1] = bd3d->dicePos[secondDie][0] - DICE_STEP_SIZE1 * 5;
    dist = sqrtf((orgX[1] - orgX[0]) * (orgX[1] - orgX[0])
                 + (bd3d->dicePos[secondDie][1] - bd3d->dicePos[firstDie][1]) * (bd3d->dicePos[secondDie][1] -
                                                                                 bd3d->dicePos[firstDie][1]));
    return (dist < getDiceSize(prd) * 1.1f);
}

void
setDicePos(BoardData * bd, BoardData3d * bd3d)
{
    int iter = 0;
    float diceSize = getDiceSize(bd->rd);
    float firstX = TRAY_WIDTH + DICE_STEP_SIZE0 * 3 + diceSize * .75f;
    int firstDie = (bd->turn == 1);
    int secondDie = !firstDie;

    bd3d->dicePos[firstDie][0] = firstX + randRange(BOARD_WIDTH + TRAY_WIDTH - firstX - diceSize * 2);
    bd3d->dicePos[firstDie][1] = randRange(DICE_AREA_HEIGHT);

    do {                        /* insure dice are not too close together */
        if (iter++ > 20) {      /* Trouble placing 2nd dice - replace 1st dice */
            setDicePos(bd, bd3d);
            return;
        }

        firstX = bd3d->dicePos[firstDie][0] + diceSize;
        bd3d->dicePos[secondDie][0] = firstX + randRange(BOARD_WIDTH + TRAY_WIDTH - firstX - diceSize * .7f);
        bd3d->dicePos[secondDie][1] = randRange(DICE_AREA_HEIGHT);
    }
    while (DiceTooClose(bd3d, bd->rd));

    bd3d->dicePos[firstDie][2] = (float) (rand() % 360);
    bd3d->dicePos[secondDie][2] = (float) (rand() % 360);

    if (ShadowsInitilised(bd3d))
        updateDiceOccPos(bd, bd3d);
}

NTH_STATIC void
drawDie(const BoardData * bd, const BoardData3d * bd3d, int num)
{
    glPushMatrix();
    /* Move to correct position for die */
    if (bd3d->shakingDice) {
        glTranslatef(bd3d->diceMovingPos[num][0], bd3d->diceMovingPos[num][1], bd3d->diceMovingPos[num][2]);
        glRotatef(bd3d->diceRotation[num].xRot, 0.f, 1.f, 0.f);
        glRotatef(bd3d->diceRotation[num].yRot, 1.f, 0.f, 0.f);
        glRotatef(bd3d->dicePos[num][2], 0.f, 0.f, 1.f);
    } else
        moveToDicePos(bd, num);
    /* Now draw dice */
    drawDice(bd, num);

    glPopMatrix();
}

static void
initViewArea(viewArea * pva)
{                               /* Initialize values to extremes */
    pva->top = -base_unit * 1000;
    pva->bottom = base_unit * 1000;
    pva->width = 0;
}

static float
getViewAreaHeight(const viewArea * pva)
{
    return pva->top - pva->bottom;
}

static float
getViewAreaWidth(const viewArea * pva)
{
    return pva->width;
}

static float
getAreaRatio(const viewArea * pva)
{
    return getViewAreaWidth(pva) / getViewAreaHeight(pva);
}

static void
addViewAreaHeightPoint(viewArea * pva, float halfRadianFOV, float boardRadAngle, float inY, float inZ)
{                               /* Rotate points by board angle */
    float adj;
    float y, z;

    y = inY * cosf(boardRadAngle) - inZ * sinf(boardRadAngle);
    z = inZ * cosf(boardRadAngle) + inY * sinf(boardRadAngle);

    /* Project height to zero depth */
    adj = z * tanf(halfRadianFOV);
    if (y > 0)
        y += adj;
    else
        y -= adj;

    /* Store max / min heights */
    if (pva->top < y)
        pva->top = y;
    if (pva->bottom > y)
        pva->bottom = y;
}

static void
workOutWidth(viewArea * pva, float halfRadianFOV, float boardRadAngle, float aspectRatio, const float ip[3])
{
    float halfRadianXFOV;
    float w = getViewAreaHeight(pva) * aspectRatio;
    float w2, l;

    float p[3];

    /* Work out X-FOV from Y-FOV */
    float halfViewHeight = getViewAreaHeight(pva) / 2;
    l = halfViewHeight / tanf(halfRadianFOV);
    halfRadianXFOV = atanf((halfViewHeight * aspectRatio) / l);

    /* Rotate z coord by board angle */
    copyPoint(p, ip);
    p[2] = ip[2] * cosf(boardRadAngle) + ip[1] * sinf(boardRadAngle);

    /* Adjust x coord by projected z value at relevant X-FOV */
    w2 = w / 2 - p[2] * tanf(halfRadianXFOV);
    l = w2 / tanf(halfRadianXFOV);
    p[0] += p[2] * (p[0] / l);

    if (pva->width < p[0] * 2)
        pva->width = p[0] * 2;
}

NTH_STATIC float
GetFOVAngle(const BoardData * bd)
{
    float temp = bd->rd->boardAngle / 20.0f;
    float skewFactor = (bd->rd->skewFactor / 100.0f) * (4 - .6f) + .6f;
    /* Magic numbers to normalize viewing distance */
    return (47 - 2.3f * temp * temp) / skewFactor;
}

static void
WorkOutViewArea(const BoardData * bd, viewArea * pva, float *pHalfRadianFOV, float aspectRatio)
{
    float p[3];
    float boardRadAngle;
    initViewArea(pva);

    boardRadAngle = (bd->rd->boardAngle * (float) G_PI) / 180.0f;
    *pHalfRadianFOV = ((GetFOVAngle(bd) * (float) G_PI) / 180.0f) / 2.0f;

    /* Sort out viewing area */
    addViewAreaHeightPoint(pva, *pHalfRadianFOV, boardRadAngle, -getBoardHeight() / 2, 0.f);
    addViewAreaHeightPoint(pva, *pHalfRadianFOV, boardRadAngle, -getBoardHeight() / 2, BASE_DEPTH + EDGE_DEPTH);

    if (bd->rd->fDiceArea) {
        float diceSize = getDiceSize(bd->rd);
        addViewAreaHeightPoint(pva, *pHalfRadianFOV, boardRadAngle, getBoardHeight() / 2 + diceSize, 0.f);
        addViewAreaHeightPoint(pva, *pHalfRadianFOV, boardRadAngle, getBoardHeight() / 2 + diceSize,
                               BASE_DEPTH + diceSize);
    } else {                    /* Bottom edge is defined by board */
        addViewAreaHeightPoint(pva, *pHalfRadianFOV, boardRadAngle, getBoardHeight() / 2, 0.f);
        addViewAreaHeightPoint(pva, *pHalfRadianFOV, boardRadAngle, getBoardHeight() / 2, BASE_DEPTH + EDGE_DEPTH);
    }

    p[0] = getBoardWidth() / 2;
    p[1] = getBoardHeight() / 2;
    p[2] = BASE_DEPTH + EDGE_DEPTH;
    workOutWidth(pva, *pHalfRadianFOV, boardRadAngle, aspectRatio, p);
}

void
SetupPerspVolume(const BoardData * bd, BoardData3d * bd3d, const renderdata * prd, GLint viewport[4])
{
    float aspectRatio = (float) viewport[2] / (float) (viewport[3]);
    if (!prd->planView) {
        viewArea va;
        float halfRadianFOV;
        double fovScale;
        float zoom;

        if (aspectRatio < .5f) {        /* Viewing area to high - cut down so board rendered correctly */
            int newHeight = viewport[2] * 2;
            viewport[1] = (viewport[3] - newHeight) / 2;
            viewport[3] = newHeight;
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            aspectRatio = .5f;
            /* Clear screen so top + bottom outside board shown ok */
            ClearScreen(prd);
        }

        /* Workout how big the board is (in 3d space) */
        WorkOutViewArea(bd, &va, &halfRadianFOV, aspectRatio);

        fovScale = zNear * tanf(halfRadianFOV);

        if (aspectRatio > getAreaRatio(&va)) {
            bd3d->vertFrustrum = fovScale;
            bd3d->horFrustrum = bd3d->vertFrustrum * aspectRatio;
        } else {
            bd3d->horFrustrum = fovScale * getAreaRatio(&va);
            bd3d->vertFrustrum = bd3d->horFrustrum / aspectRatio;
        }
        /* Setup projection matrix */
        glFrustum(-bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, zNear, zFar);

        /* Setup modelview matrix */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* Zoom back so image fills window */
        zoom = (getViewAreaHeight(&va) / 2) / tanf(halfRadianFOV);
        glTranslatef(0.f, 0.f, -zoom);

        /* Offset from centre because of perspective */
        glTranslatef(0.f, getViewAreaHeight(&va) / 2 + va.bottom, 0.f);

        /* Rotate board */
        glRotatef((float) prd->boardAngle, -1.f, 0.f, 0.f);

        /* Origin is bottom left, so move from centre */
        glTranslatef(-(getBoardWidth() / 2.0f), -((getBoardHeight()) / 2.0f), 0.f);
    } else {
        float size;

        if (aspectRatio > getBoardWidth() / getBoardHeight()) {
            size = (getBoardHeight() / 2);
            bd3d->horFrustrum = size * aspectRatio;
            bd3d->vertFrustrum = size;
        } else {
            size = (getBoardWidth() / 2);
            bd3d->horFrustrum = size;
            bd3d->vertFrustrum = size / aspectRatio;
        }
        glOrtho(-bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, 0.0, 5.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glTranslatef(-(getBoardWidth() / 2.0f), -(getBoardHeight() / 2.0f), -3.f);
    }

    /* Save matrix for later */
    glGetFloatv(GL_MODELVIEW_MATRIX, bd3d->modelMatrix);
}

extern void
SetupFlag(void)
{
    int i;
    float width = FLAG_WIDTH;
    float height = FLAG_HEIGHT;

    flag.flagNurb = NULL;

    for (i = 0; i < S_NUMPOINTS; i++) {
        flag.ctlpoints[i][0][0] = width / (S_NUMPOINTS - 1) * i;
        flag.ctlpoints[i][1][0] = width / (S_NUMPOINTS - 1) * i;
        flag.ctlpoints[i][0][1] = 0;
        flag.ctlpoints[i][1][1] = height;
        flag.ctlpoints[i][0][2] = 0;
        flag.ctlpoints[i][1][2] = 0;
    }
}

NTH_STATIC void
renderFlag(const BoardData * bd, const BoardData3d * bd3d, unsigned int curveAccuracy)
{
    /* Simple knots i.e. no weighting */
    float s_knots[S_NUMKNOTS] = { 0, 0, 0, 0, 1, 1, 1, 1 };
    float t_knots[T_NUMKNOTS] = { 0, 0, 1, 1 };

    /* Draw flag surface */
    setMaterial(&bd3d->flagMat);

    if (flag.flagNurb == NULL)
        flag.flagNurb = gluNewNurbsRenderer();

    /* Set size of polygons */
    gluNurbsProperty(flag.flagNurb, GLU_SAMPLING_TOLERANCE, 500.0f / curveAccuracy);

    gluBeginSurface(flag.flagNurb);
    gluNurbsSurface(flag.flagNurb, S_NUMKNOTS, s_knots, T_NUMKNOTS, t_knots, 3 * T_NUMPOINTS, 3,
                    &flag.ctlpoints[0][0][0], S_NUMPOINTS, T_NUMPOINTS, GL_MAP2_VERTEX_3);
    gluEndSurface(flag.flagNurb);

    /* Draw flag pole */
    glPushMatrix();

    glTranslatef(-FLAGPOLE_WIDTH, -FLAG_HEIGHT, 0.f);

    glRotatef(-90.f, 1.f, 0.f, 0.f);
    SetColour3d(.2f, .2f, .4f, 0.f);    /* Blue pole */
    gluCylinder(bd3d->qobj, (double) FLAGPOLE_WIDTH, (double) FLAGPOLE_WIDTH, (double) FLAGPOLE_HEIGHT,
                (GLint) curveAccuracy, 1);

    circleRev(FLAGPOLE_WIDTH, 0.f, curveAccuracy);
    circleRev(FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, curveAccuracy);

    glPopMatrix();

    /* Draw number on flag */
    glDisable(GL_DEPTH_TEST);

    setMaterial(&bd3d->flagNumberMat);

    glPushMatrix();
    {
        /* Move to middle of flag */
        float v[3];
        v[0] = (flag.ctlpoints[1][0][0] + flag.ctlpoints[2][0][0]) / 2.0f;
        v[1] = (flag.ctlpoints[1][0][0] + flag.ctlpoints[1][1][0]) / 2.0f;
        v[2] = (flag.ctlpoints[1][0][2] + flag.ctlpoints[2][0][2]) / 2.0f;
        glTranslatef(v[0], v[1], v[2]);
    }
    {
        /* Work out approx angle of number based on control points */
        float ang =
            atanf(-(flag.ctlpoints[2][0][2] - flag.ctlpoints[1][0][2]) /
                  (flag.ctlpoints[2][0][0] - flag.ctlpoints[1][0][0]));
        float degAng = (ang) * 180 / (float) G_PI;

        glRotatef(degAng, 0.f, 1.f, 0.f);
    }

    {
        /* Draw number */
        char flagValue[2] = "x";
        /* No specular light */
        float specular[4];
        float zero[4] = { 0, 0, 0, 0 };
        glGetLightfv(GL_LIGHT0, GL_SPECULAR, specular);
        glLightfv(GL_LIGHT0, GL_SPECULAR, zero);

        flagValue[0] = '0' + (char) abs(bd->resigned);
        glScalef(1.3f, 1.3f, 1.f);

        glLineWidth(.5f);
        glPrintCube(bd3d->cubeFont, flagValue);

        glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    }
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
}

static void
drawFlag(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    float v[4];
    int isStencil = glIsEnabled(GL_STENCIL_TEST);

    if (isStencil)
        glDisable(GL_STENCIL_TEST);

    waveFlag(bd3d->flagWaved);

    glPushMatrix();

    getFlagPos(bd, v);
    glTranslatef(v[0], v[1], v[2]);

    renderFlag(bd, bd3d, prd->curveAccuracy);

    glPopMatrix();
    if (isStencil)
        glEnable(GL_STENCIL_TEST);
}

static void
updateDieOccPos(const BoardData * bd, const BoardData3d * bd3d, Occluder * pOcc, int num)
{
    float id[4][4];

    if (bd3d->shakingDice) {
        copyPoint(pOcc->trans, bd3d->diceMovingPos[num]);

        pOcc->rot[0] = bd3d->diceRotation[num].xRot;
        pOcc->rot[1] = bd3d->diceRotation[num].yRot;
        pOcc->rot[2] = bd3d->dicePos[num][2];

        makeInverseRotateMatrixZ(pOcc->invMat, pOcc->rot[2]);

        makeInverseRotateMatrixX(id, pOcc->rot[1]);
        matrixmult(pOcc->invMat, (ConstMatrix) id);

        makeInverseRotateMatrixY(id, pOcc->rot[0]);
        matrixmult(pOcc->invMat, (ConstMatrix) id);

        makeInverseTransposeMatrix(id, pOcc->trans);
        matrixmult(pOcc->invMat, (ConstMatrix) id);
    } else {
        getDicePos(bd, num, pOcc->trans);

        makeInverseTransposeMatrix(id, pOcc->trans);

        if (bd->diceShown == DICE_ON_BOARD) {
            pOcc->rot[0] = pOcc->rot[1] = 0;
            pOcc->rot[2] = bd3d->dicePos[num][2];

            makeInverseRotateMatrixZ(pOcc->invMat, pOcc->rot[2]);
            matrixmult(pOcc->invMat, (ConstMatrix) id);
        } else {
            pOcc->rot[0] = pOcc->rot[1] = pOcc->rot[2] = 0;
            copyMatrix(pOcc->invMat, id);
        }
    }
    if (ShadowsInitilised(bd3d))
        draw_shadow_volume_extruded_edges(pOcc, bd3d->shadow_light_position, GL_QUADS);
}

void
updateDiceOccPos(const BoardData * bd, BoardData3d * bd3d)
{
    int show = DiceShowing(bd);

    bd3d->Occluders[OCC_DICE1].show = bd3d->Occluders[OCC_DICE2].show = show;
    if (show) {
        updateDieOccPos(bd, bd3d, &bd3d->Occluders[OCC_DICE1], 0);
        updateDieOccPos(bd, bd3d, &bd3d->Occluders[OCC_DICE2], 1);
    }
}

NTH_STATIC void
updateCubeOccPos(const BoardData * bd, BoardData3d * bd3d)
{
    getDoubleCubePos(bd, bd3d->Occluders[OCC_CUBE].trans);
    makeInverseTransposeMatrix(bd3d->Occluders[OCC_CUBE].invMat, bd3d->Occluders[OCC_CUBE].trans);

    bd3d->Occluders[OCC_CUBE].show = (bd->cube_use && !bd->crawford_game);
    if (ShadowsInitilised(bd3d))
        draw_shadow_volume_extruded_edges(&bd3d->Occluders[OCC_CUBE], bd3d->shadow_light_position, GL_QUADS);
}

void
updateMovingPieceOccPos(const BoardData * bd, BoardData3d * bd3d)
{
    if (bd->drag_point >= 0) {
        copyPoint(bd3d->Occluders[LAST_PIECE].trans, bd3d->dragPos);
        makeInverseTransposeMatrix(bd3d->Occluders[LAST_PIECE].invMat, bd3d->Occluders[LAST_PIECE].trans);
    } else {                    /* if (bd3d->moving) */

        copyPoint(bd3d->Occluders[LAST_PIECE].trans, bd3d->movingPos);

        if (bd3d->rotateMovingPiece > 0) {      /* rotate shadow as piece is put in bear off tray */
            float id[4][4];

            bd3d->Occluders[LAST_PIECE].rotator = 1;
            bd3d->Occluders[LAST_PIECE].rot[1] = -90 * bd3d->rotateMovingPiece * bd->turn;
            makeInverseTransposeMatrix(id, bd3d->Occluders[LAST_PIECE].trans);
            makeInverseRotateMatrixX(bd3d->Occluders[LAST_PIECE].invMat, bd3d->Occluders[LAST_PIECE].rot[1]);
            matrixmult(bd3d->Occluders[LAST_PIECE].invMat, (ConstMatrix) id);
        } else
            makeInverseTransposeMatrix(bd3d->Occluders[LAST_PIECE].invMat, bd3d->Occluders[LAST_PIECE].trans);
    }
    if (ShadowsInitilised(bd3d))
        draw_shadow_volume_extruded_edges(&bd3d->Occluders[LAST_PIECE], bd3d->shadow_light_position, GL_QUADS);
}

void
updatePieceOccPos(const BoardData * bd, BoardData3d * bd3d)
{
    unsigned int i, j, p = FIRST_PIECE;

    for (i = 0; i < 28; i++) {
        for (j = 1; j <= Abs(bd->points[i]); j++) {
            if (p > LAST_PIECE)
                break;          /* Found all pieces */
            getPiecePos(i, j, bd3d->Occluders[p].trans);

            if (i == 26 || i == 27) {   /* bars */
                float id[4][4];

                bd3d->Occluders[p].rotator = 1;
                if (i == 26)
                    bd3d->Occluders[p].rot[1] = -90;
                else
                    bd3d->Occluders[p].rot[1] = 90;
                makeInverseTransposeMatrix(id, bd3d->Occluders[p].trans);
                makeInverseRotateMatrixX(bd3d->Occluders[p].invMat, bd3d->Occluders[p].rot[1]);
                matrixmult(bd3d->Occluders[p].invMat, (ConstMatrix) id);
            } else {
                makeInverseTransposeMatrix(bd3d->Occluders[p].invMat, bd3d->Occluders[p].trans);
                bd3d->Occluders[p].rotator = 0;
            }
            if (ShadowsInitilised(bd3d))
                draw_shadow_volume_extruded_edges(&bd3d->Occluders[p], bd3d->shadow_light_position, GL_QUADS);

            p++;
        }
    }
    if (p == LAST_PIECE) {
        bd3d->Occluders[p].rotator = 0;
        updateMovingPieceOccPos(bd, bd3d);
    }
}

void
updateFlagOccPos(const BoardData * bd, BoardData3d * bd3d)
{
    if (bd->resigned) {
        freeOccluder(&bd3d->Occluders[OCC_FLAG]);
        initOccluder(&bd3d->Occluders[OCC_FLAG]);

        bd3d->Occluders[OCC_FLAG].show = 1;

        getFlagPos(bd, bd3d->Occluders[OCC_FLAG].trans);
        makeInverseTransposeMatrix(bd3d->Occluders[OCC_FLAG].invMat, bd3d->Occluders[OCC_FLAG].trans);

        /* Flag pole */
        addCube(&bd3d->Occluders[OCC_FLAG], -FLAGPOLE_WIDTH * 1.95f, -FLAG_HEIGHT, -LIFT_OFF,
                FLAGPOLE_WIDTH * 1.95f, FLAGPOLE_HEIGHT, LIFT_OFF * 2);

        /* Flag surface (approximation) */
        {
            int s;
            /* Change first ctlpoint to better match flag shape */
            float p1x = flag.ctlpoints[1][0][2];
            flag.ctlpoints[1][0][2] *= .7f;

            for (s = 0; s < S_NUMPOINTS - 1; s++) {     /* Reduce shadow size a bit to remove artifacts */
                float h = (flag.ctlpoints[s][1][1] - flag.ctlpoints[s][0][1]) * .92f - (FLAG_HEIGHT * .05f);
                float y = flag.ctlpoints[s][0][1] + FLAG_HEIGHT * .05f;
                float w = flag.ctlpoints[s + 1][0][0] - flag.ctlpoints[s][0][0];
                if (s == 2)
                    w *= .95f;
                addWonkyCube(&bd3d->Occluders[OCC_FLAG], flag.ctlpoints[s][0][0], y, flag.ctlpoints[s][0][2],
                             w, h, base_unit / 10.0f, flag.ctlpoints[s + 1][0][2] - flag.ctlpoints[s][0][2], s);
            }
            flag.ctlpoints[1][0][2] = p1x;
        }
        if (ShadowsInitilised(bd3d))
            draw_shadow_volume_extruded_edges(&bd3d->Occluders[OCC_FLAG], bd3d->shadow_light_position, GL_QUADS);
    } else {
        bd3d->Occluders[OCC_FLAG].show = 0;
    }
}

void
updateHingeOccPos(BoardData3d * bd3d, int show3dHinges)
{
    if (!ShadowsInitilised(bd3d))
        return;
    bd3d->Occluders[OCC_HINGE1].show = bd3d->Occluders[OCC_HINGE2].show = show3dHinges;
    draw_shadow_volume_extruded_edges(&bd3d->Occluders[OCC_HINGE1], bd3d->shadow_light_position, GL_QUADS);
    draw_shadow_volume_extruded_edges(&bd3d->Occluders[OCC_HINGE2], bd3d->shadow_light_position, GL_QUADS);
}

void
updateOccPos(const BoardData * bd)
{                               /* Make sure shadows are in correct place */
    if (ShadowsInitilised(bd->bd3d)) {
        updateCubeOccPos(bd, bd->bd3d);
        updateDiceOccPos(bd, bd->bd3d);
        updatePieceOccPos(bd, bd->bd3d);
    }
}

static void
MakeShadowModel(const BoardData * bd, BoardData3d * bd3d, const renderdata * prd)
{
    int i;
    if (!ShadowsInitilised(bd3d))
        return;
    TidyShadows(bd3d);

    initOccluder(&bd3d->Occluders[OCC_BOARD]);

    if (prd->roundedEdges) {
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], EDGE_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH,
                        TRAY_WIDTH - EDGE_WIDTH * 2 + BOARD_FILLET * 2, TRAY_HEIGHT - EDGE_HEIGHT + BOARD_FILLET * 2,
                        EDGE_DEPTH - LIFT_OFF);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], EDGE_WIDTH - BOARD_FILLET,
                        TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET, BASE_DEPTH,
                        TRAY_WIDTH - EDGE_WIDTH * 2 + BOARD_FILLET * 2, TRAY_HEIGHT - EDGE_HEIGHT + BOARD_FILLET * 2,
                        EDGE_DEPTH - LIFT_OFF);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET,
                        EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2 + BOARD_FILLET * 2,
                        TRAY_HEIGHT - EDGE_HEIGHT + BOARD_FILLET * 2, EDGE_DEPTH - LIFT_OFF);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - BOARD_FILLET,
                        TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT - BOARD_FILLET, BASE_DEPTH,
                        TRAY_WIDTH - EDGE_WIDTH * 2 + BOARD_FILLET * 2, TRAY_HEIGHT - EDGE_HEIGHT + BOARD_FILLET * 2,
                        EDGE_DEPTH - LIFT_OFF);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TRAY_WIDTH - BOARD_FILLET, EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH,
                        BOARD_WIDTH + BOARD_FILLET * 2, TOTAL_HEIGHT - EDGE_HEIGHT * 2 + BOARD_FILLET * 2,
                        EDGE_DEPTH - LIFT_OFF);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH - BOARD_FILLET,
                        EDGE_HEIGHT - BOARD_FILLET, BASE_DEPTH, BOARD_WIDTH + BOARD_FILLET * 2,
                        TOTAL_HEIGHT - EDGE_HEIGHT * 2 + BOARD_FILLET * 2, EDGE_DEPTH - LIFT_OFF);
        addSquare(&bd3d->Occluders[OCC_BOARD], BOARD_FILLET, BOARD_FILLET, 0.f, TOTAL_WIDTH - BOARD_FILLET * 2,
                  TOTAL_HEIGHT - BOARD_FILLET * 2, BASE_DEPTH + EDGE_DEPTH - LIFT_OFF);
    } else {
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2,
                        TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH,
                        TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH,
                        TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH,
                        TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2,
                        TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BOARD_WIDTH,
                        TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH);
        addClosedSquare(&bd3d->Occluders[OCC_BOARD], TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH,
                        BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH);
        addSquare(&bd3d->Occluders[OCC_BOARD], 0.f, 0.f, 0.f, TOTAL_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
    }
    setIdMatrix(bd3d->Occluders[OCC_BOARD].invMat);
    bd3d->Occluders[OCC_BOARD].trans[0] = bd3d->Occluders[OCC_BOARD].trans[1] = bd3d->Occluders[OCC_BOARD].trans[2] = 0;
    draw_shadow_volume_extruded_edges(&bd3d->Occluders[OCC_BOARD], bd3d->shadow_light_position, GL_QUADS);

    initOccluder(&bd3d->Occluders[OCC_HINGE1]);
    copyOccluder(&bd3d->Occluders[OCC_HINGE1], &bd3d->Occluders[OCC_HINGE2]);

    addHalfTube(&bd3d->Occluders[OCC_HINGE1], HINGE_WIDTH, HINGE_HEIGHT, prd->curveAccuracy / 2);

    bd3d->Occluders[OCC_HINGE1].trans[0] = bd3d->Occluders[OCC_HINGE2].trans[0] = (TOTAL_WIDTH) / 2.0f;
    bd3d->Occluders[OCC_HINGE1].trans[2] = bd3d->Occluders[OCC_HINGE2].trans[2] = BASE_DEPTH + EDGE_DEPTH;
    bd3d->Occluders[OCC_HINGE1].trans[1] = ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT) / 2.0f;
    bd3d->Occluders[OCC_HINGE2].trans[1] = ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT + TOTAL_HEIGHT) / 2.0f;

    makeInverseTransposeMatrix(bd3d->Occluders[OCC_HINGE1].invMat, bd3d->Occluders[OCC_HINGE1].trans);
    makeInverseTransposeMatrix(bd3d->Occluders[OCC_HINGE2].invMat, bd3d->Occluders[OCC_HINGE2].trans);

    updateHingeOccPos(bd3d, prd->fHinges3d);

    initOccluder(&bd3d->Occluders[OCC_CUBE]);
    addSquareCentered(&bd3d->Occluders[OCC_CUBE], 0.f, 0.f, 0.f, DOUBLECUBE_SIZE * .88f, DOUBLECUBE_SIZE * .88f,
                      DOUBLECUBE_SIZE * .88f);

    updateCubeOccPos(bd, bd3d);

    initOccluder(&bd3d->Occluders[OCC_DICE1]);
    addDice(&bd3d->Occluders[OCC_DICE1], getDiceSize(prd) / 2.0f);
    copyOccluder(&bd3d->Occluders[OCC_DICE1], &bd3d->Occluders[OCC_DICE2]);
    bd3d->Occluders[OCC_DICE1].rotator = bd3d->Occluders[OCC_DICE2].rotator = 1;
    updateDiceOccPos(bd, bd3d);

    initOccluder(&bd3d->Occluders[OCC_PIECE]);
    {
        float radius = PIECE_HOLE / 2.0f;
        float discradius = radius * 0.8f;
        float lip = radius - discradius;
        float height = PIECE_DEPTH - 2 * lip;

        addCylinder(&bd3d->Occluders[OCC_PIECE], 0.f, 0.f, lip, PIECE_HOLE / 2.0f - LIFT_OFF, height,
                    prd->curveAccuracy);
    }
    for (i = FIRST_PIECE; i <= LAST_PIECE; i++) {
        bd3d->Occluders[i].rot[0] = 0;
        bd3d->Occluders[i].rot[2] = 0;
        if (i != FIRST_PIECE)
            copyOccluder(&bd3d->Occluders[OCC_PIECE], &bd3d->Occluders[i]);
    }

    updatePieceOccPos(bd, bd3d);
    updateFlagOccPos(bd, bd3d);
}

static void
getCheqSize(renderdata * prd)
{
    unsigned int i, accuracy = (prd->curveAccuracy / 4) - 1;
    prd->acrossCheq = prd->downCheq = 1;
    for (i = 1; i < accuracy; i++) {
        if (((float) prd->acrossCheq) / prd->downCheq > .5)
            prd->downCheq++;
        else
            prd->acrossCheq++;
    }
}

void
preDraw3d(const BoardData * bd, BoardData3d * bd3d, renderdata * prd)
{
    if (!bd3d->qobjTex) {
        bd3d->qobjTex = gluNewQuadric();
        gluQuadricDrawStyle(bd3d->qobjTex, GLU_FILL);
        gluQuadricNormals(bd3d->qobjTex, GLU_FLAT);
        gluQuadricTexture(bd3d->qobjTex, GL_TRUE);
    }
    if (!bd3d->qobj) {
        bd3d->qobj = gluNewQuadric();
        gluQuadricDrawStyle(bd3d->qobj, GLU_FILL);
        gluQuadricNormals(bd3d->qobj, GLU_FLAT);
        gluQuadricTexture(bd3d->qobj, GL_FALSE);
    }

    if (bd3d->boardPoints)
        freeEigthPoints(&bd3d->boardPoints, prd->curveAccuracy);
    calculateEigthPoints(&bd3d->boardPoints, BOARD_FILLET, prd->curveAccuracy);

    preDrawPiece(bd3d, prd);
    preDrawDice(bd3d, prd);

    MakeShadowModel(bd, bd3d, prd);

    getCheqSize(prd);
}

void
RestrictiveDrawPiece(unsigned int pos, unsigned int depth)
{
    float newPos[3];
    getPiecePos(pos, depth, newPos);
    RestrictiveDrawFrame(newPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
}

static void
RestrictiveDoDrawDice(BoardData * bd, DiceShown dicePos)
{
    float pos[3];
    float diceSize = getDiceSize(bd->rd);
    float overSize = diceSize * 1.5f;
    ClipBox temp;
    DiceShown tempDiceShown;

    tempDiceShown = bd->diceShown;
    bd->diceShown = dicePos;

    getDicePos(bd, 0, pos);
    pos[2] -= diceSize / 2.0f;
    RestrictiveDrawFrame(pos, overSize, overSize, overSize);

    getDicePos(bd, 1, pos);
    pos[2] -= diceSize / 2.0f;
    RestrictiveDraw(&temp, pos, overSize, overSize, overSize);
    EnlargeCurrentToBox(&temp);

    bd->diceShown = tempDiceShown;
}

void
RestrictiveDrawDice(BoardData * bd)
{
    int tempTurn = 0;

    if (numRestrictFrames == -1)
        return;

    if (bd->rd->fDiceArea)
        RestrictiveDoDrawDice(bd, DICE_BELOW_BOARD);

    if (bd->diceShown != DICE_ON_BOARD) {
        tempTurn = bd->turn;
        bd->turn *= -1;
    }
    RestrictiveDoDrawDice(bd, DICE_ON_BOARD);

    if (tempTurn)
        bd->turn = tempTurn;
}

void
RestrictiveDrawCube(BoardData * bd, int old_doubled, int old_cube_owner)
{
    float pos[3];
    int temp_dob = bd->doubled, temp_cow = bd->cube_owner;
    bd->doubled = old_doubled;
    bd->cube_owner = old_cube_owner;
    getDoubleCubePos(bd, pos);
    pos[2] -= DOUBLECUBE_SIZE / 2.0f;
    RestrictiveDrawFrame(pos, DOUBLECUBE_SIZE, DOUBLECUBE_SIZE, DOUBLECUBE_SIZE);
    bd->doubled = temp_dob;
    bd->cube_owner = temp_cow;
    getDoubleCubePos(bd, pos);
    pos[2] -= DOUBLECUBE_SIZE / 2.0f;
    RestrictiveDrawFrame(pos, DOUBLECUBE_SIZE, DOUBLECUBE_SIZE, DOUBLECUBE_SIZE);
}

void
RestrictiveDrawMoveIndicator(BoardData * bd)
{                               /* Need to redraw both indicators */
    float pos[3];

    bd->turn *= -1;
    getMoveIndicatorPos(bd, pos);
    RestrictiveDrawFrame(pos, ARROW_SIZE, ARROW_SIZE, LIFT_OFF);

    bd->turn *= -1;
    getMoveIndicatorPos(bd, pos);
    RestrictiveDrawFrame(pos, ARROW_SIZE, ARROW_SIZE, LIFT_OFF);
}

void
RestrictiveDrawBoardNumbers(const BoardData3d * bd3d)
{
#define NUMBER_WIDTH (TOTAL_WIDTH - (2 * TRAY_WIDTH))
    float pos[3] = { TRAY_WIDTH + (NUMBER_WIDTH / 2.0f), TOTAL_HEIGHT - EDGE_HEIGHT + (EDGE_HEIGHT / 2.0f),
        BASE_DEPTH + EDGE_DEPTH
    };
    float textHeight = GetFontHeight3d(bd3d->numberFont);

    RestrictiveDrawFrame(pos, NUMBER_WIDTH, textHeight, LIFT_OFF);
    pos[1] = EDGE_HEIGHT / 2.0f;
    RestrictiveDrawFrame(pos, NUMBER_WIDTH, textHeight, LIFT_OFF);
}

void
RestrictiveDrawFlag(const BoardData * bd)
{
    float v[4];
    getFlagPos(bd, v);
    v[0] += FLAG_WIDTH / 2.0f - FLAGPOLE_WIDTH;
    v[2] -= FLAG_WIDTH / 2.0f;
    RestrictiveDrawFrame(v, FLAG_WIDTH, FLAGPOLE_HEIGHT, FLAG_WIDTH);
}

static void
drawBoardBase(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    if (!bd->grayBoard)
        drawTable(bd3d, prd);
    else
        drawTableGrayed(bd3d, *prd);

    if (prd->fLabels && !prd->fDynamicLabels)
        drawNumbers(bd);

    if (bd3d->State == BOARD_OPEN)
        tidyEdges(prd);

    if (bd->cube_use && !bd->crawford_game)
        drawDC(bd, bd3d, prd);
}

void
drawBoardTop(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    if (prd->fLabels && prd->fDynamicLabels)
        drawNumbers(bd);
    if (prd->showMoveIndicator)
        showMoveIndicator(bd);

    /* Draw things in correct order so transparency works correctly */
    /* First pieces, then dice, then moving pieces */
    drawPieces(bd, bd3d, prd);

    if (DiceShowing(bd)) {
        drawDie(bd, bd3d, 0);
        drawDie(bd, bd3d, 1);
    }

    if (bd3d->moving || bd->drag_point >= 0)
        drawSpecialPieces(bd, bd3d, prd);

    if (bd->resigned)
        drawFlag(bd, bd3d, prd);
}

void
drawBoard(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    drawBoardBase(bd, bd3d, prd);
    drawBoardTop(bd, bd3d, prd);
}

extern int renderingBase;
#ifdef WIN32
extern void
drawBasePreRender(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    if (bd->rd->showShadows) {
        renderingBase = TRUE;
        shadowDisplay(drawBoardBase, bd, bd3d, prd);
        renderingBase = FALSE;
    } else
        drawBoardBase(bd, bd3d, prd);

    GtkAllocation allocation;
    gtk_widget_get_allocation(bd3d->drawing_area3d, &allocation);
    SaveBufferRegion(bd3d->wglBuffer, 0, 0, allocation.width, allocation.height);
}
#endif
