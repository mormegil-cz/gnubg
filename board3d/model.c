/*
 * model.c
 * by Jon Kinsey, 2003
 *
 * Shadow model representation
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

#define TOP_EDGE -2

typedef struct _position {
    float x, y, z;
} position;

typedef struct _plane {
    float a, b, c, d;
} plane;

typedef struct _winged_edge {
    unsigned int e[2];          /* vertex index */
    int w[2];                   /* plane index */
} winged_edge;

void
initOccluder(Occluder * pOcc)
{
    pOcc->handle = (OccModel *) malloc(sizeof(OccModel));
    pOcc->handle->planes = g_array_new(FALSE, FALSE, sizeof(plane));
    pOcc->handle->edges = g_array_new(FALSE, FALSE, sizeof(winged_edge));
    pOcc->handle->points = g_array_new(FALSE, FALSE, sizeof(position));

    pOcc->shadow_list = glGenLists(1);
    pOcc->rotator = 0;
    pOcc->show = 1;
}

void
freeOccluder(Occluder * pOcc)
{
    if (pOcc->handle) {
        g_array_free(pOcc->handle->planes, TRUE);
        g_array_free(pOcc->handle->edges, TRUE);
        g_array_free(pOcc->handle->points, TRUE);
        free(pOcc->handle);
        pOcc->handle = 0;
        glDeleteLists(pOcc->shadow_list, 1);
    }
}

void
copyOccluder(const Occluder * fromOcc, Occluder * toOcc)
{
    toOcc->handle = fromOcc->handle;
    toOcc->show = fromOcc->show;
    toOcc->rotator = fromOcc->rotator;
    toOcc->shadow_list = glGenLists(1);
}

void
moveToOcc(const Occluder * pOcc)
{
    glTranslatef(pOcc->trans[0], pOcc->trans[1], pOcc->trans[2]);

    if (pOcc->rotator) {
        glRotatef(pOcc->rot[0], 0.f, 1.f, 0.f);
        glRotatef(pOcc->rot[1], 1.f, 0.f, 0.f);
        glRotatef(pOcc->rot[2], 0.f, 0.f, 1.f);
    }
}

static unsigned int
AddPos(GArray * points, float a, float b, float c)
{
    unsigned int index;
    position pos;
    pos.x = a;
    pos.y = b;
    pos.z = c;

    for (index = 0; index < points->len; index++) {
        if (!memcmp(&pos, &g_array_index(points, position, index), sizeof(position)))
            return index;
    }

    g_array_append_val(points, pos);
    return points->len - 1;
}

static void
CreatePlane(plane * p, const position * p1, const position * p2, const position * p3)
{
    float v0[3];
    float v1[3];
    float cr[3];
    float l;

    v0[0] = p2->x - p1->x;
    v0[1] = p2->y - p1->y;
    v0[2] = p2->z - p1->z;

    v1[0] = p3->x - p1->x;
    v1[1] = p3->y - p1->y;
    v1[2] = p3->z - p1->z;

    cr[0] = v0[1] * v1[2] - v0[2] * v1[1];
    cr[1] = v0[2] * v1[0] - v0[0] * v1[2];
    cr[2] = v0[0] * v1[1] - v0[1] * v1[0];

    l = sqrtf(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]);
    if (l == 0) {               /* degenerate triangle */
        p->a = p->b = p->c = p->d = 0;
        return;
    }
    p->a = cr[0] / l;
    p->b = cr[1] / l;
    p->c = cr[2] / l;

    /* signed distance of a point on the plane from the origin */
    p->d = -(p->a * p1->x + p->b * p1->y + p->c * p1->z);
}

static unsigned int
AddPlane(GArray * planes, const position * a, const position * b, const position * c)
{
    unsigned int index;
    plane p;
    CreatePlane(&p, a, b, c);

    for (index = 0; index < planes->len; index++) {
        if (!memcmp(&p, &g_array_index(planes, plane, index), sizeof(plane)))
            return index;
    }

    g_array_append_val(planes, p);
    return planes->len - 1;
}

