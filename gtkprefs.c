
/*
 * gtkprefs.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001, 2002.
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
#include "backgammon.h"

#include <ctype.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#endif

#include "drawboard.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkfile.h"
#include "gtkprefs.h"
#include "render.h"
#include "renderprefs.h"
#include "boarddim.h"
#include "gtkwindows.h"
#include "util.h"

#if USE_BOARD3D
#include "fun3d.h"
#include "misc3d.h"
#define NUM_NONPREVIEW_PAGES 2
#else
#define NUM_NONPREVIEW_PAGES 1
#endif

typedef enum _pixmapindex {
    PI_DESIGN, PI_CHEQUERS0, PI_CHEQUERS1, PI_BOARD, PI_BORDER, PI_DICE0,
    PI_DICE1, PI_CUBE, NUM_PIXMAPS
} pixmapindex;

static GtkAdjustment *apadj[ 2 ], *paAzimuth, *paElevation,
    *apadjCoefficient[ 2 ], *apadjExponent[ 2 ],
    *apadjBoard[ 4 ], *padjRound;
static GtkAdjustment *apadjDiceExponent[ 2 ], *apadjDiceCoefficient[ 2 ];
static GtkWidget *apwColour[ 2 ], *apwBoard[ 4 ],
    *pwWood, *pwWoodType, *pwHinges, *pwLightTable, *pwMoveIndicator,
    *pwWoodF, *pwNotebook, *pwLabels, *pwDynamicLabels;
#if HAVE_LIBXML2
static GList *plBoardDesigns = NULL;
#endif
#if USE_BOARD3D
GtkWidget *pwBoardType, *pwShowShadows, *pwAnimateRoll, *pwAnimateFlag, *pwCloseBoard,
	*pwDarkness, *lightLab, *darkLab, *pwLightSource, *pwDirectionalSource, *pwQuickDraw,
	*pwTestPerformance, *pmHingeCol, *frame3dOptions, *dtTextureTypeFrame,
	*pwPlanView, *pwBoardAngle, *pwSkewFactor, *skewLab, *anglelab, *pwBgTrays, *pwRoundPoints,
	*dtLightPositionFrame, *dtLightLevelsFrame, *pwRoundedEdges, *pwRoundedPiece, *pwFlatPiece,
	*pwTextureTopPiece, *pwTextureAllPiece;

GtkAdjustment *padjDarkness, *padjAccuracy, *padjBoardAngle, *padjSkewFactor, *padjLightPosX,
	*padjLightLevelAmbient, *padjLightLevelDiffuse, *padjLightLevelSpecular,
	*padjLightPosY, *padjLightPosZ, *padjDiceSize;
	int redrawChange;
	GtkTooltips *ptt;

	int pc3dDiceId[2], pcChequer2;
#endif

GtkWidget *pwPrevBoard;
renderdata rdPrefs;

#if HAVE_LIBXML2
static GtkWidget *pwDesignAdd, *pwDesignRemove, *pwDesignUpdate;
static GtkWidget *pwDesignImport, *pwDesignExport;
static GtkWidget *pwDesignList;
static GtkWidget *pwDesignAddAuthor;
static GtkWidget *pwDesignAddTitle;
#endif /* HAVE_LIBXML2 */

static GtkWidget *apwDiceColour[ 2 ];
static GtkWidget *pwCubeColour;
static GtkWidget *apwDiceDotColour[ 2 ];
static GtkWidget *apwDieColour[ 2 ];
static GtkWidget *apwDiceColourBox[ 2 ];
static int /*fWood,*/ fUpdate;

static void GetPrefs ( renderdata *prd );
void AddPages(BoardData* bd, GtkWidget* pwNotebook);

#if HAVE_LIBXML2
static void AddDesignRow ( gpointer data, gpointer user_data );
static void AddDesignRowIfNew ( gpointer data, gpointer user_data );
#endif

#if HAVE_LIBXML2

static GList *
ParseBoardDesigns ( const char *szFile, const int fDeletable );

typedef struct _boarddesign {

  gchar *szTitle;        /* Title of board design */
  gchar *szAuthor;       /* Name of author */
  gchar *szBoardDesign;  /* Command for setting board */

  int fDeletable;       /* is the board design deletable */

} boarddesign;

static boarddesign *pbdeSelected = NULL, *pbdeModified;

static GList *
read_board_designs ( void ) {

  GList *plUser, *plSystem, *plFinal;
  gchar *sz;

  sz = BuildFilename("boards.xml");
  plSystem = ParseBoardDesigns ( sz, FALSE );
  g_free ( sz );

  sz = g_build_filename(szHomeDirectory, "boards.xml", NULL);
  plUser = ParseBoardDesigns ( sz, TRUE );
  g_free ( sz );

  plFinal = g_list_concat ( plSystem, plUser );

  return plFinal;

}

static void
free_board_design (boarddesign *pbde, void *dummy) {

  if (pbde == NULL)
    return;

  g_free ( pbde->szTitle );
  g_free ( pbde->szAuthor );
  g_free ( pbde->szBoardDesign );

  g_free ( pbde );

}

static void
free_board_designs ( GList *pl ) {

  g_list_foreach ( pl, (GFunc)free_board_design, NULL );
  g_list_free ( pl );

}

static void DesignSelect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused );
static void DesignUnselect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused );

static void ParsePreferences(boarddesign *pbde, renderdata* prdNew)
{
	char *apch[2];
	gchar *sz, *pch;

	*prdNew = rdPrefs;
	pch = sz = g_strdup(pbde->szBoardDesign);
	while(ParseKeyValue(&sz, apch)) 
		RenderPreferencesParam(prdNew, apch[0], apch[1]);
		
	g_free(pch);
}

static boarddesign* FindDesign (renderdata* prdDesign)
{
	int i;
	renderdata rdTest;
	for (i = 0; i < (int)(g_list_length(plBoardDesigns) - 1); i++)
	{
		boarddesign *pbde = g_list_nth_data(plBoardDesigns, i + 1);
		if (pbde)
		{
			ParsePreferences(pbde, &rdTest);
			if (PreferenceCompare(&rdTest, prdDesign))
				return pbde;
		}
	}
	return NULL;
}
#endif /* HAVE_LIBXML2 */

static void SetTitle(void)
{	/* Update dialog title to include design name + author */
#if HAVE_LIBXML2
	boarddesign *pbde;
#endif
	char title[1024];
	GtkWidget *pwDialog = gtk_widget_get_toplevel(pwPrevBoard);

	strcpy(title, _("GNU Backgammon - Appearance"));

#if HAVE_LIBXML2
	/* Search for current settings in designs */
	strcat(title, ": ");

	pbde = FindDesign (&rdPrefs);
	if (pbde)
	{
		char design[1024];
		int row = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ), pbde);
		g_signal_handlers_block_by_func(G_OBJECT(pwDesignList), G_CALLBACK( DesignSelect ), 0);
		g_signal_handlers_block_by_func(G_OBJECT(pwDesignList), G_CALLBACK( DesignUnselect ), 0);
		gtk_clist_select_row( GTK_CLIST( pwDesignList ), row, 0);
		g_signal_handlers_unblock_by_func(G_OBJECT(pwDesignList), G_CALLBACK( DesignSelect ), 0);
		g_signal_handlers_unblock_by_func(G_OBJECT(pwDesignList), G_CALLBACK( DesignUnselect ), 0);

		gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), pbde->fDeletable );

		sprintf(design, "%s by %s (%s)", pbde->szTitle, pbde->szAuthor,
			pbde->fDeletable ? _("user defined") : _("predefined" ) );
		strcat(title, design);
		pbdeSelected = pbde;

		gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignAdd ), FALSE );
		gtk_widget_set_sensitive(pwDesignUpdate, FALSE );
	}
	else
	{
		strcat(title, _("Custom design"));
		gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignAdd ), TRUE );
		if (GTK_WIDGET_IS_SENSITIVE(pwDesignRemove))
		{
			gtk_widget_set_sensitive(pwDesignUpdate, TRUE );
			gtk_widget_set_sensitive(pwDesignRemove, FALSE );
			pbdeModified = pbdeSelected;
		}
		pbdeSelected = 0;
	}
#endif
	gtk_window_set_title( GTK_WINDOW( pwDialog ),  title);
}

extern void UpdatePreview(void)
{
	if (!fUpdate)
		return;
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
#if USE_BOARD3D
	BoardData3d *bd3d = bd->bd3d;
	renderdata *prd = bd->rd;

	if (display_is_3d(prd))
	{	/* Sort out chequer and dice special settings */
		RerenderBase(bd3d);
		if (prd->ChequerMat[0].textureInfo != prd->ChequerMat[1].textureInfo)
		{	/* Make both chequers have the same texture */
			prd->ChequerMat[1].textureInfo = prd->ChequerMat[0].textureInfo;
			prd->ChequerMat[1].pTexture = prd->ChequerMat[0].pTexture;
			UpdateColPreview(pcChequer2);
			/* Change to main area and recreate piece display lists */
			MakeCurrent3d(bd3d);
			preDraw3d(bd, bd3d, prd);
		}
		if (prd->afDieColour3d[0] &&
			!MaterialCompare(&prd->DiceMat[0], &prd->ChequerMat[0]))
		{
			memcpy(&prd->DiceMat[0], &prd->ChequerMat[0], sizeof(Material));
			prd->DiceMat[0].textureInfo = 0;
			prd->DiceMat[0].pTexture = 0;
			UpdateColPreview(pc3dDiceId[0]);
		}
		if (prd->afDieColour3d[1] &&
			!MaterialCompare(&prd->DiceMat[1], &prd->ChequerMat[1]))
		{
			memcpy(&prd->DiceMat[1], &prd->ChequerMat[1], sizeof(Material));
			prd->DiceMat[1].textureInfo = 0;
			prd->DiceMat[1].pTexture = 0;
			UpdateColPreview(pc3dDiceId[1]);
		}
		gtk_widget_set_sensitive(GTK_WIDGET(dtTextureTypeFrame), (prd->ChequerMat[0].textureInfo != NULL));
	}
	else
#endif
	{	/* Create new 2d pixmaps */
		board_free_pixmaps(bd);
		GetPrefs(&rdPrefs);
		board_create_pixmaps(pwPrevBoard, bd);
	}
}
	SetTitle();
	gtk_widget_queue_draw(pwPrevBoard);
}

static void DieColourChanged (GtkWidget *pw, gpointer pf)
{
	int f = GPOINTER_TO_INT(pf);
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));
	gtk_widget_set_sensitive(apwDiceColourBox[f], !set);

	if (!fUpdate)
		return;

#if USE_BOARD3D
	if (display_is_3d(&rdPrefs))
	{
		BoardData *bd = BOARD(pwPrevBoard)->board_data;
		bd->rd->afDieColour3d[f] = set;
		memcpy(&bd->rd->DiceMat[f], bd->rd->afDieColour3d[f] ? &bd->rd->ChequerMat[f] : &bd->rd->DiceMat[f], sizeof(Material));
		bd->rd->DiceMat[f].textureInfo = 0;
		bd->rd->DiceMat[f].pTexture = 0;

		UpdateColPreview(pc3dDiceId[f]);
	}
#endif

	UpdatePreview();
}

static void option_changed(GtkWidget *widget, GtkWidget *pw)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	if (!fUpdate)
		return;

{
#if USE_BOARD3D
	BoardData3d *bd3d = bd->bd3d;
	renderdata *prd = bd->rd;

	if (display_is_3d(prd))
	{
		ClearTextures(bd3d);
		TidyCurveAccuracy3d(bd->bd3d, rdPrefs.curveAccuracy);

		GetPrefs(&rdPrefs);
		GetTextures(bd3d, prd);

		SetupViewingVolume3d(bd, bd3d, prd);
		preDraw3d(bd, bd3d, prd);
	}
	else
#endif
	{
		board_free_pixmaps(bd);
		board_create_pixmaps(pwPrevBoard, bd);
	}
}
	UpdatePreview();
}

#if USE_BOARD3D

static void redraw_changed(GtkWidget *widget, GtkWidget **ppw)
{	/* Update 3d colour previews */
	if (!fUpdate)
		return;

	GetPrefs(&rdPrefs);
	SetPreviewLightLevel(rdPrefs.lightLevels);
	UpdateColPreviews();
}

static void DiceSizeChanged(GtkWidget *pw)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	bd->rd->diceSize = (float)padjDiceSize->value;
	if (DiceTooClose(bd->bd3d, bd->rd))
		setDicePos(bd, bd->bd3d);
	option_changed(0, 0);
}

static void HingeChanged (GtkWidget *pw)
{
	gtk_widget_set_sensitive(pmHingeCol, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pwHinges)));
	option_changed(0, 0);
}

static GtkWidget *ChequerPrefs3d( BoardData *bd)
{
    GtkWidget *pw, *pwx, *vbox, *pwhbox, *dtPieceTypeFrame;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Chequer 0:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->ChequerMat[0], DF_VARIABLE_OPACITY, TT_PIECE), FALSE, FALSE, TT_PIECE );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Chequer 1:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->ChequerMat[1], DF_VARIABLE_OPACITY, TT_PIECE | TT_DISABLED), FALSE, FALSE, TT_PIECE );
	pcChequer2 = GetPreviewId();

	dtPieceTypeFrame = gtk_frame_new(_("Piece type"));
	gtk_container_set_border_width (GTK_CONTAINER (dtPieceTypeFrame), 4);
	gtk_box_pack_start(GTK_BOX(pw), dtPieceTypeFrame, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (dtPieceTypeFrame), vbox);

	pwRoundedPiece = gtk_radio_button_new_with_label (NULL, _("Rounded disc"));
	gtk_widget_set_tooltip_text(pwRoundedPiece, _("Piece will be a rounded disc"));
	gtk_box_pack_start (GTK_BOX (vbox), pwRoundedPiece, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwRoundedPiece), (bd->rd->pieceType == PT_ROUNDED));

	pwFlatPiece = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(pwRoundedPiece), _("Flat edged disc"));
	gtk_widget_set_tooltip_text( pwFlatPiece, _("Piece will be a flat sided disc"));
	gtk_box_pack_start (GTK_BOX (vbox), pwFlatPiece, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwFlatPiece), (bd->rd->pieceType == PT_FLAT));
	g_signal_connect(G_OBJECT(pwFlatPiece), "toggled", G_CALLBACK(option_changed), bd);


	dtTextureTypeFrame = gtk_frame_new(_("Texture type"));
	gtk_container_set_border_width (GTK_CONTAINER (dtTextureTypeFrame), 4);
	gtk_box_pack_start(GTK_BOX(pw), dtTextureTypeFrame, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (dtTextureTypeFrame), vbox);
	gtk_widget_set_sensitive(GTK_WIDGET(dtTextureTypeFrame), (bd->rd->ChequerMat[0].textureInfo != NULL));

	pwTextureAllPiece = gtk_radio_button_new_with_label (NULL, _("All of piece"));
	gtk_widget_set_tooltip_text( pwTextureAllPiece, _("All of piece will be textured"));
	gtk_box_pack_start (GTK_BOX (vbox), pwTextureAllPiece, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwTextureAllPiece), (bd->rd->pieceTextureType == PTT_ALL));

	pwTextureTopPiece = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(pwTextureAllPiece), _("Top Only"));
	gtk_widget_set_tooltip_text( pwTextureTopPiece, _("Only top of piece will be textured"));
	gtk_box_pack_start (GTK_BOX (vbox), pwTextureTopPiece, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwTextureTopPiece), (bd->rd->pieceTextureType == PTT_TOP));
	g_signal_connect(G_OBJECT(pwTextureTopPiece), "toggled", G_CALLBACK(option_changed), bd);

	return pwx;
}

