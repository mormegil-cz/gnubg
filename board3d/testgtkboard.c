/*
* gtkboard.c
* by Jon Kinsey, 2003
*
* Test functions to setup BoardData structure
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of version 2 of the GNU General Public License as
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

#include <memory.h>
#include <stdlib.h>
#include "inc3d.h"
#include <string.h>

#ifdef BUILDING_LIB
#define PATH "Data//"
#else
#define PATH "..//Data//"
#endif

void DeleteTexture(Texture* texture);
int LoadTexture(Texture* texture, const char* Filename);
void setDicePos(BoardData* bd);

#ifndef BUILDING_LIB
int fClockwise; /* Player 1 moves clockwise */
int fGUIDiceArea; /* Show dice below board */
#endif

int DiceBelowBoard(BoardData *bd)
{
	return (!bd->dice[0] && !bd->dice_opponent[0] && !bd->shakingDice);
}

void SetupMat(Material* pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb, int shin, float a)
{
	pMat->ambientColour[0] = r;
	pMat->ambientColour[1] = g;
	pMat->ambientColour[2] = b;
	pMat->ambientColour[3] = a;

	pMat->diffuseColour [0] = dr;
	pMat->diffuseColour [1] = dg;
	pMat->diffuseColour [2] = db;
	pMat->diffuseColour [3] = a;

	pMat->specularColour[0] = sr;
	pMat->specularColour[1] = sg;
	pMat->specularColour[2] = sb;
	pMat->specularColour[3] = a;
	pMat->shininess = shin;

	pMat->alphaBlend = (a != 1) && (a != 0);

	pMat->pTexture = 0;
}

void SetupSimpleMatAlpha(Material* pMat, float r, float g, float b, float a)
{
	SetupMat(pMat, r, g, b, r, g, b, 0, 0, 0, 0, a);
}

void SetupSimpleMat(Material* pMat, float r, float g, float b)
{
	SetupMat(pMat, r, g, b, r, g, b, 0, 0, 0, 0, 0);
}

void SetTexture(BoardData *bd, Material* pMat, const char* filename)
{
	/* See if already loaded */
	int i;
	const char* nameStart = filename;
	/* Find start of name in filename */
	char* newStart = 0;
	do
	{
		if (!(newStart = strchr(nameStart, '\\')))
			newStart = strchr(nameStart, '/');

		if (newStart)
			nameStart = newStart + 1;
	} while(newStart);

	/* Search for name in cached list */
	for (i = 0; i < bd->numTextures; i++)
	{
		if (!strcasecmp(nameStart, bd->textureNames[i]))
		{	/* found */
			pMat->pTexture = &bd->textureList[i];
			return;
		}
	}

	/* Not found - Load new texture */
	if (LoadTexture(&bd->textureList[bd->numTextures], filename))
	{
		/* Remeber name */
		bd->textureNames[bd->numTextures] = malloc(strlen(nameStart) + 1);
		strcpy(bd->textureNames[bd->numTextures], nameStart);

		pMat->pTexture = &bd->textureList[bd->numTextures];
		bd->numTextures++;
	}
}

void RemoveTexture(BoardData *bd, Material* pMat)
{
	if (pMat->pTexture)
	{
		int i = 0;
		while (&bd->textureList[i] != pMat->pTexture)
			i++;

		DeleteTexture(&bd->textureList[i]);
		free(bd->textureNames[i]);

		while (i != bd->numTextures - 1)
		{
			bd->textureList[i] = bd->textureList[i + 1];
			bd->textureNames[i] = bd->textureNames[i + 1];
			i++;
		}
		bd->numTextures--;
		pMat->pTexture = 0;
	}
}

void InitialPos(BoardData *bd)
{	/* Set up initial board position */
	int ip[] = {0,-2,0,0,0,0,5,0,3,0,0,0,-5,5,0,0,0,-3,0,-5,0,0,0,0,2,0,0,0};
	memcpy(bd->points, ip, sizeof(bd->points));
updatePieceOccPos(bd);
}