/* For testing */
#ifdef TEST_HARNESS
void
GenerateShadowEdges(const Occluder * pOcc)
{
    unsigned int i, numEdges = pOcc->handle->edges->len;
    for (i = 0; i < numEdges; i++) {
        winged_edge *we = &g_array_index(pOcc->handle->edges, winged_edge, i);
        position *pn0 = &g_array_index(pOcc->handle->points, position, we->e[0]);
        position *pn1 = &g_array_index(pOcc->handle->points, position, we->e[1]);

        glBegin(GL_LINES);
        glVertex3f(pn0->x, pn0->y, pn0->z);
        glVertex3f(pn1->x, pn1->y, pn1->z);
        glEnd();

        {
            float xDiff = pn1->x - pn0->x;
            float yDiff = pn1->y - pn0->y;
            float zDiff = pn1->z - pn0->z;
            float xPos = pn0->x + xDiff * .80f;
            float yPos = pn0->y + yDiff * .80f;
            float zPos = pn0->z + zDiff * .80f;
            glBegin(GL_POINTS);
            glVertex3f(xPos, yPos, zPos);
            glEnd();

            if (we->w[1] >= 0) {
                xPos = pn0->x + xDiff * .20f;
                yPos = pn0->y + yDiff * .20f;
                zPos = pn0->z + zDiff * .20f;
                glBegin(GL_POINTS);
                glVertex3f(xPos, yPos, zPos);
                glEnd();
            }
        }
    }
}
#endif

static float
sqdDist(const GArray * planes, int pIndex, const float point[4])
{
    plane *p = &g_array_index(planes, plane, pIndex);
    return (p->a * point[0] + p->b * point[1] + p->c * point[2] + p->d * point[3]);
}

void
GenerateShadowVolume(const Occluder * pOcc, const float olight[4])
{
    unsigned int edgeOrder[2];
    unsigned int i, numEdges = pOcc->handle->edges->len;

    for (i = 0; i < numEdges; i++) {
        winged_edge *we = &g_array_index(pOcc->handle->edges, winged_edge, i);

        float f0 = sqdDist(pOcc->handle->planes, we->w[0], olight);
        float f1;
        if (we->w[1] >= 0)
            f1 = sqdDist(pOcc->handle->planes, we->w[1], olight);
        else {
            if (we->w[1] == TOP_EDGE && f0 < 0)
                continue;
            f1 = -f0;
        }

        if (f0 >= 0 && f1 < 0) {
            edgeOrder[0] = we->e[1];
            edgeOrder[1] = we->e[0];
        } else if (f1 >= 0 && f0 < 0) {
            edgeOrder[0] = we->e[0];
            edgeOrder[1] = we->e[1];
        } else {
            continue;
        }

        {
            position *pn0 = &g_array_index(pOcc->handle->points, position, edgeOrder[0]);
            position *pn1 = &g_array_index(pOcc->handle->points, position, edgeOrder[1]);

            /* local segment */
            glVertex3f(pn0->x, pn0->y, pn0->z);
            glVertex3f(pn1->x, pn1->y, pn1->z);
            /* segment projected to infinity (NB. w == 0) */
            glVertex4f(pn1->x * olight[3] - olight[0],
                       pn1->y * olight[3] - olight[1], pn1->z * olight[3] - olight[2], 0.f);

            glVertex4f(pn0->x * olight[3] - olight[0],
                       pn0->y * olight[3] - olight[1], pn0->z * olight[3] - olight[2], 0.f);
        }
    }
}

/* pair up edges */
static void
AddEdge(GArray * edges, const winged_edge * we)
{
    unsigned int i, numEdges = edges->len;
    for (i = 0; i < numEdges; i++) {
        winged_edge *we0 = &g_array_index(edges, winged_edge, i);
        /* facingness different between polys on edge! */
        g_assert((we0->e[0] != we->e[0] || we0->e[1] != we->e[1]));

        if (we0->e[0] == we->e[1] && we0->e[1] == we->e[0]) {
            /*  triple edge! bad... */
            g_assert((we0->w[1] == -1));

            we0->w[1] = we->w[0];       /* pair the edge and return */
            return;
        }
    }
    g_array_append_val(edges, *we);     /* otherwise, add the new edge */
}

