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
#include <GL/gl.h>
#include "inc3d.h"
#include <string.h>
#include <stdio.h>

void DeleteTexture(Texture* texture);
int LoadTexture(Texture* texture, const char* Filename);

void SetSkin1(BoardData *bd)
{
	SetTexture(bd, &bd->boxMat, TEXTURE_PATH"board.bmp");
	SetTexture(bd, &bd->baseMat, TEXTURE_PATH"base.bmp");
	SetTexture(bd, &bd->pointMat[0], TEXTURE_PATH"base.bmp");
	SetTexture(bd, &bd->pointMat[1], TEXTURE_PATH"base.bmp");

	SetTexture(bd, &bd->backGroundMat, TEXTURE_PATH"base.bmp");

	SetTexture(bd, &bd->hingeMat, TEXTURE_PATH"hinge.bmp");

	bd->pieceType = 0;
}

void SetSkin2(BoardData *bd)
{
	SetTexture(bd, &bd->checkerMat[0], TEXTURE_PATH"checker.bmp");
	SetTexture(bd, &bd->checkerMat[1], TEXTURE_PATH"checker.bmp");

	SetTexture(bd, &bd->boxMat, TEXTURE_PATH"board2.bmp");
	SetTexture(bd, &bd->baseMat, TEXTURE_PATH"base2.bmp");
	SetTexture(bd, &bd->pointMat[0], TEXTURE_PATH"base2.bmp");
	SetTexture(bd, &bd->pointMat[1], TEXTURE_PATH"base2.bmp");

	SetTexture(bd, &bd->backGroundMat, TEXTURE_PATH"base2.bmp");

	SetTexture(bd, &bd->hingeMat, TEXTURE_PATH"hinge.bmp");

	bd->pieceType = 1;
}

void SetSkin3(BoardData *bd)
{
	SetTexture(bd, &bd->checkerMat[0], TEXTURE_PATH"checker2.bmp");
	SetTexture(bd, &bd->checkerMat[1], TEXTURE_PATH"checker2.bmp");

	SetTexture(bd, &bd->boxMat, TEXTURE_PATH"board3.bmp");
	SetTexture(bd, &bd->baseMat, TEXTURE_PATH"base3.bmp");
	SetTexture(bd, &bd->pointMat[0], TEXTURE_PATH"base3.bmp");
	SetTexture(bd, &bd->pointMat[1], TEXTURE_PATH"base3.bmp");

	SetTexture(bd, &bd->backGroundMat, TEXTURE_PATH"board.bmp");

	SetTexture(bd, &bd->hingeMat, TEXTURE_PATH"hinge.bmp");

	bd->pieceType = 0;
}
/*
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
	SetTexture(bd, &bd->boxMat, TEXTURE_PATH"board.bmp");
	SetupMat(&bd->baseMat, 0, .6f, 0, 0, .6f, 0, 0, .7f, 0, 70,0);
	SetTexture(bd, &bd->baseMat, TEXTURE_PATH"base.bmp");
	SetupMat(&bd->pointMat[0], 1, 1, 1, .5f, .5f, 1, .5f, .5f, 1, 50, 1);
	SetTexture(bd, &bd->pointMat[0], TEXTURE_PATH"base.bmp");
	SetupMat(&bd->pointMat[1], 1, .2f, .2f, .5f, .4f, .4f, .5f, .4f, .4f, 50, 1);
	SetTexture(bd, &bd->pointMat[1], TEXTURE_PATH"base.bmp");

	SetupSimpleMatAlpha(&bd->pointNumberMat, .8f, .8f, .8f, 1);
	SetupSimpleMat(&bd->backGroundMat, .2f, .4f, .6f);
	SetTexture(bd, &bd->backGroundMat, TEXTURE_PATH"base.bmp");

	SetupSimpleMat(&bd->hingeMat, .75f, .85f, .1f);
	SetTexture(bd, &bd->hingeMat, TEXTURE_PATH"hinge.bmp");

	bd->pieceType = 0;
}
*/
#include "shadow.h"

void SetTestTextures(BoardData *bd, int num)
{
	if (num == 0)
		SetSkin1(bd);
	if (num == 1)
		SetSkin2(bd);
	if (num == 2)
		SetSkin3(bd);
}

void setA(Material* pMat, double val[4])
{
	SetupSimpleMatAlpha(pMat, val[0],  val[1],  val[2], val[3]);
}

