
#include <GL/gl.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "matrix.h"
#include "shadow.h"

#define TOP_EDGE -2
#define PI 3.14159265358979323846f

float (*light_position)[4];

typedef struct _position
{
	float x, y, z;
} position;

typedef struct _plane
{
	float a, b, c, d;
} plane;

typedef struct _winged_edge
{
	int e[2];  // vertex index
	int w[2];  // plane index
} winged_edge;

void initOccluder(Occluder* pOcc)
{
	pOcc->handle = (OccModel*)malloc(sizeof(OccModel));
	ListInit(&pOcc->handle->planes, sizeof(plane));
	ListInit(&pOcc->handle->edges, sizeof(winged_edge));
	ListInit(&pOcc->handle->points, sizeof(position));

	pOcc->rotator = 0;
	pOcc->show = 1;
}

void freeOccluder(Occluder* pOcc)
{
	if (pOcc->handle)
	{
		ListClear(&pOcc->handle->planes);
		ListClear(&pOcc->handle->edges);
		ListClear(&pOcc->handle->points);
		free(pOcc->handle);
		pOcc->handle = 0;
	}
}

void copyOccluder(Occluder* fromOcc, Occluder* toOcc)
{
	toOcc->handle = fromOcc->handle;
	toOcc->show = fromOcc->show;
	toOcc->rotator = fromOcc->rotator;
}

int AddPos(OccModel* pMod, float a, float b, float c)
{
	int index;
	position pos;
	pos.x = a;
	pos.y = b;
	pos.z = c;

	index = ListFind(&pMod->points, &pos);
	if (index == -1)
	{
		ListAdd(&pMod->points, &pos);
		return ListSize(&pMod->points) - 1;
	}
	else
		return index;
}

void CreatePlane(plane* p, position* p1, position* p2, position* p3)
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
	
	l = (float)sqrt(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]);
	if(l == 0) // degenerate triangle
	{
		p->a = p->b = p->c = p->d = 0;
		return;
	}
	p->a = cr[0] / l;
	p->b = cr[1] / l;
	p->c = cr[2] / l;
	
	// signed distance of a point on the plane from the origin
	p->d = -(p->a * p1->x + p->b * p1->y + p->c * p1->z);
}

int AddPlane(OccModel* pMod, position* a, position* b, position* c)
{
	int pos;
	plane p;
	CreatePlane(&p, a, b, c);
	pos = ListFind(&pMod->planes, &p);
	if (pos == -1)
	{
		ListAdd(&pMod->planes, &p);
		return ListSize(&pMod->planes) - 1;
	}
	else
		return pos;
}