static GtkWidget *DicePrefs3d( BoardData *bd, int f)
{
    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    apwDieColour[ f ] = 
      gtk_check_button_new_with_label ( _("Die colour same "
                                          "as chequer colour" ) );
    gtk_box_pack_start( GTK_BOX( pw ),
			apwDieColour[ f ], FALSE, FALSE, 0 );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( apwDieColour[ f ] ),
                                   bd->rd->afDieColour3d[ f ] );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Die colour:") ),
			FALSE, FALSE, 4 );

	apwDiceColourBox[f] = gtk_colour_picker_new3d(&bd->rd->DiceMat[f], DF_VARIABLE_OPACITY, TT_NONE);
	pc3dDiceId[f] = GetPreviewId();
    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColourBox[ f ] ),
                               ! bd->rd->afDieColour3d[ f ] );
    gtk_box_pack_start(GTK_BOX(pwhbox),apwDiceColourBox[f], FALSE, FALSE, 4);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Pip colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start(GTK_BOX(pwhbox), gtk_colour_picker_new3d(&bd->rd->DiceDotMat[f], DF_FULL_ALPHA, TT_NONE), FALSE, FALSE, TT_PIECE);

    g_signal_connect( G_OBJECT( apwDieColour[ f ] ), "toggled",
                         G_CALLBACK( DieColourChanged ), GINT_TO_POINTER(f));

	return pwx;
}

static GtkWidget *CubePrefs3d( BoardData *bd )
{
    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Cube colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->CubeMat, DF_NO_ALPHA, TT_NONE), FALSE, FALSE, TT_PIECE);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Text colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->CubeNumberMat, DF_FULL_ALPHA, TT_NONE), FALSE, FALSE, TT_PIECE);

	return pwx;
}

static GtkWidget *BoardPage3d( BoardData *bd )
{
    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Background\ncolour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->BaseMat, DF_NO_ALPHA, TT_GENERAL), FALSE, FALSE, TT_PIECE);

	pwBgTrays = gtk_check_button_new_with_label (_("Show background in bear-off trays"));
	gtk_widget_set_tooltip_text( pwBgTrays, _("If unset the bear-off trays will be drawn with the board colour"));
	gtk_box_pack_start (GTK_BOX (pw), pwBgTrays, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwBgTrays), bd->rd->bgInTrays);
	g_signal_connect(G_OBJECT(pwBgTrays), "toggled", G_CALLBACK(option_changed),0);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("First\npoint colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->PointMat[0], DF_FULL_ALPHA, TT_GENERAL), FALSE, FALSE, TT_PIECE);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Second\npoint colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->PointMat[1], DF_FULL_ALPHA, TT_GENERAL), FALSE, FALSE, TT_PIECE);

	pwRoundPoints = gtk_check_button_new_with_label (_("Rounded points"));
	gtk_widget_set_tooltip_text( pwBgTrays, _("Display the points with a rounded end"));
	gtk_box_pack_start (GTK_BOX (pw), pwRoundPoints, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pwRoundPoints), bd->rd->roundedPoints);
	g_signal_connect(G_OBJECT(pwRoundPoints), "toggled", G_CALLBACK(option_changed),0);

	return pwx;
}

static GtkWidget *BorderPage3d( BoardData *bd )
{
    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Border colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->BoxMat, DF_FULL_ALPHA, TT_GENERAL), FALSE, FALSE, TT_PIECE);

	pwRoundedEdges = gtk_check_button_new_with_label (_("Rounded board edges"));
	gtk_widget_set_tooltip_text( pwRoundedEdges, _("Toggle rounded or square edges to the board"));
	gtk_box_pack_start (GTK_BOX (pw), pwRoundedEdges, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwRoundedEdges), bd->rd->roundedEdges);
	g_signal_connect(G_OBJECT(pwRoundedEdges), "toggled", G_CALLBACK(option_changed), 0);

    pwHinges = gtk_check_button_new_with_label( _("Show hinges") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHinges ), bd->rd->fHinges3d );
    gtk_box_pack_start( GTK_BOX( pw ), pwHinges, FALSE, FALSE, 0 );
    g_signal_connect( G_OBJECT( pwHinges ), "toggled",
			       G_CALLBACK( HingeChanged ), NULL);
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Hinge colour:") ),
			FALSE, FALSE, 4 );

	pmHingeCol = gtk_colour_picker_new3d(&bd->rd->HingeMat, DF_NO_ALPHA, TT_HINGE);
    gtk_widget_set_sensitive(pmHingeCol, bd->rd->fHinges3d);
    gtk_box_pack_start(GTK_BOX(pwhbox), pmHingeCol, FALSE, FALSE, 4);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Point number\ncolour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->PointNumberMat, DF_FULL_ALPHA, TT_NONE), FALSE, FALSE, TT_PIECE);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Background\ncolour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->BackGroundMat, DF_NO_ALPHA, TT_GENERAL), FALSE, FALSE, TT_PIECE);

	return pwx;
}

#endif

extern void gtk_color_button_get_array(GtkColorButton *button, double array[4])
{
	GdkColor color;
	guint16 alpha;

	gtk_color_button_get_color(button, &color);
	alpha = gtk_color_button_get_alpha(button);

	array[0] = (gdouble)color.red / 65535.0;
	array[1] = (gdouble)color.green / 65535.0;
	array[2] = (gdouble)color.blue / 65535.0;
	array[3] = (gdouble)alpha / 65535;

}

extern void gtk_color_button_set_from_array(GtkColorButton *button, double array[4])
{
	GdkColor color;
	guint16 alpha;

	color.red = array[0]*65535;
	color.green = array[1]*65535;
	color.blue = array[2]*65535;
	alpha = array[3]*65535;

	gtk_color_button_set_color(button, &color);
	gtk_color_button_set_alpha(button, alpha);
}

static GtkWidget *ChequerPrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox, *pwx, *pwScale, *pwBox;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    apadj[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new( bd->rd->arRefraction[ f ],
						     1.0, 3.5, 0.1, 1.0,
						     0.0 ) );
    apadjCoefficient[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd->arCoefficient[ f ], 0.0, 1.0, 0.1, 0.1, 0.0 ) );
    apadjExponent[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd->arExponent[ f ], 1.0, 100.0, 1.0, 10.0, 0.0 ) );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Colour:") ),
			FALSE, FALSE, 4 );

    apwColour[ f ] = gtk_color_button_new();
    g_object_set(G_OBJECT(apwColour[f]), "use-alpha", TRUE, NULL);
    g_signal_connect(G_OBJECT(apwColour[f]), "color-set", UpdatePreview, NULL);
    gtk_box_pack_start( GTK_BOX( pwhbox ),apwColour[ f ] , TRUE, TRUE, 4 );

    gtk_color_button_set_from_array( GTK_COLOR_BUTTON( apwColour[ f ] ), bd->rd->aarColour[ f ] );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );
	g_signal_connect( G_OBJECT( apadj[ f ] ),
			       "value-changed",
			       G_CALLBACK( UpdatePreview ), NULL);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
	g_signal_connect( G_OBJECT( apadjCoefficient[ f ] ),
			       "value-changed",
			       G_CALLBACK( UpdatePreview ), NULL);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Diffuse") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Specular") ), FALSE,
			FALSE, 4 );
	g_signal_connect( G_OBJECT( apadjExponent[ f ] ),
			       "value-changed",
			       G_CALLBACK( UpdatePreview ), NULL);
   
	if (f == 0)
	{
		padjRound = GTK_ADJUSTMENT( gtk_adjustment_new( 1.0 - bd->rd->rRound, 0, 1,
								0.1, 0.1, 0 ) );
		g_signal_connect( G_OBJECT( padjRound ), "value-changed",
					   G_CALLBACK( UpdatePreview ), NULL);
		pwScale = gtk_hscale_new( padjRound );
		gtk_widget_set_size_request( pwScale, 100, -1 );
		gtk_scale_set_draw_value( GTK_SCALE( pwScale ), FALSE );
		gtk_scale_set_digits( GTK_SCALE( pwScale ), 2 );

		pwBox = gtk_hbox_new( FALSE, 0 );
		gtk_box_pack_start ( GTK_BOX ( pwBox ), gtk_label_new( _("Chequer shape:") ), FALSE, FALSE, 0 );
		gtk_box_pack_start ( GTK_BOX ( pw ), pwBox, FALSE, FALSE, 4 );

		pwBox = gtk_hbox_new( FALSE, 0 );

		gtk_box_pack_start( GTK_BOX( pwBox ), gtk_label_new( _("Flat") ),
				FALSE, FALSE, 4 );
		gtk_box_pack_start( GTK_BOX( pwBox ), pwScale, FALSE, FALSE, 0 );
		gtk_box_pack_start( GTK_BOX( pwBox ), gtk_label_new( _("Round") ),
				FALSE, FALSE, 4 );
    
		gtk_box_pack_start ( GTK_BOX ( pw ), pwBox, FALSE, FALSE, 4 );
	}
    
    return pwx;
}

static GtkWidget *DicePrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;
    GtkWidget *pwvbox;
    GtkWidget *pwFrame;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    /* frame with colour selections for the dice */

    pwFrame = gtk_frame_new ( _("Die colour") );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwFrame, 
                         FALSE, FALSE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );

    apwDieColour[ f ] = 
      gtk_check_button_new_with_label ( _("Die colour same "
                                          "as chequer colour" ) );
    gtk_box_pack_start( GTK_BOX( pwvbox ),
			apwDieColour[ f ], FALSE, FALSE, 0 );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( apwDieColour[ f ] ),
                                   bd->rd->afDieColour[ f ] );

    apwDiceColourBox[ f ] = gtk_vbox_new ( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwvbox ),
			apwDiceColourBox[ f ], FALSE, FALSE, 0 );

    apadjDiceCoefficient[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd->arDiceCoefficient[ f ], 0.0, 1.0, 0.1, 0.1, 0.0 ) );
    apadjDiceExponent[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd->arDiceExponent[ f ], 1.0, 100.0, 1.0, 10.0, 0.0 ) );
    
    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ),
			pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Die colour:") ),
			FALSE, FALSE, 4 );
    apwDiceColour[ f ] = gtk_color_button_new();
    g_signal_connect(G_OBJECT(apwDiceColour[ f ]), "color-set", UpdatePreview, NULL);
    gtk_color_button_set_from_array( GTK_COLOR_BUTTON( apwDiceColour[ f ] ), bd->rd->aarDiceColour[ f ] );
    gtk_box_pack_start( GTK_BOX( pwhbox ), apwDiceColour[ f ], TRUE, TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjDiceCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
	g_signal_connect( G_OBJECT( apadjDiceCoefficient[ f ] ),
			       "value-changed",
			       G_CALLBACK( UpdatePreview ), NULL);
    
    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), 
                        gtk_label_new( _("Diffuse") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjDiceExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), 
                        gtk_label_new( _("Specular") ), FALSE,
			FALSE, 4 );
	g_signal_connect( G_OBJECT( apadjDiceExponent[ f ] ),
			       "value-changed",
			       G_CALLBACK( UpdatePreview ), NULL);

    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColourBox[ f ] ),
                               ! bd->rd->afDieColour[ f ] );

    /* colour of dot on dice */
    
    gtk_box_pack_start ( GTK_BOX( pw ),
			pwhbox = gtk_hbox_new( FALSE, 0 ), FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Pip colour:") ),
			FALSE, FALSE, 4 );

    apwDiceDotColour[ f ] = gtk_color_button_new();
    g_signal_connect(G_OBJECT(apwDiceDotColour[ f ]), "color-set", UpdatePreview, NULL);
    gtk_color_button_set_from_array( GTK_COLOR_BUTTON( apwDiceDotColour[ f ] ), bd->rd->aarDiceDotColour[ f ] );
    gtk_box_pack_start( GTK_BOX( pwhbox ), apwDiceDotColour[ f ], TRUE, TRUE, 4 );
    
    g_signal_connect( G_OBJECT( apwDieColour[ f ] ), "toggled",
                         G_CALLBACK( DieColourChanged ), GINT_TO_POINTER(f));

    return pwx;
}

static GtkWidget *CubePrefs( BoardData *bd ) {


    GtkWidget *pw;
    GtkWidget *pwx;

    pw = gtk_vbox_new( FALSE, 4 );
    
    pwx = gtk_hbox_new ( FALSE, 4 );
    
    gtk_box_pack_start ( GTK_BOX ( pw ), pwx, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX( pwx ), gtk_label_new( _("Cube colour:") ),
			FALSE, FALSE, 0 );
    
    pwCubeColour = gtk_color_button_new();
    g_signal_connect(G_OBJECT(pwCubeColour), "color-set", UpdatePreview, NULL);
    gtk_color_button_set_from_array(GTK_COLOR_BUTTON(pwCubeColour), bd->rd->arCubeColour);
    gtk_box_pack_start ( GTK_BOX ( pwx ), pwCubeColour, TRUE, TRUE, 0 );

    /* FIXME add cube text colour settings */

    return pw;

}

static GtkWidget *BoardPage( BoardData *bd ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i, j;
    GtkWidget *pwx;
    static const char *asz[ 4 ] = {
	N_("Background colour:"),
	NULL,
	N_("First point colour:"),
	N_("Second point colour:"),
    };
    
    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    for( j = 0; j < 4; j++ ) {
	if( j == 1 )
	    continue; /* colour 1 unused */
	apadjBoard[ j ] = GTK_ADJUSTMENT( gtk_adjustment_new(
            bd->rd->aSpeckle[ j ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

	for( i = 0; i < 3; i++ )
	    ar[ i ] = bd->rd->aanBoardColour[ j ][ i ] / 255.0;
    
	gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			    FALSE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( pwhbox ),
			    gtk_label_new( gettext( asz[ j ] ) ),
			    FALSE, FALSE, 4 );
	apwBoard[j] = gtk_color_button_new();
	g_signal_connect(G_OBJECT(apwBoard[j]), "color-set", UpdatePreview, NULL);
	gtk_color_button_set_from_array( GTK_COLOR_BUTTON( apwBoard[ j ] ), ar );
	gtk_box_pack_start( GTK_BOX( pwhbox ),
			    apwBoard[ j ],
				TRUE, TRUE, 4 );

	gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Smooth") ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
				apadjBoard[ j ] ), TRUE, TRUE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Speckled") ),
			    FALSE, FALSE, 4 );
	g_signal_connect( G_OBJECT( apadjBoard[ j ] ),
				   "value-changed",
				   G_CALLBACK( UpdatePreview ), NULL);
    }
    
    return pwx;
}

static void ToggleWood( GtkWidget *pw, BoardData *bd ) {

    int fWood;
    
    fWood = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
    
    gtk_widget_set_sensitive( pwWoodType, fWood );
    gtk_widget_set_sensitive( apwBoard[ 1 ], !fWood );
}

