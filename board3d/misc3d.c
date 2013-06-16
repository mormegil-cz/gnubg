/*
 * misc3d.c
 * by Jon Kinsey, 2003
 *
 * Helper functions for 3d drawing
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
#ifdef WIN32
#include <io.h>
#endif
#include "renderprefs.h"
#include "sound.h"
#include "export.h"
#include "gtkgame.h"
#ifdef WIN32
#include "wglbuffer.h"
#endif
#include "util.h"
#include <glib/gstdio.h>
#include "gtklocdefs.h"

#define MAX_FRAMES 10
#define DOT_SIZE 32
#define MAX_DIST ((DOT_SIZE / 2) * (DOT_SIZE / 2))
#define MIN_DIST ((DOT_SIZE / 2) * .70f * (DOT_SIZE / 2) * .70f)
#define TEXTURE_FILE "textures.txt"
#define TEXTURE_FILE_VERSION 3
#define BUF_SIZE 100

/* Performance test data */
static double testStartTime;
static int numFrames;
#define TEST_TIME 3000.0f

static int stopNextTime;
static int slide_move;
NTH_STATIC double animStartTime = 0;

static guint idleId = 0;
static idleFunc *pIdleFun;
static BoardData *pIdleBD;
Flag3d flag;                    /* Only one flag */

static gboolean
idle(BoardData3d * bd3d)
{
    if (pIdleFun(bd3d))
        DrawScene3d(bd3d);

    return TRUE;
}

void
StopIdle3d(const BoardData * bd, BoardData3d * bd3d)
{                               /* Animation has finished (or could have been interruptted) */
    /* If interruptted - reset dice/moving piece details */
    if (bd3d->shakingDice) {
        bd3d->shakingDice = 0;
        updateDiceOccPos(bd, bd3d);
        gtk_main_quit();
    }
    if (bd3d->moving) {
        bd3d->moving = 0;
        updatePieceOccPos(bd, bd3d);
        animation_finished = TRUE;
        gtk_main_quit();
    }

    if (idleId) {
        g_source_remove(idleId);
        idleId = 0;
    }
}

NTH_STATIC void
setIdleFunc(BoardData * bd, idleFunc * pFun)
{
    if (idleId) {
        g_source_remove(idleId);
        idleId = 0;
    }
    pIdleFun = pFun;
    pIdleBD = bd;

    idleId = g_idle_add((GSourceFunc) idle, bd->bd3d);
}

/* Test function to show normal direction */
#if 0
static void
CheckNormal()
{
    float len;
    GLfloat norms[3];
    glGetFloatv(GL_CURRENT_NORMAL, norms);

    len = sqrtf(norms[0] * norms[0] + norms[1] * norms[1] + norms[2] * norms[2]);
    if (fabs(len - 1) > 0.000001)
        len = len;              /*break here */
    norms[0] *= .1f;
    norms[1] *= .1f;
    norms[2] *= .1f;

    glBegin(GL_LINES);
    glVertex3f(0, 0, 0);
    glVertex3f(norms[0], norms[1], norms[2]);
    glEnd();
    glPointSize(5);
    glBegin(GL_POINTS);
    glVertex3f(norms[0], norms[1], norms[2]);
    glEnd();
}
#endif

void
CheckOpenglError(void)
{
    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR)
        g_print("OpenGL Error: %s\n", gluErrorString(glErr));
}

NTH_STATIC void
SetupLight3d(BoardData3d * bd3d, const renderdata * prd)
{
    float lp[4];
    float al[4], dl[4], sl[4];

    copyPoint(lp, prd->lightPos);
    lp[3] = (float) (prd->lightType == LT_POSITIONAL);

    /* If directioinal vector is from origin */
    if (lp[3] == 0) {
        lp[0] -= getBoardWidth() / 2.0f;
        lp[1] -= getBoardHeight() / 2.0f;
    }
    glLightfv(GL_LIGHT0, GL_POSITION, lp);

    al[0] = al[1] = al[2] = prd->lightLevels[0] / 100.0f;
    al[3] = 1;
    glLightfv(GL_LIGHT0, GL_AMBIENT, al);

    dl[0] = dl[1] = dl[2] = prd->lightLevels[1] / 100.0f;
    dl[3] = 1;
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dl);

    sl[0] = sl[1] = sl[2] = prd->lightLevels[2] / 100.0f;
    sl[3] = 1;
    glLightfv(GL_LIGHT0, GL_SPECULAR, sl);

    /* Shadow light position */
    memcpy(bd3d->shadow_light_position, lp, sizeof(float[4]));
    if (ShadowsInitilised(bd3d)) {
        int i;
        for (i = 0; i < NUM_OCC; i++)
            draw_shadow_volume_extruded_edges(&bd3d->Occluders[i], bd3d->shadow_light_position, GL_QUADS);
    }
}

#ifdef WIN32
/* Determine if a particular extension is supported */
typedef char *(WINAPI * fGetExtStr) (HDC);
extern int
extensionSupported(const char *extension)
{
    static const char *extensionsString = NULL;
    if (extensionsString == NULL)
        extensionsString = (const char *) glGetString(GL_EXTENSIONS);

    if ((extensionsString != NULL) && strstr(extensionsString, extension) != 0)
        return TRUE;

    if (StrNCaseCmp(extension, "WGL_", (gsize) strlen("WGL_")) == 0) {  /* Look for wgl extension */
        static const char *wglExtString = NULL;
        if (wglExtString == NULL) {
            fGetExtStr wglGetExtensionsStringARB;
            wglGetExtensionsStringARB = (fGetExtStr) wglGetProcAddress("wglGetExtensionsStringARB");
            if (!wglGetExtensionsStringARB)
                return FALSE;

            wglExtString = wglGetExtensionsStringARB(wglGetCurrentDC());
        }
        if (strstr(wglExtString, extension) != 0)
            return TRUE;
    }

    return FALSE;
}

#ifndef GL_VERSION_1_2
#ifndef GL_EXT_separate_specular_color
#define GL_LIGHT_MODEL_COLOR_CONTROL_EXT    0x81F8
#define GL_SEPARATE_SPECULAR_COLOR_EXT      0x81FA
#endif
#endif

typedef BOOL(APIENTRY * PFNWGLSWAPINTERVALFARPROC) (int);

extern int
setVSync(int state)
{
    static PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;

    if (state == -1)
        return FALSE;

    if (!wglSwapIntervalEXT) {
        if (!extensionSupported("WGL_EXT_swap_control"))
            return FALSE;

        wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC) wglGetProcAddress("wglSwapIntervalEXT");
    }
    if ((wglSwapIntervalEXT == NULL) || (wglSwapIntervalEXT(state) == 0))
        return FALSE;

    return TRUE;
}
#endif

static void
CreateTexture(unsigned int *pID, int width, int height, const unsigned char *bits)
{
    /* Create texture */
    glGenTextures(1, (GLuint *) pID);
    glBindTexture(GL_TEXTURE_2D, *pID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    /* Read bits */
    glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, bits);
}

static void
CreateDotTexture(unsigned int *pDotTexture)
{
    unsigned int i, j;
    unsigned char *data = (unsigned char *) malloc(sizeof(*data) * DOT_SIZE * DOT_SIZE * 3);
    unsigned char *pData = data;
    g_assert(pData);

    for (i = 0; i < DOT_SIZE; i++) {
        for (j = 0; j < DOT_SIZE; j++) {
            float xdiff = ((float) i) + .5f - DOT_SIZE / 2;
            float ydiff = ((float) j) + .5f - DOT_SIZE / 2;
            float dist = xdiff * xdiff + ydiff * ydiff;
            float percentage = 1 - ((dist - MIN_DIST) / (MAX_DIST - MIN_DIST));
            unsigned char value;
            if (percentage <= 0)
                value = 0;
            else if (percentage >= 1)
                value = 255;
            else
                value = (unsigned char) (255 * percentage);
            pData[0] = pData[1] = pData[2] = value;
            pData += 3;
        }
    }
    CreateTexture(pDotTexture, DOT_SIZE, DOT_SIZE, data);
    free(data);
}

int
CreateFonts(BoardData3d * bd3d)
{
    if (!CreateNumberFont(&bd3d->numberFont, FONT_VERA, FONT_PITCH, FONT_SIZE, FONT_HEIGHT_RATIO))
        return FALSE;

    if (!CreateNumberFont
        (&bd3d->cubeFont, FONT_VERA_SERIF_BOLD, CUBE_FONT_PITCH, CUBE_FONT_SIZE, CUBE_FONT_HEIGHT_RATIO))
        return FALSE;

    return TRUE;
}

void
InitGL(const BoardData * bd)
{
    float gal[4];
    /* Turn on light 0 */
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);

    /* No global ambient light */
    gal[0] = gal[1] = gal[2] = gal[3] = 0;
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gal);

    /* Local specular viewpoint */
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    /* Default to <= depth testing */
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    /* Back face culling */
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    /* Nice hints */
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    /* Default blend function for alpha-blending */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Generate normal co-ords for nurbs */
    glEnable(GL_AUTO_NORMAL);

    if (bd) {
        BoardData3d *bd3d = bd->bd3d;
        /* Setup some 3d things */
        if (!CreateFonts(bd3d))
            g_print("Error creating fonts\n");

        shadowInit(bd3d, bd->rd);
#ifdef GL_VERSION_1_2
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#else
        if (extensionSupported("GL_EXT_separate_specular_color"))
            glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL_EXT, GL_SEPARATE_SPECULAR_COLOR_EXT);
#endif
        CreateDotTexture(&bd3d->dotTexture);
#ifdef WIN32
        {
            static int UseBufferRegions = -1;
            if (UseBufferRegions == -1)
                UseBufferRegions = wglBufferInitialize();
#if 0                           /* Remove this until option added to control this */
            if (UseBufferRegions == 1) {
                bd3d->wglBuffer = CreateBufferRegion(WGL_BACK_COLOR_BUFFER_BIT_ARB | WGL_DEPTH_BUFFER_BIT_ARB);
                bd3d->fBuffers = (bd->bd3d->wglBuffer != NULL);
            }
#endif
        }