static void
addALine( /*lint -e{818} */ Occluder * pOcc, float x, float y, float z, float x2, float y2, float z2, float x3,
         float y3, float z3, int otherEdge)
{                               /* Declaring pOcc as constant isn't useful as pointer member is modified (error 818) */
    winged_edge we;
    int planeNum;
    unsigned int p1 = AddPos(pOcc->handle->points, x, y, z);
    unsigned int p2 = AddPos(pOcc->handle->points, x2, y2, z2);

    position pn3;
    pn3.x = x3;
    pn3.y = y3;
    pn3.z = z3;

    planeNum =
        (int) AddPlane(pOcc->handle->planes, &g_array_index(pOcc->handle->points, position, p1),
                       &g_array_index(pOcc->handle->points, position, p2), &pn3);

    we.e[0] = p1;
    we.e[1] = p2;
    we.w[0] = planeNum;
    we.w[1] = otherEdge;        /* subsequent attempt to add this edge will replace w[1] */
    AddEdge(pOcc->handle->edges, &we);
}

static void
addLine(Occluder * pOcc, float x, float y, float z, float x2, float y2, float z2, float x3, float y3, float z3)
{
    addALine(pOcc, x, y, z, x2, y2, z2, x3, y3, z3, -1);
}

static void
addLineV(Occluder * pOcc, const float v1[3], const float v2[3], const float v3[3])
{
    addLine(pOcc, v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], v3[0], v3[1], v3[2]);
}

static void
addTopLine(Occluder * pOcc, float x, float y, float z, float x2, float y2, float z2)
{
    float z3;
    z3 = z - .1f;

    addALine(pOcc, x, y, z, x2, y2, z2, x, y, z3, TOP_EDGE);
}

void
addClosedSquare(Occluder * pOcc, float x, float y, float z, float w, float h, float d)
{
    addLine(pOcc, x, y, z, x, y, z + d, x + .1f, y, z);
    addLine(pOcc, x, y + h, z, x, y + h, z + d, x + .1f, y + h, z);
    addLine(pOcc, x + w, y + h, z, x + w, y + h, z + d, x + w + .1f, y + h, z);
    addLine(pOcc, x + w, y, z, x + w, y, z + d, x + w - .1f, y, z);

    addLine(pOcc, x, y, z + d, x, y, z, x, y + .1f, z);
    addLine(pOcc, x, y + h, z + d, x, y + h, z, x, y + h - .1f, z);
    addLine(pOcc, x + w, y + h, z + d, x + w, y + h, z, x + w, y + h + .1f, z);
    addLine(pOcc, x + w, y, z + d, x + w, y, z, x + w, y + .1f, z);

    addTopLine(pOcc, x, y, z + d, x, y + h, z + d);
    addTopLine(pOcc, x, y + h, z + d, x + w, y + h, z + d);
    addTopLine(pOcc, x + w, y + h, z + d, x + w, y, z + d);
    addTopLine(pOcc, x + w, y, z + d, x, y, z + d);
}

void
addSquare(Occluder * pOcc, float x, float y, float z, float w, float h, float d)
{
    addLine(pOcc, x, y, z, x, y, z + d, x + .1f, y, z);
    addLine(pOcc, x, y + h, z, x, y + h, z + d, x + .1f, y + h, z);
    addLine(pOcc, x + w, y + h, z, x + w, y + h, z + d, x + w - .1f, y + h, z);
    addLine(pOcc, x + w, y, z, x + w, y, z + d, x + w - .1f, y, z);

    addLine(pOcc, x, y, z + d, x, y, z, x, y + .1f, z);
    addLine(pOcc, x, y + h, z + d, x, y + h, z, x, y + h - .1f, z);
    addLine(pOcc, x + w, y + h, z + d, x + w, y + h, z, x + w, y + h - .1f, z);
    addLine(pOcc, x + w, y, z + d, x + w, y, z, x + w, y + .1f, z);

    addTopLine(pOcc, x, y + h, z + d, x, y, z + d);
    addTopLine(pOcc, x + w, y + h, z + d, x, y + h, z + d);
    addTopLine(pOcc, x + w, y, z + d, x + w, y + h, z + d);
    addTopLine(pOcc, x, y, z + d, x + w, y, z + d);
}

void
addSquareCentered(Occluder * pOcc, float x, float y, float z, float w, float h, float d)
{
    x -= w / 2.0f;
    y -= h / 2.0f;
    z -= d / 2.0f;

    addSquare(pOcc, x, y, z, w, h, d);
}