void EmptyPos(BoardData *bd)
{	/* All checkers home */
	int ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,-15};
	memcpy(bd->points, ip, sizeof(bd->points));
updatePieceOccPos(bd);
}

void InitBoard3d(BoardData *bd)
{
	int i, j;
	/* Assign random rotation to each board position */
	for (i = 0; i < 28; i++)
		for (j = 0; j < 15; j++)
			bd->pieceRotation[i][j] = rand() % 360;

	bd->cube = 1;
	bd->cube_owner = 0;
	bd->doubled = 0;

	bd->crawford_game = 0;
	bd->cube_use = 1;

	bd->State = BOARD_OPEN;

	fGUIDiceArea = 1;

	bd->moving = 0;

	bd->drag_point = -1;

	bd->direction = 1;
	fClockwise = 0;

	bd->turn = 1;

	bd->colour = 0;
	bd->showIndicator = 1;

	bd->shakingDice = 0;

	bd->dice_roll[0] = bd->dice_roll[1] = -1;
setDicePos(bd);
	SetupSimpleMat(&bd->gap, 0, 0, 0);

	/* Set initial curve accuracy */
	bd->step_accuracy = 36;

	bd->numTextures = 0;
	for (i = 0; i < MAX_TEXTURES; i++)
		bd->textureList[i].texID = 0;

	bd->LightAmbient = .5f;
	bd->LightDiffuse = .7f;
	bd->LightSpecular = 1;

	bd->LightPosition[0] = 0;
	bd->LightPosition[1] = base_unit * 40;
	bd->LightPosition[2] = base_unit * 70;
	bd->LightPosition[3] = 1;

#ifndef BUILDING_LIB
	InitialPos(bd);
#endif

	bd->fovAngle = 45.0f;
	bd->boardAngle = 25.0f;

	bd->resigned = 0;
	SetupMat(&bd->flagMat, 1, 1, 1, 1, 1, 1, 1, 1, 1, 50, 0);

	SetShadowDimness(bd, 50);
}

void TidyBoard(BoardData *bd)
{
	int i;
	for (i = 0; i < bd->numTextures; i++)
	{
		DeleteTexture(&bd->textureList[i]);
		free(bd->textureNames[i]);
	}
	bd->numTextures = 0;
}

void CloseBoard(BoardData* bd)
{
	EmptyPos(bd);
	bd->State = BOARD_CLOSED;
	/* Turn off most things so they don't interfere when board closed/opening */
	bd->cube_use = 0;
	bd->colour = 0;
	bd->direction = 1;
	fClockwise = 0;
	/* Sort out logo */
	SetupSimpleMat(&bd->logoMat, bd->boxMat.ambientColour[0], bd->boxMat.ambientColour[1], bd->boxMat.ambientColour[2]);
	if (bd->boxMat.pTexture)
	{
		static int test = 0;
		if (test)
			SetTexture(bd, &bd->logoMat, PATH"logo.bmp");
		else
			SetTexture(bd, &bd->logoMat, PATH"logo2.bmp");
		test = !test;
	}
}

