/*
 * export.c
 *
 * by Joern Thyssen  <jthyssen@dk.ibm.com>, 2002
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#if HAVE_LIBPNG
#include <png.h>
#endif

#include "analysis.h"
#include "backgammon.h"
#include "drawboard.h"
#include "export.h"
#include "eval.h"
#include "positionid.h"
#include "render.h"
#include "renderprefs.h"
#include "matchid.h"
#include "i18n.h"
#include "boardpos.h"
#include "boarddim.h"


extern void
CommandExportGameEquityEvolution (char *sz)
{

  CommandNotImplemented( sz );

}


extern void
CommandExportMatchEquityEvolution (char *sz)
{

  CommandNotImplemented( sz );

}

#if HAVE_LIBPNG


/* size of html images in steps of BOARD_WIDTH x BOARD_HEIGHT
   as defined in boarddim.h */

extern int
WritePNG (const char *sz, unsigned char *puch, int nStride,
	  const int nSizeX, const int nSizeY)
{

  FILE *pf;
  png_structp ppng;
  png_infop pinfo;
  png_text atext[3];
  int i;

  if (!(pf = fopen (sz, "wb")))
    return -1;

  if (!(ppng = png_create_write_struct (PNG_LIBPNG_VER_STRING,
					NULL, NULL, NULL)))
    {
      fclose (pf);
      return -1;

    }

  if (!(pinfo = png_create_info_struct (ppng)))
    {
      fclose (pf);
      png_destroy_write_struct (&ppng, NULL);
      return -1;
    }

  if (setjmp (png_jmpbuf (ppng)))
    {
      outputerr (sz);
      fclose (pf);
      png_destroy_write_struct (&ppng, &pinfo);
      return -1;
    }

  png_init_io (ppng, pf);

  png_set_IHDR (ppng, pinfo, nSizeX, nSizeY, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE);

  /* text */

  atext[0].key = "Title";
  atext[0].text = "Backgammon board";
  atext[0].compression = PNG_TEXT_COMPRESSION_NONE;

  atext[1].key = "author";
  atext[1].text = "GNU Backgammon" VERSION;
  atext[1].compression = PNG_TEXT_COMPRESSION_NONE;

#ifdef PNG_iTXt_SUPPORTED
  text_ptr[0].lang = NULL;
  text_ptr[1].lang = NULL;
#endif
  png_set_text (ppng, pinfo, atext, 2);

  png_write_info (ppng, pinfo);

  {
    png_bytep aprow[nSizeY];
    for (i = 0; i < nSizeY; ++i)
      aprow[i] = puch + nStride * i;

    png_write_image (ppng, aprow);

  }

  png_write_end (ppng, pinfo);

  png_destroy_write_struct (&ppng, &pinfo);

  fclose (pf);

  return 0;

}

#if USE_BOARD3D

#include "gtkboard.h"

void GenerateImage3d(renderdata *prd, const char* szName,
	const int nSize, const int nSizeX, const int nSizeY)
{
	unsigned char *puch;
	BoardData* bd = BOARD(pwBoard)->board_data;
	BoardData bdpw;
	GdkPixmap *ppm = gdk_pixmap_new(bd->drawing_area->window, nSizeX * nSize, nSizeY * nSize, -1 );
	void *glpixPreview;

	if (!(puch = (unsigned char *) malloc (nSizeX * nSizeY * nSize * nSize * 3)))
	{
		outputerr ("malloc");
		return;
	}
	/* Create preview area */
	glpixPreview = CreatePreviewBoard3d(&bdpw, ppm);

	/* Copy current settings */
	CopySettings3d(bd, &bdpw);
	
	/* Draw board */
	RenderBoard3d(&bdpw, prd, glpixPreview, puch);

	WritePNG(szName, puch, nSizeX * nSize * 3, nSizeX * nSize, nSizeY * nSize);

	gdk_pixmap_unref(ppm);
	free(puch);
}
#endif