static GtkWidget *BorderPage( BoardData *bd ) {

    GtkWidget *pw;
    gdouble ar[ 4 ];
    int i;
    static const char *aszWood[] = {
	N_ ("Alder"), 
        N_ ("Ash"), 
        N_ ("Basswood"), 
        N_ ("Beech"), 
        N_ ("Cedar"), 
        N_ ("Ebony"), 
        N_ ("Fir"), 
        N_ ("Maple"),
	N_ ("Oak"), 
        N_ ("Pine"), 
        N_ ("Redwood"), 
        N_ ("Walnut"), 
        N_ ("Willow")
    };
    woodtype bw;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ),
			pwWood = gtk_radio_button_new_with_label( NULL,
								  _("Wooden") ),
			FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwWoodType = gtk_combo_box_new_text(),
			FALSE, FALSE, 4 );

	for (bw = 0; bw < WOOD_PAINT; bw++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(pwWoodType), gettext(aszWood[bw]));

	if (bd->rd->wt == WOOD_PAINT)
		gtk_combo_box_set_active(GTK_COMBO_BOX(pwWoodType), 0 );
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(pwWoodType), bd->rd->wt );

    g_signal_connect( G_OBJECT( pwWoodType ), "changed",
			       G_CALLBACK( UpdatePreview ), NULL);
    
    gtk_box_pack_start( GTK_BOX( pw ),
			pwWoodF = gtk_radio_button_new_with_label_from_widget(
			    GTK_RADIO_BUTTON( pwWood ), _("Painted") ),
			FALSE, FALSE, 0 );

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->rd->wt != WOOD_PAINT ?
						     pwWood : pwWoodF ),
				  TRUE );
    
    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->rd->aanBoardColour[ 1 ][ i ] / 255.0;

    apwBoard[1] = gtk_color_button_new();
    g_signal_connect(G_OBJECT(apwBoard[1]), "color-set", UpdatePreview, NULL);
    gtk_box_pack_start( GTK_BOX( pw ), apwBoard[ 1 ],
			FALSE, FALSE, 0 );
    gtk_color_button_set_from_array( GTK_COLOR_BUTTON( apwBoard[ 1 ] ), ar );

    pwHinges = gtk_check_button_new_with_label( _("Show hinges") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHinges ), bd->rd->fHinges );
    gtk_box_pack_start( GTK_BOX( pw ), pwHinges, FALSE, FALSE, 0 );
    g_signal_connect( G_OBJECT( pwHinges ), "toggled",
			       G_CALLBACK( UpdatePreview ), NULL);

    g_signal_connect( G_OBJECT( pwWood ), "toggled",
			G_CALLBACK( ToggleWood ), bd );
    g_signal_connect( G_OBJECT( pwWood ), "toggled",
			       G_CALLBACK( UpdatePreview ), NULL);
    
    gtk_widget_set_sensitive( pwWoodType, bd->rd->wt != WOOD_PAINT );
    gtk_widget_set_sensitive( apwBoard[ 1 ], bd->rd->wt == WOOD_PAINT);
    
    return pwx;
}

static void BoardPrefsOK( GtkWidget *pw, GtkWidget *mainBoard )
{
	BoardData *bd = BOARD(mainBoard)->board_data;
    GetPrefs(&rdPrefs);
	
#if USE_BOARD3D
    if (gtk_gl_init_success)
    {
	    redrawChange = FALSE;
	    rdPrefs.quickDraw = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwQuickDraw));

	    TidyCurveAccuracy3d(bd->bd3d, bd->rd->curveAccuracy);
    }
	if (display_is_3d(&rdPrefs))
	{
		/* Delete old objects */
		ClearTextures(bd->bd3d);
	}
	else
#endif
	{
		if (bd->diceShown == DICE_ON_BOARD && bd->x_dice[0] <= 0)
		{	/* Make sure dice are visible */
			RollDice2d(bd);
		}
	}
	board_free_pixmaps( bd );

	rdPrefs.fShowIDs = bd->rd->fShowIDs;
	rdPrefs.fDiceArea = bd->rd->fDiceArea;
	rdPrefs.fShowGameInfo = bd->rd->fShowGameInfo;
	rdPrefs.nSize = bd->rd->nSize;

	/* Copy new settings to main board */
	*bd->rd = rdPrefs;

#if USE_BOARD3D
{
	BoardData3d *bd3d = bd->bd3d;
	renderdata *prd = bd->rd;

	DisplayCorrectBoardType(bd, bd3d, prd);
	SetSwitchModeMenuText();

	if (display_is_3d(prd))
	{
		MakeCurrent3d(bd3d);
		GetTextures(bd3d, prd);
		preDraw3d(bd, bd3d, prd);
		SetupViewingVolume3d(bd, bd3d, prd);
		ShowFlag3d(bd, bd3d, prd);
		if (bd->diceShown == DICE_ON_BOARD)
			setDicePos(bd, bd3d);	/* Make sure dice appear ok */
		RestrictiveRedraw();
	}
}
#endif
	board_create_pixmaps( mainBoard, bd );
	gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
	/* Make sure chequers correct below board */
	gtk_widget_queue_draw(bd->table);
}

static void WorkOut2dLight(renderdata* prd)
{
    prd->arLight[ 2 ] = (float)sinf( paElevation->value / 180 * G_PI );
    prd->arLight[ 0 ] = (float)(cosf( paAzimuth->value / 180 * G_PI ) *
    	sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] ));
    prd->arLight[ 1 ] = (float)(sinf( paAzimuth->value / 180 * G_PI ) *
	    sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] ));
}

static void LightChanged2d( GtkWidget *pwWidget, void* data )
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	if (!fUpdate)
		return;
	WorkOut2dLight(bd->rd);
	board_free_pixmaps( bd );
	board_create_pixmaps( pwPrevBoard, bd );

	UpdatePreview();
}

static void LabelsToggled( GtkWidget *pwWidget, void* data )
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	int showLabels = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwLabels ) );
	gtk_widget_set_sensitive( GTK_WIDGET( pwDynamicLabels ), showLabels );
	/* Update preview */
	if (!fUpdate)
		return;
	bd->rd->fLabels = showLabels;
	bd->rd->fDynamicLabels = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwDynamicLabels ) );
#if USE_BOARD3D
	if (display_is_2d(bd->rd))
#endif
	{
		board_free_pixmaps( bd );
		board_create_pixmaps( pwPrevBoard, bd );
	}
	option_changed(0, 0);
}

static void MoveIndicatorToggled( GtkWidget *pwWidget, void* data )
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	/* Update preview */
	if (!fUpdate)
		return;
	bd->rd->showMoveIndicator = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwMoveIndicator) );
#if USE_BOARD3D
	if (display_is_2d(bd->rd))
#endif
	{
		board_free_pixmaps( bd );
		board_create_pixmaps( pwPrevBoard, bd );
	}
	UpdatePreview();
}

#if USE_BOARD3D

static void toggle_display_type(GtkWidget *widget, BoardData* bd)
{
	int i;
	int state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	int numPages = 
	g_list_length(GTK_NOTEBOOK(pwNotebook)->children);
	/* Show pages with correct 2d/3d settings */
	for (i = numPages - 1; i >= NUM_NONPREVIEW_PAGES; i--)
		gtk_notebook_remove_page(GTK_NOTEBOOK(pwNotebook), i);

	gtk_notebook_remove_page(GTK_NOTEBOOK(pwNotebook), 1);

	rdPrefs.fDisplayType = state ? DT_3D : DT_2D;

	if (display_is_3d(&rdPrefs))
	{
		DoAcceleratedCheck(bd->bd3d, widget);
		updateDiceOccPos(bd, bd->bd3d);
	}
	else
	{
		board_free_pixmaps( bd );
		board_create_pixmaps( pwPrevBoard, bd );
	}

	AddPages(bd, pwNotebook);
	gtk_widget_set_sensitive(pwTestPerformance, (display_is_3d(&rdPrefs)));

#if USE_BOARD3D
	DisplayCorrectBoardType(bd, bd->bd3d, bd->rd);
#endif
	/* Make sure everything is correctly sized */
	gtk_widget_queue_resize(gtk_widget_get_toplevel(pwNotebook));

	SetTitle();
}

static void toggle_quick_draw(GtkWidget *widget, int init)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(pwShowShadows, !set);
	gtk_widget_set_sensitive(pwCloseBoard, !set);

	if (set)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwShowShadows), 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwCloseBoard), 0);
		if (init != -1)
			GTKShowWarning(WARN_QUICKDRAW_MODE, widget);
	}
}

static void toggle_show_shadows(GtkWidget *widget, int init)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(lightLab, set);
	gtk_widget_set_sensitive(pwDarkness, set);
	gtk_widget_set_sensitive(darkLab, set);
	if (set && init != -1)
		GTKShowWarning(WARN_SET_SHADOWS, widget);
	
	option_changed(0, 0);
}

static void toggle_planview(GtkWidget *widget, GtkWidget *pw)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(pwSkewFactor, !set);
	gtk_widget_set_sensitive(pwBoardAngle, !set);
	gtk_widget_set_sensitive(skewLab, !set);
	gtk_widget_set_sensitive(anglelab, !set);
	
	option_changed(0, 0);
}

static void DoTestPerformance(GtkWidget *pw, GtkWidget* board)
{
	BoardData *bd = BOARD(board)->board_data;
	char str[255];
	char *msg;
	float fps;

	GTKSetCurrentParent(pw);
	if (!GetInputYN(_("Save settings and test 3d performance for 3 seconds?")))
		return;

	BoardPrefsOK(pw, board);

	ProcessGtkEvents();

	fps = TestPerformance3d(bd);

	if (fps >= 30)
		msg = _("3d Performance is very fast.\n");
	else if (fps >= 15)
		msg = _("3d Performance is good.\n");
	else if (fps >= 10)
		msg = _("3d Performance is ok.\n");
	else if (fps >= 5)
		msg = _("3d Performance is poor.\n");
	else
		msg = _("3d Performance is very poor.\n");

	sprintf(str, _("%s\n(%.1f frames per second)\n"), msg, fps);

	outputl(str);

	if (fps <= 5)
	{	/* Give some advice, hopefully to speed things up */
		if (bd->rd->showShadows)
			outputl(_("Disable shadows to improve performance"));
		else if (!bd->rd->quickDraw)
			outputl(_("Try the quick draw option to improve performance"));
		else
			outputl(_("The quick draw option will not change the result of this performance test"));
	}
	outputx();
}

#endif

static void Add2dLightOptions(GtkWidget* pwx, renderdata* prd)
{
    float rAzimuth, rElevation;
	GtkWidget* pScale;

    pwLightTable = gtk_table_new( 2, 2, FALSE );
    gtk_box_pack_start( GTK_BOX( pwx ), pwLightTable, FALSE, FALSE, 4 );
    
    gtk_table_attach( GTK_TABLE( pwLightTable ), gtk_label_new( _("Light azimuth") ),
		      0, 1, 0, 1, 0, 0, 4, 2 );
    gtk_table_attach( GTK_TABLE( pwLightTable ), gtk_label_new( _("Light elevation") ),
		      0, 1, 1, 2, 0, 0, 4, 2 );

    rElevation = (float)(asinf( prd->arLight[ 2 ] ) * 180 / G_PI);
	{
		float s = (float)sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] );
		if (s == 0)
			rAzimuth = 0;
		else
		{
			float ac = (float)acosf( prd->arLight[ 0 ] / s );
			if (ac == 0)
				rAzimuth = 0;
			else
				rAzimuth = (float)(ac * 180 / G_PI);
		}
	}
    if( prd->arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;
    
    paAzimuth = GTK_ADJUSTMENT( gtk_adjustment_new( rAzimuth, 0.0, 360.0, 1.0,
						    30.0, 0.0 ) );
	pScale = gtk_hscale_new( paAzimuth );
	gtk_widget_set_size_request(pScale, 150, -1);
    gtk_table_attach( GTK_TABLE( pwLightTable ), pScale,
		      1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
	g_signal_connect( G_OBJECT( paAzimuth ), "value-changed",
			       G_CALLBACK( LightChanged2d ), NULL);

    paElevation = GTK_ADJUSTMENT( gtk_adjustment_new( rElevation, 0.0, 90.0,
						      1.0, 10.0, 0.0 ) );
    gtk_table_attach( GTK_TABLE( pwLightTable ), gtk_hscale_new( paElevation ),
		      1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
	g_signal_connect( G_OBJECT( paElevation ), "value-changed",
			       G_CALLBACK( LightChanged2d ), NULL);

	/* FIXME add settings for ambient light */
}

#if USE_BOARD3D
static GtkWidget *LightingPage(BoardData *bd)
{
    GtkWidget *dtBox, *pwx;
	GtkWidget *vbox, *vbox2, *frameBox, *hBox, *lab,
		*pwLightPosX, *pwLightLevelAmbient, *pwLightLevelDiffuse, *pwLightLevelSpecular,
		*pwLightPosY, *pwLightPosZ, *dtLightSourceFrame;

	pwx = gtk_hbox_new ( FALSE, 0);
	
	dtBox = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_start(GTK_BOX(pwx), dtBox, FALSE, FALSE, 0);

	if (display_is_3d(&rdPrefs))
	{
		dtBox = gtk_vbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(pwx), dtBox, FALSE, FALSE, 0);

		dtLightSourceFrame = gtk_frame_new(_("Light Source Type"));
		gtk_container_set_border_width (GTK_CONTAINER (dtLightSourceFrame), 4);
		gtk_box_pack_start(GTK_BOX(dtBox), dtLightSourceFrame, FALSE, FALSE, 0);
		
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (dtLightSourceFrame), vbox);
		
		pwLightSource = gtk_radio_button_new_with_label (NULL, _("Positional"));
		gtk_widget_set_tooltip_text( pwLightSource, _("This is a fixed light source, like a lamp"));
		gtk_box_pack_start (GTK_BOX (vbox), pwLightSource, FALSE, FALSE, 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwLightSource), (bd->rd->lightType == LT_POSITIONAL));
		
		pwDirectionalSource = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(pwLightSource), _("Directional"));
		gtk_widget_set_tooltip_text( pwDirectionalSource, _("This is a light direction, like the sun"));
		gtk_box_pack_start (GTK_BOX (vbox), pwDirectionalSource, FALSE, FALSE, 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwDirectionalSource), (bd->rd->lightType == LT_DIRECTIONAL));
		g_signal_connect(G_OBJECT(pwDirectionalSource), "toggled", G_CALLBACK(option_changed), bd);

		dtLightPositionFrame = gtk_frame_new(_("Light Position"));
		gtk_container_set_border_width (GTK_CONTAINER (dtLightPositionFrame), 4);
		gtk_box_pack_start(GTK_BOX(dtBox), dtLightPositionFrame, FALSE, FALSE, 0);

		frameBox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (dtLightPositionFrame), frameBox);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new(_("Left"));
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightPosX = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightPos[0],
								-1.5, 4, .1, 1, 0));
		g_signal_connect( G_OBJECT( padjLightPosX ), "value-changed",
					G_CALLBACK( option_changed ), NULL);
		pwLightPosX = gtk_hscale_new(padjLightPosX);
		gtk_scale_set_draw_value(GTK_SCALE(pwLightPosX), FALSE);
		gtk_widget_set_size_request(pwLightPosX, 150, -1);
		gtk_box_pack_start(GTK_BOX(hBox), pwLightPosX, TRUE, TRUE, 0);

		lab = gtk_label_new(_("Right"));
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		hBox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		vbox2 = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hBox), vbox2, FALSE, FALSE, 0);

		lab = gtk_label_new(_("Bottom"));
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		padjLightPosY = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightPos[1],
								-1.5, 4, .1, 1, 0));
		g_signal_connect( G_OBJECT( padjLightPosY ), "value-changed",
					G_CALLBACK( option_changed ), NULL);
		pwLightPosY = gtk_vscale_new(padjLightPosY);
		gtk_scale_set_draw_value(GTK_SCALE(pwLightPosY), FALSE);

		gtk_widget_set_size_request(pwLightPosY, -1, 70);
		gtk_box_pack_start(GTK_BOX(vbox2), pwLightPosY, TRUE, TRUE, 0);

		lab = gtk_label_new(_("Top"));
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		vbox2 = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hBox), vbox2, FALSE, FALSE, 0);

		lab = gtk_label_new(_("Low"));
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		padjLightPosZ = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightPos[2],
								.5, 5, .1, 1, 0));
		g_signal_connect( G_OBJECT( padjLightPosZ ), "value-changed",
					G_CALLBACK( option_changed ), NULL);
		pwLightPosZ = gtk_vscale_new(padjLightPosZ);
		gtk_scale_set_draw_value(GTK_SCALE(pwLightPosZ), FALSE);

		gtk_box_pack_start(GTK_BOX(vbox2), pwLightPosZ, TRUE, TRUE, 0);

		lab = gtk_label_new(_("High"));
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		dtLightLevelsFrame = gtk_frame_new(_("Light Levels"));
		gtk_container_set_border_width(GTK_CONTAINER(dtLightLevelsFrame), 4);
		gtk_box_pack_start(GTK_BOX(dtBox), dtLightLevelsFrame, FALSE, FALSE, 0);

		frameBox = gtk_vbox_new(FALSE, 0);
		gtk_container_add (GTK_CONTAINER(dtLightLevelsFrame), frameBox);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new(_("Ambient"));
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightLevelAmbient = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightLevels[0],
								0, 100, 1, 10, 0));
		g_signal_connect( G_OBJECT( padjLightLevelAmbient ), "value-changed",
					G_CALLBACK( option_changed ), NULL);
		pwLightLevelAmbient = gtk_hscale_new(padjLightLevelAmbient);
		gtk_widget_set_tooltip_text( pwLightLevelAmbient, _("Ambient light specifies the general light level"));
		gtk_box_pack_start(GTK_BOX(hBox), pwLightLevelAmbient, TRUE, TRUE, 0);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new(_("Diffuse"));
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightLevelDiffuse = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightLevels[1],
								0, 100, 1, 10, 0));
		g_signal_connect( G_OBJECT( padjLightLevelDiffuse ), "value-changed",
					G_CALLBACK( option_changed ), NULL);
		pwLightLevelDiffuse = gtk_hscale_new(padjLightLevelDiffuse);
		gtk_widget_set_tooltip_text( pwLightLevelDiffuse, _("Diffuse light specifies light from the light source"));
		gtk_box_pack_start(GTK_BOX(hBox), pwLightLevelDiffuse, TRUE, TRUE, 0);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new(_("Specular"));
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightLevelSpecular = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightLevels[2],
								0, 100, 1, 10, 0));
		g_signal_connect( G_OBJECT( padjLightLevelSpecular ), "value-changed",
					G_CALLBACK( option_changed ), NULL);
		pwLightLevelSpecular = gtk_hscale_new(padjLightLevelSpecular);
		gtk_widget_set_tooltip_text( pwLightLevelSpecular, _("Specular light is reflected light off shiny surfaces"));
		gtk_box_pack_start(GTK_BOX(hBox), pwLightLevelSpecular, TRUE, TRUE, 0);
	}
	else
		Add2dLightOptions(dtBox, bd->rd);

	gtk_widget_show_all(pwx);

	return pwx;
}

