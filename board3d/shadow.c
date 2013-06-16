/*
 * shadow.c
 * by Jon Kinsey, 2003
 *
 * 3d shadow functions
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

static int midStencilVal;

extern int
ShadowsInitilised(const BoardData3d * bd3d)
{
    return (bd3d != NULL && bd3d->shadowsInitialised);
}

void
shadowInit(BoardData3d * bd3d, renderdata * prd)
{
    int i;
    GLint stencilBits;

    if (bd3d->shadowsInitialised)
        return;

    /* Darkness as percentage of ambient light */
    prd->dimness = ((prd->lightLevels[1] / 100.0f) * (100 - prd->shadowDarkness)) / 100;

    for (i = 0; i < NUM_OCC; i++)
        bd3d->Occluders[i].handle = 0;

    /* Check the stencil buffer is present */
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    if (!stencilBits) {
        g_print("No stencil buffer - no shadows\n");
        return;
    }
    midStencilVal = powi(2, stencilBits - 1);
    glClearStencil(midStencilVal);

    bd3d->shadowsInitialised = TRUE;
}

#ifdef TEST_HARNESS
void GenerateShadowEdges(const Occluder * pOcc);
extern void
draw_shadow_volume_edges(Occluder * pOcc)
{
    if (pOcc->show) {
        glPushMatrix();
        moveToOcc(pOcc);
        GenerateShadowEdges(pOcc);
        glPopMatrix();
    }
}
#endif
extern void
draw_shadow_volume_extruded_edges( /*lint -e{818} */ Occluder * pOcc, const float light_position[4], unsigned int prim)
{
    if (pOcc->show) {
        float olight[4];
        mult_matrix_vec((ConstMatrix) pOcc->invMat, light_position, olight);

        glNewList(pOcc->shadow_list, GL_COMPILE);
        glPushMatrix();
        moveToOcc(pOcc);
        glBegin(prim);
        GenerateShadowVolume(pOcc, olight);
        glEnd();
        glPopMatrix();
        glEndList();
    }
}

int renderingBase = FALSE;

static void
DrawShadows(const BoardData3d * bd3d)
{
    int i;
    if (renderingBase) {
        glCallList(bd3d->Occluders[OCC_BOARD].shadow_list);
        if (bd3d->Occluders[OCC_HINGE1].show) {
            glCallList(bd3d->Occluders[OCC_HINGE1].shadow_list);
            glCallList(bd3d->Occluders[OCC_HINGE2].shadow_list);
        }
    } else {
        for (i = 0; i < NUM_OCC; i++) {
            if (bd3d->Occluders[i].show)
                glCallList(bd3d->Occluders[i].shadow_list);
        }
    }
}

static void
draw_shadow_volume_to_stencil(const BoardData3d * bd3d)
{
    /* First clear the stencil buffer */
    glClear(GL_STENCIL_BUFFER_BIT);

    /* Disable drawing to colour buffer, lighting and don't update depth buffer */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);

    /* Z-pass approach */
    glStencilFunc(GL_ALWAYS, midStencilVal, (GLuint) ~ 0);

    glCullFace(GL_FRONT);
    glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

    DrawShadows(bd3d);

    glCullFace(GL_BACK);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

    DrawShadows(bd3d);

    /* Enable colour buffer, lighting and depth buffer */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
}

void
shadowDisplay(void (*drawScene) (const BoardData *, const BoardData3d *, const renderdata *), const BoardData * bd,
              const BoardData3d * bd3d, const renderdata * prd)
{
    /* Pass 1: Draw model, ambient light only (some diffuse to vary shadow darkness) */
    float zero[4] = { 0, 0, 0, 0 };
    float d1[4];
    float specular[4];
    float diffuse[4];

    drawScene(bd, bd3d, prd);

    /* Create shadow volume in stencil buffer */
    glEnable(GL_STENCIL_TEST);
    draw_shadow_volume_to_stencil(bd3d);

    /* Pass 2: Redraw model, full light in non-shadowed areas */
    glStencilFunc(GL_NOTEQUAL, midStencilVal, (GLuint) ~ 0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glGetLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    d1[0] = d1[1] = d1[2] = d1[3] = prd->dimness;
    glLightfv(GL_LIGHT0, GL_DIFFUSE, d1);
    /* No specular light */
    glGetLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_SPECULAR, zero);

    if (renderingBase)
        drawScene(bd, bd3d, prd);
    else
        drawBoard(bd, bd3d, prd);

    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

    glDisable(GL_STENCIL_TEST);
}