static int
GenerateImage (renderimages * pri, renderdata * prd,
	       int anBoard[2][25],
	       const char *szName,
	       const int nSize, const int nSizeX, const int nSizeY,
	       const int nOffsetX, const int nOffsetY,
	       const int fMove, const int fTurn, const int fCube,
	       const int anDice[2], const int nCube, const int fDoubled,
	       const int fCubeOwner)
{

  unsigned char *puch;
  int anCubePosition[2];
  int anDicePosition[2][2];
  int nOrient;
  int doubled, color;
  /* FIXME: resignations */
  int anResignPosition[2];
  int fResign = 0, nResignOrientation = 0;
  int anArrowPosition[ 2 ];
  int cube_owner;

  if (!fMove)
    SwapSides (anBoard);

  /* allocate memory for board */

  if (!( puch = (unsigned char *) malloc ( BOARD_WIDTH * BOARD_HEIGHT *
					   nSize * nSize * 3 ) ))
    {
      outputerr ("malloc");
      return -1;
    }

  /* calculate cube position */

  if (fDoubled)
    doubled = fTurn ? -1 : 1;
  else
    doubled = 0;

  if ( ! fCubeOwner )
    cube_owner = 1;
  else if ( fCubeOwner == 1 )
    cube_owner = -1;
  else
    cube_owner = 0;


  CubePosition( FALSE, fCube, doubled, cube_owner,
                &anCubePosition[ 0 ], &anCubePosition[ 1 ], &nOrient );

  /* calculate dice position */

  /* FIXME */

  if (anDice[0])
    {
      anDicePosition[0][0] = 22 + fMove * 48;
      anDicePosition[0][1] = 32;
      anDicePosition[1][0] = 32 + fMove * 48;
      anDicePosition[1][1] = 32;
    }
  else
    {
      anDicePosition[0][0] = -7 * nSize;
      anDicePosition[0][1] = 0;
      anDicePosition[1][0] = -7 * nSize;
      anDicePosition[1][1] = -1;
    }

  ArrowPosition( fClockwise, nSize, 
                 &anArrowPosition[ 0 ], &anArrowPosition[ 1 ] );

  /* render board */

  color = fMove;

  CalculateArea( prd, puch, BOARD_WIDTH * nSize * 3, pri, anBoard, NULL,
		 (int *) anDice, anDicePosition,
		 color, anCubePosition,
		 LogCube( nCube ) + ( doubled != 0 ),
		 nOrient,
		 anResignPosition, fResign, nResignOrientation,
		 anArrowPosition, ms.gs != GAME_NONE, fMove == 1,
		 0, 0, BOARD_WIDTH * nSize, BOARD_HEIGHT * nSize);

  /* crop */

  if ( nSizeX < BOARD_WIDTH || nSizeY < BOARD_HEIGHT )
    {
      int i, j, k;

      j = 0;
      k = nOffsetY * nSize * BOARD_WIDTH + nOffsetX;
      for (i = 0; i < nSizeY * nSize; ++i)
	{
	  /* loop over each row */
	  memmove (puch + j, puch + k, nSizeX * nSize * 3);

	  j += nSizeX * nSize * 3;
	  k += BOARD_WIDTH * nSize;

	}

    }

  /* write png */

  WritePNG (szName, puch, nSizeX * nSize * 3, nSizeX * nSize, nSizeY * nSize);

	free(puch);

  return 0;
}



extern void
CommandExportPositionPNG (char *sz)
{

  renderimages ri;
  renderdata rd;

  sz = NextToken (&sz);

  if (ms.gs == GAME_NONE)
    {
      outputl (_("No game in progress (type `new game' to start one)."));
      return;
    }

  if (!sz || !*sz)
    {
      outputl (_("You must specify a file to export to (see `help export "
		 "position png')."));
      return;
    }

  if (!confirmOverwrite (sz, fConfirmSave))
    return;

  /* generate PNG image */

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		GenerateImage3d( &rdAppearance, sz, exsExport.nPNGSize,
				 BOARD_WIDTH, BOARD_HEIGHT );
	}
	else
#endif
{
  memcpy (&rd, &rdAppearance, sizeof rd);

  rd.nSize = exsExport.nPNGSize;

  assert (rd.nSize >= 1);

  RenderImages (&rd, &ri);

  GenerateImage (&ri, &rd, ms.anBoard, sz,
		 exsExport.nPNGSize, BOARD_WIDTH, BOARD_HEIGHT, 0, 0,
		 ms.fMove, ms.fTurn, fCubeUse, ms.anDice, ms.nCube,
		 ms.fDoubled, ms.fCubeOwner);

  FreeImages (&ri);
}

}


#endif /* HAVE_LIBPNG */


/*
 * For documentation of format, see ImportSnowieTxt in import.c
 */