void SetSkin1(BoardData *bd)
{
	SetupSimpleMat(&bd->diceMat[0], 0.85f, 0.2f, 0.2f);
	SetupSimpleMatAlpha(&bd->diceDotMat[0], 0, 0, 0, 1);
	SetupSimpleMat(&bd->diceMat[1], .2f, .2f, .2f);
	SetupSimpleMatAlpha(&bd->diceDotMat[1], .9f, .9f, .9f, 1);

	SetupSimpleMat(&bd->cubeMat, .7f, .7f, .65f);
	SetupSimpleMatAlpha(&bd->cubeNumberMat, 0, 0, .4f, 1);

	SetupSimpleMatAlpha(&bd->checkerMat[0], .85f, .2f, .2f, .7f);
	SetupSimpleMatAlpha(&bd->checkerMat[1], .2f, .2f, .2f, .7f);

	SetupSimpleMatAlpha(&bd->boxMat, .85f, .45f, .15f, 1);
	SetTexture(bd, &bd->boxMat, PATH"board.bmp");
	SetupSimpleMat(&bd->baseMat, 0, .6f, 0);
	SetTexture(bd, &bd->baseMat, PATH"base.bmp");
	SetupSimpleMatAlpha(&bd->pointMat[0], 1, 1, 1, 1);
	SetTexture(bd, &bd->pointMat[0], PATH"base.bmp");
	SetupSimpleMatAlpha(&bd->pointMat[1], 1, .2f, .2f, 1);
	SetTexture(bd, &bd->pointMat[1], PATH"base.bmp");

	SetupSimpleMatAlpha(&bd->pointNumberMat, .8f, .8f, .8f, 1);
	SetupSimpleMat(&bd->backGroundMat, .2f, .4f, .6f);
	SetTexture(bd, &bd->backGroundMat, PATH"base.bmp");

	SetupSimpleMat(&bd->hingeMat, .75f, .85f, .1f);
	SetTexture(bd, &bd->hingeMat, PATH"hinge.bmp");

	bd->pieceType = 0;
}

void SetSkin2(BoardData *bd)
{
	SetupSimpleMat(&bd->diceMat[0], 1, 0, .5f);
	SetupSimpleMatAlpha(&bd->diceDotMat[0], 1, 1, 0, 1);
	SetupSimpleMat(&bd->diceMat[1], 0, .5f, 1);
	SetupSimpleMatAlpha(&bd->diceDotMat[1], 1, .5f, 0, 1);

	SetupSimpleMat(&bd->cubeMat, .8f, .8f, .8f);
	SetupSimpleMatAlpha(&bd->cubeNumberMat, 0, 0, .6f, 1);

	SetupSimpleMatAlpha(&bd->checkerMat[0], 1, .6f, .6f, 1);
	SetupSimpleMatAlpha(&bd->checkerMat[1], .8f, .5f, .8f, 1);
	SetTexture(bd, &bd->checkerMat[0], PATH"checker.bmp");
	SetTexture(bd, &bd->checkerMat[1], PATH"checker.bmp");

	SetupSimpleMatAlpha(&bd->boxMat, .9f, .9f, .9f, 1);
	SetTexture(bd, &bd->boxMat, PATH"board2.bmp");
	SetupSimpleMat(&bd->baseMat, .9f, .9f, .9f);
	SetTexture(bd, &bd->baseMat, PATH"base2.bmp");
	SetupSimpleMatAlpha(&bd->pointMat[0], .8f, .8f, 1, 1);
	SetTexture(bd, &bd->pointMat[0], PATH"base2.bmp");
	SetupSimpleMatAlpha(&bd->pointMat[1], .5f, 0, .5f, 1);
	SetTexture(bd, &bd->pointMat[1], PATH"base2.bmp");

	SetupSimpleMatAlpha(&bd->pointNumberMat, .5f, .5f, .5f, 1);
	SetupSimpleMat(&bd->backGroundMat, .4f, .6f, .2f);
	SetTexture(bd, &bd->backGroundMat, PATH"base2.bmp");

	bd->pieceType = 1;
}