void
addWonkyCube(Occluder * pOcc, float x, float y, float z, float w, float h, float d, float s, int full)
{
    if (full == 0) {
        addLine(pOcc, x, y + h, z, x, y, z, x, y, z + .1f);
        addLine(pOcc, x, y, z, x, y + h, z, x + .1f, y, z);

        addLine(pOcc, x, y, z, x, y, z + d, x + .1f, y, z);
        addLine(pOcc, x, y + h, z, x, y + h, z + d, x + .1f, y + h, z);
        addLine(pOcc, x, y, z + d, x, y, z, x, y + .1f, z);
        addLine(pOcc, x, y + h, z + d, x, y + h, z, x, y + h - .1f, z);

        addLine(pOcc, x, y + h, z + d, x, y, z + d, x, y, z + d - .1f);
        addLine(pOcc, x, y, z + d, x, y + h, z + d, x + .1f, y, z + d);
    }
    /* Bottom */
    addLine(pOcc, x + w, y + h, z + s, x, y + h, z, x, y + h, z + .1f);
    addLine(pOcc, x, y, z, x + w, y, z + s, x + w, y, z + s + .1f);

    addLine(pOcc, x, y + h, z, x + w, y + h, z + s, x, y + h - .1f, z);
    addLine(pOcc, x + w, y, z + s, x, y, z, x + w, y + .1f, z + s);

    if (full == 2) {
        /* Sides */
        addLine(pOcc, x + w, y + h, z + s, x + w, y + h, z + s + d, x + w - .1f, y + h, z + s);
        addLine(pOcc, x + w, y, z + s, x + w, y, z + s + d, x + w - .1f, y, z + s);
        addLine(pOcc, x + w, y + h, z + s + d, x + w, y + h, z + s, x + w, y + h - .1f, z + s);
        addLine(pOcc, x + w, y, z + s + d, x + w, y, z + s, x + w, y + .1f, z + s);

        addLine(pOcc, x + w, y, z + s, x + w, y + h, z + s, x + w, y + h, z + s + .1f);
        addLine(pOcc, x + w, y + h, z + s, x + w, y, z + s, x + w - .1f, y + h, z + s);

        addLine(pOcc, x + w, y, z + s + d, x + w, y + h, z + s + d, x + w, y + h, z + s + d - .1f);
        addLine(pOcc, x + w, y + h, z + s + d, x + w, y, z + s + d, x + w - .1f, y + h, z + s + d);
    }
    /* Top */
    addLine(pOcc, x + w, y + h, z + s + d, x, y + h, z + d, x, y + h, z + d - .1f);
    addLine(pOcc, x, y, z + d, x + w, y, z + s + d, x + w, y, z + s + d - .1f);

    addLine(pOcc, x, y + h, z + d, x + w, y + h, z + s + d, x, y + h - .1f, z + d);
    addLine(pOcc, x + w, y, z + s + d, x, y, z + d, x + w, y + .1f, z + s + d);
}

void
addCube(Occluder * pOcc, float x, float y, float z, float w, float h, float d)
{
    /* Bottom */
    addLine(pOcc, x, y + h, z, x, y, z, x, y, z + .1f);
    addLine(pOcc, x + w, y + h, z, x, y + h, z, x, y + h, z + .1f);
    addLine(pOcc, x + w, y, z, x + w, y + h, z, x + w, y + h, z + .1f);
    addLine(pOcc, x, y, z, x + w, y, z, x + w, y, z + .1f);

    addLine(pOcc, x, y, z, x, y + h, z, x + .1f, y, z);
    addLine(pOcc, x, y + h, z, x + w, y + h, z, x, y + h - .1f, z);
    addLine(pOcc, x + w, y + h, z, x + w, y, z, x + w - .1f, y + h, z);
    addLine(pOcc, x + w, y, z, x, y, z, x + w, y + .1f, z);

    /* Sides */
    addLine(pOcc, x, y, z, x, y, z + d, x + .1f, y, z);
    addLine(pOcc, x, y + h, z, x, y + h, z + d, x + .1f, y + h, z);
    addLine(pOcc, x + w, y + h, z, x + w, y + h, z + d, x + w - .1f, y + h, z);
    addLine(pOcc, x + w, y, z, x + w, y, z + d, x + w - .1f, y, z);

    addLine(pOcc, x, y, z + d, x, y, z, x, y + .1f, z);
    addLine(pOcc, x, y + h, z + d, x, y + h, z, x, y + h - .1f, z);
    addLine(pOcc, x + w, y + h, z + d, x + w, y + h, z, x + w, y + h - .1f, z);
    addLine(pOcc, x + w, y, z + d, x + w, y, z, x + w, y + .1f, z);

    /* Top */
    addLine(pOcc, x, y + h, z + d, x, y, z + d, x, y, z + d - .1f);
    addLine(pOcc, x + w, y + h, z + d, x, y + h, z + d, x, y + h, z + d - .1f);
    addLine(pOcc, x + w, y, z + d, x + w, y + h, z + d, x + w, y + h, z + d - .1f);
    addLine(pOcc, x, y, z + d, x + w, y, z + d, x + w, y, z + d - .1f);

    addLine(pOcc, x, y, z + d, x, y + h, z + d, x + .1f, y, z + d);
    addLine(pOcc, x, y + h, z + d, x + w, y + h, z + d, x, y + h - .1f, z + d);
    addLine(pOcc, x + w, y + h, z + d, x + w, y, z + d, x + w - .1f, y + h, z + d);
    addLine(pOcc, x + w, y, z + d, x, y, z + d, x + w, y + .1f, z + d);
}