void GenerateShadowEdges(Occluder* pOcc)
{	/* For testing */
	int i;
	int numEdges = ListSize(&pOcc->handle->edges);
	for (i = 0; i < numEdges; i++)
	{
		winged_edge *we = (winged_edge*)ListGet(&pOcc->handle->edges, i);
		position *pn0 = (position*)ListGet(&pOcc->handle->points, we->e[0]);
		position *pn1 = (position*)ListGet(&pOcc->handle->points, we->e[1]);

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

			if (we->w[1] >= 0)
			{
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

float sqdDist(OccModel* pMod, int pIndex, float point[4])
{
	plane* p = (plane*)ListGet(&pMod->planes, pIndex);

	return (p->a * point[0] + p->b * point[1] + p->c * point[2] + p->d * point[3]);
}

void GenerateShadowVolume(Occluder* pOcc)
{
	int edgeOrder[2];
	int i;
	int numEdges = ListSize(&pOcc->handle->edges);
	float olight[4];
	mult_matrix_vec(pOcc->invMat, *light_position, olight);

	for (i = 0; i < numEdges; i++)
	{
		winged_edge *we = (winged_edge*)ListGet(&pOcc->handle->edges, i);

		float f0 = sqdDist(pOcc->handle, we->w[0], olight);
		float f1;
		if(we->w[1] >= 0)
			f1 = sqdDist(pOcc->handle, we->w[1], olight);
		else
		{
			if (we->w[1] == TOP_EDGE && f0 < 0)
				continue;
			f1 = -f0;
		}

		if(f0 >= 0 && f1 < 0)
		{
			edgeOrder[0] = we->e[1];
			edgeOrder[1] = we->e[0];
		}
		else if(f1 >= 0 && f0 < 0)
		{
			edgeOrder[0] = we->e[0];
			edgeOrder[1] = we->e[1];
		}
		else
		{
			continue;
		}

		{
			position *pn0 = (position*)ListGet(&pOcc->handle->points, edgeOrder[0]);
			position *pn1 = (position*)ListGet(&pOcc->handle->points, edgeOrder[1]);

			// local segment
			glVertex3f(pn0->x, pn0->y, pn0->z);
			glVertex3f(pn1->x, pn1->y, pn1->z);
			// segment projected to infinity (NB. w == 0)
			glVertex4f(pn1->x * olight[3] - olight[0],
				pn1->y * olight[3] - olight[1],
				pn1->z * olight[3] - olight[2],
				0);

			glVertex4f(pn0->x * olight[3] - olight[0],
				pn0->y * olight[3] - olight[1],
				pn0->z * olight[3] - olight[2],
				0);
		}
	}
}

/* pair up edges */
void AddEdge(OccModel* pMod, winged_edge* we)
{
	int i;
	int numEdges = ListSize(&pMod->edges);
	for (i = 0; i < numEdges; i++)
	{
		winged_edge *we0 = (winged_edge*)ListGet(&pMod->edges, i);
		/* facingness different between polys on edge! */
		assert((we0->e[0] != we->e[0] || we0->e[1] != we->e[1]) && "facingness different between polys on edge!");

		if(we0->e[0] == we->e[1]  && we0->e[1] == we->e[0])
		{
			assert((we0->w[1] == -1) && "triple edge! bad...");

			we0->w[1] = we->w[0]; // pair the edge and return
			return;
		}
	}
	ListAdd(&pMod->edges, we);  // otherwise, add the new edge
}

void addALine(Occluder* pOcc, float x, float y, float z, float x2, float y2, float z2, float x3, float y3, float z3, int otherEdge)
{
	winged_edge we;
	int plane;
	int p1 = AddPos(pOcc->handle, x, y, z);
	int p2 = AddPos(pOcc->handle, x2, y2, z2);

	position pn3;
	pn3.x = x3;
	pn3.y = y3;
	pn3.z = z3;

	plane = AddPlane(pOcc->handle, (position*)ListGet(&pOcc->handle->points, p1), (position*)ListGet(&pOcc->handle->points, p2), &pn3);

	we.e[0] = p1;
	we.e[1] = p2;
	we.w[0] = plane;
	we.w[1] = otherEdge;  // subsequent attempt to add this edge will replace w[1] 
	AddEdge(pOcc->handle, &we);
}

void addLine(Occluder* pOcc, float x, float y, float z, float x2, float y2, float z2, float x3, float y3, float z3)
{
	addALine(pOcc, x, y, z, x2, y2, z2, x3, y3, z3, -1);
}

void addTopLine(Occluder* pOcc, float x, float y, float z, float x2, float y2, float z2)
{
	float z3;
	z3 = z - .1f;

	addALine(pOcc, x, y, z, x2, y2, z2, x, y, z3, TOP_EDGE);
}

void addClosedSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d)
{
	addLine(pOcc, x, y, z, x, y, z + d, x + .1f, y, z);
	addLine(pOcc, x, y + h, z, x, y + h, z + d, x + .1f, y + h, z);
	addLine(pOcc, x + w, y + h, z, x + w, y + h, z + d, x + w +.1f, y + h, z);
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

void addSquare(Occluder* pOcc, float x, float y, float z, float w, float h, float d)
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

void addSquareCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d)
{
	x -= w / 2.0f;
	y -= h / 2.0f;
	z -= d / 2.0f;
	
	addSquare(pOcc, x, y, z, w, h, d);
}

void addWonkyCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d, float s, int full)
{
if (full == 0)
{
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

if (full == 2)
{
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

void addCube(Occluder* pOcc, float x, float y, float z, float w, float h, float d)
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

void addCubeCentered(Occluder* pOcc, float x, float y, float z, float w, float h, float d)
{
	x -= w / 2.0f;
	y -= h / 2.0f;
	z -= d / 2.0f;

	addCube(pOcc, x, y, z, w, h, d);
}

void addCylinder(Occluder* pOcc, float x, float y, float z, float r, float d, int numSteps)
{
	float step = (2 * PI) / numSteps;
	float *xPts = (float *)malloc(sizeof(float) * numSteps);
	float *yPts = (float *)malloc(sizeof(float) * numSteps);
	int i;

	for (i = 0; i < numSteps; i++)
	{
		float ang = step * i + (step / 2.0f);
		xPts[i] = (float)sin(ang) * r;
		yPts[i] = (float)cos(ang) * r;
	}
	for (i = 0; i < numSteps; i++)
	{
		int next = ((i + 1) == numSteps) ? 0 : i + 1;
		int prev = (i == 0) ? numSteps - 1 : i - 1;

		addLine(pOcc, x + xPts[next], y + yPts[next], z + d, x + xPts[i], y + yPts[i], z + d, 
				x + xPts[next], y + yPts[next], z + d - .1f);
		addLine(pOcc, x + xPts[i], y + yPts[i], z + d, x + xPts[next], y + yPts[next], z + d,
				x, y, z + d);

		addLine(pOcc, x + xPts[i], y + yPts[i], z, x + xPts[next], y + yPts[next], z, 
				x + xPts[i], y + yPts[i], z + .1f);
		addLine(pOcc, x + xPts[next], y + yPts[next], z, x + xPts[i], y + yPts[i], z,
				x, y, z);

		addLine(pOcc, x + xPts[i], y + yPts[i], z, x + xPts[i], y + yPts[i], z + d, x + xPts[prev], y + yPts[prev], z);
		addLine(pOcc, x + xPts[i], y + yPts[i], z + d, x + xPts[i], y + yPts[i], z, x + xPts[next], y + yPts[next], z);
	}
	free(xPts);
	free(yPts);
}

void addHalfTube(Occluder* pOcc, float r, float h, int numSteps)
{
	float step = ((2 * PI) / numSteps) / 2.0f;
	float *xPts = (float *)malloc(sizeof(float) * (numSteps + 1));
	float *yPts = (float *)malloc(sizeof(float) * (numSteps + 1));
	int i;

	for (i = 0; i <= numSteps; i++)
	{
		float ang = step * i - (PI / 2.0f);
		xPts[i] = (float)sin(ang) * r;
		yPts[i] = (float)cos(ang) * r;
	}
	for (i = 0; i < numSteps; i++)
	{
		addLine(pOcc, xPts[i + 1], h, yPts[i + 1], xPts[i], h, yPts[i], xPts[i + 1], h - .1f, yPts[i + 1]);
		addLine(pOcc, xPts[i], h, yPts[i], xPts[i + 1], h, yPts[i + 1], 0, h, 0);

		addLine(pOcc, xPts[i], 0, yPts[i], xPts[i + 1], 0, yPts[i + 1], xPts[i], .1f, yPts[i]);
		addLine(pOcc, xPts[i + 1], 0, yPts[i + 1], xPts[i], 0, yPts[i], 0, 0, 0);

		if (i == 0)
			addLine(pOcc, xPts[i], 0, yPts[i], xPts[i], h, yPts[i], xPts[i], 0, yPts[i] - .1f);
		else
			addLine(pOcc, xPts[i], 0, yPts[i], xPts[i], h, yPts[i], xPts[i - 1], 0, yPts[i - 1]);

		addLine(pOcc, xPts[i], h, yPts[i], xPts[i], 0, yPts[i], xPts[i + 1], 0, yPts[i + 1]);
	}
	addLine(pOcc, xPts[i], 0, yPts[i], xPts[i], h, yPts[i], xPts[i - 1], 0, yPts[i - 1]);
	addLine(pOcc, xPts[i], h, yPts[i], xPts[i], 0, yPts[i], xPts[i], 0, yPts[i] - .1f);

	free(xPts);
	free(yPts);
}