#endif
    }
}

void
setMaterial(const Material * pMat)
{
    glMaterialfv(GL_FRONT, GL_AMBIENT, pMat->ambientColour);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, pMat->diffuseColour);
    glMaterialfv(GL_FRONT, GL_SPECULAR, pMat->specularColour);
    glMateriali(GL_FRONT, GL_SHININESS, pMat->shine);

    if (pMat->pTexture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, pMat->pTexture->texID);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
}

float
Dist2d(float a, float b)
{                               /* Find 3rd side size */
    float sqdD = a * a - b * b;
    if (sqdD > 0)
        return sqrtf(sqdD);
    else
        return 0;
}

/* Texture functions */

static GList *textures;

static const char *TextureTypeStrs[TT_COUNT] = { "general", "piece", "hinge" };

GList *
GetTextureList(TextureType type)
{
    GList *glist = NULL;
    GList *pl;
    glist = g_list_append(glist, NO_TEXTURE_STRING);

    for (pl = textures; pl; pl = pl->next) {
        TextureInfo *text = (TextureInfo *) pl->data;
        if (text->type == type)
            glist = g_list_append(glist, text->name);
    }
    return glist;
}

void
FindNamedTexture(TextureInfo ** textureInfo, char *name)
{
    GList *pl;
    for (pl = textures; pl; pl = pl->next) {
        TextureInfo *text = (TextureInfo *) pl->data;
        if (!StrCaseCmp(text->name, name)) {
            *textureInfo = text;
            return;
        }
    }
    *textureInfo = 0;
    /* Only warn user if textures.txt file has been loaded */
    if (g_list_length(textures) > 0)
        g_print("Texture %s not in texture info file\n", name);
}

void
FindTexture(TextureInfo ** textureInfo, const char *file)
{
    GList *pl;
    for (pl = textures; pl; pl = pl->next) {
        TextureInfo *text = (TextureInfo *) pl->data;
        if (!StrCaseCmp(text->file, file)) {
            *textureInfo = text;
            return;
        }
    }
    {                           /* Not in texture list, see if old texture on disc */
        char *szFile = BuildFilename(file);
        if (szFile && g_file_test(szFile, G_FILE_TEST_IS_REGULAR)) {
            /* Add entry for unknown texture */
            TextureInfo text;
            strcpy(text.file, file);
            strcpy(text.name, file);
            text.type = TT_NONE;        /* Don't show in lists */

            *textureInfo = (TextureInfo *) malloc(sizeof(TextureInfo));
            **textureInfo = text;

            textures = g_list_append(textures, *textureInfo);

            return;
        }
        g_free(szFile);
    }

    *textureInfo = 0;
    /* Only warn user if in 3d */
    if (display_is_3d(GetMainAppearance()))
        g_print("Texture %s not in texture info file\n", file);
}

void
LoadTextureInfo(void)
{
    FILE *fp;
    gchar *szFile;
    char buf[BUF_SIZE];

    textures = NULL;

    szFile = BuildFilename(TEXTURE_FILE);
    fp = g_fopen(szFile, "r");
    g_free(szFile);

    if (!fp) {
        g_print("Error: Texture file (%s) not found\n", TEXTURE_FILE);
        return;
    }

    if (!fgets(buf, BUF_SIZE, fp) || atoi(buf) != TEXTURE_FILE_VERSION) {
        g_print("Error: Texture file (%s) out of date\n", TEXTURE_FILE);
        fclose(fp);
        return;
    }

    do {
        int err, found, i, val;
        size_t len;
        TextureInfo text;

        err = 0;

        /* filename */
        if (!fgets(buf, BUF_SIZE, fp))
            break;              /* finished */

        len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            len--;
            buf[len] = '\0';
        }
        if (len > FILENAME_SIZE) {
            g_print("Texture filename %s too big, maximum length %d.  Entry ignored.\n", buf, FILENAME_SIZE);
            err = 1;
            strcpy(text.file, "");
        } else
            strcpy(text.file, buf);

        /* name */
        if (!fgets(buf, BUF_SIZE, fp)) {
            g_print("Error in texture file info.\n");
            fclose(fp);
            return;
        }
        len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            len--;
            buf[len] = '\0';
        }
        if (len > NAME_SIZE) {
            g_print("Texture name %s too big, maximum length %d.  Entry ignored.\n", buf, NAME_SIZE);
            err = 1;
        } else
            strcpy(text.name, buf);

        /* type */
        if (!fgets(buf, BUF_SIZE, fp)) {
            g_print("Error in texture file info.\n");
            fclose(fp);
            return;
        }
        len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            len--;
            buf[len] = '\0';
        }
        found = -1;
        val = 2;
        for (i = 0; i < TT_COUNT; i++) {
            if (!StrCaseCmp(buf, TextureTypeStrs[i])) {
                found = i;
                break;
            }
            val *= 2;
        }
        if (found == -1) {
            g_print("Unknown texture type %s.  Entry ignored.\n", buf);
            err = 1;
        } else
            text.type = (TextureType) val;

        if (!err) {             /* Add texture type */
            TextureInfo *pNewText = (TextureInfo *) malloc(sizeof(TextureInfo));
            g_assert(pNewText);
            *pNewText = text;
            textures = g_list_append(textures, pNewText);
        }
    } while (!feof(fp));
    fclose(fp);
}

static void
DeleteTexture(Texture * texture)
{
    if (texture->texID)
        glDeleteTextures(1, (GLuint *) & texture->texID);

    texture->texID = 0;
}

int
LoadTexture(Texture * texture, const char *filename)
{
    unsigned char *bits = 0;
    int n;
    GError *pix_error = NULL;
    GdkPixbuf *fpixbuf, *pixbuf;

    if (!filename)
        return 0;

    if (g_file_test(filename, G_FILE_TEST_EXISTS))
        fpixbuf = gdk_pixbuf_new_from_file(filename, &pix_error);
    else {
        gchar *tmp = BuildFilename(filename);
        fpixbuf = gdk_pixbuf_new_from_file(tmp, &pix_error);
        g_free(tmp);
    }

    if (pix_error) {
        g_print("Failed to open texture: %s\n", filename);
        return 0;               /* failed to load file */
    }

    pixbuf = gdk_pixbuf_flip(fpixbuf, FALSE);
    g_object_unref(fpixbuf);

    bits = gdk_pixbuf_get_pixels(pixbuf);

    texture->width = gdk_pixbuf_get_width(pixbuf);
    texture->height = gdk_pixbuf_get_height(pixbuf);

    if (!bits) {
        g_print("Failed to load texture: %s\n", filename);
        return 0;               /* failed to load file */
    }

    if (texture->width != texture->height) {
        g_print("Failed to load texture %s. width (%d) different to height (%d)\n",
                filename, texture->width, texture->height);
        return 0;               /* failed to load file */
    }
    /* Check size is a power of 2 */
    frexp((double) texture->width, &n);
    if (texture->width != powi(2, n - 1)) {
        g_print("Failed to load texture %s, size (%d) isn't a power of 2\n", filename, texture->width);
        return 0;               /* failed to load file */
    }

    CreateTexture(&texture->texID, texture->width, texture->height, bits);

    g_object_unref(pixbuf);

    return 1;
}

static
    void
SetTexture(BoardData3d * bd3d, Material * pMat, const char *filename)
{
    /* See if already loaded */
    int i;
    const char *nameStart = filename;
    /* Find start of name in filename */
    const char *newStart = 0;

    do {
        if ((newStart = strchr(nameStart, '\\')) == NULL)
            newStart = strchr(nameStart, '/');

        if (newStart)
            nameStart = newStart + 1;
    } while (newStart);

    /* Search for name in cached list */
    for (i = 0; i < bd3d->numTextures; i++) {
        if (!StrCaseCmp(nameStart, bd3d->textureName[i])) {     /* found */
            pMat->pTexture = &bd3d->textureList[i];
            return;
        }
    }

    /* Not found - Load new texture */
    if (bd3d->numTextures == MAX_TEXTURES - 1) {
        g_print("Error: Too many textures loaded...\n");
        return;
    }

    if (LoadTexture(&bd3d->textureList[bd3d->numTextures], filename)) {
        /* Remeber name */
        bd3d->textureName[bd3d->numTextures] = (char *) malloc(strlen(nameStart) + 1);
        strcpy(bd3d->textureName[bd3d->numTextures], nameStart);

        pMat->pTexture = &bd3d->textureList[bd3d->numTextures];
        bd3d->numTextures++;
    }
}

static void
GetTexture(BoardData3d * bd3d, Material * pMat)
{
    if (pMat->textureInfo) {
        char buf[100];
        strcpy(buf, TEXTURE_PATH);
        strcat(buf, pMat->textureInfo->file);
        SetTexture(bd3d, pMat, buf);
    } else
        pMat->pTexture = 0;
}

void
GetTextures(BoardData3d * bd3d, renderdata * prd)
{
    GetTexture(bd3d, &prd->ChequerMat[0]);
    GetTexture(bd3d, &prd->ChequerMat[1]);
    GetTexture(bd3d, &prd->BaseMat);
    GetTexture(bd3d, &prd->PointMat[0]);
    GetTexture(bd3d, &prd->PointMat[1]);
    GetTexture(bd3d, &prd->BoxMat);
    GetTexture(bd3d, &prd->HingeMat);
    GetTexture(bd3d, &prd->BackGroundMat);
}