#endif

static GtkWidget *GeneralPage( BoardData *bd, GtkWidget* bdMain ) {

    GtkWidget *pw, *pwx;
#if USE_BOARD3D
	GtkWidget *dtBox, *button, *dtFrame, *hBox, *lab, *pwev, *pwhbox, *pwvbox,
			*pwAccuracy, *pwDiceSize;
	pwQuickDraw = 0;
#endif

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

#if USE_BOARD3D
	dtFrame = gtk_frame_new(_("Display Type"));
	gtk_container_set_border_width (GTK_CONTAINER (dtFrame), 4);
	gtk_box_pack_start(GTK_BOX( pw ), dtFrame, FALSE, FALSE, 0);
	
	dtBox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (dtFrame), dtBox);
	
	pwBoardType = gtk_radio_button_new_with_label (NULL, _("2d Board"));
	gtk_box_pack_start (GTK_BOX (dtBox), pwBoardType, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwBoardType), (display_is_2d(bd->rd)));
	
	button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(pwBoardType), _("3d Board"));
	if (!gtk_gl_init_success)
		gtk_widget_set_sensitive(button, FALSE);
	gtk_box_pack_start (GTK_BOX (dtBox), button, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), (display_is_3d(bd->rd)));
	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(toggle_display_type), bd);
#endif

    pwLabels = gtk_check_button_new_with_label( _("Numbered point labels") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwLabels ), bd->rd->fLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwLabels, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text( pwLabels, _("Show or hide point numbers"));

    pwDynamicLabels = gtk_check_button_new_with_label( _("Dynamic labels") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwDynamicLabels ), 
                                  bd->rd->fDynamicLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwDynamicLabels, FALSE, FALSE, 0 );
    g_signal_connect( G_OBJECT( pwDynamicLabels ), "toggled",
					G_CALLBACK( LabelsToggled ), 0 );

    g_signal_connect( G_OBJECT( pwLabels ), "toggled",
                        G_CALLBACK( LabelsToggled ), 0);
    LabelsToggled(0, 0);
    
    gtk_widget_set_tooltip_text( pwDynamicLabels,
			  _("Update the labels so they are correct "
                            "for the player on roll"));

	pwMoveIndicator = gtk_check_button_new_with_label (_("Show move indicator"));
	gtk_widget_set_tooltip_text( pwMoveIndicator, _("Show or hide arrow indicating who is moving"));
	gtk_box_pack_start (GTK_BOX (pw), pwMoveIndicator, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwMoveIndicator), bd->rd->showMoveIndicator);
    g_signal_connect(G_OBJECT(pwMoveIndicator), "toggled", G_CALLBACK(MoveIndicatorToggled), 0);
#if USE_BOARD3D
	pwvbox = gtk_vbox_new( FALSE, 0 );
	gtk_box_pack_start(GTK_BOX( pwx ), pwvbox, FALSE, FALSE, 0);

	frame3dOptions = gtk_frame_new(_("3d Options"));
	gtk_container_set_border_width (GTK_CONTAINER (frame3dOptions), 4);
	gtk_box_pack_start(GTK_BOX(pwvbox), frame3dOptions, FALSE, FALSE, 0);

    pw = gtk_vbox_new( FALSE, 0 );
	gtk_container_add (GTK_CONTAINER (frame3dOptions), pw);
	
	pwShowShadows = gtk_check_button_new_with_label (_("Show shadows"));
	gtk_widget_set_tooltip_text( pwShowShadows, _("Display shadows, this option requires a fast graphics card"));
	gtk_box_pack_start (GTK_BOX (pw), pwShowShadows, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwShowShadows), bd->rd->showShadows);
	g_signal_connect(G_OBJECT(pwShowShadows), "toggled", G_CALLBACK(toggle_show_shadows), NULL);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	lightLab = gtk_label_new(_("light"));
	gtk_box_pack_start(GTK_BOX(hBox), lightLab, FALSE, FALSE, 0);

	padjDarkness = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->shadowDarkness,
						     3, 100, 1, 10, 0));
	g_signal_connect( G_OBJECT( padjDarkness ), "value-changed",
			       G_CALLBACK( option_changed ), NULL);
	pwDarkness = gtk_hscale_new(padjDarkness);
	gtk_widget_set_tooltip_text( pwDarkness, _("Vary the darkness of the shadows"));
	gtk_scale_set_draw_value(GTK_SCALE(pwDarkness), FALSE);
	gtk_box_pack_start(GTK_BOX(hBox), pwDarkness, TRUE, TRUE, 0);

	darkLab = gtk_label_new(_("dark"));
	gtk_box_pack_start(GTK_BOX(hBox), darkLab, FALSE, FALSE, 0);

	toggle_show_shadows(pwShowShadows, -1);

	pwAnimateRoll = gtk_check_button_new_with_label (_("Animate dice rolls"));
	gtk_widget_set_tooltip_text( pwAnimateRoll, _("Dice rolls will shake across board"));
	gtk_box_pack_start (GTK_BOX (pw), pwAnimateRoll, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwAnimateRoll), bd->rd->animateRoll);
	
	pwAnimateFlag = gtk_check_button_new_with_label (_("Animate resignation flag"));
	gtk_widget_set_tooltip_text( pwAnimateFlag, _("Waves resignation flag"));
	gtk_box_pack_start (GTK_BOX (pw), pwAnimateFlag, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwAnimateFlag), bd->rd->animateFlag);

	pwCloseBoard = gtk_check_button_new_with_label (_("Close board on exit"));
	gtk_widget_set_tooltip_text( pwCloseBoard, _("When you quit gnubg, the board will close"));
	gtk_box_pack_start (GTK_BOX (pw), pwCloseBoard, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwCloseBoard), bd->rd->closeBoardOnExit );

	pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
	gtk_box_pack_start(GTK_BOX(pw), pwev, FALSE, FALSE, 0);
	pwhbox = gtk_hbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(pwev), pwhbox);


	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	anglelab = gtk_label_new(_("Board angle: "));
	gtk_box_pack_start(GTK_BOX(hBox), anglelab, FALSE, FALSE, 0);

	padjBoardAngle = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->boardAngle,
										0, 60, 1, 10, 0));
	g_signal_connect( G_OBJECT( padjBoardAngle ), "value-changed",
			       G_CALLBACK( option_changed ), NULL);
	pwBoardAngle = gtk_hscale_new(padjBoardAngle);
	gtk_widget_set_tooltip_text( pwBoardAngle, _("Vary the angle the board is tilted at"));
	gtk_scale_set_digits( GTK_SCALE( pwBoardAngle ), 0 );
    gtk_widget_set_size_request( pwBoardAngle, 100, -1 );
	gtk_box_pack_start(GTK_BOX(hBox), pwBoardAngle, FALSE, FALSE, 0);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	skewLab = gtk_label_new(_("FOV skew: "));
	gtk_box_pack_start(GTK_BOX(hBox), skewLab, FALSE, FALSE, 0);

	padjSkewFactor = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->skewFactor,
										0, 100, 1, 10, 0));
	g_signal_connect( G_OBJECT( padjSkewFactor ), "value-changed",
			       G_CALLBACK( option_changed ), NULL);
	pwSkewFactor = gtk_hscale_new(padjSkewFactor);
    gtk_widget_set_size_request( pwSkewFactor, 100, -1 );
	gtk_widget_set_tooltip_text( pwSkewFactor, _("Vary the field-of-view of the 3d display"));
	gtk_scale_set_digits( GTK_SCALE( pwSkewFactor ), 0 );
	gtk_box_pack_start(GTK_BOX(hBox), pwSkewFactor, FALSE, FALSE, 0);

	pwPlanView = gtk_check_button_new_with_label (_("Plan view"));
	gtk_widget_set_tooltip_text( pwPlanView, _("Display the 3d board with a 2d overhead view"));
	gtk_box_pack_start (GTK_BOX (pw), pwPlanView, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwPlanView), bd->rd->planView);
	g_signal_connect(G_OBJECT(pwPlanView), "toggled", G_CALLBACK(toggle_planview), NULL);
	toggle_planview(pwPlanView, 0);

	lab = gtk_label_new(_("Curve accuracy"));
	gtk_box_pack_start (GTK_BOX (pw), lab, FALSE, FALSE, 0);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	lab = gtk_label_new(_("low"));
	gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

	padjAccuracy = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->curveAccuracy,
						     8, 60, 4, 12, 0));
	g_signal_connect( G_OBJECT( padjAccuracy ), "value-changed",
			       G_CALLBACK( option_changed ), NULL);
	pwAccuracy = gtk_hscale_new(padjAccuracy);
	gtk_widget_set_tooltip_text( pwAccuracy, _("Change how accurately curves are drawn."
		" If performance is slow try lowering this value."
		" Increasing this value will only have an effect on large displays"));
	gtk_scale_set_draw_value(GTK_SCALE(pwAccuracy), FALSE);
	gtk_box_pack_start(GTK_BOX(hBox), pwAccuracy, TRUE, TRUE, 0);

	lab = gtk_label_new(_("high"));
	gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);


	lab = gtk_label_new(_("Dice size"));
	gtk_box_pack_start (GTK_BOX (pw), lab, FALSE, FALSE, 0);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	lab = gtk_label_new(_("small"));
	gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

	padjDiceSize = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->diceSize,
						     1.5, 4, .1, 1, 0));
	g_signal_connect( G_OBJECT( padjDiceSize ), "value-changed",
			       G_CALLBACK( DiceSizeChanged ), NULL);
	pwDiceSize = gtk_hscale_new(padjDiceSize);
	gtk_widget_set_tooltip_text( pwDiceSize, _("Vary the size of the dice"));
	gtk_scale_set_draw_value(GTK_SCALE(pwDiceSize), FALSE);
	gtk_box_pack_start(GTK_BOX(hBox), pwDiceSize, TRUE, TRUE, 0);

	lab = gtk_label_new(_("large"));
	gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

	pwQuickDraw = gtk_check_button_new_with_label (_("Quick drawing"));
	gtk_widget_set_tooltip_text( pwQuickDraw, _("Fast drawing option to improve performance"));
	gtk_box_pack_start (GTK_BOX (pw), pwQuickDraw, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwQuickDraw), bd->rd->quickDraw);
	g_signal_connect(G_OBJECT(pwQuickDraw), "toggled", G_CALLBACK(toggle_quick_draw), NULL);
	toggle_quick_draw(pwQuickDraw, -1);

	pwTestPerformance = gtk_button_new_with_label(_("Test performance"));
	gtk_widget_set_sensitive(pwTestPerformance, (display_is_3d(bd->rd)));
	gtk_box_pack_start(GTK_BOX(pwvbox), pwTestPerformance, FALSE, FALSE, 4);
	g_signal_connect(G_OBJECT(pwTestPerformance), "clicked",
				   G_CALLBACK(DoTestPerformance), bdMain);

#else
	Add2dLightOptions(pw, bd->rd);
#endif
    return pwx;
}

#if HAVE_LIBXML2

/* functions for board design */

