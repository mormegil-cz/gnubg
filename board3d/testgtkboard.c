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
//	SetTexture(bd, &bd->chequerMat[0], TEXTURE_PATH"checker.bmp");
//	SetTexture(bd, &bd->chequerMat[1], TEXTURE_PATH"checker.bmp");

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
//	SetTexture(bd, &bd->chequerMat[0], TEXTURE_PATH"checker2.bmp");
//	SetTexture(bd, &bd->chequerMat[1], TEXTURE_PATH"checker2.bmp");

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

	SetupSimpleMatAlpha(&bd->chequerMat[0], .85f, .2f, .2f, .7f);
	SetupSimpleMatAlpha(&bd->chequerMat[1], .2f, .2f, .2f, .7f);

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

	SetupMat(&bd->chequerMat[0], .85f, .2f, .2f, .95f, .2f, .35f, 1, .5f, .5f, 25, .9f);
	SetupMat(&bd->chequerMat[1], .2f, .2f, .2f, 0, 0, 1, 0, 0, 1, 25, .7f);

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

	SetupMat(&bd->chequerMat[0], .85f, .2f, .2f, .95f, .2f, .35f, 1, .5f, .5f, 25, .9f);
	SetupMat(&bd->chequerMat[1], .2f, .2f, .2f, 0, 0, 1, 0, 0, 1, 25, .7f);

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

void GetTexture(BoardData* bd, Material* pMat)
{
	if (pMat->textureInfo)
	{
		char buf[100];
		strcpy(buf, TEXTURE_PATH);
		strcat(buf, pMat->textureInfo->file);
		SetTexture(bd, pMat, buf);
	}
	else
		pMat->pTexture = 0;
}

void GetTextures(BoardData* bd)
{
	GetTexture(bd, &bd->chequerMat[0]);
	GetTexture(bd, &bd->chequerMat[1]);
	GetTexture(bd, &bd->baseMat);
	GetTexture(bd, &bd->pointMat[0]);
	GetTexture(bd, &bd->pointMat[1]);
	GetTexture(bd, &bd->boxMat);
	GetTexture(bd, &bd->hingeMat);
	GetTexture(bd, &bd->backGroundMat);
}

void testSet3dSetting(BoardData* bd, const renderdata *prd)
{
	bd->pieceType = prd->pieceType;

	bd->showHinges = prd->fHinges;

	memcpy(bd->chequerMat, prd->rdChequerMat, sizeof(Material[2]));

	memcpy(&bd->diceMat[0], prd->afDieColour[0] ? &prd->rdChequerMat[0] : &prd->rdDiceMat[0], sizeof(Material));
	memcpy(&bd->diceMat[1], prd->afDieColour[1] ? &prd->rdChequerMat[1] : &prd->rdDiceMat[1], sizeof(Material));
	bd->diceMat[0].textureInfo = bd->diceMat[1].textureInfo = 0;

	memcpy(bd->diceDotMat, prd->rdDiceDotMat, sizeof(Material[2]));

	memcpy(&bd->cubeMat, &prd->rdCubeMat, sizeof(Material));
	memcpy(&bd->cubeNumberMat, &prd->rdCubeNumberMat, sizeof(Material));

	memcpy(&bd->baseMat, &prd->rdBaseMat, sizeof(Material));
	memcpy(bd->pointMat, prd->rdPointMat, sizeof(Material[2]));

	memcpy(&bd->boxMat, &prd->rdBoxMat, sizeof(Material));
	memcpy(&bd->hingeMat, &prd->rdHingeMat, sizeof(Material));
	memcpy(&bd->pointNumberMat, &prd->rdPointNumberMat, sizeof(Material));
	memcpy(&bd->backGroundMat, &prd->rdBackGroundMat, sizeof(Material));
}

void CopySettings3d(BoardData* from, BoardData* to)
{
	memcpy(to->chequerMat, from->chequerMat, sizeof(Material[2]));

	memcpy(to->diceMat, from->diceMat, sizeof(Material[2]));
	memcpy(to->diceDotMat, from->diceDotMat, sizeof(Material[2]));

	memcpy(&to->cubeMat, &from->cubeMat, sizeof(Material));
	memcpy(&to->cubeNumberMat, &from->cubeNumberMat, sizeof(Material));

	memcpy(&to->baseMat, &from->baseMat, sizeof(Material));
	memcpy(&to->pointMat[0], &from->pointMat[0], sizeof(Material));
	memcpy(&to->pointMat[1], &from->pointMat[1], sizeof(Material));

	memcpy(&to->boxMat, &from->boxMat, sizeof(Material));
	memcpy(&to->hingeMat, &from->hingeMat, sizeof(Material));
	memcpy(&to->pointNumberMat, &from->pointNumberMat, sizeof(Material));
	memcpy(&to->backGroundMat, &from->backGroundMat, sizeof(Material));

	to->pieceType = from->pieceType;
	to->showHinges = from->showHinges;
}