void
Set3dSettings(renderdata * prdnew, const renderdata * prd)
{
    prdnew->pieceType = prd->pieceType;
    prdnew->pieceTextureType = prd->pieceTextureType;
    prdnew->fHinges3d = prd->fHinges3d;
    prdnew->showMoveIndicator = prd->showMoveIndicator;
    prdnew->showShadows = prd->showShadows;
    prdnew->quickDraw = prd->quickDraw;
    prdnew->roundedEdges = prd->roundedEdges;
    prdnew->bgInTrays = prd->bgInTrays;
    prdnew->roundedPoints = prd->roundedPoints;
    prdnew->shadowDarkness = prd->shadowDarkness;
    prdnew->curveAccuracy = prd->curveAccuracy;
    prdnew->skewFactor = prd->skewFactor;
    prdnew->boardAngle = prd->boardAngle;
    prdnew->diceSize = prd->diceSize;
    prdnew->planView = prd->planView;

    memcpy(prdnew->ChequerMat, prd->ChequerMat, sizeof(Material[2]));
    memcpy(&prdnew->DiceMat[0], prd->afDieColour3d[0] ? &prd->ChequerMat[0] : &prd->DiceMat[0], sizeof(Material));
    memcpy(&prdnew->DiceMat[1], prd->afDieColour3d[1] ? &prd->ChequerMat[1] : &prd->DiceMat[1], sizeof(Material));
    prdnew->DiceMat[0].textureInfo = prdnew->DiceMat[1].textureInfo = 0;
    prdnew->DiceMat[0].pTexture = prdnew->DiceMat[1].pTexture = 0;
    memcpy(prdnew->DiceDotMat, prd->DiceDotMat, sizeof(Material[2]));

    memcpy(&prdnew->CubeMat, &prd->CubeMat, sizeof(Material));
    memcpy(&prdnew->CubeNumberMat, &prd->CubeNumberMat, sizeof(Material));

    memcpy(&prdnew->BaseMat, &prd->BaseMat, sizeof(Material));
    memcpy(prdnew->PointMat, prd->PointMat, sizeof(Material[2]));

    memcpy(&prdnew->BoxMat, &prd->BoxMat, sizeof(Material));
    memcpy(&prdnew->HingeMat, &prd->HingeMat, sizeof(Material));
    memcpy(&prdnew->PointNumberMat, &prd->PointNumberMat, sizeof(Material));
    memcpy(&prdnew->BackGroundMat, &prd->BackGroundMat, sizeof(Material));
}

/* Return v position, d distance along path segment */
static float
moveAlong(float d, PathType type, const float start[3], const float end[3], float v[3], float *rotate)
{
    float per, lineLen;

    if (type == PATH_LINE) {
        float xDiff = end[0] - start[0];
        float yDiff = end[1] - start[1];
        float zDiff = end[2] - start[2];

        lineLen = sqrtf(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff);
        if (d <= lineLen) {
            per = d / lineLen;
            v[0] = start[0] + xDiff * per;
            v[1] = start[1] + yDiff * per;
            v[2] = start[2] + zDiff * per;

            return -1;
        }
    } else if (type == PATH_PARABOLA) {
        float dist = end[0] - start[0];
        lineLen = fabsf(dist);
        if (d <= lineLen) {
            v[0] = start[0] + d * (dist / lineLen);
            v[1] = start[1];
            v[2] = start[2] + 10 * (-d * d + lineLen * d);

            return -1;
        }
    } else if (type == PATH_PARABOLA_12TO3) {
        float dist = end[0] - start[0];
        lineLen = fabsf(dist);
        if (d <= lineLen) {
            v[0] = start[0] + d * (dist / lineLen);
            v[1] = start[1];
            d += lineLen;
            v[2] = start[2] + 10 * (-d * d + lineLen * 2 * d) - 10 * lineLen * lineLen;
            return -1;
        }
    } else {
        float xCent, zCent, xRad, zRad;
        float yOff, yDiff = end[1] - start[1];

        xRad = end[0] - start[0];
        zRad = end[2] - start[2];
        lineLen = (float) G_PI *((fabsf(xRad) + fabsf(zRad)) / 2.0f) / 2.0f;
        if (d <= lineLen) {
            per = d / lineLen;
            if (rotate)
                *rotate = per;

            if (type == PATH_CURVE_9TO12) {
                xCent = end[0];
                zCent = start[2];
                yOff = yDiff * cosf((G_PI / 2.0f) * per);
            } else {
                xCent = start[0];
                zCent = end[2];
                yOff = yDiff * sinf((G_PI / 2.0f) * per);
            }

            if (type == PATH_CURVE_9TO12) {
                v[0] = xCent - xRad * cosf((G_PI / 2.0f) * per);
                v[1] = end[1] - yOff;
                v[2] = zCent + zRad * sinf((G_PI / 2.0f) * per);
            } else {
                v[0] = xCent + xRad * sinf((G_PI / 2.0f) * per);
                v[1] = start[1] + yOff;
                v[2] = zCent - zRad * cosf((G_PI / 2.0f) * per);
            }
            return -1;
        }
    }
    /* Finished path segment */
    return lineLen;
}

/* Return v position, d distance along path p */
int
movePath(Path * p, float d, float *rotate, float v[3])
{
    float done;
    d -= p->mileStone;

    while (!finishedPath(p) && (done = moveAlong(d, p->pathType[p->state], p->pts[p->state], p->pts[p->state + 1], v, rotate)) != -1) { /* Next path segment */
        d -= done;
        p->mileStone += done;
        p->state++;
    }
    return !finishedPath(p);
}

void
initPath(Path * p, const float start[3])
{
    p->state = 0;
    p->numSegments = 0;
    p->mileStone = 0;
    copyPoint(p->pts[0], start);
}

int
finishedPath(const Path * p)
{
    return (p->state == p->numSegments);
}

void
addPathSegment(Path * p, PathType type, const float point[3])
{
    if (type == PATH_PARABOLA_12TO3) {  /* Move start y up to top of parabola */
        float l = p->pts[p->numSegments][0] - point[0];
        p->pts[p->numSegments][2] += 10 * l * l;
    }

    p->pathType[p->numSegments] = type;
    p->numSegments++;
    copyPoint(p->pts[p->numSegments], point);
}

/* Return a random number in 0-range */
float
randRange(float range)
{
    return range * ((float) rand() / (float) RAND_MAX);
}

/* Setup dice test to initial pos */
void
initDT(diceTest * dt, int x, int y, int z)
{
    dt->top = 0;
    dt->bottom = 5;

    dt->side[0] = 3;
    dt->side[1] = 1;
    dt->side[2] = 2;
    dt->side[3] = 4;

    /* Simulate rotations to determine actual dice position */
    while (x--) {
        int temp = dt->top;
        dt->top = dt->side[0];
        dt->side[0] = dt->bottom;
        dt->bottom = dt->side[2];
        dt->side[2] = temp;
    }
    while (y--) {
        int temp = dt->top;
        dt->top = dt->side[1];
        dt->side[1] = dt->bottom;
        dt->bottom = dt->side[3];
        dt->side[3] = temp;
    }
    while (z--) {
        int temp = dt->side[0];
        dt->side[0] = dt->side[3];
        dt->side[3] = dt->side[2];
        dt->side[2] = dt->side[1];
        dt->side[1] = temp;
    }
}

float ***
Alloc3d(unsigned int x, unsigned int y, unsigned int z)
{                               /* Allocate 3d array */
    unsigned int i, j;
    float ***array = (float ***) malloc(sizeof(float **) * x);
    g_assert(array);
    for (i = 0; i < x; i++) {
        array[i] = (float **) malloc(sizeof(float *) * y);
        for (j = 0; j < y; j++)
            array[i][j] = (float *) malloc(sizeof(float) * z);
    }
    return array;
}

void
Free3d(float ***array, unsigned int x, unsigned int y)
{                               /* Free 3d array */
    unsigned int i, j;
    for (i = 0; i < x; i++) {
        for (j = 0; j < y; j++)
            free(array[i][j]);
        free(array[i]);
    }
    free(array);
}

void
cylinder(float radius, float height, unsigned int accuracy, const Texture * texture)
{
    unsigned int i;
    float angle = 0;
    float circum = (float) G_PI * radius * 2 / (accuracy + 1);
    float step = (2 * (float) G_PI) / accuracy;
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < accuracy + 1; i++) {
        glNormal3f(sinf(angle), cosf(angle), 0.f);
        if (texture)
            glTexCoord2f(circum * i / (radius * 2), 0.f);
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, 0.f);

        if (texture)
            glTexCoord2f(circum * i / (radius * 2), height / (radius * 2));
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);

        angle += step;
    }
    glEnd();
}

void
circleOutlineOutward(float radius, float height, unsigned int accuracy)
{                               /* Draw an ouline of a disc in current z plane with outfacing normals */
    unsigned int i;
    float angle, step;

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_LINE_STRIP);
    for (i = 0; i <= accuracy; i++) {
        glNormal3f(sinf(angle), cosf(angle), 0.f);
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);
        angle -= step;
    }
    glEnd();
}

void
circleOutline(float radius, float height, unsigned int accuracy)
{                               /* Draw an ouline of a disc in current z plane */
    unsigned int i;
    float angle, step;

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_LINE_STRIP);
    for (i = 0; i <= accuracy; i++) {
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);
        angle -= step;
    }
    glEnd();
}

void
circle(float radius, float height, unsigned int accuracy)
{                               /* Draw a disc in current z plane */
    unsigned int i;
    float angle, step;

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.f, 0.f, height);
    for (i = 0; i <= accuracy; i++) {
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);
        angle -= step;
    }
    glEnd();
}

void
circleSloped(float radius, float startHeight, float endHeight, unsigned int accuracy)
{                               /* Draw a disc in sloping z plane */
    unsigned int i;
    float angle, step;

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.f, 0.f, startHeight);
    for (i = 0; i <= accuracy; i++) {
        float height = ((cosf(angle) + 1) / 2) * (endHeight - startHeight);
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, startHeight + height);
        angle -= step;
    }
    glEnd();
}

void
circleRev(float radius, float height, unsigned int accuracy)
{                               /* Draw a disc with reverse winding in current z plane */
    unsigned int i;
    float angle, step;

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.f, 0.f, height);
    for (i = 0; i <= accuracy; i++) {
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);
        angle += step;
    }
    glEnd();
}