static void
UseDesign ( void ) {

  int i, j;
  gdouble ar[ 4 ];
  gfloat rAzimuth, rElevation;
  renderdata newPrefs;
#if USE_BOARD3D
  BoardData *bd = BOARD(pwPrevBoard)->board_data;
#endif

  fUpdate = FALSE;

#if USE_BOARD3D
  if (display_is_3d(&rdPrefs))
    ClearTextures(bd->bd3d);
#endif

  ParsePreferences(pbdeSelected, &newPrefs);

#if USE_BOARD3D

	/* Set only 2D or 3D options */
	if (display_is_3d(&rdPrefs))
	{
		Set3dSettings(&rdPrefs, &newPrefs);
		GetTextures(bd->bd3d, bd->rd);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwLightSource), (newPrefs.lightType == LT_POSITIONAL));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwDirectionalSource), (newPrefs.lightType == LT_DIRECTIONAL));

		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjLightPosX), newPrefs.lightPos[0]);

		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjLightPosY), newPrefs.lightPos[1]);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjLightPosZ), newPrefs.lightPos[2]);

		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjLightLevelAmbient), newPrefs.lightLevels[0]);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjLightLevelDiffuse), newPrefs.lightLevels[1]);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjLightLevelSpecular), newPrefs.lightLevels[2]);

		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjBoardAngle), newPrefs.boardAngle);

		gtk_adjustment_set_value(GTK_ADJUSTMENT(padjSkewFactor), newPrefs.skewFactor);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwTextureAllPiece), (newPrefs.pieceTextureType == PTT_ALL));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwTextureTopPiece), (newPrefs.pieceTextureType == PTT_TOP));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwRoundedPiece), (newPrefs.pieceType == PT_ROUNDED));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwFlatPiece), (newPrefs.pieceType == PT_FLAT));

		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwRoundedEdges ),
									newPrefs.roundedEdges );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwBgTrays ),
									newPrefs.bgInTrays );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwRoundPoints ),
									newPrefs.roundedPoints );

		for(i = 0; i < 2; i++)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(apwDieColour[i]), newPrefs.afDieColour3d[i]);

		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( pwHinges ), 
									newPrefs.fHinges3d );

		redrawChange = TRUE;
	}
	else
	#endif
	{
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwMoveIndicator ),
									newPrefs.showMoveIndicator );

		/* chequers */

		for ( i = 0; i < 2; ++i ) {
			gtk_color_button_set_from_array ( GTK_COLOR_BUTTON ( apwColour[ i ] ),
											newPrefs.aarColour[ i ] );

			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadj[ i ] ),
									newPrefs.arRefraction[ i ] );

			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjCoefficient[ i ] ),
									newPrefs.arCoefficient[ i ] );

			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjExponent[ i ] ),
									newPrefs.arExponent[ i ] );
		}

		/* dice + dice dot */

		for ( i = 0; i < 2; ++i ) {

			gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColourBox[ i ] ),
									! newPrefs.afDieColour[ i ] );

			gtk_color_button_set_from_array( GTK_COLOR_BUTTON( apwDiceColour[ i ] ),
										newPrefs.afDieColour[ i ] ? 
										newPrefs.aarColour[ i ] :
										newPrefs.aarDiceColour[ i ] );

			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjDiceExponent[ i ] ),
									newPrefs.afDieColour[ i ] ? 
									newPrefs.arExponent[ i ] :
									newPrefs.arDiceExponent[ i ] );

			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjDiceCoefficient[ i ] ),
									newPrefs.afDieColour[ i ] ?
									newPrefs.arCoefficient[ i ] :
									newPrefs.arDiceCoefficient[ i ] );


			/* die dot */

			gtk_color_button_set_from_array ( GTK_COLOR_BUTTON ( apwDiceDotColour[ i ] ), 
											newPrefs.aarDiceDotColour[ i ] );

		}

		/* cube colour */

		gtk_color_button_set_from_array ( GTK_COLOR_BUTTON ( pwCubeColour ), 
										newPrefs.arCubeColour );

		/* board + points */

		for ( i = 0; i < 4; ++i ) {

			if ( i != 1 ) 
			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjBoard[ i ] ),
										newPrefs.aSpeckle[ i ] / 128.0 );

			for ( j = 0; j < 3; j++ )
			ar[ j ] = newPrefs.aanBoardColour[ i ][ j ] / 255.0;

			gtk_color_button_set_from_array ( GTK_COLOR_BUTTON ( apwBoard[ i ]),
											ar );
		}

		/* board, border, and points */

		gtk_combo_box_set_active(GTK_COMBO_BOX(pwWoodType), newPrefs.wt );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( newPrefs.wt != WOOD_PAINT ?
														pwWood : pwWoodF ),
										TRUE );

		gtk_widget_set_sensitive( pwWoodType, newPrefs.wt != WOOD_PAINT );
		gtk_widget_set_sensitive( apwBoard[ 1 ], newPrefs.wt == WOOD_PAINT);

		/* round */

		gtk_adjustment_set_value ( GTK_ADJUSTMENT ( padjRound ),
									1.0f - newPrefs.rRound );

		/* light */

		rElevation = (float)(asinf( newPrefs.arLight[ 2 ] ) * 180 / G_PI);
			if ( fabs ( newPrefs.arLight[ 2 ] - 1.0f ) < 1e-5 ) 
			rAzimuth = 0.0;
			else
			rAzimuth = (float)(
				acosf( newPrefs.arLight[ 0 ] / sqrt( 1.0 - newPrefs.arLight[ 2 ] *
												newPrefs.arLight[ 2 ] ) ) * 180 / G_PI);
		if( newPrefs.arLight[ 1 ] < 0 )
			rAzimuth = 360 - rAzimuth;

		gtk_adjustment_set_value ( GTK_ADJUSTMENT ( paAzimuth ),
									rAzimuth );
		gtk_adjustment_set_value ( GTK_ADJUSTMENT ( paElevation ),
									rElevation );

		for(i = 0; i < 2; i++)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(apwDieColour[i]), newPrefs.afDieColour[i]);

		gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( pwHinges ), 
									newPrefs.fHinges );

		GetPrefs(&rdPrefs);
	}

	fUpdate = TRUE;
	option_changed(0, 0);
}

static void
WriteTheDesign( boarddesign *pbde, FILE *pf ) {

  fputs ( "   <board-design>\n\n", pf );

  /* <about> */

  fprintf ( pf,
            "      <about>\n"
            "         <title>%s</title>\n"
            "         <author>%s</author>\n"
            "      </about>\n\n"
            "      <design>%s   </design>\n\n",
            pbde->szTitle ? pbde->szTitle : _("unknown title"),
            pbde->szAuthor ? pbde->szAuthor : _("unknown author"),
            pbde->szBoardDesign ? pbde->szBoardDesign : "" );

  fputs ( "   </board-design>\n\n\n", pf );

  }

static void
WriteDesignOnlyDeletables ( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;
  FILE *pf = user_data;

  if ( ! pbde )
    return;

  if ( ! pbde->fDeletable )
    /* predefined board design */
    return;

  WriteTheDesign( pbde, pf );

}

static void
WriteDesign( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;
  FILE *pf = user_data;

  if ( ! pbde )
    return;

  WriteTheDesign( pbde, pf );

}

static void
WriteDesignHeader( const char *szFile, FILE *pf ) {

  time_t t;

  fprintf ( pf, 
            "<?xml version=\"1.0\" encoding=\"" GNUBG_CHARSET "\"?>\n"
            "\n"
            "<!--\n"
            "\n"
            "    %s\n"
            "       generated by " VERSION_STRING "\n", szFile );
  fputs ( "       ", pf );
  time ( &t );
  fputs ( ctime ( &t ), pf );
  fputs ( "\n"
          "    $Id$\n"
          "\n"
          " -->\n"
          "\n"
          "\n"
          "<board-designs>\n"
          "\n", pf );

}

static void 
WriteDesignFooter( FILE *pf ) {

  fputs ( "</board-designs>\n", pf );

}



static void
DesignSave ( GtkWidget *pw, gpointer data ) {

  gchar *szFile;
  FILE *pf;
  GList **pplBoardDesigns = (GList **) data;

  szFile = g_build_filename ( szHomeDirectory, "boards.xml", NULL);

  if ( ! ( pf = g_fopen ( szFile, "w+" ) ) ) {
    outputerr ( szFile );
    g_free ( szFile );
    return;
  }

  WriteDesignHeader( szFile, pf );
  g_list_foreach ( *pplBoardDesigns, WriteDesignOnlyDeletables, pf );
  WriteDesignFooter( pf );

  fclose ( pf );
  
  g_free ( szFile );

}

static void DesignAddOK( GtkWidget *pw, boarddesign *pbde ) {

  pbde->szAuthor = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddAuthor ), 
                                           0, -1 );
  pbde->szTitle = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddTitle ), 
                                           0, -1 );
  
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}

static void
DesignAddChanged ( GtkWidget *pw, GtkWidget *pwDialog ) {

  char *szAuthor = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddAuthor ), 
                                           0, -1 );
  char *szTitle = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddTitle ), 
                                          0, -1 );

  GtkWidget *pwOK = DialogArea ( GTK_WIDGET ( pwDialog ), DA_OK );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwOK ), 
                             szAuthor && szTitle &&
                             *szAuthor && *szTitle );

}

static void
DesignAddTitle ( boarddesign *pbde ) {

  GtkWidget *pwDialog;
  GtkWidget *pwvbox;
  GtkWidget *pwhbox;

  pwDialog = GTKCreateDialog( _("GNU Backgammon - Add current board design"), 
			DT_QUESTION, NULL, DIALOG_FLAG_MODAL, G_CALLBACK( DesignAddOK ), pbde );

  pwvbox = gtk_vbox_new ( FALSE, 4 );
  gtk_container_add ( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), 
                      pwvbox );

  /* title */

  pwhbox = gtk_hbox_new ( FALSE, 4 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );


  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Title of new design:") ),
                       FALSE, FALSE, 4 );

  pwDesignAddTitle = gtk_entry_new ();
  gtk_entry_set_activates_default(GTK_ENTRY(pwDesignAddTitle), TRUE);
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAddTitle, 
                       FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignAddTitle ), "changed",
                       G_CALLBACK ( DesignAddChanged ), pwDialog );

  /* author */

  pwhbox = gtk_hbox_new ( FALSE, 4 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );


  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Author of new design:") ),
                       FALSE, FALSE, 4 );

  pwDesignAddAuthor = gtk_entry_new ();
  gtk_entry_set_activates_default(GTK_ENTRY(pwDesignAddAuthor), TRUE);
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAddAuthor, 
                       FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignAddAuthor ), "changed",
                       G_CALLBACK ( DesignAddChanged ), pwDialog );

  /* show dialog */

  gtk_widget_grab_focus( pwDesignAddTitle );

  DesignAddChanged ( NULL, pwDialog );
  
  GTKRunDialog(pwDialog);
}

static void WriteDesignString(boarddesign *pbde, renderdata *prd)
{
  char szTemp[2048], *pTemp;
  gchar buf1[G_ASCII_DTOSTR_BUF_SIZE], buf2[G_ASCII_DTOSTR_BUF_SIZE],
	  buf3[G_ASCII_DTOSTR_BUF_SIZE], buf4[G_ASCII_DTOSTR_BUF_SIZE];

  float rElevation = (float)(asinf( prd->arLight[ 2 ] ) * 180 / G_PI);
  float rAzimuth = ( fabs ( prd->arLight[ 2 ] - 1.0f ) < 1e-5 ) ? 0.0f : 
    (float)(acosf( prd->arLight[ 0 ] / sqrt( 1.0 - prd->arLight[ 2 ] *
                                    prd->arLight[ 2 ] ) ) * 180 / G_PI);

  if( prd->arLight[ 1 ] < 0 )
    rAzimuth = 360 - rAzimuth;

  pTemp = szTemp;
  pTemp += sprintf(pTemp,
            "\n"
            "         board=#%02X%02X%02X;%sf\n"
            "         border=#%02X%02X%02X\n",
            /* board */
            prd->aanBoardColour[ 0 ][ 0 ], prd->aanBoardColour[ 0 ][ 1 ], 
            prd->aanBoardColour[ 0 ][ 2 ],
	    g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->aSpeckle[ 0 ] / 128.0f ),
            /* border */
            prd->aanBoardColour[ 1 ][ 0 ], prd->aanBoardColour[ 1 ][ 1 ], 
            prd->aanBoardColour[ 1 ][ 2 ]);

  pTemp += sprintf(pTemp,
            "         wood=%s\n"
            "         hinges=%c\n"
            "         light=%s;%s\n"
            "         shape=%s\n",
            /* wood ... */
            aszWoodName[ prd->wt ],
            prd->fHinges ? 'y' : 'n',
            g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.0f", rAzimuth),
	    g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%0.0f", rElevation),
	    g_ascii_formatd(buf3, G_ASCII_DTOSTR_BUF_SIZE, "%0.1f", 1.0 - prd->rRound));

  pTemp += sprintf(pTemp,
            "         chequers0=#%02X%02X%02X;%s;%s;%s;%s\n",
			/* chequers0 */
			(int) ( prd->aarColour[ 0 ][ 0 ] * 0xFF ),
			(int) ( prd->aarColour[ 0 ][ 1 ] * 0xFF ), 
			(int) ( prd->aarColour[ 0 ][ 2 ] * 0xFF ), 
			g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->aarColour[ 0 ][ 3 ]),
			g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arRefraction[ 0 ]), 
			g_ascii_formatd(buf3, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arCoefficient[ 0 ]),
			g_ascii_formatd(buf4, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arExponent[ 0 ]));
	  
  pTemp += sprintf(pTemp,
            "         chequers1=#%02X%02X%02X;%s;%s;%s;%s\n",
			/* chequers1 */
			(int) ( prd->aarColour[ 1 ][ 0 ] * 0xFF ),
			(int) ( prd->aarColour[ 1 ][ 1 ] * 0xFF ), 
			(int) ( prd->aarColour[ 1 ][ 2 ] * 0xFF ), 
			g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->aarColour[ 1 ][ 3 ] ),
			g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arRefraction[ 1 ] ),  
			g_ascii_formatd(buf3, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arCoefficient[ 1 ] ),
			g_ascii_formatd(buf4, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arExponent[ 1 ] ));

  pTemp += sprintf(pTemp,
            "         dice0=#%02X%02X%02X;%s;%s;%c\n",
             /* dice0 */
             (int) ( prd->aarDiceColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceColour[ 0 ][ 2 ] * 0xFF ), 
             g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arDiceCoefficient[ 0 ] ),
	     g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arDiceExponent[ 0 ] ),
             prd->afDieColour[ 0 ] ? 'y' : 'n');

  pTemp += sprintf(pTemp,
            "         dice1=#%02X%02X%02X;%s;%s;%c\n",
             /* dice1 */
	     (int) ( prd->aarDiceColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceColour[ 1 ][ 2 ] * 0xFF ), 
             g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arDiceCoefficient[ 1 ] ),
	     g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->arDiceExponent[ 1 ] ),
             prd->afDieColour[ 1 ] ? 'y' : 'n');

  pTemp += sprintf(pTemp,
            "         dot0=#%02X%02X%02X\n"
            "         dot1=#%02X%02X%02X\n"
            "         cube=#%02X%02X%02X\n",
             /* dot0 */
	     (int) ( prd->aarDiceDotColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceDotColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceDotColour[ 0 ][ 2 ] * 0xFF ), 
             /* dot1 */
	     (int) ( prd->aarDiceDotColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceDotColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceDotColour[ 1 ][ 2 ] * 0xFF ), 
             /* cube */
	     (int) ( prd->arCubeColour[ 0 ] * 0xFF ),
	     (int) ( prd->arCubeColour[ 1 ] * 0xFF ), 
	     (int) ( prd->arCubeColour[ 2 ] * 0xFF ));


  pTemp += sprintf(pTemp,
            "         points0=#%02X%02X%02X;%s\n"
            "         points1=#%02X%02X%02X;%s\n",
             /* points0 */
	     prd->aanBoardColour[ 2 ][ 0 ], prd->aanBoardColour[ 2 ][ 1 ], 
	     prd->aanBoardColour[ 2 ][ 2 ],
	     g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->aSpeckle[ 2 ] / 128.0f),
             /* points1 */
	     prd->aanBoardColour[ 3 ][ 0 ], prd->aanBoardColour[ 3 ][ 1 ], 
	     prd->aanBoardColour[ 3 ][ 2 ],
	     g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%0.2f", prd->aSpeckle[ 3 ] / 128.0f ));