#if 0                           /* No longer used */
void
addCubeCentered(Occluder * pOcc, float x, float y, float z, float w, float h, float d)
{
    x -= w / 2.0f;
    y -= h / 2.0f;
    z -= d / 2.0f;

    addCube(pOcc, x, y, z, w, h, d);
}
#endif

void
addCylinder(Occluder * pOcc, float x, float y, float z, float r, float d, unsigned int numSteps)
{
    float step = (2 * (float) G_PI) / numSteps;
    float *xPts = (float *) malloc(sizeof(float) * numSteps);
    float *yPts = (float *) malloc(sizeof(float) * numSteps);
    unsigned int i;
    g_assert(xPts && yPts);

    for (i = 0; i < numSteps; i++) {
        float ang = step * i + (step / 2.0f);
        xPts[i] = sinf(ang) * r;
        yPts[i] = cosf(ang) * r;
    }
    for (i = 0; i < numSteps; i++) {
        unsigned int next = ((i + 1) == numSteps) ? 0 : i + 1;
        unsigned int prev = (i == 0) ? numSteps - 1 : i - 1;

        addLine(pOcc, x + xPts[next], y + yPts[next], z + d, x + xPts[i], y + yPts[i], z + d,
                x + xPts[next], y + yPts[next], z + d - .1f);
        addLine(pOcc, x + xPts[i], y + yPts[i], z + d, x + xPts[next], y + yPts[next], z + d, x, y, z + d);

        addLine(pOcc, x + xPts[i], y + yPts[i], z, x + xPts[next], y + yPts[next], z,
                x + xPts[i], y + yPts[i], z + .1f);
        addLine(pOcc, x + xPts[next], y + yPts[next], z, x + xPts[i], y + yPts[i], z, x, y, z);

        addLine(pOcc, x + xPts[i], y + yPts[i], z, x + xPts[i], y + yPts[i], z + d, x + xPts[prev], y + yPts[prev], z);
        addLine(pOcc, x + xPts[i], y + yPts[i], z + d, x + xPts[i], y + yPts[i], z, x + xPts[next], y + yPts[next], z);
    }
    free(xPts);
    free(yPts);
}