void
circleTex(float radius, float height, unsigned int accuracy, const Texture * texture)
{                               /* Draw a disc in current z plane with a texture */
    unsigned int i;
    float angle, step;

    if (!texture) {
        circle(radius, height, accuracy);
        return;
    }

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(.5f, .5f);
    glVertex3f(0.f, 0.f, height);
    for (i = 0; i <= accuracy; i++) {
        glTexCoord2f((sinf(angle) * radius + radius) / (radius * 2), (cosf(angle) * radius + radius) / (radius * 2));
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);
        angle -= step;
    }
    glEnd();
}

void
circleRevTex(float radius, float height, unsigned int accuracy, const Texture * texture)
{                               /* Draw a disc with reverse winding in current z plane with a texture */
    unsigned int i;
    float angle, step;

    if (!texture) {
        circleRev(radius, height, accuracy);
        return;
    }

    step = (2 * (float) G_PI) / accuracy;
    angle = 0;
    glNormal3f(0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(.5f, .5f);
    glVertex3f(0.f, 0.f, height);
    for (i = 0; i <= accuracy; i++) {
        glTexCoord2f((sinf(angle) * radius + radius) / (radius * 2), (cosf(angle) * radius + radius) / (radius * 2));
        glVertex3f(sinf(angle) * radius, cosf(angle) * radius, height);
        angle += step;
    }
    glEnd();
}

void
drawBox(int type, float x, float y, float z, float w, float h, float d, const Texture * texture)
{                               /* Draw a box with normals and optional textures */
    float repX, repY;
    float normX, normY, normZ;
    float w2 = w / 2.0f, h2 = h / 2.0f, d2 = d / 2.0f;

    glPushMatrix();
    glTranslatef(x + w2, y + h2, z + d2);
    glScalef(w2, h2, d2);

    /* Scale normals */
    normX = w2;
    normY = h2;
    normZ = d2;

    glBegin(GL_QUADS);

    if (texture) {
        repX = (w * TEXTURE_SCALE) / texture->width;
        repY = (h * TEXTURE_SCALE) / texture->height;

        /* Front Face */
        glNormal3f(0.f, 0.f, normZ);
        if (type & BOX_SPLITTOP) {
            glTexCoord2f(0.f, 0.f);
            glVertex3f(-1.f, -1.f, 1.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(1.f, -1.f, 1.f);
            glTexCoord2f(repX, repY / 2.0f);
            glVertex3f(1.f, 0.f, 1.f);
            glTexCoord2f(0.f, repY / 2.0f);
            glVertex3f(-1.f, 0.f, 1.f);

            glTexCoord2f(0.f, repY / 2.0f);
            glVertex3f(-1.f, 0.f, 1.f);
            glTexCoord2f(repX, repY / 2.0f);
            glVertex3f(1.f, 0.f, 1.f);
            glTexCoord2f(repX, repY);
            glVertex3f(1.f, 1.f, 1.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(-1.f, 1.f, 1.f);
        } else if (type & BOX_SPLITWIDTH) {
            glTexCoord2f(0.f, 0.f);
            glVertex3f(-1.f, -1.f, 1.f);
            glTexCoord2f(repX / 2.0f, 0.f);
            glVertex3f(0.f, -1.f, 1.f);
            glTexCoord2f(repX / 2.0f, repY);
            glVertex3f(0.f, 1.f, 1.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(-1.f, 1.f, 1.f);

            glTexCoord2f(repX / 2.0f, 0.f);
            glVertex3f(0.f, -1.f, 1.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(1.f, -1.f, 1.f);
            glTexCoord2f(repX, repY);
            glVertex3f(1.f, 1.f, 1.f);
            glTexCoord2f(repX / 2.0f, repY);
            glVertex3f(0.f, 1.f, 1.f);
        } else {
            glTexCoord2f(0.f, 0.f);
            glVertex3f(-1.f, -1.f, 1.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(1.f, -1.f, 1.f);
            glTexCoord2f(repX, repY);
            glVertex3f(1.f, 1.f, 1.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(-1.f, 1.f, 1.f);
        }
        if (!(type & BOX_NOENDS)) {
            /* Top Face */
            glNormal3f(0.f, normY, 0.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(-1.f, 1.f, -1.f);
            glTexCoord2f(0.f, 0.f);
            glVertex3f(-1.f, 1.f, 1.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(1.f, 1.f, 1.f);
            glTexCoord2f(repX, repY);
            glVertex3f(1.f, 1.f, -1.f);
            /* Bottom Face */
            glNormal3f(0.f, -normY, 0.f);
            glTexCoord2f(repX, repY);
            glVertex3f(-1.f, -1.f, -1.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(1.f, -1.f, -1.f);
            glTexCoord2f(0.f, 0.f);
            glVertex3f(1.f, -1.f, 1.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(-1.f, -1.f, 1.f);
        }
        if (!(type & BOX_NOSIDES)) {
            /* Right face */
            glNormal3f(normX, 0.f, 0.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(1.f, -1.f, -1.f);
            glTexCoord2f(repX, repY);
            glVertex3f(1.f, 1.f, -1.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(1.f, 1.f, 1.f);
            glTexCoord2f(0.f, 0.f);
            glVertex3f(1.f, -1.f, 1.f);
            /* Left Face */
            glNormal3f(-normX, 0.f, 0.f);
            glTexCoord2f(0.f, 0.f);
            glVertex3f(-1.f, -1.f, -1.f);
            glTexCoord2f(repX, 0.f);
            glVertex3f(-1.f, -1.f, 1.f);
            glTexCoord2f(repX, repY);
            glVertex3f(-1.f, 1.f, 1.f);
            glTexCoord2f(0.f, repY);
            glVertex3f(-1.f, 1.f, -1.f);
        }
    } else {                    /* no texture co-ords */
        /* Front Face */
        glNormal3f(0.f, 0.f, normZ);
        if (type & BOX_SPLITTOP) {
            glVertex3f(-1.f, -1.f, 1.f);
            glVertex3f(1.f, -1.f, 1.f);
            glVertex3f(1.f, 0.f, 1.f);
            glVertex3f(-1.f, 0.f, 1.f);

            glVertex3f(-1.f, 0.f, 1.f);
            glVertex3f(1.f, 0.f, 1.f);
            glVertex3f(1.f, 1.f, 1.f);
            glVertex3f(-1.f, 1.f, 1.f);
        } else if (type & BOX_SPLITWIDTH) {
            glVertex3f(-1.f, -1.f, 1.f);
            glVertex3f(0.f, -1.f, 1.f);
            glVertex3f(0.f, 1.f, 1.f);
            glVertex3f(-1.f, 1.f, 1.f);

            glVertex3f(0.f, -1.f, 1.f);
            glVertex3f(1.f, -1.f, 1.f);
            glVertex3f(1.f, 1.f, 1.f);
            glVertex3f(0.f, 1.f, 1.f);
        } else {
            glVertex3f(-1.f, -1.f, 1.f);
            glVertex3f(1.f, -1.f, 1.f);
            glVertex3f(1.f, 1.f, 1.f);
            glVertex3f(-1.f, 1.f, 1.f);
        }

        if (!(type & BOX_NOENDS)) {
            /* Top Face */
            glNormal3f(0.f, normY, 0.f);
            glVertex3f(-1.f, 1.f, -1.f);
            glVertex3f(-1.f, 1.f, 1.f);
            glVertex3f(1.f, 1.f, 1.f);
            glVertex3f(1.f, 1.f, -1.f);
            /* Bottom Face */
            glNormal3f(0.f, -normY, 0.f);
            glVertex3f(-1.f, -1.f, -1.f);
            glVertex3f(1.f, -1.f, -1.f);
            glVertex3f(1.f, -1.f, 1.f);
            glVertex3f(-1.f, -1.f, 1.f);
        }
        if (!(type & BOX_NOSIDES)) {
            /* Right face */
            glNormal3f(normX, 0.f, 0.f);
            glVertex3f(1.f, -1.f, -1.f);
            glVertex3f(1.f, 1.f, -1.f);
            glVertex3f(1.f, 1.f, 1.f);
            glVertex3f(1.f, -1.f, 1.f);
            /* Left Face */
            glNormal3f(-normX, 0.f, 0.f);
            glVertex3f(-1.f, -1.f, -1.f);
            glVertex3f(-1.f, -1.f, 1.f);
            glVertex3f(-1.f, 1.f, 1.f);
            glVertex3f(-1.f, 1.f, -1.f);
        }
    }
    glEnd();
    glPopMatrix();
}

void
drawCube(float size)
{                               /* Draw a simple cube */
    glPushMatrix();
    glScalef(size / 2.0f, size / 2.0f, size / 2.0f);

    glBegin(GL_QUADS);
    /* Front Face */
    glVertex3f(-1.f, -1.f, 1.f);
    glVertex3f(1.f, -1.f, 1.f);
    glVertex3f(1.f, 1.f, 1.f);
    glVertex3f(-1.f, 1.f, 1.f);
    /* Top Face */
    glVertex3f(-1.f, 1.f, -1.f);
    glVertex3f(-1.f, 1.f, 1.f);
    glVertex3f(1.f, 1.f, 1.f);
    glVertex3f(1.f, 1.f, -1.f);
    /* Bottom Face */
    glVertex3f(-1.f, -1.f, -1.f);
    glVertex3f(1.f, -1.f, -1.f);
    glVertex3f(1.f, -1.f, 1.f);
    glVertex3f(-1.f, -1.f, 1.f);
    /* Right face */
    glVertex3f(1.f, -1.f, -1.f);
    glVertex3f(1.f, 1.f, -1.f);
    glVertex3f(1.f, 1.f, 1.f);
    glVertex3f(1.f, -1.f, 1.f);
    /* Left Face */
    glVertex3f(-1.f, -1.f, -1.f);
    glVertex3f(-1.f, -1.f, 1.f);
    glVertex3f(-1.f, 1.f, 1.f);
    glVertex3f(-1.f, 1.f, -1.f);
    glEnd();

    glPopMatrix();
}

void
drawRect(float x, float y, float z, float w, float h, const Texture * texture)
{                               /* Draw a rectangle */
    float repX, repY, tuv;

    glPushMatrix();
    glTranslatef(x + w / 2, y + h / 2, z);
    glScalef(w / 2.0f, h / 2.0f, 1.f);
    glNormal3f(0.f, 0.f, 1.f);

    if (texture) {
        tuv = TEXTURE_SCALE / texture->width;
        repX = w * tuv;
        repY = h * tuv;

        glBegin(GL_QUADS);
        glTexCoord2f(0.f, 0.f);
        glVertex3f(-1.f, -1.f, 0.f);
        glTexCoord2f(repX, 0.f);
        glVertex3f(1.f, -1.f, 0.f);
        glTexCoord2f(repX, repY);
        glVertex3f(1.f, 1.f, 0.f);
        glTexCoord2f(0.f, repY);
        glVertex3f(-1.f, 1.f, 0.f);
        glEnd();
    } else {
        glBegin(GL_QUADS);
        glVertex3f(-1.f, -1.f, 0.f);
        glVertex3f(1.f, -1.f, 0.f);
        glVertex3f(1.f, 1.f, 0.f);
        glVertex3f(-1.f, 1.f, 0.f);
        glEnd();
    }

    glPopMatrix();
}

void
drawSplitRect(float x, float y, float z, float w, float h, const Texture * texture)
{                               /* Draw a rectangle in 2 bits */
    float repX, repY, tuv;

    glPushMatrix();
    glTranslatef(x + w / 2, y + h / 2, z);
    glScalef(w / 2.0f, h / 2.0f, 1.f);
    glNormal3f(0.f, 0.f, 1.f);

    if (texture) {
        tuv = TEXTURE_SCALE / texture->width;
        repX = w * tuv;
        repY = h * tuv;

        glBegin(GL_QUADS);
        glTexCoord2f(0.f, 0.f);
        glVertex3f(-1.f, -1.f, 0.f);
        glTexCoord2f(repX, 0.f);
        glVertex3f(1.f, -1.f, 0.f);
        glTexCoord2f(repX, repY / 2.0f);
        glVertex3f(1.f, 0.f, 0.f);
        glTexCoord2f(0.f, repY / 2.0f);
        glVertex3f(-1.f, 0.f, 0.f);

        glTexCoord2f(0.f, repY / 2.0f);
        glVertex3f(-1.f, 0.f, 0.f);
        glTexCoord2f(repX, repY / 2.0f);
        glVertex3f(1.f, 0.f, 0.f);
        glTexCoord2f(repX, repY);
        glVertex3f(1.f, 1.f, 0.f);
        glTexCoord2f(0.f, repY);
        glVertex3f(-1.f, 1.f, 0.f);
        glEnd();
    } else {
        glBegin(GL_QUADS);
        glVertex3f(-1.f, -1.f, 0.f);
        glVertex3f(1.f, -1.f, 0.f);
        glVertex3f(1.f, 0.f, 0.f);
        glVertex3f(-1.f, 0.f, 0.f);

        glVertex3f(-1.f, 0.f, 0.f);
        glVertex3f(1.f, 0.f, 0.f);
        glVertex3f(1.f, 1.f, 0.f);
        glVertex3f(-1.f, 1.f, 0.f);
        glEnd();
    }

    glPopMatrix();
}

void
drawChequeredRect(float x, float y, float z, float w, float h, int across, int down, const Texture * texture)
{                               /* Draw a rectangle split into (across x down) chequers */
    int i, j;
    float hh = h / down;
    float ww = w / across;

    glPushMatrix();
    glTranslatef(x, y, z);
    glNormal3f(0.f, 0.f, 1.f);

    if (texture) {
        float tuv = TEXTURE_SCALE / texture->width;
        float tw = ww * tuv;
        float th = hh * tuv;
        float ty = 0.f;

        for (i = 0; i < down; i++) {
            float xx = 0, tx = 0;
            glPushMatrix();
            glTranslatef(0.f, hh * i, 0.f);
            glBegin(GL_QUAD_STRIP);
            for (j = 0; j <= across; j++) {
                glTexCoord2f(tx, ty + th);
                glVertex2f(xx, hh);
                glTexCoord2f(tx, ty);
                glVertex2f(xx, 0.f);
                xx += ww;
                tx += tw;
            }
            ty += th;
            glEnd();
            glPopMatrix();
        }
    } else {
        for (i = 0; i < down; i++) {
            float xx = 0;
            glPushMatrix();
            glTranslatef(0.f, hh * i, 0.f);
            glBegin(GL_QUAD_STRIP);
            for (j = 0; j <= across; j++) {
                glVertex2f(xx, hh);
                glVertex2f(xx, 0.f);
                xx += ww;
            }
            glEnd();
            glPopMatrix();
        }
    }
    glPopMatrix();
}

void
QuarterCylinder(float radius, float len, unsigned int accuracy, const Texture * texture)
{
    unsigned int i;
    float angle;
    float d;
    float sar, car;
    float dInc = 0;

    /* texture unit value */
    float tuv;
    if (texture) {
        float st = sinf((2 * G_PI) / accuracy) * radius;
        float ct = (cosf((2 * G_PI) / accuracy) - 1) * radius;
        dInc = sqrtf(st * st + ct * ct);
        tuv = (TEXTURE_SCALE) / texture->width;
    } else
        tuv = 0;

    d = 0;
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < accuracy / 4 + 1; i++) {
        angle = ((float) i * 2.f * (float) G_PI) / accuracy;
        glNormal3f(sinf(angle), 0.f, cosf(angle));

        sar = sinf(angle) * radius;
        car = cosf(angle) * radius;

        if (tuv)
            glTexCoord2f(len * tuv, d * tuv);
        glVertex3f(sar, len, car);

        if (tuv) {
            glTexCoord2f(0.f, d * tuv);
            d -= dInc;
        }
        glVertex3f(sar, 0.f, car);
    }
    glEnd();
}

void
QuarterCylinderSplayedRev(float radius, float len, unsigned int accuracy, const Texture * texture)
{
    unsigned int i;
    float angle;
    float d;
    float sar, car;
    float dInc = 0;

    /* texture unit value */
    float tuv;
    if (texture) {
        float st = sinf((2 * G_PI) / accuracy) * radius;
        float ct = (cosf((2 * G_PI) / accuracy) - 1) * radius;
        dInc = sqrtf(st * st + ct * ct);
        tuv = (TEXTURE_SCALE) / texture->width;
    } else
        tuv = 0;

    d = 0;
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < accuracy / 4 + 1; i++) {
        angle = ((float) i * 2.f * (float) G_PI) / accuracy;
        glNormal3f(sinf(angle), 0.f, cosf(angle));

        sar = sinf(angle) * radius;
        car = cosf(angle) * radius;

        if (tuv)
            glTexCoord2f((len + car) * tuv, d * tuv);
        glVertex3f(sar, len + car, car);

        if (tuv) {
            glTexCoord2f(-car * tuv, d * tuv);
            d -= dInc;
        }
        glVertex3f(sar, -car, car);
    }
    glEnd();
}

void
QuarterCylinderSplayed(float radius, float len, unsigned int accuracy, const Texture * texture)
{
    unsigned int i;
    float angle;
    float d;
    float sar, car;
    float dInc = 0;

    /* texture unit value */
    float tuv;
    if (texture) {
        float st = sinf((2 * G_PI) / accuracy) * radius;
        float ct = (cosf((2 * G_PI) / accuracy) - 1) * radius;
        dInc = sqrtf(st * st + ct * ct);
        tuv = (TEXTURE_SCALE) / texture->width;
    } else
        tuv = 0;

    d = 0;
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < accuracy / 4 + 1; i++) {
        angle = ((float) i * 2.f * (float) G_PI) / accuracy;
        glNormal3f(sinf(angle), 0.f, cosf(angle));

        sar = sinf(angle) * radius;
        car = cosf(angle) * radius;

        if (tuv)
            glTexCoord2f((len - car) * tuv, d * tuv);
        glVertex3f(sar, len - car, car);

        if (tuv) {
            glTexCoord2f(car * tuv, d * tuv);
            d -= dInc;
        }
        glVertex3f(sar, car, car);
    }
    glEnd();
}

void
drawCornerEigth(float **const *boardPoints, float radius, unsigned int accuracy)
{
    unsigned int i, ns;
    int j;

    for (i = 0; i < accuracy / 4; i++) {
        ns = (accuracy / 4) - (i + 1);
        glBegin(GL_TRIANGLE_STRIP);
        glNormal3f(boardPoints[i][ns + 1][0] / radius, boardPoints[i][ns + 1][1] / radius,
                   boardPoints[i][ns + 1][2] / radius);
        glVertex3f(boardPoints[i][ns + 1][0], boardPoints[i][ns + 1][1], boardPoints[i][ns + 1][2]);
        for (j = (int) ns; j >= 0; j--) {
            glNormal3f(boardPoints[i + 1][j][0] / radius, boardPoints[i + 1][j][1] / radius,
                       boardPoints[i + 1][j][2] / radius);
            glVertex3f(boardPoints[i + 1][j][0], boardPoints[i + 1][j][1], boardPoints[i + 1][j][2]);
            glNormal3f(boardPoints[i][j][0] / radius, boardPoints[i][j][1] / radius, boardPoints[i][j][2] / radius);
            glVertex3f(boardPoints[i][j][0], boardPoints[i][j][1], boardPoints[i][j][2]);
        }
        glEnd();
    }
}

void
calculateEigthPoints(float ****boardPoints, float radius, unsigned int accuracy)
{
    unsigned int i, j, ns;

    float lat_angle;
    float lat_step;
    float latitude;
    float new_radius;
    float angle;
    float step = 0;
    unsigned int corner_steps = (accuracy / 4) + 1;
    *boardPoints = Alloc3d(corner_steps, corner_steps, 3);

    lat_angle = 0;
    lat_step = (2 * (float) G_PI) / accuracy;

    /* Calculate corner 1/8th sphere points */
    for (i = 0; i < (accuracy / 4) + 1; i++) {
        latitude = sinf(lat_angle) * radius;
        new_radius = Dist2d(radius, latitude);

        angle = 0;
        ns = (accuracy / 4) - i;
        if (ns > 0)
            step = (2.f * (float) G_PI) / (ns * 4.f);

        for (j = 0; j <= ns; j++) {
            (*boardPoints)[i][j][0] = sinf(angle) * new_radius;
            (*boardPoints)[i][j][1] = latitude;
            (*boardPoints)[i][j][2] = cosf(angle) * new_radius;

            angle += step;
        }
        lat_angle += lat_step;
    }
}

void
freeEigthPoints(float ****boardPoints, unsigned int accuracy)
{
    unsigned int corner_steps = (accuracy / 4) + 1;
    if (*boardPoints)
        Free3d(*boardPoints, corner_steps, corner_steps);
    *boardPoints = 0;
}

void
SetColour3d(float r, float g, float b, float a)
{
    Material col;
    SetupSimpleMatAlpha(&col, r, g, b, a);
    setMaterial(&col);
}

void
SetMovingPieceRotation(const BoardData * bd, BoardData3d * bd3d, unsigned int pt)
{                               /* Make sure piece is rotated correctly while dragging */
    bd3d->movingPieceRotation = bd3d->pieceRotation[pt][Abs(bd->points[pt])];
}

void
PlaceMovingPieceRotation(const BoardData * bd, BoardData3d * bd3d, unsigned int dest, unsigned int src)
{                               /* Make sure rotation is correct in new position */
    bd3d->pieceRotation[src][Abs(bd->points[src])] = bd3d->pieceRotation[dest][Abs(bd->points[dest] - 1)];
    bd3d->pieceRotation[dest][Abs(bd->points[dest]) - 1] = bd3d->movingPieceRotation;
}

static void
getProjectedCoord(const float pos[3], float *x, float *y)
{                               /* Work out where point (x, y, z) is on the screen */
    GLint viewport[4];
    GLdouble mvmatrix[16], projmatrix[16], xd, yd, zd;

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);

    if (gluProject
        ((GLdouble) pos[0], (GLdouble) pos[1], (GLdouble) pos[2], mvmatrix, projmatrix, viewport, &xd, &yd,
         &zd) == GL_FALSE)
        g_print("Error calling gluProject()\n");

    *x = (float) xd;
    *y = (float) yd;
}

static int freezeRestrict = 0;
static ClipBox cb[MAX_FRAMES], eraseCb, lastCb;
int numRestrictFrames = 0;

static float
BoxWidth(const ClipBox * pCb)
{
    return pCb->xx - pCb->x;
}

static float
BoxHeight(const ClipBox * pCb)
{
    return pCb->yy - pCb->y;
}

static float
BoxMidWidth(const ClipBox * pCb)
{
    return pCb->x + BoxWidth(pCb) / 2;
}

static float
BoxMidHeight(const ClipBox * pCb)
{
    return pCb->y + BoxHeight(pCb) / 2;
}

static void
CopyBox(ClipBox * pTo, const ClipBox * pFrom)
{
    *pTo = *pFrom;
}

static void
EnlargeTo(ClipBox * pCb, float x, float y)
{
    if (x < pCb->x)
        pCb->x = x;
    if (y < pCb->y)
        pCb->y = y;
    if (x > pCb->xx)
        pCb->xx = x;
    if (y > pCb->yy)
        pCb->yy = y;
}

void
EnlargeCurrentToBox(const ClipBox * pOtherCb)
{
    EnlargeTo(&cb[numRestrictFrames], pOtherCb->x, pOtherCb->y);
    EnlargeTo(&cb[numRestrictFrames], pOtherCb->xx, pOtherCb->yy);
}

static void
InitBox(ClipBox * pCb, float x, float y)
{
    pCb->x = pCb->xx = x;
    pCb->y = pCb->yy = y;
}

static void
RationalizeBox(ClipBox * pCb)
{
    int midX, midY, maxXoff, maxYoff;
    /* Make things a bit bigger to avoid slight drawing errors */
    pCb->x -= .5f;
    pCb->xx += .5f;
    pCb->y -= .5f;
    pCb->yy += .5f;
    midX = (int) BoxMidWidth(pCb);
    midY = (int) BoxMidHeight(pCb);
    maxXoff = MAX(midX - (int) pCb->x, (int) pCb->xx - midX) + 1;
    maxYoff = MAX(midY - (int) pCb->y, (int) pCb->yy - midY) + 1;
    pCb->x = (float) (midX - maxXoff);
    pCb->xx = (float) (midX + maxXoff);
    pCb->y = (float) (midY - maxYoff);
    pCb->yy = (float) (midY + maxYoff);
}

void
RestrictiveRedraw(void)
{
    numRestrictFrames = -1;
}

void
RestrictiveDraw(ClipBox * pCb, const float pos[3], float width, float height, float depth)
{
    float tpos[3];
    float x, y;

    copyPoint(tpos, pos);
    tpos[0] -= width / 2.0f;
    tpos[1] -= height / 2.0f;

    getProjectedCoord(tpos, &x, &y);
    InitBox(pCb, x, y);

    tpos[0] += width;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);

    tpos[1] += height;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);

    tpos[0] -= width;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);

    tpos[1] -= height;
    tpos[2] += depth;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);

    tpos[0] += width;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);

    tpos[1] += height;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);

    tpos[0] -= width;
    getProjectedCoord(tpos, &x, &y);
    EnlargeTo(pCb, x, y);
}

void
RestrictiveDrawFrame(const float pos[3], float width, float height, float depth)
{
    if (numRestrictFrames != -1) {
        numRestrictFrames++;
        if (numRestrictFrames == MAX_FRAMES) {  /* Too many drawing requests - just redraw whole screen */
            RestrictiveRedraw();
            return;
        }
        RestrictiveDraw(&cb[numRestrictFrames], pos, width, height, depth);
    }
}

void
RestrictiveDrawFrameWindow(int x, int y, int width, int height)
{
    if (numRestrictFrames != -1) {
        numRestrictFrames++;
        if (numRestrictFrames == MAX_FRAMES) {  /* Too many drawing requests - just redraw whole screen */
            RestrictiveRedraw();
            return;
        }
        cb[numRestrictFrames].x = (float) x;
        cb[numRestrictFrames].y = (float) y;
        cb[numRestrictFrames].xx = (float) x + width;
        cb[numRestrictFrames].yy = (float) y + height;
    }
}

void
RestrictiveRender(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    while (numRestrictFrames > 0) {
        RationalizeBox(&cb[numRestrictFrames]);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPickMatrix((double) BoxMidWidth(&cb[numRestrictFrames]), (double) BoxMidHeight(&cb[numRestrictFrames]),
                      (double) BoxWidth(&cb[numRestrictFrames]), (double) BoxHeight(&cb[numRestrictFrames]), viewport);

        /* Setup projection matrix - using saved values */
        if (prd->planView)
            glOrtho(-bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, 0.0, 5.0);
        else
            glFrustum(-bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, zNear, zFar);

        glMatrixMode(GL_MODELVIEW);
        glViewport((int) (cb[numRestrictFrames].x), (int) (cb[numRestrictFrames].y),
                   (int) BoxWidth(&cb[numRestrictFrames]), (int) BoxHeight(&cb[numRestrictFrames]));

        drawBoard(bd, bd3d, prd);

        if (!freezeRestrict)
            numRestrictFrames--;
        else {
            if (numRestrictFrames > 1)
                numRestrictFrames--;
            else {
                freezeRestrict = 0;
                break;
            }
        }
    }
    /* Restore matrixes */
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

int
MouseMove3d(const BoardData * bd, BoardData3d * bd3d, const renderdata * prd, int x, int y)
{
    if (bd->drag_point >= 0) {
        getProjectedPieceDragPos(x, y, bd3d->dragPos);
        updateMovingPieceOccPos(bd, bd3d);

        if (prd->quickDraw && numRestrictFrames != -1) {
            if (!freezeRestrict)
                CopyBox(&eraseCb, &lastCb);

            RestrictiveDraw(&cb[numRestrictFrames], bd3d->dragPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
            freezeRestrict++;

            CopyBox(&lastCb, &cb[numRestrictFrames]);
            EnlargeCurrentToBox(&eraseCb);
        }
        return 1;
    } else
        return 0;
}

void
RestrictiveStartMouseMove(unsigned int pos, unsigned int depth)
{
    float erasePos[3];
    if (numRestrictFrames != -1) {
        getPiecePos(pos, depth, erasePos);
        RestrictiveDrawFrame(erasePos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
        CopyBox(&eraseCb, &cb[numRestrictFrames]);
    }
    freezeRestrict = 1;
}

void
RestrictiveEndMouseMove(unsigned int pos, unsigned int depth)
{
    float newPos[3];
    getPiecePos(pos, depth, newPos);

    if (numRestrictFrames == -1)
        return;

    if (pos == 26 || pos == 27) {
        newPos[2] -= PIECE_HOLE / 2.0f;
        RestrictiveDraw(&cb[numRestrictFrames], newPos, PIECE_HOLE, PIECE_HOLE, PIECE_HOLE);
    } else
        RestrictiveDraw(&cb[numRestrictFrames], newPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);

    if (freezeRestrict)
        EnlargeCurrentToBox(&eraseCb);
    else
        EnlargeCurrentToBox(&lastCb);

    freezeRestrict = 0;
}

static void
updateDicePos(Path * path, DiceRotation * diceRot, float dist, float pos[3])
{
    if (movePath(path, dist, 0, pos)) {
        diceRot->xRot = (dist * diceRot->xRotFactor + diceRot->xRotStart) * 360;
        diceRot->yRot = (dist * diceRot->yRotFactor + diceRot->yRotStart) * 360;
    } else {                    /* Finished - set to end point */
        copyPoint(pos, path->pts[path->numSegments]);
        diceRot->xRot = diceRot->yRot = 0;
    }
}

static void
SetupMove(BoardData * bd, BoardData3d * bd3d)
{
    unsigned int destDepth;
    unsigned int target = convert_point(animate_move_list[slide_move], animate_player);
    unsigned int dest = convert_point(animate_move_list[slide_move + 1], animate_player);
    int dir = animate_player ? 1 : -1;

    bd->points[target] -= dir;

    animStartTime = get_time();

    destDepth = Abs(bd->points[dest]) + 1;
    if ((Abs(bd->points[dest]) == 1) && (dir != SGN(bd->points[dest])))
        destDepth--;

    setupPath(bd, &bd3d->piecePath, &bd3d->rotateMovingPiece, target, Abs(bd->points[target]) + 1, dest, destDepth);
    copyPoint(bd3d->movingPos, bd3d->piecePath.pts[0]);

    SetMovingPieceRotation(bd, bd3d, target);

    updatePieceOccPos(bd, bd3d);
}

static int firstFrame;

static int
idleAnimate(BoardData3d * bd3d)
{
    BoardData *bd = pIdleBD;
    float elapsedTime = (float) ((get_time() - animStartTime) / 1000.0f);
    float vel = .2f + nGUIAnimSpeed * .3f;
    float animateDistance = elapsedTime * vel;
    renderdata *prd = bd->rd;

    if (stopNextTime) {         /* Stop now - last animation frame has been drawn */
        StopIdle3d(bd, bd->bd3d);
        gtk_main_quit();
        return 1;
    }

    if (bd3d->moving) {
        float old_pos[3];
        ClipBox temp;
        float *pRotate = 0;
        if (bd3d->rotateMovingPiece != -1 && bd3d->piecePath.state == 2)
            pRotate = &bd3d->rotateMovingPiece;

        copyPoint(old_pos, bd3d->movingPos);

        if (!movePath(&bd3d->piecePath, animateDistance, pRotate, bd3d->movingPos)) {
            TanBoard points;
            unsigned int moveStart = convert_point(animate_move_list[slide_move], animate_player);
            unsigned int moveDest = convert_point(animate_move_list[slide_move + 1], animate_player);

            if ((Abs(bd->points[moveDest]) == 1) && (bd->turn != SGN(bd->points[moveDest]))) {  /* huff */
                unsigned int bar;
                if (bd->turn == 1)
                    bar = 0;
                else
                    bar = 25;
                bd->points[bar] -= bd->turn;
                bd->points[moveDest] = 0;

                if (prd->quickDraw)
                    RestrictiveDrawPiece(bar, Abs(bd->points[bar]));
            }

            bd->points[moveDest] += bd->turn;

            /* Update pip-count mid move */
            read_board(bd, points);
            update_pipcount(bd, (ConstTanBoard) points);

            PlaceMovingPieceRotation(bd, bd3d, moveDest, moveStart);

            if (prd->quickDraw && numRestrictFrames != -1) {
                float new_pos[3];
                getPiecePos(moveDest, Abs(bd->points[moveDest]), new_pos);
                if (moveDest == 26 || moveDest == 27) {
                    new_pos[2] -= PIECE_HOLE / 2.0f;
                    RestrictiveDraw(&temp, new_pos, PIECE_HOLE, PIECE_HOLE, PIECE_HOLE);
                } else
                    RestrictiveDraw(&temp, new_pos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
            }

            /* Next piece */
            slide_move += 2;

            if (slide_move >= 8 || animate_move_list[slide_move] < 0) { /* All done */
                bd3d->moving = 0;
                updatePieceOccPos(bd, bd3d);
                animation_finished = TRUE;
                stopNextTime = 1;
            } else
                SetupMove(bd, bd3d);

            playSound(SOUND_CHEQUER);
        } else {
            updateMovingPieceOccPos(bd, bd3d);
            if (prd->quickDraw && numRestrictFrames != -1)
                RestrictiveDraw(&temp, bd3d->movingPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
        }
        if (prd->quickDraw && numRestrictFrames != -1) {
            RestrictiveDrawFrame(old_pos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
            EnlargeCurrentToBox( /*lint -e(645) */ &temp);      /* temp is initilized above (lint error 645) */
        }
    }

    if (bd3d->shakingDice) {
        if (!finishedPath(&bd3d->dicePaths[0]))
            updateDicePos(&bd3d->dicePaths[0], &bd3d->diceRotation[0], animateDistance / 2.0f, bd3d->diceMovingPos[0]);
        if (!finishedPath(&bd3d->dicePaths[1]))
            updateDicePos(&bd3d->dicePaths[1], &bd3d->diceRotation[1], animateDistance / 2.0f, bd3d->diceMovingPos[1]);

        if (finishedPath(&bd3d->dicePaths[0]) && finishedPath(&bd3d->dicePaths[1])) {
            bd3d->shakingDice = 0;
            stopNextTime = 1;
        }
        updateDiceOccPos(bd, bd3d);

        if (bd->rd->quickDraw && numRestrictFrames != -1) {
            float pos[3];
            float overSize;
            ClipBox temp;

            overSize = getDiceSize(bd->rd) * 1.5f;
            copyPoint(pos, bd3d->diceMovingPos[0]);
            pos[2] -= getDiceSize(bd->rd) / 2.0f;
            RestrictiveDrawFrame(pos, overSize, overSize, overSize);

            copyPoint(pos, bd3d->diceMovingPos[1]);
            pos[2] -= getDiceSize(bd->rd) / 2.0f;
            RestrictiveDraw(&temp, pos, overSize, overSize, overSize);
            EnlargeCurrentToBox(&temp);

            CopyBox(&temp, &cb[numRestrictFrames]);
            if (!firstFrame)
                EnlargeCurrentToBox(&eraseCb);
            else if (firstFrame == -1)
                RestrictiveRedraw();
            else
                firstFrame = 0;

            CopyBox(&eraseCb, &temp);
        }
    }

    return 1;
}

void
RollDice3d(BoardData * bd, BoardData3d * bd3d, const renderdata * prd)
{                               /* animate the dice roll if not below board */
    setDicePos(bd, bd3d);
    GTKSuspendInput();

    if (prd->animateRoll) {
        animStartTime = get_time();

        bd3d->shakingDice = 1;
        stopNextTime = 0;
        setIdleFunc(bd, idleAnimate);

        setupDicePaths(bd, bd3d->dicePaths, bd3d->diceMovingPos, bd3d->diceRotation);
        /* Make sure shadows are in correct place */
        UpdateShadows(bd->bd3d);
        if (prd->quickDraw) {   /* Mark this as the first frame (or -1 to indicate full draw in progress) */
            if (numRestrictFrames == -1)
                firstFrame = -1;
            else
                firstFrame = 1;
        }
        gtk_main();
    } else {
        /* Show dice on board */
        gtk_widget_queue_draw(bd3d->drawing_area3d);
        while (gtk_events_pending())
            gtk_main_iteration();
    }
    GTKResumeInput();
}

void
AnimateMove3d(BoardData * bd, BoardData3d * bd3d)
{
    slide_move = 0;
    bd3d->moving = 1;

    SetupMove(bd, bd3d);

    stopNextTime = 0;
    setIdleFunc(bd, idleAnimate);
    GTKSuspendInput();
    gtk_main();
    GTKResumeInput();
}

NTH_STATIC int
idleWaveFlag(BoardData3d * bd3d)
{
    BoardData *bd = pIdleBD;
    float elapsedTime = (float) (get_time() - animStartTime);
    bd3d->flagWaved = elapsedTime / 200;
    updateFlagOccPos(bd, bd3d);
    RestrictiveDrawFlag(bd);
    return 1;
}

void
ShowFlag3d(BoardData * bd, BoardData3d * bd3d, const renderdata * prd)
{
    bd3d->flagWaved = 0;

    if (prd->animateFlag && bd->resigned && ms.gs == GAME_PLAYING && bd->playing && (ap[bd->turn == 1 ? 0 : 1].pt == PLAYER_HUMAN)) {   /* not for computer turn */
        animStartTime = get_time();
        setIdleFunc(bd, idleWaveFlag);
    } else
        StopIdle3d(bd, bd->bd3d);

    waveFlag(bd3d->flagWaved);
    updateFlagOccPos(bd, bd3d);

    RestrictiveDrawFlag(bd);
}

static int
idleCloseBoard(BoardData3d * bd3d)
{
    BoardData *bd = pIdleBD;
    float elapsedTime = (float) (get_time() - animStartTime);
    if (bd3d->State == BOARD_CLOSED) {  /* finished */
        StopIdle3d(bd, bd->bd3d);
#ifdef WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
        gtk_main_quit();

        return 1;
    }

    bd3d->perOpen = (elapsedTime / 3000);
    if (bd3d->perOpen >= 1) {
        bd3d->perOpen = 1;
        bd3d->State = BOARD_CLOSED;
    }

    return 1;
}

static int
idleTestPerformance(BoardData3d * bd3d)
{
    BoardData *bd = pIdleBD;
    float elapsedTime = (float) (get_time() - testStartTime);
    if (elapsedTime > TEST_TIME) {
        StopIdle3d(bd, bd3d);
        gtk_main_quit();
        return 0;
    }
    numFrames++;
    return 1;
}

float
TestPerformance3d(BoardData * bd)
{
    float elapsedTime;

    setIdleFunc(bd, idleTestPerformance);
    testStartTime = get_time();
    numFrames = 0;
    gtk_main();
    elapsedTime = (float) (get_time() - testStartTime);

    return (numFrames / (elapsedTime / 1000.0f));
}

NTH_STATIC void
EmptyPos(BoardData * bd)
{                               /* All checkers home */
    int ip[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, -15 };
    memcpy(bd->points, ip, sizeof(bd->points));
    updatePieceOccPos(bd, bd->bd3d);
}

void
CloseBoard3d(BoardData * bd, BoardData3d * bd3d, renderdata * prd)
{
    bd3d->fBuffers = FALSE;
    EmptyPos(bd);
    bd3d->State = BOARD_CLOSED;
    /* Turn off most things so they don't interfere when board closed/opening */
    bd->cube_use = 0;
    bd->colour = 0;
    bd->direction = 1;
    bd->resigned = 0;
    fClockwise = 0;
    GTKSuspendInput();

    prd->showShadows = 0;
    prd->showMoveIndicator = 0;
    prd->fLabels = 0;
    bd->diceShown = DICE_NOT_SHOWN;
    bd3d->State = BOARD_CLOSING;

    /* Random logo */
    SetupSimpleMat(&bd3d->logoMat, 1.f, 1.f, 1.f);
    SetTexture(bd3d, &bd3d->logoMat, TEXTURE_PATH "logo.png");

    animStartTime = get_time();
    bd3d->perOpen = 0;
    setIdleFunc(bd, idleCloseBoard);
    /* Push matrix as idleCloseBoard assumes last matrix is on stack */
    glPushMatrix();

    gtk_main();
}

void
SetupViewingVolume3d(const BoardData * bd, BoardData3d * bd3d, const renderdata * prd)
{
    GLint viewport[4];
    float tempMatrix[16];
    glGetIntegerv(GL_VIEWPORT, viewport);

    memcpy(tempMatrix, bd3d->modelMatrix, sizeof(float[16]));

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    SetupPerspVolume(bd, bd3d, prd, viewport);

    SetupLight3d(bd3d, prd);
    calculateBackgroundSize(bd3d, viewport);
    if (memcmp(tempMatrix, bd3d->modelMatrix, sizeof(float[16])))
        RestrictiveRedraw();

    RerenderBase(bd3d);
}

void
SetupMat(Material * pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb,
         int shin, float a)
{
    pMat->ambientColour[0] = r;
    pMat->ambientColour[1] = g;
    pMat->ambientColour[2] = b;
    pMat->ambientColour[3] = a;

    pMat->diffuseColour[0] = dr;
    pMat->diffuseColour[1] = dg;
    pMat->diffuseColour[2] = db;
    pMat->diffuseColour[3] = a;

    pMat->specularColour[0] = sr;
    pMat->specularColour[1] = sg;
    pMat->specularColour[2] = sb;
    pMat->specularColour[3] = a;
    pMat->shine = shin;

    pMat->alphaBlend = (a != 1) && (a != 0);

    pMat->textureInfo = NULL;
    pMat->pTexture = NULL;
}

void
SetupSimpleMatAlpha(Material * pMat, float r, float g, float b, float a)
{
    SetupMat(pMat, r, g, b, r, g, b, 0.f, 0.f, 0.f, 0, a);
}

void
SetupSimpleMat(Material * pMat, float r, float g, float b)
{
    SetupMat(pMat, r, g, b, r, g, b, 0.f, 0.f, 0.f, 0, 0.f);
}

/* Not currently used
 * void RemoveTexture(Material* pMat)
 * {
 * if (pMat->pTexture)
 * {
 * int i = 0;
 * while (&bd->textureList[i] != pMat->pTexture)
 * i++;
 * 
 * DeleteTexture(&bd->textureList[i]);
 * free(bd->textureName[i]);
 * 
 * while (i != bd->numTextures - 1)
 * {
 * bd->textureList[i] = bd->textureList[i + 1];
 * bd->textureName[i] = bd->textureName[i + 1];
 * i++;
 * }
 * bd->numTextures--;
 * pMat->pTexture = 0;
 * }
 * }
 */

void
ClearTextures(BoardData3d * bd3d)
{
    int i;
    if (!bd3d->numTextures)
        return;

    MakeCurrent3d(bd3d);

    for (i = 0; i < bd3d->numTextures; i++) {
        DeleteTexture(&bd3d->textureList[i]);
        free(bd3d->textureName[i]);
    }
    bd3d->numTextures = 0;
}

static void
free_texture(gpointer data, gpointer UNUSED(userdata))
{
    free(data);
}

void
DeleteTextureList(void)
{
    g_list_foreach(textures, free_texture, NULL);
    g_list_free(textures);
}

void
InitBoard3d(BoardData * bd, BoardData3d * bd3d)
{                               /* Initilize 3d parts of boarddata */
    int i, j;
    /* Assign random rotation to each board position */
    for (i = 0; i < 28; i++)
        for (j = 0; j < 15; j++)
            bd3d->pieceRotation[i][j] = rand() % 360;

    bd3d->shadowsInitialised = FALSE;
    bd3d->shadowsOutofDate = TRUE;
    bd3d->State = BOARD_OPEN;
    bd3d->moving = 0;
    bd3d->shakingDice = 0;
    bd->drag_point = -1;
    bd->DragTargetHelp = 0;
    bd3d->fBasePreRendered = FALSE;

    SetupSimpleMat(&bd3d->gapColour, 0.f, 0.f, 0.f);
    SetupMat(&bd3d->flagMat, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 50, 0.f);
    SetupMat(&bd3d->flagNumberMat, 0.f, 0.f, .4f, 0.f, 0.f, .4f, 1.f, 1.f, 1.f, 100, 1.f);

    bd3d->diceList = bd3d->DCList = bd3d->pieceList = 0;
    bd3d->qobjTex = bd3d->qobj = 0;

    bd3d->numTextures = 0;

    bd3d->boardPoints = NULL;
    bd3d->numberFont = bd3d->cubeFont = NULL;

    memset(bd3d->modelMatrix, 0, sizeof(float[16]));

    bd->bd3d->fBuffers = FALSE;
}

#if HAVE_LIBPNG

void
GenerateImage3d(const char *szName, unsigned int nSize, unsigned int nSizeX, unsigned int nSizeY)
{
    unsigned char *puch;
    BoardData *bd = BOARD(pwBoard)->board_data;
    GdkPixbuf *pixbuf;
    GError *error = NULL;
    unsigned int line;
    unsigned int width = nSize * nSizeX;
    unsigned int height = nSize * nSizeY;

    /* Allocate buffer for image, height + 1 as extra line needed to invert image (opengl renders 'upside down') */
    if ((puch = (unsigned char *) malloc(width * (height + 1) * 3)) == NULL) {
        outputerr("malloc");
        return;
    }

    RenderToBuffer3d(bd, bd->bd3d, width, height, puch);

    /* invert image (y axis) */
    for (line = 0; line < height / 2; line++) {
        unsigned int lineSize = width * 3;
        /* Swap two lines at a time */
        memcpy(puch + height * lineSize, puch + line * lineSize, lineSize);
        memcpy(puch + line * lineSize, puch + ((height - line) - 1) * lineSize, lineSize);
        memcpy(puch + ((height - line) - 1) * lineSize, puch + height * lineSize, lineSize);
    }

    pixbuf = gdk_pixbuf_new_from_data(puch, GDK_COLORSPACE_RGB, FALSE, 8,
                                      (int) width, (int) height, (int) width * 3, NULL, NULL);

    gdk_pixbuf_save(pixbuf, szName, "png", &error, NULL);
    if (error) {
        outputerrf("png failed: %s\n", error->message);
        g_error_free(error);
    }
    g_object_unref(pixbuf);
    free(puch);
}

#endif

extern GtkWidget *
GetDrawingArea3d(const BoardData3d * bd3d)
{
    return bd3d->drawing_area3d;
}

extern char *
MaterialGetTextureFilename(const Material * pMat)
{
    return pMat->textureInfo->file;
}

extern void
TidyCurveAccuracy3d(BoardData3d * bd3d, unsigned int accuracy)
{
    freeEigthPoints(&bd3d->boardPoints, accuracy);
}

extern void
DrawScene3d(const BoardData3d * bd3d)
{
    gtk_widget_queue_draw(bd3d->drawing_area3d);
}

extern int
Animating3d(const BoardData3d * bd3d)
{
    return (bd3d->shakingDice || bd3d->moving);
}

extern void
RerenderBase(BoardData3d * bd3d)
{
    bd3d->fBasePreRendered = FALSE;
}

extern gboolean
display_is_3d(const renderdata * prd)
{
    g_assert(prd->fDisplayType == DT_2D || (prd->fDisplayType == DT_3D && gtk_gl_init_success));
    return (prd->fDisplayType == DT_3D);
}

extern gboolean
display_is_2d(const renderdata * prd)
{
    displaytype fdt = prd->fDisplayType;
    g_assert(fdt == DT_2D || fdt == DT_3D);
    return (fdt == DT_2D ? TRUE : FALSE);
}

extern void
Draw3d(const BoardData * bd)
{                               /* Render board: quick drawing, standard or 2 passes for shadows */
#ifdef WIN32
    GtkAllocation allocation;
    gtk_widget_get_allocation(bd->bd3d->drawing_area3d, &allocation);

    if (bd->bd3d->fBuffers) {
        if (bd->bd3d->fBasePreRendered)
            RestoreBufferRegion(bd->bd3d->wglBuffer, 0, 0, allocation.width, allocation.height);
        else {
            drawBasePreRender(bd, bd->bd3d, bd->rd);
            bd->bd3d->fBasePreRendered = TRUE;
        }

        if (bd->rd->showShadows)
            shadowDisplay(drawBoardTop, bd, bd->bd3d, bd->rd);
        else
            drawBoardTop(bd, bd->bd3d, bd->rd);
    } else
#endif
    {
        if (bd->rd->showShadows)
            shadowDisplay(drawBoard, bd, bd->bd3d, bd->rd);
        else
            drawBoard(bd, bd->bd3d, bd->rd);
    }
}

static int diceRollingSave;
void
SuspendDiceRolling(renderdata * prd)
{
    diceRollingSave = prd->animateRoll;
    prd->animateRoll = FALSE;
}

void
ResumeDiceRolling(renderdata * prd)
{
    prd->animateRoll = diceRollingSave;
}