#if USE_BOARD3D
  pTemp += sprintf(pTemp,
            "         hinges3d=%c\n"
            "         piecetype=%d\n"
            "         piecetexturetype=%d\n"
			"         roundededges=%c\n"
			"         bgintrays=%c\n"
			"         roundedpoints=%c\n"
            "         lighttype=%c\n"
            "         lightposx=%s lightposy=%s lightposz=%s\n"
            "         lightambient=%d lightdiffuse=%d lightspecular=%d\n"
			"         chequers3d0=%s\n"
			"         chequers3d1=%s\n"
			"         dice3d0=%s\n"
			"         dice3d1=%s\n"
			"         dot3d0=%s\n"
			"         dot3d1=%s\n"
			"         cube3d=%s\n"
			"         cubetext3d=%s\n"
			"         base3d=%s\n"
			"         points3d0=%s\n"
			"         points3d1=%s\n"
			"         border3d=%s\n"
			"         hinge3d=%s\n"
			"         numbers3d=%s\n"
			"         background3d=%s\n"
            "\n",
            prd->fHinges3d ? 'y' : 'n',
			prd->pieceType,
			prd->pieceTextureType,
			prd->roundedEdges ? 'y' : 'n',
			prd->bgInTrays ? 'y' : 'n',
			prd->roundedPoints ? 'y' : 'n',
			prd->lightType == LT_POSITIONAL ? 'p' : 'd',
			g_ascii_formatd(buf1, G_ASCII_DTOSTR_BUF_SIZE, "%f", prd->lightPos[0]),
		    g_ascii_formatd(buf2, G_ASCII_DTOSTR_BUF_SIZE, "%f", prd->lightPos[1]),
			g_ascii_formatd(buf3, G_ASCII_DTOSTR_BUF_SIZE, "%f", prd->lightPos[2]),
			prd->lightLevels[0], prd->lightLevels[1], prd->lightLevels[2],
			WriteMaterial(&prd->ChequerMat[0]),
			WriteMaterial(&prd->ChequerMat[1]),
			WriteMaterialDice(prd, 0),
			WriteMaterialDice(prd, 1),
			WriteMaterial(&prd->DiceDotMat[0]),
			WriteMaterial(&prd->DiceDotMat[1]),
			WriteMaterial(&prd->CubeMat),
			WriteMaterial(&prd->CubeNumberMat),
			WriteMaterial(&prd->BaseMat),
			WriteMaterial(&prd->PointMat[0]),
			WriteMaterial(&prd->PointMat[1]),
			WriteMaterial(&prd->BoxMat),
			WriteMaterial(&prd->HingeMat),
			WriteMaterial(&prd->PointNumberMat),
			WriteMaterial(&prd->BackGroundMat));
#endif

  pbde->szBoardDesign = g_malloc(strlen(szTemp) + 1);
  strcpy(pbde->szBoardDesign, szTemp);
}

static void ShowSelectedRow(void)
{
	int row = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ), pbdeSelected);
	if( gtk_clist_row_is_visible(GTK_CLIST( pwDesignList ), row) == GTK_VISIBILITY_NONE )
		gtk_clist_moveto(GTK_CLIST( pwDesignList ), row, 0, 1, 0 );
}

#if USE_BOARD3D

static void Set2dColour(double newcol[4], Material* pMat)
{
	newcol[0] = (pMat->ambientColour[0] + pMat->diffuseColour[0]) / 2;
	newcol[1] = (pMat->ambientColour[1] + pMat->diffuseColour[1]) / 2;
	newcol[2] = (pMat->ambientColour[2] + pMat->diffuseColour[2]) / 2;
	newcol[3] = (pMat->ambientColour[3] + pMat->diffuseColour[3]) / 2;
}

static void Set2dColourChar(unsigned char newcol[4], Material* pMat)
{
	newcol[0] = (unsigned char)(((pMat->ambientColour[0] + pMat->diffuseColour[0]) / 2) * 255);
	newcol[1] = (unsigned char)(((pMat->ambientColour[1] + pMat->diffuseColour[1]) / 2) * 255);
	newcol[2] = (unsigned char)(((pMat->ambientColour[2] + pMat->diffuseColour[2]) / 2) * 255);
	newcol[3] = (unsigned char)(((pMat->ambientColour[3] + pMat->diffuseColour[3]) / 2) * 255);
}

static void Set3dColour(Material* pMat, double col[4])
{
	pMat->ambientColour[0] = pMat->diffuseColour[0] = (float)col[0];
	pMat->ambientColour[1] = pMat->diffuseColour[1] = (float)col[1];
	pMat->ambientColour[2] = pMat->diffuseColour[2] = (float)col[2];
	pMat->ambientColour[3] = pMat->diffuseColour[3] = 1;
}

static void Set3dColourChar(Material* pMat, unsigned char col[4])
{
	pMat->ambientColour[0] = pMat->diffuseColour[0] = ((float)col[0]) / 255;
	pMat->ambientColour[1] = pMat->diffuseColour[1] = ((float)col[1]) / 255;
	pMat->ambientColour[2] = pMat->diffuseColour[2] = ((float)col[2]) / 255;
	pMat->ambientColour[3] = pMat->diffuseColour[3] = 1;
}

static void CopyNewSettingsToOtherDimension(renderdata* prd)
{
	if (display_is_3d(prd))
	{	/* Create rough 2d settings based on new 3d settings */
		prd->wt = WOOD_PAINT;
		Set2dColour(prd->aarColour[0], &prd->ChequerMat[0]);
		Set2dColour(prd->aarColour[1], &prd->ChequerMat[1]);
		Set2dColour(prd->aarDiceColour[0], &prd->DiceMat[0]);
		Set2dColour(prd->aarDiceColour[1], &prd->DiceMat[1]);
		Set2dColour(prd->aarDiceDotColour[0], &prd->DiceDotMat[0]);
		Set2dColour(prd->aarDiceDotColour[1], &prd->DiceDotMat[1]);
		prd->afDieColour[0] = prd->afDieColour3d[0];
		prd->afDieColour[1] = prd->afDieColour3d[1];
		prd->fHinges = prd->fHinges3d;
		Set2dColour(prd->arCubeColour, &prd->CubeMat);
		Set2dColourChar(prd->aanBoardColour[0], &prd->BaseMat);
		Set2dColourChar(prd->aanBoardColour[1], &prd->BoxMat);
		Set2dColourChar(prd->aanBoardColour[2], &prd->PointMat[0]);
		Set2dColourChar(prd->aanBoardColour[3], &prd->PointMat[1]);
	}
	else
	{	/* Create rough 3d settings based on new 2d settings */
		Set3dColour(&prd->ChequerMat[0], prd->aarColour[0]);
		Set3dColour(&prd->ChequerMat[1], prd->aarColour[1]);
		Set3dColour(&prd->DiceMat[0], prd->aarDiceColour[0]);
		Set3dColour(&prd->DiceMat[1], prd->aarDiceColour[1]);
		Set3dColour(&prd->DiceDotMat[0], prd->aarDiceDotColour[0]);
		Set3dColour(&prd->DiceDotMat[1], prd->aarDiceDotColour[1]);
		prd->afDieColour3d[0] = prd->afDieColour[0];
		prd->afDieColour3d[1] = prd->afDieColour[1];
		prd->fHinges3d = prd->fHinges;
		Set3dColour(&prd->CubeMat, prd->arCubeColour);
		Set3dColourChar(&prd->BaseMat, prd->aanBoardColour[0]);
		Set3dColourChar(&prd->BoxMat, prd->aanBoardColour[1]);
		Set3dColourChar(&prd->PointMat[0], prd->aanBoardColour[2]);
		Set3dColourChar(&prd->PointMat[1], prd->aanBoardColour[3]);
	}
}
#endif

static void
DesignAdd ( GtkWidget *pw, gpointer data ) {

  boarddesign *pbde;
  GList **pplBoardDesigns = data;
  renderdata rdNew;

  if ( ! ( pbde = (boarddesign *) g_malloc ( sizeof ( boarddesign ) ) ) ) {
    outputerr ( "allocate boarddesign" );
    return;
  }

  /* name and author of board */

  pbde->szTitle = pbde->szAuthor = pbde->szBoardDesign = NULL;

  GTKSetCurrentParent(pw);
  DesignAddTitle ( pbde );
  if ( ! pbde->szTitle || ! pbde->szAuthor ) {
    g_free ( pbde );
    return;
  }

  /* get board design */
  GetPrefs(&rdPrefs);
  rdNew = rdPrefs;
#if USE_BOARD3D
  CopyNewSettingsToOtherDimension(&rdNew);
#endif

  WriteDesignString(pbde, &rdNew);

  pbde->fDeletable = TRUE;

  *pplBoardDesigns = g_list_append ( *pplBoardDesigns, (gpointer) pbde );

  AddDesignRow ( pbde, pwDesignList );

  DesignSave(pw, data);

  pbdeSelected = pbde;

  SetTitle();
  ShowSelectedRow();
}

static void ExportDesign ( GtkWidget *pw, gpointer data ) 
{
	GList* designs;
	gchar *pch;
	gchar *szFile;
	FILE *pf;
	boarddesign *pbde;
	renderdata rdNew;

        if (! (pch = szFile =
                                GTKFileSelect(_("Export Design"), NULL, NULL, NULL,
                                        GTK_FILE_CHOOSER_ACTION_SAVE))) return;

	szFile = NextToken( &szFile );

	/* 
	* Copy current design 
	*/

	if ( ! ( pbde = (boarddesign *) g_malloc ( sizeof ( boarddesign ) ) ) ) {
		outputerr ( "allocate boarddesign" );
		return;
	}

	/* get title from current selection if possible */

	if ( g_list_length( GTK_CLIST( pwDesignList )->selection ) == 1 ) {
		int selrow = 
		GPOINTER_TO_INT( GTK_CLIST( pwDesignList )->selection->data );
		boarddesign *pbdeCurrent = 
		gtk_clist_get_row_data ( GTK_CLIST ( pwDesignList ), selrow );
		pbde->szTitle = g_strdup( pbdeCurrent->szTitle );
		pbde->szAuthor = g_strdup( pbdeCurrent->szAuthor );
	}
	else {
		/* user defined */
		pbde->szTitle = g_strdup( _("User defined") );
		pbde->szAuthor = g_strdup( _("User" ) );
	}

	/* get actual board design */

	if (pbdeSelected)
	{	/* Exporting current design so just get settings */
		ParsePreferences(pbdeSelected, &rdNew);
	}
	else
	{
		/* new design, so get settings and copy to other dimension */
		GetPrefs(&rdPrefs);
		rdNew = rdPrefs;
#if USE_BOARD3D
		CopyNewSettingsToOtherDimension(&rdNew);
#endif
	}

	WriteDesignString(pbde, &rdNew);

	pbde->fDeletable = TRUE;

	/* if file exists, read in any designs */
	if (!(designs = ParseBoardDesigns(szFile, TRUE)))
	{	/* None found so create empty list */
		designs = g_list_alloc();
	}
	designs = g_list_append(designs, (gpointer)pbde);

	/* write designs to file */

	if ( ! ( pf = g_fopen ( szFile, "w+" ) ) ) {
		outputerr ( szFile );
		free_board_design( pbde, NULL );
		g_free ( pch );
		return;
	}

	WriteDesignHeader( szFile, pf );
	g_list_foreach(designs, WriteDesign, pf);
	WriteDesignFooter( pf );
	fclose ( pf );

	free_board_design( pbde, NULL );
	g_free ( pch );
}

static void ImportDesign ( GtkWidget *pw, gpointer data ) 
{
	gchar *pch;
	gchar *szFile;
	GList *new_designs;
	gint old_length;
	gint num_added;

        if (! (pch = szFile =
                                GTKFileSelect(_("Import Design"), NULL, NULL, NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN))) return;

	szFile = NextToken( &szFile );

	if ( ! ( new_designs = ParseBoardDesigns( szFile, TRUE ) ) ) {
		/* no designs found */
		outputl( _("File not found or no designs in file.") );
		outputx();
		g_free( pch );
		return;
	}

	g_free( pch );

	/* add designs to current list */

	/* FIXME: show dialog instead */
	outputl( _("Adding new designs:") );

	old_length = g_list_length( plBoardDesigns );

	g_list_foreach ( new_designs, AddDesignRowIfNew, pwDesignList );

	num_added = g_list_length( plBoardDesigns ) - old_length;

	outputf( ngettext( "%d design added.\n",
						"%d designs added.\n", num_added ),
				num_added );
	outputx();

	if (num_added > 0)
	{
		DesignSave(pw, data);
		pbdeSelected = g_list_nth_data(plBoardDesigns, old_length);
		UseDesign();
	}
}


static void RemoveDesign ( GtkWidget *pw, gpointer data )
{
  GList **pplBoardDesigns = (GList **) data;
  int i = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ),
                                         pbdeSelected );
  char prompt[200];
  sprintf(prompt, _("Permently remove design %s?"), pbdeSelected->szTitle);
  if (!GetInputYN(prompt))
		return;

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );

  *pplBoardDesigns = g_list_remove ( *pplBoardDesigns, pbdeSelected );

  gtk_clist_remove ( GTK_CLIST ( pwDesignList ), i );

  DesignSave(pw, data);

  SetTitle();
}

static void UpdateDesign( GtkWidget *pw, gpointer data )
{
  renderdata newPrefs;
  char prompt[200];
#if USE_BOARD3D
	if (display_is_3d(&rdPrefs))
		sprintf(prompt, _("Permanently overwrite 3d settings for design %s?"), pbdeModified->szTitle);
	else
		sprintf(prompt, _("Permanently overwrite 2d settings for design %s?"), pbdeModified->szTitle);
#else
	sprintf(prompt, _("Permanently overwrite settings for design %s?"), pbdeModified->szTitle);
#endif
	if (!GetInputYN(prompt))
		return;

	gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUpdate ), FALSE );

#if USE_BOARD3D
	if (display_is_3d(&rdPrefs))
	{
		/* Get current (2d) settings for design */
		ParsePreferences(pbdeModified, &newPrefs);

		/* Overwrite 3d settings with current values */
		Set3dSettings(&newPrefs, &rdPrefs);
	}
	else