void set(Material* pMat, double val[3], int a)
{
	double temp[4];
	temp[0] = val[0];
	temp[1] = val[1];
	temp[2] = val[2];
	temp[3] = a;
	setA(pMat, temp);
}

void setX(Material* pMat, unsigned char val[3], int a)
{
	int i;
	double ar[3];
	for( i = 0; i < 3; i++ )
		ar[ i ] = val[i] / 255.0;

	set(pMat, ar, a);
}

void testSet3dSetting(BoardData* bd, renderdata *prd, int testRow)
{
	bd->pieceType = 0;
	ClearTextures(bd);

//	updateHingeOccPos(bd);

	setA(&bd->checkerMat[0], prd->aarColour[0]);
	setA(&bd->checkerMat[1], prd->aarColour[1]);

	set(&bd->diceMat[0], prd->afDieColour[0] ? prd->aarColour[0] : prd->aarDiceColour[0], 0);
	set(&bd->diceMat[1], prd->afDieColour[1] ? prd->aarColour[1] : prd->aarDiceColour[1], 0);
	set(&bd->diceDotMat[0], prd->aarDiceDotColour[0], 1);
	set(&bd->diceDotMat[1], prd->aarDiceDotColour[1], 1);

	set(&bd->cubeMat, prd->arCubeColour, 0);
	{
	double testNumberCol[3] = {0, 0, .4};
	double testPointNumberCol[3] = {.8, .8, .8};
	double testBackgroundCol[3] = {.2f, .4f, .6f};
	double testHingeCol[3] = {.75f, .85f, .1f};

	set(&bd->cubeNumberMat, testNumberCol, 1);
	set(&bd->pointNumberMat, testPointNumberCol, 1);
	set(&bd->backGroundMat, testBackgroundCol, 1);
	set(&bd->hingeMat, testHingeCol, 1);
	}

	setX(&bd->boxMat, prd->aanBoardColour[1], 1);
	setX(&bd->baseMat, prd->aanBoardColour[0], 0);
	setX(&bd->pointMat[0], prd->aanBoardColour[2], 1);
	setX(&bd->pointMat[1], prd->aanBoardColour[3], 1);

	bd->showHinges = prd->fHinges;

	SetTestTextures(bd, testRow);

	preDraw3d(bd);
}

void CopyTexture(BoardData* from, BoardData* to, Material* fromMat, Material* toMat)
{
	char textureFile[255];
	int i;

	if (!fromMat->pTexture || !fromMat->pTexture->texID)
		return;

	i = 0;
	while (&from->textureList[i] != fromMat->pTexture)
		i++;

	sprintf(textureFile, TEXTURE_PATH"%s", from->textureName[i]);

	SetTexture(to, toMat, textureFile);
}

void CopySettings3d(BoardData* from, BoardData* to)
{
	ClearTextures(to);

	memcpy(&to->checkerMat[0], &from->checkerMat[0], sizeof(Material));
	memcpy(&to->checkerMat[1], &from->checkerMat[1], sizeof(Material));

	memcpy(&to->diceMat[0], &from->diceMat[0], sizeof(Material));
	memcpy(&to->diceMat[1], &from->diceMat[1], sizeof(Material));
	memcpy(&to->diceDotMat[0], &from->diceDotMat[0], sizeof(Material));
	memcpy(&to->diceDotMat[1], &from->diceDotMat[1], sizeof(Material));

	memcpy(&to->cubeMat, &from->cubeMat, sizeof(Material));

	memcpy(&to->boxMat, &from->boxMat, sizeof(Material));
	memcpy(&to->baseMat, &from->baseMat, sizeof(Material));
	memcpy(&to->pointMat[0], &from->pointMat[0], sizeof(Material));
	memcpy(&to->pointMat[1], &from->pointMat[1], sizeof(Material));

	CopyTexture(from, to, &from->checkerMat[0], &to->checkerMat[0]);
	CopyTexture(from, to, &from->checkerMat[1], &to->checkerMat[1]);

	CopyTexture(from, to, &from->boxMat, &to->boxMat);
	CopyTexture(from, to, &from->baseMat, &to->baseMat);
	CopyTexture(from, to, &from->pointMat[0], &to->pointMat[0]);
	CopyTexture(from, to, &from->pointMat[1], &to->pointMat[1]);

	CopyTexture(from, to, &from->backGroundMat, &to->backGroundMat);
	CopyTexture(from, to, &from->hingeMat, &to->hingeMat);

	to->pieceType = from->pieceType;
	to->showHinges = from->showHinges;

	preDraw3d(to);
}