void SetSkin3(BoardData *bd)
{
	SetupSimpleMatAlpha(&bd->diceMat[0], .2f, .5f, .2f, .75f);
	SetupSimpleMatAlpha(&bd->diceDotMat[0], 1, 1, .9f, 1);
	SetupSimpleMatAlpha(&bd->diceMat[1], .8f, .4f, .6f, .75f);
	SetupSimpleMatAlpha(&bd->diceDotMat[1], 0, .3f, 0, 1);

	SetupSimpleMat(&bd->cubeMat, .6f, .6f, .6f);
	SetupSimpleMatAlpha(&bd->cubeNumberMat, 0, 0, .5f, 1);

	SetupSimpleMatAlpha(&bd->checkerMat[0], 1, .8f, .45f, 1);
	SetupSimpleMatAlpha(&bd->checkerMat[1], .55f, .35f, 0, 1);
	SetTexture(bd, &bd->checkerMat[0], PATH"checker2.bmp");
	SetTexture(bd, &bd->checkerMat[1], PATH"checker2.bmp");

	SetupSimpleMatAlpha(&bd->boxMat, .9f, .7f, .35f, 1);
	SetTexture(bd, &bd->boxMat, PATH"board3.bmp");
	SetupSimpleMat(&bd->baseMat, .9f, .7f, .35f);
	SetTexture(bd, &bd->baseMat, PATH"base3.bmp");
	SetupSimpleMatAlpha(&bd->pointMat[0], 1, .8f, .45f, 1);
	SetTexture(bd, &bd->pointMat[0], PATH"base3.bmp");
	SetupSimpleMatAlpha(&bd->pointMat[1], .55f, .35f, 0, 1);
	SetTexture(bd, &bd->pointMat[1], PATH"base3.bmp");

	SetupSimpleMatAlpha(&bd->pointNumberMat, 1, 1, 1, 1);
	SetupSimpleMat(&bd->backGroundMat, .6f, .2f, .4f);
	SetTexture(bd, &bd->backGroundMat, PATH"board.bmp");

	SetupSimpleMat(&bd->hingeMat, .75f, .85f, .1f);
	SetTexture(bd, &bd->hingeMat, PATH"hinge.bmp");

	bd->pieceType = 0;
}

void SetSkin4(BoardData *bd)
{
	SetupSimpleMat(&bd->diceMat[0], 0.85f, 0.2f, 0.2f);
	SetupSimpleMatAlpha(&bd->diceDotMat[0], 0, 0, 0, 1);
	SetupSimpleMat(&bd->diceMat[1], .2f, .2f, .2f);
	SetupSimpleMatAlpha(&bd->diceDotMat[1], .9f, .9f, .9f, 1);

	SetupSimpleMat(&bd->cubeMat, .7f, .7f, .65f);
	SetupSimpleMatAlpha(&bd->cubeNumberMat, 0, 0, .4f, 1);

	SetupSimpleMatAlpha(&bd->checkerMat[0], .85f, .2f, .2f, .7f);
	SetupSimpleMatAlpha(&bd->checkerMat[1], .2f, .2f, .2f, .7f);

	SetupSimpleMat(&bd->baseMat, 0, .6f, 0);
	SetupSimpleMatAlpha(&bd->boxMat, .85f, .45f, .15f, 1);
	SetupSimpleMatAlpha(&bd->pointMat[0], 1, 1, 1, 1);
	SetupSimpleMatAlpha(&bd->pointMat[1], 1, .2f, .2f, 1);

	SetupSimpleMatAlpha(&bd->pointNumberMat, .8f, .8f, .8f, 1);
	SetupSimpleMat(&bd->backGroundMat, .2f, .4f, .6f);

	SetupSimpleMat(&bd->hingeMat, .75f, .85f, .1f);

	bd->pieceType = 0;
}

void SetSkin5(BoardData *bd)
{
	SetupMat(&bd->diceMat[0], 0.85f, 0.2f, 0.2f, .45f, .4f, .4f, 0.85f, 0.2f, 0.2f, 15, 0);
	SetupSimpleMatAlpha(&bd->diceDotMat[0], 0, 0, 0, 1);
	SetupMat(&bd->diceMat[1], 0.2f, 0.2f, 0.2f, 0, 0, 1, 0.2f, 0.2f, 0.2f, 15, 0);
	SetupSimpleMatAlpha(&bd->diceDotMat[1], .9f, .9f, .9f, 1);

	SetupMat(&bd->cubeMat, .7f, .7f, .65f, .5f, .5f, .5f, .5f, .5f, .5f, 50, 0);
	SetupSimpleMatAlpha(&bd->cubeNumberMat, 0, 0, .4f, 1);

	SetupMat(&bd->checkerMat[0], .85f, .2f, .2f, .95f, .2f, .35f, 1, .5f, .5f, 25, .9f);
	SetupMat(&bd->checkerMat[1], .2f, .2f, .2f, 0, 0, 1, 0, 0, 1, 25, .7f);

	SetupMat(&bd->baseMat, 0, .6f, 0, 0, .6f, 0, 0, .7f, 0, 70, 0);
	SetupMat(&bd->boxMat, .85f, .45f, .15f, .9f, .5f, .1f, .9f, .5f, .1f, 70, 1);
	SetupMat(&bd->pointMat[0], 1, 1, 1, .5f, .5f, 1, .5f, .5f, 1, 50, 1);
	SetupMat(&bd->pointMat[1], 1, .2f, .2f, .5f, .4f, .4f, .5f, .4f, .4f, 50, 1);

	SetupSimpleMatAlpha(&bd->pointNumberMat, .8f, .8f, .8f, 1);
	SetupSimpleMat(&bd->backGroundMat, .2f, .4f, .6f);

	SetupSimpleMat(&bd->hingeMat, .75f, .85f, .1f);

	bd->pieceType = 0;
}