#endif
	{
#if USE_BOARD3D
		/* Get current (3d) design settings */
		renderdata designPrefs;
		ParsePreferences(pbdeModified, &designPrefs);
#endif

		/* Copy current (2d) settings */
		GetPrefs(&rdPrefs);
		newPrefs = rdPrefs;

#if USE_BOARD3D
		/* Overwrite 3d design settings */
		Set3dSettings(&newPrefs, &designPrefs);
#endif
	}
	/* Save updated design */
	WriteDesignString(pbdeModified, &newPrefs);
	DesignSave(pw, data);

	SetTitle();
}

static void
AddDesignRowIfNew( gpointer data, gpointer user_data ) {

  renderdata rdNew;
  GtkWidget *pwList = user_data;
  boarddesign *pbde = data;
  char *asz[ 1 ];
  gint i;

  if (!pbde)
    return;

  ParsePreferences(pbde, &rdNew);
  if (FindDesign(&rdNew))
  {
    outputf("Design %s already present\n", pbde->szTitle);
    return;
  }

  asz[ 0 ] = pbde->szTitle;

  i = gtk_clist_append ( GTK_CLIST ( pwList ), asz );
  gtk_clist_set_row_data ( GTK_CLIST ( pwList ), i, pbde );

  plBoardDesigns = g_list_append ( plBoardDesigns, (gpointer)pbde );
  outputf("Design %s added\n", pbde->szTitle);
}

static void
AddDesignRow ( gpointer data, gpointer user_data ) {

  GtkWidget *pwList = user_data;
  boarddesign *pbde = data;
  char *asz[ 1 ];
  gint i;

  if ( ! pbde )
    return;

  asz[ 0 ] = pbde->szTitle;

  i = gtk_clist_append ( GTK_CLIST ( pwList ), asz );
  gtk_clist_set_row_data ( GTK_CLIST ( pwList ), i, pbde );
}

static void DesignSelect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused ) {

    if (GTK_WIDGET_IS_SENSITIVE(pwDesignAdd))
	{
		GTKSetCurrentParent(GTK_WIDGET(pw));
    	if (!GetInputYN(_("Select new design and lose current changes?")))
		{
			pbdeModified = gtk_clist_get_row_data(GTK_CLIST(pwDesignList), nRow);
			gtk_widget_set_sensitive(GTK_WIDGET(pwDesignUpdate), pbdeModified->fDeletable);
			return;
		}
	}

    pbdeSelected = gtk_clist_get_row_data ( GTK_CLIST ( pwDesignList ),
                                          nRow );

    UseDesign();
}

static void DesignUnselect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused )
{
	gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );
	gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUpdate ), FALSE );

	pbdeSelected = NULL;

        
}

static GtkWidget *
DesignPage ( GList **pplBoardDesigns, BoardData *bd ) {

  GtkWidget *pwhbox;
  GtkWidget *pwScrolled;
  GtkWidget *pwPage;
  
  pwPage = gtk_vbox_new ( FALSE, 4 );

  /* CList with board designs */

  pwScrolled = gtk_scrolled_window_new( NULL, NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScrolled ),
				GTK_POLICY_AUTOMATIC,
				GTK_POLICY_AUTOMATIC );
  gtk_container_add ( GTK_CONTAINER ( pwPage ), pwScrolled );

  pwDesignList = gtk_clist_new( 1 );
  gtk_clist_set_column_auto_resize( GTK_CLIST( pwDesignList ), 0, TRUE );
/* Should be set to browse mode really, but gtk gets confused when
	the "lose current settings" warning is shown (dialog freezes)
  gtk_clist_set_selection_mode( GTK_CLIST( pwDesignList ), GTK_SELECTION_BROWSE );
*/
  g_list_foreach ( *pplBoardDesigns, AddDesignRow, pwDesignList );
  gtk_container_add ( GTK_CONTAINER ( pwScrolled ), pwDesignList );

  g_signal_connect( G_OBJECT( pwDesignList ), "select-row",
                      G_CALLBACK( DesignSelect ), NULL );
  g_signal_connect( G_OBJECT( pwDesignList ), "unselect-row",
                      G_CALLBACK( DesignUnselect ), NULL );

  /* button: use design */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwPage ), pwhbox, FALSE, FALSE, 4 );

  /* 
   * buttons 
   */

  /* add current design */

  pwDesignAdd = gtk_button_new_with_label ( _("Add current design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAdd, FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignAdd ), "clicked",
                       G_CALLBACK ( DesignAdd ), pplBoardDesigns );

  /* remove design */

  pwDesignRemove = gtk_button_new_with_label ( _("Remove design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignRemove, FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignRemove ), "clicked",
                       G_CALLBACK ( RemoveDesign ), pplBoardDesigns );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );

  /* update design */

  pwDesignUpdate = gtk_button_new_with_label ( _("Update design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignUpdate, FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignUpdate ), "clicked",
                     G_CALLBACK ( UpdateDesign ), pplBoardDesigns );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUpdate ), FALSE );

  /* export design */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwPage ), pwhbox, FALSE, FALSE, 4 );

  pwDesignExport = gtk_button_new_with_label ( _("Export design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignExport, FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignExport ), "clicked",
                       G_CALLBACK ( ExportDesign ), pplBoardDesigns );

  /* import design */

  pwDesignImport = gtk_button_new_with_label ( _("Import design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignImport, FALSE, FALSE, 4 );

  g_signal_connect( G_OBJECT ( pwDesignImport ), "clicked",
                       G_CALLBACK ( ImportDesign ), pplBoardDesigns );

  gtk_widget_show_all( pwPage );

  return pwPage;

}

#endif /* HAVE_LIBXML2 */

static void
BoardPrefsDestroy ( GtkWidget *pw, void * arg) {

	fUpdate = FALSE;

#if HAVE_LIBXML2
	free_board_designs ( plBoardDesigns );
#endif /* HAVE_LIBXML2 */
}

static void GetPrefs ( renderdata* prd ) {

    int i, j;
    gdouble ar[ 4 ];

#if USE_BOARD3D
	prd->fDisplayType = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwBoardType ) ) ? DT_2D : DT_3D;
	if (display_is_3d(&rdPrefs))
	{
		int newCurveAccuracy;
		/* dice colour same as chequer colour */
		for ( i = 0; i < 2; ++i )
		{
			prd->afDieColour3d[ i ] = 
			gtk_toggle_button_get_active ( 
				GTK_TOGGLE_BUTTON ( apwDieColour[ i ] ) );
		}

		prd->showShadows = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwShowShadows));
		prd->shadowDarkness = (int)padjDarkness->value;
		/* Darkness as percentage of ambient light */
		prd->dimness = ((prd->lightLevels[1] / 100.0f) * (100 - prd->shadowDarkness)) / 100;

		prd->animateRoll = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwAnimateRoll));
		prd->animateFlag = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwAnimateFlag));
		prd->closeBoardOnExit = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwCloseBoard));

		newCurveAccuracy = (int)padjAccuracy->value;
		newCurveAccuracy -= (newCurveAccuracy % 4);
		prd->curveAccuracy = newCurveAccuracy;

		prd->lightType = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwLightSource) ) ? LT_POSITIONAL : LT_DIRECTIONAL;
		prd->lightPos[0] = (float)padjLightPosX->value;
		prd->lightPos[1] = (float)padjLightPosY->value;
		prd->lightPos[2] = (float)padjLightPosZ->value;
		prd->lightLevels[0] = (int)padjLightLevelAmbient->value;
		prd->lightLevels[1] = (int)padjLightLevelDiffuse->value;
		prd->lightLevels[2] = (int)padjLightLevelSpecular->value;

		prd->boardAngle = (int)padjBoardAngle->value;
		prd->skewFactor = (int)padjSkewFactor->value;
		prd->pieceType = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwRoundedPiece) ) ? PT_ROUNDED : PT_FLAT;
		prd->pieceTextureType = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwTextureAllPiece) ) ? PTT_ALL : PTT_TOP;
		prd->diceSize = (float)padjDiceSize->value;
		prd->roundedEdges = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwRoundedEdges));
		prd->bgInTrays = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwBgTrays));
		prd->roundedPoints = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwRoundPoints));
		prd->planView = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwPlanView));

		prd->fHinges3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwHinges ) );
	}
	else
#endif
{
    /* dice colour same as chequer colour */
    for ( i = 0; i < 2; ++i )
	{
      prd->afDieColour[ i ] = 
        gtk_toggle_button_get_active ( 
           GTK_TOGGLE_BUTTON ( apwDieColour[ i ] ) );
	}

    for( i = 0; i < 2; i++ ) {
	prd->arRefraction[ i ] = (float)gtk_adjustment_get_value(apadj[ i ]);
	prd->arCoefficient[ i ] = (float)gtk_adjustment_get_value(apadjCoefficient[ i ]);
	prd->arExponent[ i ] = (float)gtk_adjustment_get_value(apadjExponent[ i ]);

        prd->arDiceCoefficient[ i ] = (float)apadjDiceCoefficient[ i ]->value;
	prd->arDiceExponent[ i ] = (float)apadjDiceExponent[ i ]->value;
    }
    
    gtk_color_button_get_array( GTK_COLOR_BUTTON( apwColour[ 0 ] ), ar );
    for( i = 0; i < 4; i++ )
	prd->aarColour[ 0 ][ i ] = ar[ i ];

    gtk_color_button_get_array( GTK_COLOR_BUTTON( apwColour[ 1 ] ), ar );
    for( i = 0; i < 4; i++ )
	prd->aarColour[ 1 ][ i ] = ar[ i ];

    gtk_color_button_get_array( GTK_COLOR_BUTTON( apwDiceColour[ 0 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	prd->aarDiceColour[ 0 ][ i ] = ar[ i ];

    gtk_color_button_get_array( GTK_COLOR_BUTTON( apwDiceColour[ 1 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	prd->aarDiceColour[ 1 ][ i ] = ar[ i ];

    for ( j = 0; j < 2; ++j ) {

      gtk_color_button_get_array( 
           GTK_COLOR_BUTTON( apwDiceDotColour[ j ] ), 
           ar );
      for( i = 0; i < 3; i++ )
	prd->aarDiceDotColour[ j ][ i ] = ar[ i ];
    }

    /* cube colour */
    gtk_color_button_get_array( GTK_COLOR_BUTTON( pwCubeColour ), ar );

	for( i = 0; i < 3; i++ )
		prd->arCubeColour[ i ] = ar[ i ];

    /* board colour */

    for( j = 0; j < 4; j++ ) {
	gtk_color_button_get_array( GTK_COLOR_BUTTON( apwBoard[ j ] ),
				      ar );
	for( i = 0; i < 3; i++ )
	    prd->aanBoardColour[ j ][ i ] = (unsigned char)(ar[ i ] * 0xFF);
    }
    
    prd->aSpeckle[ 0 ] = (int)(apadjBoard[ 0 ]->value * 0x80);
/*    prd->aSpeckle[ 1 ] = (int)(apadjBoard[ 1 ]->value * 0x80); */
    prd->aSpeckle[ 2 ] = (int)(apadjBoard[ 2 ]->value * 0x80);
    prd->aSpeckle[ 3 ] = (int)(apadjBoard[ 3 ]->value * 0x80);
    
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pwWood ) ))
	    prd->wt = gtk_combo_box_get_active(GTK_COMBO_BOX(pwWoodType));
	else
		prd->wt = WOOD_PAINT;

    prd->rRound = 1.0f - (float)padjRound->value;

	prd->fHinges = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwHinges ) );

	WorkOut2dLight(prd);
}

	prd->fDynamicLabels = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwDynamicLabels ) );
	prd->fLabels = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwLabels ) );
	prd->showMoveIndicator = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwMoveIndicator));
    
    prd->fClockwise = fClockwise; /* yuck */
}

static void append_preview_page( GtkWidget *pwNotebook, GtkWidget *pwPage,
				 char *szLabel, pixmapindex pi ) {

    GtkWidget *pw;

    pw = gtk_hbox_new( FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwPage, TRUE, TRUE, 0 );
    gtk_widget_show_all( pw );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), pw,
			      gtk_label_new( szLabel ) );
}

void AddPages(BoardData* bd, GtkWidget* pwNotebook)
{
#if USE_BOARD3D
	gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
				LightingPage( bd ),
				gtk_label_new( _("Lighting") ) );

	gtk_widget_set_sensitive(frame3dOptions, display_is_3d(&rdPrefs));

#endif

#if HAVE_LIBXML2
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      DesignPage( &plBoardDesigns, bd ),
			      gtk_label_new(_("Designs") ) );
#endif /* HAVE_LIBXML2 */

#if USE_BOARD3D
	if (display_is_3d(&rdPrefs))
	{
		ResetPreviews();
		append_preview_page( pwNotebook, ChequerPrefs3d( bd ),
			_("Chequers"), PI_CHEQUERS0 );
		append_preview_page( pwNotebook, BoardPage3d( bd ), _("Board"),
			PI_BOARD );
		append_preview_page( pwNotebook, BorderPage3d( bd ), _("Border"),
			PI_BORDER );
		append_preview_page( pwNotebook, DicePrefs3d( bd, 0 ),
			_("Dice (0)"), PI_DICE0 );
		append_preview_page( pwNotebook, DicePrefs3d( bd, 1 ),
			_("Dice (1)"), PI_DICE1 );
	    append_preview_page( pwNotebook, CubePrefs3d( bd ), _("Cube"), PI_CUBE );
	}
	else
#endif
	{
		append_preview_page( pwNotebook, ChequerPrefs( bd, 0 ),
				 _("Chequers (0)"), PI_CHEQUERS0 );
		append_preview_page( pwNotebook, ChequerPrefs( bd, 1 ),
				 _("Chequers (1)"), PI_CHEQUERS1 );
		append_preview_page( pwNotebook, BoardPage( bd ), _("Board"),
				 PI_BOARD );
		append_preview_page( pwNotebook, BorderPage( bd ), _("Border"),
				 PI_BORDER );
		append_preview_page( pwNotebook, DicePrefs( bd, 0 ), _("Dice (0)"),
				 PI_DICE0 );
		append_preview_page( pwNotebook, DicePrefs( bd, 1 ), _("Dice (1)"),
				 PI_DICE1 );
		append_preview_page( pwNotebook, CubePrefs( bd ), _("Cube"), PI_CUBE );
	}
}

static void ChangePage(GtkNotebook *notebook, GtkNotebookPage *page, 
				guint page_num, gpointer user_data)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	unsigned int dicePage = NUM_NONPREVIEW_PAGES + PI_DICE0;

	if (!fUpdate)
		return;

#if USE_BOARD3D
	if (display_is_3d(&rdPrefs))
		dicePage -= 1;
#endif
	/* Make sure correct dice preview visible */

	if ((page_num == dicePage && bd->turn == 1) ||
		(page_num == dicePage + 1 && bd->turn == -1))
	{
		bd->turn = -bd->turn;
#if USE_BOARD3D
		if (display_is_3d(&rdPrefs))
			setDicePos(bd, bd->bd3d);
		else
#endif
			RollDice2d(bd);

		option_changed(0, 0);
	}