static void
ExportSnowieTxt (FILE * pf, const matchstate * pms)
{

  int i, j;

  /* length of match */

  fprintf (pf, "%d;", pms->nMatchTo);

  /* Jacoby rule */

  fprintf (pf, "%d;", (!pms->nMatchTo && fJacoby) ? 1 : 0);

  /* unknown field (perhaps variant */

  fputs ("0;", pf);

  /* unknown field */

  fprintf (pf, "%d;", pms->nMatchTo ? 0 : 1);

  /* player on roll (0 = 1st player) */

  fprintf (pf, "%d;", pms->fMove);

  /* player names */

  fprintf (pf, "%s;%s;", ap[pms->fMove].szName, ap[!pms->fMove].szName);

  /* crawford game */

  i = pms->nMatchTo &&
    ((pms->anScore[0] == (pms->nMatchTo - 1)) ||
     (pms->anScore[1] == (pms->nMatchTo - 1))) &&
    pms->fCrawford && !pms->fPostCrawford;

  fprintf (pf, "%d;", i);

  /* scores */

  fprintf (pf, "%d;%d;", pms->anScore[pms->fMove], pms->anScore[!pms->fMove]);

  /* cube value */

  fprintf (pf, "%d;", pms->nCube);

  /* cube owner */

  if (pms->fCubeOwner == -1)
    i = 0;
  else if (pms->fCubeOwner == pms->fMove)
    i = 1;
  else
    i = -1;

  fprintf (pf, "%d;", i);

  /* chequers on the bar for player on roll */

  fprintf (pf, "%d;", -pms->anBoard[0][24]);

  /* chequers on the board */

  for (i = 0; i < 24; ++i)
    {
      if (pms->anBoard[1][i])
	j = pms->anBoard[1][i];
      else
	j = -pms->anBoard[0][23 - i];

      fprintf (pf, "%d;", j);

    }

  /* chequers on the bar for opponent */

  fprintf (pf, "%d;", pms->anBoard[1][24]);

  /* dice */

  fprintf (pf, "%d;%d;",
	   (pms->anDice[0] > 0) ? pms->anDice[0] : 0,
	   (pms->anDice[1] > 0) ? pms->anDice[1] : 0);

  fputs ("\n", pf);


}


extern void
CommandExportPositionSnowieTxt (char *sz)
{

  FILE *pf;

  sz = NextToken (&sz);

  if (ms.gs == GAME_NONE)
    {
      outputl (_("No game in progress (type `new game' to start one)."));
      return;
    }

  if (!sz || !*sz)
    {
      outputl (_("You must specify a file to export to (see `help export "
		 "position snowietxt')."));
      return;
    }

  if (!confirmOverwrite (sz, fConfirmSave))
    return;

  if (!strcmp (sz, "-"))
    pf = stdout;
  else if (!(pf = fopen (sz, "w")))
    {
      outputerr (sz);
      return;
    }

  ExportSnowieTxt (pf, &ms);

  if (pf != stdout)
    fclose (pf);

  setDefaultFileName (sz, PATH_SNOWIE_TXT);

}


static void
WriteInt16 (FILE * pf, int n)
{

  /* Write a little-endian, signed (2's complement) 16-bit integer.
     This is inefficient on hardware which is already little-endian
     or 2's complement, but at least it's portable. */

  /*  Let's just make things simple right now! */

  /* FIXME what about error handling? */

  fwrite (&n, 2, 1, pf);
  return;
#if 0

  fread (auch, 2, 1, pf);
  n = auch[0] | (auch[1] << 8);

  if (n >= 0x8000)
    n -= 0x10000;

  return n;
#endif
}