void SetSkin6(BoardData *bd)
{
	SetupMat(&bd->diceMat[0], 0.85f, 0.2f, 0.2f, .45f, .4f, .4f, 0.85f, 0.2f, 0.2f, 15, 0);
	SetupSimpleMatAlpha(&bd->diceDotMat[0], 0, 0, 0, 1);
	SetupMat(&bd->diceMat[1], 0.2f, 0.2f, 0.2f, 0, 0, 1, 0.2f, 0.2f, 0.2f, 15, 0);
	SetupSimpleMatAlpha(&bd->diceDotMat[1], .9f, .9f, .9f, 1);

	SetupMat(&bd->cubeMat, .7f, .7f, .65f, .5f, .5f, .5f, .5f, .5f, .5f, 50, 0);
	SetupSimpleMatAlpha(&bd->cubeNumberMat, 0, 0, .4f, 1);

	SetupMat(&bd->checkerMat[0], .85f, .2f, .2f, .95f, .2f, .35f, 1, .5f, .5f, 25, .9f);
	SetupMat(&bd->checkerMat[1], .2f, .2f, .2f, 0, 0, 1, 0, 0, 1, 25, .7f);

	SetupMat(&bd->boxMat, .85f, .45f, .15f, .9f, .5f, .1f, .9f, .5f, .1f, 70, 1);
	SetTexture(bd, &bd->boxMat, PATH"board.bmp");
	SetupMat(&bd->baseMat, 0, .6f, 0, 0, .6f, 0, 0, .7f, 0, 70,0);
	SetTexture(bd, &bd->baseMat, PATH"base.bmp");
	SetupMat(&bd->pointMat[0], 1, 1, 1, .5f, .5f, 1, .5f, .5f, 1, 50, 1);
	SetTexture(bd, &bd->pointMat[0], PATH"base.bmp");
	SetupMat(&bd->pointMat[1], 1, .2f, .2f, .5f, .4f, .4f, .5f, .4f, .4f, 50, 1);
	SetTexture(bd, &bd->pointMat[1], PATH"base.bmp");

	SetupSimpleMatAlpha(&bd->pointNumberMat, .8f, .8f, .8f, 1);
	SetupSimpleMat(&bd->backGroundMat, .2f, .4f, .6f);
	SetTexture(bd, &bd->backGroundMat, PATH"base.bmp");

	SetupSimpleMat(&bd->hingeMat, .75f, .85f, .1f);
	SetTexture(bd, &bd->hingeMat, PATH"hinge.bmp");

	bd->pieceType = 0;
}

void SetSkin(BoardData *bd, int num)
{
	TidyBoard(bd);
	if (num == 1)
		SetSkin1(bd);
	if (num == 2)
		SetSkin2(bd);
	if (num == 3)
		SetSkin3(bd);
	if (num == 4)
		SetSkin4(bd);
	if (num == 5)
		SetSkin5(bd);
	if (num == 6)
		SetSkin6(bd);

	preDrawThings(bd);
	updateHingeOccPos(bd);
}
