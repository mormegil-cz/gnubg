
#include <GL/gl.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "matrix.h"
#include "shadow.h"
#include "inc3d.h"

int midStencilVal;
float dim = .3f;
float (*light_position)[4];

Occluder Occluders[NUM_OCC];

extern void GenerateShadowVolume(Occluder* pOcc);
extern void GenerateShadowEdges(Occluder* pOcc);

void shadowInit()
{	// Check the stencil buffer is present
	int i;
	int stencilBits;
	glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
	if (!stencilBits)
	{
		g_print("No stencil buffer - no shadows\n");
		return;
	}
	midStencilVal = (int)pow(2, stencilBits - 1);
	glClearStencil(midStencilVal);

	for (i = 0; i < NUM_OCC; i++)
		Occluders[i].handle = 0;
}

void moveToOcc(Occluder* pOcc)
{
	glTranslatef(pOcc->trans[0], pOcc->trans[1], pOcc->trans[2]);

	if (pOcc->rotator)
	{
		glRotatef(pOcc->rot[0], 0, 1, 0);
		glRotatef(pOcc->rot[1], 1, 0, 0);
		glRotatef(pOcc->rot[2], 0, 0, 1);
	}
}

void draw_shadow_volume_edges(Occluder* pOcc)
{
	if (pOcc->show)
	{
		glPushMatrix();
		moveToOcc(pOcc);
		GenerateShadowEdges(pOcc);
		glPopMatrix();
	}
}

void draw_shadow_volume_extruded_edges(Occluder* pOcc, int prim)
{
	if (pOcc->show)
	{
		glPushMatrix();
		moveToOcc(pOcc);
		glBegin(prim);
		GenerateShadowVolume(pOcc);
		glEnd();
		glPopMatrix();
	}
}

void draw_shadow_volume_to_stencil()
{
	int i;

	/* First clear the stencil buffer */
	glClear(GL_STENCIL_BUFFER_BIT);

	/* Disable colour buffer lighting and don't update depth buffer */
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_LIGHTING);
	glDepthMask(GL_FALSE);

	/* Z-pass approach */
	glStencilFunc(GL_ALWAYS, midStencilVal, ~0);

	glCullFace(GL_FRONT);
	glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

	for (i = 0; i < NUM_OCC; i++)
		draw_shadow_volume_extruded_edges(&Occluders[i], GL_QUADS);

	glCullFace(GL_BACK);
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

	for (i = 0; i < NUM_OCC; i++)
		draw_shadow_volume_extruded_edges(&Occluders[i], GL_QUADS);

	/* Enable colour buffer, lighting and depth buffer */
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glEnable(GL_LIGHTING);
}

void shadowDisplay(void (*drawScene)())
{
	/* Pass 1: Draw model, ambient light only (some diffuse to vary shadow darkness) */
	float zero[4] = {0,0,0,0};
	float d1[4] = {dim, dim, dim, dim};
	float specular[4];
	float diffuse[4];
	glGetLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, d1);
	/* No specular light */
	glGetLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_SPECULAR, zero);

	drawScene();

	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

	/* Create shadow volume in stencil buffer */
	glEnable(GL_STENCIL_TEST);
	draw_shadow_volume_to_stencil();

	/* Pass 2: Redraw model, full light in non-shadowed areas */
	glStencilFunc(GL_EQUAL, midStencilVal, ~0);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	drawScene();

	glDisable(GL_STENCIL_TEST);
}