extern void
CommandExportPositionJF (char *sz)
{
  /* I think this function works now correctly. If there is some
   * inconsistencies between import pos and export pos, it's probably a
   * bug in the import code. --Oystein
   */

  FILE *fp;
  int i;
  unsigned char c;
  int anBoard[2][25];

  sz = NextToken (&sz);

  if (ms.gs == GAME_NONE)
    {
      outputl (_("No game in progress (type `new game' to start one)."));
      return;
    }

  if (!sz || !*sz)
    {
      outputl (_("You must specify a file to export to (see `help export "
		 "position pos')."));
      return;
    }


  if (!confirmOverwrite (sz, fConfirmSave))
    return;

  if (!strcmp (sz, "-"))
    fp = stdout;
  else if (!(fp = fopen (sz, "wb")))
    {
      outputerr (sz);
      return;
    }

  WriteInt16 (fp, 126);		/* Always write in JellyFish 3.0 format */
  WriteInt16 (fp, 0);		/* Never save with Caution */
  WriteInt16 (fp, 0);		/* This is unused */

  WriteInt16 (fp, fCubeUse);
  WriteInt16 (fp, fJacoby);
  WriteInt16 (fp, (nBeavers != 0));

  WriteInt16 (fp, ms.nCube);
  WriteInt16 (fp, ms.fCubeOwner + 1);

  if ((ms.gs == GAME_OVER) || (ms.gs == GAME_NONE))
    {
      WriteInt16 (fp, 0);
    }
  else
    {
      ms.anDice[0] > 0 ? WriteInt16 (fp, ms.fTurn + 1) : WriteInt16 (fp,
								     ms.
								     fTurn +
								     3);
    }
  /* 0 means starting position.
     If you add 2 to the player (to get 3 or 4)   Sure?
     it means that the player is on roll
     but the dice have not been rolled yet. */

  WriteInt16 (fp, 0);
  WriteInt16 (fp, 0);		/* FIXME Test this! */
  /* These two variables are used when you use movement #1,
     (two buttons) and tells how many moves you have left
     to play with the left and the right die, respectively.
     Initialized to 1 (if you roll a double, left = 4 and
     right = 0). If movement #2 (one button), only the first
     one (left) is used to store both dice.  */

  WriteInt16 (fp, 0);		/* Not in use */

  ms.nMatchTo ? WriteInt16 (fp, 1) : WriteInt16 (fp, 3);
  /* 1 = match, 3 = game */

  WriteInt16 (fp, 2);		/* FIXME */
  /* 1 = 2 players, 2 = JF plays one side */

  WriteInt16 (fp, 7);		/* Use level 7 */

  WriteInt16 (fp, ms.nMatchTo);
  /* 0 if single game  */

  if (ms.nMatchTo == 0)
    {
      WriteInt16 (fp, 0);
      WriteInt16 (fp, 0);
    }
  else
    {
      WriteInt16 (fp, ms.anScore[0]);
      WriteInt16 (fp, ms.anScore[1]);
    }

  c = strlen (ap[0].szName);
  fwrite (&c, 1, 1, fp);
  for (i = 0; i < c; i++)
    fwrite (&ap[0].szName[i], 1, 1, fp);

  c = strlen (ap[1].szName);
  fwrite (&c, 1, 1, fp);
  for (i = 0; i < c; i++)
    fwrite (&ap[1].szName[i], 1, 1, fp);

  WriteInt16 (fp, 0);		/* FIXME Check gnubg setting */
  /* TRUE if lower die is to be drawn to the left  */

  if ((ms.fPostCrawford == FALSE) && (ms.fCrawford == TRUE))
    WriteInt16 (fp, 2);
  else if ((ms.fPostCrawford == TRUE) && (ms.fCrawford == FALSE))
    WriteInt16 (fp, 3);
  else
    WriteInt16 (fp, 1);

  WriteInt16 (fp, 0);		/* JF played last. Must be FALSE */

  c = 0;
  fwrite (&c, 1, 1, fp);	/* Length of "last move" string */

  WriteInt16 (fp, ms.anDice[0]);
  WriteInt16 (fp, ms.anDice[1]);

  /* The Jellyfish format saves the current board and the board
   * before the move was done, such that undo should be possible. It's
   * possible to save just the current board twice as done below. */

  memcpy (anBoard, ms.anBoard, 2 * 25 * sizeof (int));

  if (!ms.fMove)
    SwapSides (anBoard);

  /* Player 0 on bar */
  WriteInt16 (fp, anBoard[1][24] + 20);
  WriteInt16 (fp, anBoard[1][24] + 20);

  /* Board */
  for (i = 24; i > 0; i--)
    {
      WriteInt16 (fp, anBoard[0][24 - i] ?
		  -anBoard[0][24 - i] + 20 : anBoard[1][i - 1] + 20);
      WriteInt16 (fp, anBoard[0][24 - i] ?
		  -anBoard[0][24 - i] + 20 : anBoard[1][i - 1] + 20);
    }
  /* Player on bar */
  WriteInt16 (fp, -anBoard[0][24] + 20);
  WriteInt16 (fp, -anBoard[0][24] + 20);

  fclose (fp);

  setDefaultFileName (sz, PATH_POS);

  return;
}