void
addHalfTube(Occluder * pOcc, float r, float h, unsigned int numSteps)
{
    float step = ((2 * (float) G_PI) / numSteps) / 2.0f;
    float *xPts = (float *) malloc(sizeof(float) * (numSteps + 1));
    float *yPts = (float *) malloc(sizeof(float) * (numSteps + 1));
    unsigned int i;
    g_assert(xPts && yPts);

    for (i = 0; i <= numSteps; i++) {
        float ang = step * i - ((float) G_PI / 2.0f);
        xPts[i] = sinf(ang) * r;
        yPts[i] = cosf(ang) * r;
    }
    for (i = 0; i < numSteps; i++) {
        addLine(pOcc, xPts[i + 1], h, yPts[i + 1], xPts[i], h, yPts[i], xPts[i + 1], h - .1f, yPts[i + 1]);
        addLine(pOcc, xPts[i], h, yPts[i], xPts[i + 1], h, yPts[i + 1], 0.f, h, 0.f);

        addLine(pOcc, xPts[i], 0.f, yPts[i], xPts[i + 1], 0.f, yPts[i + 1], xPts[i], .1f, yPts[i]);
        addLine(pOcc, xPts[i + 1], 0.f, yPts[i + 1], xPts[i], 0.f, yPts[i], 0.f, 0.f, 0.f);

        if (i == 0)
            addLine(pOcc, xPts[i], 0.f, yPts[i], xPts[i], h, yPts[i], xPts[i], 0.f, yPts[i] - .1f);
        else
            addLine(pOcc, xPts[i], 0.f, yPts[i], xPts[i], h, yPts[i], xPts[i - 1], 0.f, yPts[i - 1]);

        addLine(pOcc, xPts[i], h, yPts[i], xPts[i], 0.f, yPts[i], xPts[i + 1], 0.f, yPts[i + 1]);
    }
    addLine(pOcc, xPts[i], 0.f, yPts[i], xPts[i], h, yPts[i], xPts[i - 1], 0.f, yPts[i - 1]);
    addLine(pOcc, xPts[i], h, yPts[i], xPts[i], 0.f, yPts[i], xPts[i], 0.f, yPts[i] - .1f);

    free(xPts);
    free(yPts);
}

static float
GetValue(float x, float y, float d, unsigned int c, unsigned int a, unsigned int b)
{                               /* Map (x, y, d) to corner c, face a return b co-ord */
    unsigned int i = c / 4, j = (c / 2) % 2, k = c % 2;
    unsigned int minus, val;
    if ((i + j + k) % 2)
        val = ((7 - (b + a)) % 3) + 1;
    else
        val = ((b + 3 - a) % 3) + 1;

    minus = ((k && b == 0) || (j && b == 1) || (i && b == 2));

    switch (val) {
    case 1:
        return minus ? -x : x;
    case 2:
        return minus ? -y : y;
    case 3:
        return minus ? -d : d;
    default:
        return 0;
    }
}

static void
GetCoords(float x, float y, float d, unsigned int c, unsigned int f, float v[3])
{                               /* Map (x, y, d) to corner c, face f put result in v */
    v[0] = GetValue(x, y, d, c, f, 0);
    v[1] = GetValue(x, y, d, c, f, 1);
    v[2] = GetValue(x, y, d, c, f, 2);
}

void
addDice(Occluder * pOcc, float size)
{                               /* Hard-coded numSteps to keep model simple + doesn't work correctly when > 8... */
    unsigned int numSteps = 8;
    float step = (2 * (float) G_PI) / numSteps;
    float *xPts = (float *) malloc(sizeof(float) * numSteps);
    float *yPts = (float *) malloc(sizeof(float) * numSteps);
    unsigned int i, c, f;
    g_assert(xPts && yPts);

    for (i = 0; i < numSteps; i++) {
        float ang = step * i;
        xPts[i] = sinf(ang) * size;
        yPts[i] = cosf(ang) * size;
    }

    for (c = 0; c < 8; c++) {
        for (f = 0; f < 3; f++) {
            for (i = 0; i < numSteps / 4; i++) {
                unsigned int prevFace = (f + 2) % 3;
                unsigned int nextFace = (f + 1) % 3;
                unsigned int oppPoint = numSteps / 4 - i;

                float v1[3], v2[3], v3[3];
                GetCoords(xPts[i], yPts[i], size, c, f, v1);
                GetCoords(xPts[i + 1], yPts[i + 1], size, c, f, v2);

                GetCoords(0.f, 0.f, size, c, f, v3);
                addLineV(pOcc, v1, v2, v3);

                if (i == 0)
                    GetCoords(xPts[oppPoint - 1], yPts[oppPoint - 1], size, c, prevFace, v3);
                else
                    GetCoords(xPts[oppPoint], yPts[oppPoint], size, c, nextFace, v3);
                addLineV(pOcc, v2, v1, v3);

                if (i > 0) {
                    addLineV(pOcc, v1, v3, v2);

                    GetCoords(xPts[i], yPts[i], size, c, prevFace, v2);
                    addLineV(pOcc, v3, v1, v2);
                }
            }
        }
    }

    free(xPts);
    free(yPts);
}