#if HAVE_LIBXML2
	if (page_num == NUM_NONPREVIEW_PAGES)
		ShowSelectedRow();
#endif

#if USE_BOARD3D
	if (display_is_3d(&rdPrefs) && redrawChange)
	{
		redraw_changed(NULL, NULL);
		redrawChange = FALSE;
	}
#endif
}

static void pref_dialog_map(GtkWidget *window, BoardData *bd)
{
#if USE_BOARD3D
	DisplayCorrectBoardType(bd, bd->bd3d, bd->rd);
	redrawChange = FALSE;
	bd->rd->quickDraw = FALSE;
#endif
}

extern void BoardPreferences(GtkWidget *pwBoard)
{
	GtkWidget *pwDialog, *pwHbox;
	BoardData *bd;

	/* Set up board with current preferences, hide unwanted windows */
	CopyAppearance(&rdPrefs);
	rdPrefs.fShowIDs = FALSE;
	rdPrefs.fDiceArea = FALSE;
	rdPrefs.fShowGameInfo = FALSE;
	/* Create preview board */
	pwPrevBoard = board_new(&rdPrefs);

	bd = BOARD(pwPrevBoard)->board_data;
	InitBoardPreview(bd);
	RollDice2d(bd);

    pwDialog = GTKCreateDialog( _("GNU Backgammon - Appearance"), DT_QUESTION, NULL, DIALOG_FLAG_MODAL | DIALOG_FLAG_MINMAXBUTTONS,
			     G_CALLBACK( BoardPrefsOK ), pwBoard );

#if USE_BOARD3D
    if (gtk_gl_init_success)
    {
	    SetPreviewLightLevel(bd->rd->lightLevels);
		setDicePos(bd, bd->bd3d);
    }
#endif

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwHbox = gtk_hbox_new(FALSE, 0) );

    pwNotebook = gtk_notebook_new();

    gtk_notebook_set_scrollable( GTK_NOTEBOOK( pwNotebook ), TRUE );
    gtk_notebook_popup_enable( GTK_NOTEBOOK( pwNotebook ) );
    
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
#if !USE_BOARD3D
	/* Make sure preview is big enough in 2d mode */
    gtk_widget_set_size_request ( GTK_WIDGET ( pwNotebook ), -1, 360 );
#endif

	gtk_box_pack_start( GTK_BOX(pwHbox), pwNotebook, FALSE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX(pwHbox), pwPrevBoard, TRUE, TRUE, 0 );

    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      GeneralPage( bd, pwBoard ),
			      gtk_label_new( _("General") ) );

#if HAVE_LIBXML2
    plBoardDesigns = read_board_designs ();
#endif
	AddPages(bd, pwNotebook);

    g_signal_connect( G_OBJECT( pwNotebook ), "switch-page",
			G_CALLBACK( ChangePage ), 0);

	g_signal_connect( G_OBJECT( pwDialog ), "destroy",
			G_CALLBACK( BoardPrefsDestroy ), NULL );

	SetTitle();

	gtk_notebook_set_current_page(GTK_NOTEBOOK(pwNotebook), NUM_NONPREVIEW_PAGES);

	g_signal_connect(pwDialog, "map", G_CALLBACK(pref_dialog_map), bd);

	fUpdate = TRUE;
	GTKRunDialog(pwDialog);
}

extern void BoardPreferencesStart( GtkWidget *pwBoard ) {

    BoardData *bd = BOARD( pwBoard )->board_data;

    if( GTK_WIDGET_REALIZED( pwBoard ) )
	board_free_pixmaps( bd );
}

extern void BoardPreferencesDone( GtkWidget *pwBoard )
{
    BoardData *bd = BOARD( pwBoard )->board_data;

	if( GTK_WIDGET_REALIZED( pwBoard ) )
	{
		board_create_pixmaps( pwBoard, bd );
{
#if USE_BOARD3D
		BoardData3d *bd3d = bd->bd3d;
		renderdata *prd = bd->rd;

		DisplayCorrectBoardType(bd, bd3d, prd);
		if (display_is_3d(prd))
			updateOccPos(bd);
		else
			StopIdle3d(bd, bd->bd3d);

		if (display_is_2d(prd))
#endif
		{
			gtk_widget_queue_draw(bd->drawing_area);
			gtk_widget_queue_draw(bd->dice_area);
		}
		gtk_widget_queue_draw(bd->table);
}
	}
}

#if USE_BOARD3D

static int IsWhiteColour3d(Material* pMat)
{
	return (pMat->ambientColour[0] == 1) && (pMat->ambientColour[1] == 1) && (pMat->ambientColour[2] == 1) &&
		(pMat->diffuseColour[0] == 1) && (pMat->diffuseColour[1] == 1) && (pMat->diffuseColour[2] == 1) &&
		(pMat->specularColour[0] == 1) && (pMat->specularColour[1] == 1) && (pMat->specularColour[2] == 1);
}

static int IsBlackColour3d(Material* pMat)
{
	return (pMat->ambientColour[0] == 0) && (pMat->ambientColour[1] == 0) && (pMat->ambientColour[2] == 0) &&
		(pMat->diffuseColour[0] == 0) && (pMat->diffuseColour[1] == 0) && (pMat->diffuseColour[2] == 0) &&
		(pMat->specularColour[0] == 0) && (pMat->specularColour[1] == 0) && (pMat->specularColour[2] == 0);
}

void Default3dSettings(BoardData* bd)
{	/* If no 3d settings loaded, set 3d appearance to first design */
#if HAVE_LIBXML2
	/* Check if colours are set to default values */
	if (IsWhiteColour3d(&bd->rd->ChequerMat[0]) && IsBlackColour3d(&bd->rd->ChequerMat[1]) &&
		IsWhiteColour3d(&bd->rd->DiceMat[0]) && IsBlackColour3d(&bd->rd->DiceMat[1]) &&
		IsBlackColour3d(&bd->rd->DiceDotMat[0]) && IsWhiteColour3d(&bd->rd->DiceDotMat[1]) &&
		IsWhiteColour3d(&bd->rd->CubeMat) && IsBlackColour3d(&bd->rd->CubeNumberMat) &&
		IsWhiteColour3d(&bd->rd->PointMat[0]) && IsBlackColour3d(&bd->rd->PointMat[1]) &&
		IsBlackColour3d(&bd->rd->BoxMat) &&
		IsWhiteColour3d(&bd->rd->PointNumberMat) &&
		IsWhiteColour3d(&bd->rd->BackGroundMat))
	{
		/* Copy appearance to preserve current 2d settings */
		renderdata rdNew;
		CopyAppearance(&rdNew);

		plBoardDesigns = read_board_designs ();
		if (plBoardDesigns && g_list_length(plBoardDesigns) > 0)
		{
			boarddesign *pbde = g_list_nth_data(plBoardDesigns, 1);
			if (pbde->szBoardDesign)
			{
				char *apch[ 2 ];
				gchar *sz, *pch;

				pch = sz = g_strdup ( pbde->szBoardDesign );
				while( ParseKeyValue( &sz, apch ) ) 
					RenderPreferencesParam( &rdNew, apch[ 0 ], apch[ 1 ] );

				g_free ( pch );

				/* Copy 3d settings from rdNew to main appearance settings */
				bd->rd->pieceType = rdNew.pieceType;
				bd->rd->pieceTextureType = rdNew.pieceTextureType;
				bd->rd->fHinges3d = rdNew.fHinges3d;
				bd->rd->showMoveIndicator = rdNew.showMoveIndicator;
				bd->rd->showShadows = rdNew.showShadows;
				bd->rd->roundedEdges = rdNew.roundedEdges;
				bd->rd->bgInTrays = rdNew.bgInTrays;
				bd->rd->roundedPoints = rdNew.roundedPoints;
				bd->rd->shadowDarkness = rdNew.shadowDarkness;
				bd->rd->curveAccuracy = rdNew.curveAccuracy;
				bd->rd->skewFactor = rdNew.skewFactor;
				bd->rd->boardAngle = rdNew.boardAngle;
				bd->rd->diceSize = rdNew.diceSize;
				bd->rd->planView = rdNew.planView;
				bd->rd->quickDraw = rdNew.quickDraw;

				memcpy(bd->rd->ChequerMat, rdNew.ChequerMat, sizeof(Material[2]));
				memcpy(bd->rd->DiceMat, rdNew.DiceMat, sizeof(Material[2]));
				bd->rd->DiceMat[0].textureInfo = bd->rd->DiceMat[1].textureInfo = 0;
				bd->rd->DiceMat[0].pTexture = bd->rd->DiceMat[1].pTexture = 0;

				memcpy(bd->rd->DiceDotMat, rdNew.DiceDotMat, sizeof(Material[2]));

				memcpy(&bd->rd->CubeMat, &rdNew.CubeMat, sizeof(Material));
				memcpy(&bd->rd->CubeNumberMat, &rdNew.CubeNumberMat, sizeof(Material));

				memcpy(&bd->rd->BaseMat, &rdNew.BaseMat, sizeof(Material));
				memcpy(bd->rd->PointMat, rdNew.PointMat, sizeof(Material[2]));

				memcpy(&bd->rd->BoxMat, &rdNew.BoxMat, sizeof(Material));
				memcpy(&bd->rd->HingeMat, &rdNew.HingeMat, sizeof(Material));
				memcpy(&bd->rd->PointNumberMat, &rdNew.PointNumberMat, sizeof(Material));
				memcpy(&bd->rd->BackGroundMat, &rdNew.BackGroundMat, sizeof(Material));

				memset(&bd->rd->lightPos, 0,sizeof(rdNew.lightPos));
				memset(&bd->rd->lightLevels, 0, sizeof(rdNew.lightLevels));
				memcpy(&bd->rd->lightPos, &rdNew.lightPos, sizeof(rdNew.lightPos));
				memcpy(&bd->rd->lightLevels, &rdNew.lightLevels, sizeof(rdNew.lightLevels));

				memcpy(&bd->rd->afDieColour3d, &rdNew.afDieColour3d, sizeof(rdNew.afDieColour3d));
			}
		}
		free_board_designs ( plBoardDesigns );
	}
#endif /* HAVE_LIBXML2 */
}
#endif

#if HAVE_LIBXML2

typedef enum _parsestate {
  STATE_NONE,
  STATE_BOARD_DESIGNS,
  STATE_BOARD_DESIGN,
  STATE_ABOUT,
  STATE_TITLE, STATE_AUTHOR,
  STATE_DESIGN
} parsestate;

typedef struct _parsecontext {

  GList *pl;  /* list of board designs */

  parsestate aps[ 10 ];      /* the current tag */
  int ips;                   /* depth of stack */

  boarddesign *pbde;
  int i, j;      /* counters */
  int err;     /* return code */
  int fDeletable; /* is the board design deletable */

} parsecontext;


static void
PushState ( parsecontext *ppc, const parsestate ps ) {

  if ( ppc->ips > 9 ) {
    ppc->err = TRUE;
    return;
  }

  ++(ppc->ips);
  ppc->aps[ ppc->ips ] = ps;

}

static void
PopState ( parsecontext *ppc ) {

 (ppc->ips)--;

}


static void Err( void *pv, const char *msg, ... ) {

  parsecontext *ppc = pv;
  va_list args;
  
  ppc->err = TRUE;
  
  va_start(args, msg);
  g_vprintf(msg, args);
  va_end(args);
  
}

static void ScanEndElement( void *pv, const xmlChar *pchName ) {
  
  parsecontext *ppc = pv;

  /* pop element */

  PopState ( ppc );

}



static void ScanCharacters( void *pv, const xmlChar *pchIn, int cch ) {

  parsecontext *ppc = pv;
  char *sz = g_strndup ( (char*)pchIn, cch );
  char *pc;

  switch ( ppc->aps[ ppc->ips ] ) {
  case STATE_BOARD_DESIGNS:
  case STATE_BOARD_DESIGN:
    /* no chars in these elements */
    break;

  case STATE_TITLE:

    pc = ppc->pbde->szTitle;
    ppc->pbde->szTitle = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  case STATE_AUTHOR:

    pc = ppc->pbde->szAuthor;
    ppc->pbde->szAuthor = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  case STATE_DESIGN:
    pc = ppc->pbde->szBoardDesign;
    ppc->pbde->szBoardDesign = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  default:
    break;
  }

  g_free ( sz );

}

static void ScanStartElement( void *pv, const xmlChar *pchName,
                              const xmlChar **ppchAttrs ) {

  parsecontext *ppc = pv;

  if ( ! strcmp ( (char*)pchName, "board-designs" ) && ppc->ips == -1 ) {

    PushState ( ppc, STATE_BOARD_DESIGNS );

  }
  else if ( ! strcmp ( (char*)pchName, "board-design" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGNS ) {

    /* allocate list if empty */
    if ( ! ppc->pl )
      ppc->pl = g_list_alloc ();

    ppc->pbde = (boarddesign *) g_malloc ( sizeof ( boarddesign ) );
    memset ( ppc->pbde, 0, sizeof ( boarddesign ) );
    ppc->pbde->fDeletable = ppc->fDeletable;
    ppc->pl = g_list_append ( ppc->pl, (gpointer) ppc->pbde );

    PushState ( ppc, STATE_BOARD_DESIGN );

  }
  else if ( ! strcmp ( (char*) pchName, "about" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGN ) {

    PushState ( ppc, STATE_ABOUT );

  }
  else if ( ! strcmp ( (char*)pchName, "title" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_TITLE );
    ppc->pbde->szTitle = g_strdup ( "" );

  }
  else if ( ! strcmp( (char*) pchName, "author" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_AUTHOR );
    ppc->pbde->szAuthor = g_strdup ( "" );

  }
  else if ( ! strcmp( (char*) pchName, "design" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGN ) {

    PushState ( ppc, STATE_DESIGN );
    ppc->pbde->szBoardDesign = g_strdup ( "" );

  }
  else
    printf ( "ignoring start tag \"%s\"\n", pchName );


}


static xmlSAXHandler xsaxScan = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    ScanStartElement, /* startElement */
    ScanEndElement, /* endElement */
    NULL, /* reference */
    ScanCharacters, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    Err, /* xmlParserWarning */
    Err, /* xmlParserError */
    Err, /* fatal error */
    NULL, /* getParameterEntity */
    NULL, /* cdataBlock; */
    NULL,  /* externalSubset; */
    TRUE
};

static GList *
ParseBoardDesigns ( const char *szFile, const int fDeletable ) {

  xmlParserCtxt *pxpc;
  parsecontext pc;
  char *contents;
  gsize size;


  pc.ips = -1;
  pc.pl = NULL;
  pc.err = FALSE;
  pc.fDeletable = fDeletable;

  /* create parser context */

  if (!g_file_test(szFile, G_FILE_TEST_IS_REGULAR))
          return NULL;
  if (!g_file_get_contents(szFile, &contents, &size, NULL))
          return NULL;

  pxpc = xmlCreateMemoryParserCtxt ( contents, (int)size );
  if ( ! pxpc )
          return NULL;

  pxpc->sax = &xsaxScan;
  pxpc->userData = &pc;

  /* parse document */

  xmlParseDocument ( pxpc );

  if ( pc.err )
    free_board_designs ( pc.pl );

  return pc.err ? NULL : pc.pl;

}

#endif /* HAVE_LIBXML2 */
