/*
 * gtkprefs.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001, 2002.
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

#include <ctype.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <time.h>

#if HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#endif

#include "backgammon.h"
#include "drawboard.h"
#include "gtkboard.h"
#include "gtkcolour.h"
#include "gtkgame.h"
#include "gtkprefs.h"
#include "i18n.h"
#include "path.h"
#include "render.h"
#include "renderprefs.h"
#include "boarddim.h"

#if USE_BOARD3D
#define NUM_NONPREVIEW_PAGES 2
#else
#define NUM_NONPREVIEW_PAGES 1
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
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
    *pwWood, *pwWoodType, *pwWoodMenu, *pwHinges, *pwLightTable, *pwMoveIndicator,
    *pwWoodF, *pwNotebook, *pwLabels, *pwDynamicLabels;
#if HAVE_LIBXML2
static GList *plBoardDesigns;
#endif
#if USE_BOARD3D
GtkWidget *pwBoardType, *pwShowShadows, *pwAnimateRoll, *pwAnimateFlag, *pwCloseBoard,
	*pwDarkness, *lightLab, *darkLab, *pwLightSource, *pwDirectionalSource, *pwQuickDraw,
	*pwTestPerformance, *pmHingeCol, *pieceTypeCombo, *textureTypeCombo, *frame3dOptions,
	*pwPlanView, *pwBoardAngle, *pwSkewFactor, *skewLab, *anglelab, *pwBgTrays,
	*dtLightSourceFrame, *dtLightPositionFrame, *dtLightLevelsFrame, *pwRoundedEdges;
GtkAdjustment *padjDarkness, *padjAccuracy, *padjBoardAngle, *padjSkewFactor, *padjLightPosX,
	*padjLightLevelAmbient, *padjLightLevelDiffuse, *padjLightLevelSpecular,
	*padjLightPosY, *padjLightPosZ, *padjDiceSize;
	int redrawChange;
	GtkTooltips *ptt;

	static char lastPieceStr[50], lastTextureStr[50];
	char* pieceTypeStr[NUM_PIECE_TYPES] = {N_("Rounded disc"), N_("Flat edged disc")};
	char* textureTypeStr[NUM_TEXTURE_TYPES] = {N_("Top only"), N_("All of piece")};

	int pc3dDiceId[2], pcChequer2;
#endif

GtkWidget *pwPrevBoard;
renderdata rdPrefs;

#if HAVE_LIBXML2
static GtkWidget *pwDesignAdd, *pwDesignRemove, *pwDesignUpdate;
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
#endif

#if USE_BOARD3D
void ResetPreviews();
void UpdateColPreviews();
int GetPreviewId();
void UpdateColPreview(int ID);
void SetPreviewLightLevel(int levels[3]);
void Setup3dColourPicker(GtkWidget* parent, GdkWindow* wind);
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

static boarddesign *pbdeSelected = NULL;

static GList *
read_board_designs ( void ) {

  GList *plUser, *plSystem, *plFinal;
  gchar *sz;

  plSystem = ParseBoardDesigns ( "boards.xml", FALSE );

  sz = g_strdup_printf ( "%s/.gnubg/boards.xml", szHomeDirectory );
  plUser = ParseBoardDesigns ( sz, TRUE );
  g_free ( sz );

  plFinal = g_list_concat ( plSystem, plUser );

  return plFinal;

}

static void
free_board_design ( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;

  if ( ! pbde )
    return;

  g_free ( pbde->szTitle );
  g_free ( pbde->szAuthor );
  g_free ( pbde->szBoardDesign );

  free ( data );

}

static void
free_board_designs ( GList *pl ) {

  g_list_foreach ( pl, free_board_design, NULL );
  g_list_free ( pl );

}

#endif /* HAVE_LIBXML2 */

static void DesignSelect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused );
static void DesignUnselect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused );

void SetTitle()
{	/* Update dialog title to include design name + author */
	int i = 0;
	char title[1024];
	int found = FALSE;
	GtkWidget *pwDialog = gtk_widget_get_toplevel(pwPrevBoard);

	strcpy(title, _("GNU Backgammon - Appearance"));

	pbdeSelected = 0;
#if HAVE_LIBXML2
{	/* Search for current settings in designs */
	gchar *sz, *pch;
	renderdata rdTest;
	char *apch[ 2 ];
	strcat(title, ": ");
	for (i = 0; i < g_list_length(plBoardDesigns) - 1; i++)
	{
		boarddesign *pbde = g_list_nth_data(plBoardDesigns, i + 1);
		if (pbde)
		{
			rdTest = rdPrefs;
			pch = sz = g_strdup(pbde->szBoardDesign);
			while (ParseKeyValue(&sz, apch)) 
			RenderPreferencesParam(&rdTest, apch[ 0 ], apch[ 1 ]);
			g_free(pch);
			if (PreferenceCompare(&rdTest, &rdPrefs))
			{
				char design[1024];
				int row = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ), pbde);
				gtk_signal_handler_block_by_func(GTK_OBJECT(pwDesignList), GTK_SIGNAL_FUNC( DesignSelect ), 0);
				gtk_signal_handler_block_by_func(GTK_OBJECT(pwDesignList), GTK_SIGNAL_FUNC( DesignUnselect ), 0);
				gtk_clist_select_row( GTK_CLIST( pwDesignList ), row, 0);
				gtk_signal_handler_unblock_by_func(GTK_OBJECT(pwDesignList), GTK_SIGNAL_FUNC( DesignSelect ), 0);
				gtk_signal_handler_unblock_by_func(GTK_OBJECT(pwDesignList), GTK_SIGNAL_FUNC( DesignUnselect ), 0);

				gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), pbde->fDeletable );

				sprintf(design, "%s by %s (%s)", pbde->szTitle, pbde->szAuthor,
					pbde->fDeletable ? _("user defined") : _("predefined" ) );
				strcat(title, design);
				found = TRUE;
				pbdeSelected = pbde;
				break;
			}
		}
	}
	gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignAdd ), !found );
	if (!found)
		strcat(title, _("Custom design"));

	gtk_widget_set_sensitive(pwDesignUpdate, !found && GTK_WIDGET_IS_SENSITIVE(pwDesignRemove) );
}
#endif
	gtk_window_set_title( GTK_WINDOW( pwDialog ),  title);
}

void UpdatePreview(GtkWidget **ppw)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;

	if (!fUpdate)
		return;

#if USE_BOARD3D
	if (bd->rd->fDisplayType == DT_3D)
	{	/* Sort out chequer and dice special settings */
		if (bd->rd->ChequerMat[0].textureInfo != bd->rd->ChequerMat[1].textureInfo)
		{	/* Make both chequers have the same texture */
			bd->rd->ChequerMat[1].textureInfo = bd->rd->ChequerMat[0].textureInfo;
			bd->rd->ChequerMat[1].pTexture = bd->rd->ChequerMat[0].pTexture;
			UpdateColPreview(pcChequer2);
			/* Change to main area and recreate piece display lists */
			MakeCurrent3d(bd->drawing_area3d);
			preDraw3d(bd);
		}
		if (bd->rd->afDieColour3d[0] &&
			!MaterialCompare(&bd->rd->DiceMat[0], &bd->rd->ChequerMat[0]))
		{
			memcpy(&bd->rd->DiceMat[0], &bd->rd->ChequerMat[0], sizeof(Material));
			bd->rd->DiceMat[0].textureInfo = 0;
			bd->rd->DiceMat[0].pTexture = 0;
			UpdateColPreview(pc3dDiceId[0]);
		}
		if (bd->rd->afDieColour3d[1] &&
			!MaterialCompare(&bd->rd->DiceMat[1], &bd->rd->ChequerMat[1]))
		{
			memcpy(&bd->rd->DiceMat[1], &bd->rd->ChequerMat[1], sizeof(Material));
			bd->rd->DiceMat[1].textureInfo = 0;
			bd->rd->DiceMat[1].pTexture = 0;
			UpdateColPreview(pc3dDiceId[1]);
		}
	}
	else
#endif
	{	/* Create new 2d pixmaps */
		board_free_pixmaps( bd );
		GetPrefs(&rdPrefs);
		board_create_pixmaps( pwPrevBoard, bd );
	}
	SetTitle();
	gtk_widget_queue_draw(pwPrevBoard);
}

void DieColourChanged (GtkWidget *pw, int f)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));
	gtk_widget_set_sensitive(apwDiceColourBox[f], !set);

	if (!fUpdate)
		return;

#if USE_BOARD3D
	if (rdPrefs.fDisplayType == DT_3D)
	{
		BoardData *bd = BOARD(pwPrevBoard)->board_data;
		bd->rd->afDieColour3d[f] = set;
		memcpy(&bd->rd->DiceMat[f], bd->rd->afDieColour3d[f] ? &bd->rd->ChequerMat[f] : &bd->rd->DiceMat[f], sizeof(Material));
		bd->rd->DiceMat[f].textureInfo = 0;
		bd->rd->DiceMat[f].pTexture = 0;

		UpdateColPreview(pc3dDiceId[f]);
	}
#endif

	UpdatePreview(0);
}

void option_changed(GtkWidget *widget, GtkWidget *pw)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	if (!fUpdate)
		return;

#if USE_BOARD3D
	if (bd->rd->fDisplayType == DT_3D)
	{
		ClearTextures(bd);
		freeEigthPoints(&bd->boardPoints, rdPrefs.curveAccuracy);

		GetPrefs(&rdPrefs);
		GetTextures(bd);

		SetupViewingVolume3d(bd);
		preDraw3d(bd);
	}
	else
#endif
	{
		board_free_pixmaps( bd );
		board_create_pixmaps( pwPrevBoard, bd );
	}
	UpdatePreview(0);
}

#if USE_BOARD3D

void redraw_changed(GtkWidget *widget, GtkWidget **ppw)
{	/* Update 3d colour previews */
	if (!fUpdate)
		return;

	GetPrefs(&rdPrefs);
	SetPreviewLightLevel(rdPrefs.lightLevels);
	UpdateColPreviews();
}

void DiceSizeChanged(GtkWidget *pw)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	bd->rd->diceSize = padjDiceSize->value;
	if (DiceTooClose(bd))
		setDicePos(bd);
	option_changed(0, 0);
}

void HingeChanged (GtkWidget *pw)
{
	gtk_widget_set_sensitive(pmHingeCol, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pwHinges)));
	option_changed(0, 0);
}

PieceType getPieceType(char* str)
{
	int i;
	for (i = 0; i < NUM_PIECE_TYPES; i++)
	{
		if (!strcmp(str, pieceTypeStr[i]))
			return i;
	}

	g_print("unknown piece type %s\n", str);
	return 0;
}

PieceTextureType getPieceTextureType(char* str)
{
	int i;

	for (i = 0; i < NUM_TEXTURE_TYPES; i++)
	{
		if (!strcmp(str, textureTypeStr[i]))
			return i;
	}

	g_print("unknown texture type %s\n", str);
	return 0;
}

void PieceChange(GtkList *list, gpointer user_data)
{
	const char* current = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pieceTypeCombo)->entry));

	if (current && *current && *lastPieceStr && (strcmp(current, lastPieceStr)))
	{
		strcpy(lastPieceStr, current);
		option_changed(0, 0);
	}
}

void textureChange(GtkList *list, gpointer user_data)
{
	const char* current = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(textureTypeCombo)->entry));

	if (current && *current && *lastTextureStr && (strcmp(current, lastTextureStr)))
	{
		strcpy(lastTextureStr, current);
		option_changed(0, 0);
	}
}

static GtkWidget *ChequerPrefs3d( BoardData *bd)
{
    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;
	GList *glist;
	int i;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Chequer 0:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->ChequerMat[0], DF_VARIABLE_OPACITY, TT_PIECE), TRUE, TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Chequer 1:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->ChequerMat[1], DF_VARIABLE_OPACITY, TT_PIECE | TT_DISABLED), TRUE, TRUE, 4 );
	pcChequer2 = GetPreviewId();

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Piece type:") ),
			FALSE, FALSE, 4 );

	pieceTypeCombo = gtk_combo_new();
	gtk_combo_set_value_in_list(GTK_COMBO(pieceTypeCombo), TRUE, FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(GTK_COMBO(pieceTypeCombo)->entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(pieceTypeCombo)->list), "selection-changed",
							GTK_SIGNAL_FUNC(PieceChange), 0);
    gtk_box_pack_start( GTK_BOX( pwhbox ), pieceTypeCombo, FALSE, FALSE, 0);

	glist = NULL;
	for (i = 0; i < NUM_PIECE_TYPES; i++)
		glist = g_list_append(glist, pieceTypeStr[i]);
	*lastPieceStr = '\0';	/* Don't update values */
	gtk_combo_set_popdown_strings(GTK_COMBO(pieceTypeCombo), glist);
	g_list_free(glist);

	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(pieceTypeCombo)->entry), pieceTypeStr[bd->rd->pieceType]);
	strcpy(lastPieceStr, pieceTypeStr[bd->rd->pieceType]);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Texture type:") ),
			FALSE, FALSE, 4 );

	textureTypeCombo = gtk_combo_new();
	gtk_combo_set_value_in_list(GTK_COMBO(textureTypeCombo), TRUE, FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(GTK_COMBO(textureTypeCombo)->entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(textureTypeCombo)->list), "selection-changed",
							GTK_SIGNAL_FUNC(textureChange), 0);
	gtk_box_pack_start( GTK_BOX( pwhbox ), textureTypeCombo, FALSE, FALSE, 0);

	glist = NULL;
	for (i = 0; i < NUM_TEXTURE_TYPES; i++)
		glist = g_list_append(glist, textureTypeStr[i]);
	*lastTextureStr = '\0';	/* Don't update values */
	gtk_combo_set_popdown_strings(GTK_COMBO(textureTypeCombo), glist);
	g_list_free(glist);

	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(textureTypeCombo)->entry), textureTypeStr[bd->rd->pieceTextureType]);
	strcpy(lastTextureStr, textureTypeStr[bd->rd->pieceTextureType]);

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
    gtk_box_pack_start(GTK_BOX(pwhbox),apwDiceColourBox[f], TRUE, TRUE, 4);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Pip colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start(GTK_BOX(pwhbox), gtk_colour_picker_new3d(&bd->rd->DiceDotMat[f], DF_FULL_ALPHA, TT_NONE), TRUE, TRUE, 4);

    gtk_signal_connect ( GTK_OBJECT( apwDieColour[ f ] ), "toggled",
                         GTK_SIGNAL_FUNC( DieColourChanged ), (void*)f);

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
		gtk_colour_picker_new3d(&bd->rd->CubeMat, DF_NO_ALPHA, TT_NONE), TRUE, TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Text colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->CubeNumberMat, DF_FULL_ALPHA, TT_NONE), TRUE, TRUE, 4 );

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
		gtk_colour_picker_new3d(&bd->rd->BaseMat, DF_NO_ALPHA, TT_GENERAL), TRUE, TRUE, 4 );

	pwBgTrays = gtk_check_button_new_with_label ("Show background in bear-off trays");
	gtk_tooltips_set_tip(ptt, pwBgTrays, ("If unset the bear-off trays will be drawn with the board colour"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwBgTrays, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwBgTrays), bd->rd->bgInTrays);
	gtk_signal_connect(GTK_OBJECT(pwBgTrays), "toggled", GTK_SIGNAL_FUNC(option_changed),0);// (GtkObject*) ( pwPreview + PI_BOARD ));

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("First\npoint colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->PointMat[0], DF_FULL_ALPHA, TT_GENERAL), TRUE, TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Second\npoint colour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->PointMat[1], DF_FULL_ALPHA, TT_GENERAL), TRUE, TRUE, 4 );

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
		gtk_colour_picker_new3d(&bd->rd->BoxMat, DF_FULL_ALPHA, TT_GENERAL), TRUE, TRUE, 4 );

	pwRoundedEdges = gtk_check_button_new_with_label (_("Rounded board edges"));
	gtk_tooltips_set_tip(ptt, pwRoundedEdges, _("Toggle rounded or square edges to the board"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwRoundedEdges, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwRoundedEdges), bd->rd->roundedEdges);
	gtk_signal_connect(GTK_OBJECT(pwRoundedEdges), "toggled", GTK_SIGNAL_FUNC(option_changed), 0);//(GtkObject*) ( pwPreview + PI_BORDER ));

    pwHinges = gtk_check_button_new_with_label( _("Show hinges") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHinges ), bd->rd->fHinges3d );
    gtk_box_pack_start( GTK_BOX( pw ), pwHinges, FALSE, FALSE, 0 );
    gtk_signal_connect_object( GTK_OBJECT( pwHinges ), "toggled",
			       GTK_SIGNAL_FUNC( HingeChanged ), 0);
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Hinge colour:") ),
			FALSE, FALSE, 4 );

	pmHingeCol = gtk_colour_picker_new3d(&bd->rd->HingeMat, DF_NO_ALPHA, TT_HINGE);
    gtk_widget_set_sensitive(pmHingeCol, bd->rd->fHinges3d);
    gtk_box_pack_start(GTK_BOX(pwhbox), pmHingeCol, TRUE, TRUE, 4);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Point number\ncolour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->PointNumberMat, DF_FULL_ALPHA, TT_NONE), TRUE, TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Background\ncolour:") ),
			FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
		gtk_colour_picker_new3d(&bd->rd->BackGroundMat, DF_NO_ALPHA, TT_GENERAL), TRUE, TRUE, 4 );

	return pwx;
}

#endif

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

    gtk_box_pack_start( GTK_BOX( pwhbox ),
			apwColour[ f ] = gtk_colour_picker_new(UpdatePreview, 0/*&pwPreview[PI_CHEQUERS0 + f]*/), TRUE,
			TRUE, 4 );
    
    gtk_colour_picker_set_has_opacity_control(GTK_COLOUR_PICKER(apwColour[f]), TRUE);
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwColour[ f ] ),
				   bd->rd->aarColour[ f ] );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );
	gtk_signal_connect_object( GTK_OBJECT( apadj[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ), 0);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
	gtk_signal_connect_object( GTK_OBJECT( apadjCoefficient[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Diffuse") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Specular") ), FALSE,
			FALSE, 4 );
	gtk_signal_connect_object( GTK_OBJECT( apadjExponent[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);
   
	if (f == 0)
	{
		padjRound = GTK_ADJUSTMENT( gtk_adjustment_new( 1.0 - bd->rd->rRound, 0, 1,
								0.1, 0.1, 0 ) );
		gtk_signal_connect_object( GTK_OBJECT( padjRound ), "value-changed",
					   GTK_SIGNAL_FUNC( UpdatePreview ), NULL );
		pwScale = gtk_hscale_new( padjRound );
#if GTK_CHECK_VERSION(2,0,0)
		gtk_widget_set_size_request( pwScale, 100, -1 );
#else
		gtk_widget_set_usize ( GTK_WIDGET ( pwScale ), 100, -1 );
#endif
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
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			apwDiceColour[ f ] = gtk_colour_picker_new(UpdatePreview,0),
			TRUE, TRUE, 4 );

    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwDiceColour[ f ] ),
				   bd->rd->aarDiceColour[ f ] );

    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    /*
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );
    */

    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjDiceCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
	gtk_signal_connect_object( GTK_OBJECT( apadjDiceCoefficient[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);
    
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
	gtk_signal_connect_object( GTK_OBJECT( apadjDiceExponent[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);

    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColourBox[ f ] ),
                               ! bd->rd->afDieColour[ f ] );

    /* colour of dot on dice */
    
    gtk_box_pack_start ( GTK_BOX( pw ),
			pwhbox = gtk_hbox_new( FALSE, 0 ), FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Pip colour:") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			apwDiceDotColour[ f ] = gtk_colour_picker_new(UpdatePreview,0),
			TRUE, TRUE, 4 );
    
    gtk_colour_picker_set_colour( 
        GTK_COLOUR_PICKER( apwDiceDotColour[ f ] ),
        bd->rd->aarDiceDotColour[ f ] );

    gtk_signal_connect ( GTK_OBJECT( apwDieColour[ f ] ), "toggled",
                         GTK_SIGNAL_FUNC( DieColourChanged ), (void*)f);

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
    
    gtk_box_pack_start ( GTK_BOX ( pwx ), 
                         pwCubeColour = gtk_colour_picker_new(UpdatePreview,0),
			 TRUE, TRUE, 0 );
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( pwCubeColour ),
				   bd->rd->arCubeColour );

    /* FIXME add cube text colour settings */

    return pw;

}

static GtkWidget *BoardPage( BoardData *bd ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i, j;
    GtkWidget *pwx;
    static char *asz[ 4 ] = {
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
	gtk_box_pack_start( GTK_BOX( pwhbox ),
			    apwBoard[ j ] = gtk_colour_picker_new(UpdatePreview,0),
				TRUE, TRUE, 4 );
	gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwBoard[ j ] ),
				      ar );

	gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Smooth") ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
				apadjBoard[ j ] ), TRUE, TRUE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Speckled") ),
			    FALSE, FALSE, 4 );
	gtk_signal_connect_object( GTK_OBJECT( apadjBoard[ j ] ),
				   "value-changed",
				   GTK_SIGNAL_FUNC( UpdatePreview ),0);
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
    static char *aszWood[] = {
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

    gtk_box_pack_start( GTK_BOX( pw ), pwWoodType = gtk_option_menu_new(),
			FALSE, FALSE, 4 );

    pwWoodMenu = gtk_menu_new();
    for( bw = 0; bw < WOOD_PAINT; bw++ ) 
	gtk_menu_shell_append( GTK_MENU_SHELL( pwWoodMenu ),
			       gtk_menu_item_new_with_label( 
                                  gettext ( aszWood[ bw ] ) ) );

    gtk_option_menu_set_menu( GTK_OPTION_MENU( pwWoodType ), pwWoodMenu );
    if( bd->rd->wt != WOOD_PAINT )
	gtk_option_menu_set_history( GTK_OPTION_MENU( pwWoodType ),
				     bd->rd->wt );

#if GTK_CHECK_VERSION(2,0,0)
    gtk_signal_connect_object( GTK_OBJECT( pwWoodType ), "changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);
#else
    gtk_signal_connect_object( GTK_OBJECT( pwWoodMenu ), "selection-done",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);
#endif
    
    gtk_box_pack_start( GTK_BOX( pw ),
			pwWoodF = gtk_radio_button_new_with_label_from_widget(
			    GTK_RADIO_BUTTON( pwWood ), _("Painted") ),
			FALSE, FALSE, 0 );

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->rd->wt != WOOD_PAINT ?
						     pwWood : pwWoodF ),
				  TRUE );
    
    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->rd->aanBoardColour[ 1 ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwBoard[ 1 ] =
			gtk_colour_picker_new(UpdatePreview,0),
			FALSE, FALSE, 0 );
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwBoard[ 1 ] ),
				   ar );

    pwHinges = gtk_check_button_new_with_label( _("Show hinges") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHinges ), bd->rd->fHinges );
    gtk_box_pack_start( GTK_BOX( pw ), pwHinges, FALSE, FALSE, 0 );
    gtk_signal_connect_object( GTK_OBJECT( pwHinges ), "toggled",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);

    gtk_signal_connect( GTK_OBJECT( pwWood ), "toggled",
			GTK_SIGNAL_FUNC( ToggleWood ), bd );
    gtk_signal_connect_object( GTK_OBJECT( pwWood ), "toggled",
			       GTK_SIGNAL_FUNC( UpdatePreview ),0);
    
    gtk_widget_set_sensitive( pwWoodType, bd->rd->wt != WOOD_PAINT );
    gtk_widget_set_sensitive( apwBoard[ 1 ], bd->rd->wt == WOOD_PAINT);
    
    return pwx;
}

static void BoardPrefsOK( GtkWidget *pw, GtkWidget *mainBoard ) {

	BoardData *bd = BOARD(mainBoard)->board_data;
    GetPrefs(&rdPrefs);
	
#if USE_BOARD3D
	redrawChange = FALSE;
	rdPrefs.quickDraw = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwQuickDraw));

	if (rdPrefs.fDisplayType == DT_3D)
	{
		/* Delete old objects */
		ClearTextures(bd);
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
	DisplayCorrectBoardType(bd);

	if (bd->rd->fDisplayType == DT_3D)
	{
		GetTextures(bd);
		preDraw3d(bd);
		SetupViewingVolume3d(bd);
		ShowFlag3d(bd);
		if (bd->diceShown == DICE_ON_BOARD)
			setDicePos(bd);	/* Make sure dice appear ok */
		RestrictiveRedraw();
	}
#endif
	board_create_pixmaps( mainBoard, bd );
	gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
	/* Make sure chequers correct below board */
	gtk_widget_queue_draw(bd->table);
}

void WorkOut2dLight(renderdata* prd)
{
    prd->arLight[ 2 ] = sinf( paElevation->value / 180 * M_PI );
    prd->arLight[ 0 ] = cosf( paAzimuth->value / 180 * M_PI ) *
    	sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] );
    prd->arLight[ 1 ] = sinf( paAzimuth->value / 180 * M_PI ) *
	    sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] );
}

static void LightChanged2d( GtkWidget *pwWidget, void* data )
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	if (!fUpdate)
		return;
	WorkOut2dLight(bd->rd);
	board_free_pixmaps( bd );
	board_create_pixmaps( pwPrevBoard, bd );

	UpdatePreview(0);
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
	if (bd->rd->fDisplayType == DT_2D)
#endif
	{
		board_free_pixmaps( bd );
		board_create_pixmaps( pwPrevBoard, bd );
	}
	UpdatePreview(0);
}

static void MoveIndicatorToggled( GtkWidget *pwWidget, void* data )
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	/* Update preview */
	if (!fUpdate)
		return;
	bd->rd->showMoveIndicator = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwMoveIndicator) );
#if USE_BOARD3D
	if (bd->rd->fDisplayType == DT_2D)
#endif
	{
		board_free_pixmaps( bd );
		board_create_pixmaps( pwPrevBoard, bd );
	}
	UpdatePreview(0);
}

#if USE_BOARD3D

void toggle_display_type(GtkWidget *widget, BoardData* bd)
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

	if (rdPrefs.fDisplayType == DT_3D)
	{
		/* Make sure 3d code is initialized */
		Init3d();

		updateDiceOccPos(bd);
	}			

	AddPages(bd, pwNotebook);
	gtk_widget_set_sensitive(pwTestPerformance, (rdPrefs.fDisplayType == DT_3D));

#if USE_BOARD3D
	DisplayCorrectBoardType(bd);
#endif
	/* Make sure everything is correctly sized */
	gtk_widget_queue_resize(gtk_widget_get_toplevel(pwNotebook));

	SetTitle();
}

void toggle_quick_draw(GtkWidget *widget, int init)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(pwShowShadows, !set);
	gtk_widget_set_sensitive(pwCloseBoard, !set);

	if (set)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwShowShadows), 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwCloseBoard), 0);
		if (init != -1)
			GTKShowWarning(WARN_QUICKDRAW_MODE);
	}
}

void toggle_show_shadows(GtkWidget *widget, int init)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(lightLab, set);
	gtk_widget_set_sensitive(pwDarkness, set);
	gtk_widget_set_sensitive(darkLab, set);
	if (set && init != -1)
		GTKShowWarning(WARN_SET_SHADOWS);
	
	option_changed(0, 0);
}

void toggle_planview(GtkWidget *widget, GtkWidget *pw)
{
	int set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(pwSkewFactor, !set);
	gtk_widget_set_sensitive(pwBoardAngle, !set);
	gtk_widget_set_sensitive(skewLab, !set);
	gtk_widget_set_sensitive(anglelab, !set);
	
	option_changed(0, 0);
}

void DoTestPerformance(GtkWidget *pw, GtkWidget* board)
{
	BoardData *bd = BOARD(board)->board_data;
	char str[255];
	char *msg;
	int fps;
	monitor m;

	if (!GetInputYN(_("Save settings and test 3d performance for 3 seconds?")))
		return;

	BoardPrefsOK(pw, board);

	while (gtk_events_pending())
	    gtk_main_iteration();

	SuspendInput(&m);
	fps = TestPerformance3d(bd);
	ResumeInput(&m);

	if (fps >= 30)
		msg = "3d Performance is very fast.\n";
	else if (fps >= 15)
		msg = "3d Performance is good.\n";
	else if (fps >= 10)
		msg = "3d Performance is ok.\n";
	else if (fps >= 5)
		msg = "3d Performance is poor.\n";
	else
		msg = "3d Performance is very poor.\n";

	sprintf(str, "%s\n(%d frames per second)\n", msg, fps);

	outputl(str);

	if (fps <= 5)
	{	/* Give some advice, hopefully to speed things up */
		if (bd->rd->showShadows)
			outputl("Disable shadows to improve performance");
		else if (!bd->rd->quickDraw)
			outputl("Try the quick draw option to improve performance");
		else
			outputl("The quick draw option will not change the result of this performance test");
	}
	outputx();
}

#endif

void Add2dLightOptions(GtkWidget* pwx, renderdata* prd)
{
    float rAzimuth, rElevation;
	GtkWidget* pScale;

    pwLightTable = gtk_table_new( 2, 2, FALSE );
    gtk_box_pack_start( GTK_BOX( pwx ), pwLightTable, FALSE, FALSE, 4 );
    
    gtk_table_attach( GTK_TABLE( pwLightTable ), gtk_label_new( _("Light azimuth") ),
		      0, 1, 0, 1, 0, 0, 4, 2 );
    gtk_table_attach( GTK_TABLE( pwLightTable ), gtk_label_new( _("Light elevation") ),
		      0, 1, 1, 2, 0, 0, 4, 2 );

    rElevation = asinf( prd->arLight[ 2 ] ) * 180 / M_PI;
	{
		float s = sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] );
		if (s == 0)
			rAzimuth = 0;
		else
		{
			float ac = acosf( prd->arLight[ 0 ] / s );
			if (ac == 0)
				rAzimuth = 0;
			else
				rAzimuth = ac * 180 / M_PI;
		}
	}
    if( prd->arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;
    
    paAzimuth = GTK_ADJUSTMENT( gtk_adjustment_new( rAzimuth, 0.0, 360.0, 1.0,
						    30.0, 0.0 ) );
	pScale = gtk_hscale_new( paAzimuth );
	gtk_widget_set_usize(pScale, 150, -1);
    gtk_table_attach( GTK_TABLE( pwLightTable ), pScale,
		      1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
	gtk_signal_connect_object( GTK_OBJECT( paAzimuth ), "value-changed",
			       GTK_SIGNAL_FUNC( LightChanged2d ), NULL );

    paElevation = GTK_ADJUSTMENT( gtk_adjustment_new( rElevation, 0.0, 90.0,
						      1.0, 10.0, 0.0 ) );
    gtk_table_attach( GTK_TABLE( pwLightTable ), gtk_hscale_new( paElevation ),
		      1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
	gtk_signal_connect_object( GTK_OBJECT( paElevation ), "value-changed",
			       GTK_SIGNAL_FUNC( LightChanged2d ), NULL );

	/* FIXME add settings for ambient light */
}

GtkWidget *LightingPage(BoardData *bd)
{
    GtkWidget *dtBox, *pwx;
#if USE_BOARD3D
	GtkWidget *vbox, *vbox2, *frameBox, *hBox, *lab,
		*pwLightPosX, *pwLightLevelAmbient, *pwLightLevelDiffuse, *pwLightLevelSpecular,
		*pwLightPosY, *pwLightPosZ;
#endif

	pwx = gtk_hbox_new ( FALSE, 0);
	
	dtBox = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_start(GTK_BOX(pwx), dtBox, FALSE, FALSE, 0);

#if USE_BOARD3D
	if (rdPrefs.fDisplayType == DT_3D)
	{
		dtBox = gtk_vbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(pwx), dtBox, FALSE, FALSE, 0);

		dtLightSourceFrame = gtk_frame_new("Light Source Type");
		gtk_container_set_border_width (GTK_CONTAINER (dtLightSourceFrame), 4);
		gtk_box_pack_start(GTK_BOX(dtBox), dtLightSourceFrame, FALSE, FALSE, 0);
		
		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (dtLightSourceFrame), vbox);
		
		pwLightSource = gtk_radio_button_new_with_label (NULL, "Positional");
		gtk_tooltips_set_tip(ptt, pwLightSource, _("This is a fixed light source, like a lamp"), 0);
		gtk_box_pack_start (GTK_BOX (vbox), pwLightSource, FALSE, FALSE, 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwLightSource), (bd->rd->lightType == LT_POSITIONAL));
		
		pwDirectionalSource = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(pwLightSource), "Directional");
		gtk_tooltips_set_tip(ptt, pwDirectionalSource, _("This is a light direction, like the sun"), 0);
		gtk_box_pack_start (GTK_BOX (vbox), pwDirectionalSource, FALSE, FALSE, 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwDirectionalSource), (bd->rd->lightType == LT_DIRECTIONAL));
		gtk_signal_connect(GTK_OBJECT(pwDirectionalSource), "toggled", GTK_SIGNAL_FUNC(option_changed), bd);

		dtLightPositionFrame = gtk_frame_new("Light Position");
		gtk_container_set_border_width (GTK_CONTAINER (dtLightPositionFrame), 4);
		gtk_box_pack_start(GTK_BOX(dtBox), dtLightPositionFrame, FALSE, FALSE, 0);

		frameBox = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (dtLightPositionFrame), frameBox);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new("Left");
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightPosX = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightPos[0],
								-1.5, 4, .1, 1, 0));
		gtk_signal_connect_object( GTK_OBJECT( padjLightPosX ), "value-changed",
					GTK_SIGNAL_FUNC( option_changed ), NULL );
		pwLightPosX = gtk_hscale_new(padjLightPosX);
		gtk_scale_set_draw_value(GTK_SCALE(pwLightPosX), FALSE);
		gtk_widget_set_usize(pwLightPosX, 150, -1);
		gtk_box_pack_start(GTK_BOX(hBox), pwLightPosX, TRUE, TRUE, 0);

		lab = gtk_label_new("Right");
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		hBox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		vbox2 = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hBox), vbox2, FALSE, FALSE, 0);

		lab = gtk_label_new("Bottom");
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		padjLightPosY = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightPos[1],
								-1.5, 4, .1, 1, 0));
		gtk_signal_connect_object( GTK_OBJECT( padjLightPosY ), "value-changed",
					GTK_SIGNAL_FUNC( option_changed ), NULL );
		pwLightPosY = gtk_vscale_new(padjLightPosY);
		gtk_scale_set_draw_value(GTK_SCALE(pwLightPosY), FALSE);

		gtk_widget_set_usize(pwLightPosY, -1, 70);
		gtk_box_pack_start(GTK_BOX(vbox2), pwLightPosY, TRUE, TRUE, 0);

		lab = gtk_label_new("Top");
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		vbox2 = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hBox), vbox2, FALSE, FALSE, 0);

		lab = gtk_label_new("Low");
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		padjLightPosZ = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightPos[2],
								.5, 5, .1, 1, 0));
		gtk_signal_connect_object( GTK_OBJECT( padjLightPosZ ), "value-changed",
					GTK_SIGNAL_FUNC( option_changed ), NULL );
		pwLightPosZ = gtk_vscale_new(padjLightPosZ);
		gtk_scale_set_draw_value(GTK_SCALE(pwLightPosZ), FALSE);

		gtk_box_pack_start(GTK_BOX(vbox2), pwLightPosZ, TRUE, TRUE, 0);

		lab = gtk_label_new("High");
		gtk_box_pack_start(GTK_BOX(vbox2), lab, FALSE, FALSE, 0);

		dtLightLevelsFrame = gtk_frame_new("Light Levels");
		gtk_container_set_border_width(GTK_CONTAINER(dtLightLevelsFrame), 4);
		gtk_box_pack_start(GTK_BOX(dtBox), dtLightLevelsFrame, FALSE, FALSE, 0);

		frameBox = gtk_vbox_new(FALSE, 0);
		gtk_container_add (GTK_CONTAINER(dtLightLevelsFrame), frameBox);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new("Ambient");
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightLevelAmbient = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightLevels[0],
								0, 100, 1, 10, 0));
		gtk_signal_connect_object( GTK_OBJECT( padjLightLevelAmbient ), "value-changed",
					GTK_SIGNAL_FUNC( option_changed ), NULL );
		pwLightLevelAmbient = gtk_hscale_new(padjLightLevelAmbient);
		gtk_tooltips_set_tip(ptt, pwLightLevelAmbient, _("Ambient light specifies the general light level"), 0);
		gtk_box_pack_start(GTK_BOX(hBox), pwLightLevelAmbient, TRUE, TRUE, 0);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new("Diffuse");
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightLevelDiffuse = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightLevels[1],
								0, 100, 1, 10, 0));
		gtk_signal_connect_object( GTK_OBJECT( padjLightLevelDiffuse ), "value-changed",
					GTK_SIGNAL_FUNC( option_changed ), NULL );
		pwLightLevelDiffuse = gtk_hscale_new(padjLightLevelDiffuse);
		gtk_tooltips_set_tip(ptt, pwLightLevelDiffuse, _("Diffuse light specifies light from the light source"), 0);
		gtk_box_pack_start(GTK_BOX(hBox), pwLightLevelDiffuse, TRUE, TRUE, 0);

		hBox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start(GTK_BOX(frameBox), hBox, FALSE, FALSE, 0);

		lab = gtk_label_new("Specular");
		gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

		padjLightLevelSpecular = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->lightLevels[2],
								0, 100, 1, 10, 0));
		gtk_signal_connect_object( GTK_OBJECT( padjLightLevelSpecular ), "value-changed",
					GTK_SIGNAL_FUNC( option_changed ), NULL );
		pwLightLevelSpecular = gtk_hscale_new(padjLightLevelSpecular);
		gtk_tooltips_set_tip(ptt, pwLightLevelSpecular, _("Specular light is reflected light off shiny surfaces"), 0);
		gtk_box_pack_start(GTK_BOX(hBox), pwLightLevelSpecular, TRUE, TRUE, 0);
	}
	else
#endif
		Add2dLightOptions(dtBox, bd->rd);

	gtk_widget_show_all(pwx);

	return pwx;
}

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
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwBoardType), (bd->rd->fDisplayType == DT_2D));
	
	button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(pwBoardType), _("3d Board"));
	gtk_box_pack_start (GTK_BOX (dtBox), button, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), (bd->rd->fDisplayType == DT_3D));
	gtk_signal_connect(GTK_OBJECT(button), "toggled", GTK_SIGNAL_FUNC(toggle_display_type), bd);
#endif

    pwLabels = gtk_check_button_new_with_label( _("Numbered point labels") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwLabels ), bd->rd->fLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwLabels, FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pwLabels, _("Show or hide point numbers"), NULL );

    pwDynamicLabels = gtk_check_button_new_with_label( _("Dynamic labels") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwDynamicLabels ), 
                                  bd->rd->fDynamicLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwDynamicLabels, FALSE, FALSE, 0 );
    gtk_signal_connect( GTK_OBJECT( pwDynamicLabels ), "toggled",
					GTK_SIGNAL_FUNC( LabelsToggled ), 0 );

    gtk_signal_connect( GTK_OBJECT( pwLabels ), "toggled",
                        GTK_SIGNAL_FUNC( LabelsToggled ), 0);
    LabelsToggled(0, 0);
    
    gtk_tooltips_set_tip( ptt, pwDynamicLabels,
			  _("Update the labels so they are correct "
                            "for the player on roll"), NULL );

	pwMoveIndicator = gtk_check_button_new_with_label (_("Show move indicator"));
	gtk_tooltips_set_tip(ptt, pwMoveIndicator, _("Show or hide arrow indicating who is moving"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwMoveIndicator, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwMoveIndicator), bd->rd->showMoveIndicator);
    gtk_signal_connect(GTK_OBJECT(pwMoveIndicator), "toggled", GTK_SIGNAL_FUNC(MoveIndicatorToggled), 0);
#if USE_BOARD3D
	pwvbox = gtk_vbox_new( FALSE, 0 );
	gtk_box_pack_start(GTK_BOX( pwx ), pwvbox, FALSE, FALSE, 0);

	frame3dOptions = gtk_frame_new(_("3d Options"));
	gtk_container_set_border_width (GTK_CONTAINER (frame3dOptions), 4);
	gtk_box_pack_start(GTK_BOX(pwvbox), frame3dOptions, FALSE, FALSE, 0);

    pw = gtk_vbox_new( FALSE, 0 );
	gtk_container_add (GTK_CONTAINER (frame3dOptions), pw);
	
	pwShowShadows = gtk_check_button_new_with_label (_("Show shadows"));
	gtk_tooltips_set_tip(ptt, pwShowShadows, _("Display shadows, this option requires a fast graphics card"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwShowShadows, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwShowShadows), bd->rd->showShadows);
	gtk_signal_connect(GTK_OBJECT(pwShowShadows), "toggled", GTK_SIGNAL_FUNC(toggle_show_shadows), NULL);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	lightLab = gtk_label_new(_("light"));
	gtk_box_pack_start(GTK_BOX(hBox), lightLab, FALSE, FALSE, 0);

	padjDarkness = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->shadowDarkness,
						     3, 100, 1, 10, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjDarkness ), "value-changed",
			       GTK_SIGNAL_FUNC( option_changed ), NULL );
	pwDarkness = gtk_hscale_new(padjDarkness);
	gtk_tooltips_set_tip(ptt, pwDarkness, _("Vary the darkness of the shadows"), 0);
	gtk_scale_set_draw_value(GTK_SCALE(pwDarkness), FALSE);
	gtk_box_pack_start(GTK_BOX(hBox), pwDarkness, TRUE, TRUE, 0);

	darkLab = gtk_label_new(_("dark"));
	gtk_box_pack_start(GTK_BOX(hBox), darkLab, FALSE, FALSE, 0);

	toggle_show_shadows(pwShowShadows, -1);

	pwAnimateRoll = gtk_check_button_new_with_label (_("Animate dice rolls"));
	gtk_tooltips_set_tip(ptt, pwAnimateRoll, _("Dice rolls will shake across board"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwAnimateRoll, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwAnimateRoll), bd->rd->animateRoll);
	
	pwAnimateFlag = gtk_check_button_new_with_label (_("Animate resignation flag"));
	gtk_tooltips_set_tip(ptt, pwAnimateFlag, _("Waves resignation flag"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwAnimateFlag, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwAnimateFlag), bd->rd->animateFlag);

	pwCloseBoard = gtk_check_button_new_with_label (_("Close board on exit"));
	gtk_tooltips_set_tip(ptt, pwCloseBoard, _("When you quit gnubg, the board will close"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwCloseBoard, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwCloseBoard), bd->rd->closeBoardOnExit );

	pwev = gtk_event_box_new();
	gtk_box_pack_start(GTK_BOX(pw), pwev, FALSE, FALSE, 0);
	pwhbox = gtk_hbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(pwev), pwhbox);


	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	anglelab = gtk_label_new(_("Board angle: "));
	gtk_box_pack_start(GTK_BOX(hBox), anglelab, FALSE, FALSE, 0);

	padjBoardAngle = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->boardAngle,
										0, 60, 1, 10, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjBoardAngle ), "value-changed",
			       GTK_SIGNAL_FUNC( option_changed ), NULL );
	pwBoardAngle = gtk_hscale_new(padjBoardAngle);
	gtk_tooltips_set_tip(ptt, pwBoardAngle, _("Vary the angle the board is tilted at"), 0);
	gtk_scale_set_digits( GTK_SCALE( pwBoardAngle ), 0 );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_widget_set_size_request( pwBoardAngle, 100, -1 );
#else
    gtk_widget_set_usize ( GTK_WIDGET ( pwBoardAngle ), 100, -1 );
#endif
	gtk_box_pack_start(GTK_BOX(hBox), pwBoardAngle, FALSE, FALSE, 0);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	skewLab = gtk_label_new("FOV skew: ");
	gtk_box_pack_start(GTK_BOX(hBox), skewLab, FALSE, FALSE, 0);

	padjSkewFactor = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->skewFactor,
										0, 100, 1, 10, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjSkewFactor ), "value-changed",
			       GTK_SIGNAL_FUNC( option_changed ), NULL );
	pwSkewFactor = gtk_hscale_new(padjSkewFactor);
#if GTK_CHECK_VERSION(2,0,0)
    gtk_widget_set_size_request( pwSkewFactor, 100, -1 );
#else
    gtk_widget_set_usize ( GTK_WIDGET ( pwSkewFactor ), 100, -1 );
#endif
	gtk_tooltips_set_tip(ptt, pwSkewFactor, "Vary the field-of-view of the 3d display", 0);
	gtk_scale_set_digits( GTK_SCALE( pwSkewFactor ), 0 );
	gtk_box_pack_start(GTK_BOX(hBox), pwSkewFactor, FALSE, FALSE, 0);

	pwPlanView = gtk_check_button_new_with_label (_("Plan view"));
	gtk_tooltips_set_tip(ptt, pwPlanView, "Display the 3d board with a 2d overhead view", 0);
	gtk_box_pack_start (GTK_BOX (pw), pwPlanView, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwPlanView), bd->rd->planView);
	gtk_signal_connect(GTK_OBJECT(pwPlanView), "toggled", GTK_SIGNAL_FUNC(toggle_planview), NULL);
	toggle_planview(pwPlanView, 0);

	lab = gtk_label_new(_("Curve accuracy"));
	gtk_box_pack_start (GTK_BOX (pw), lab, FALSE, FALSE, 0);

	hBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (pw), hBox, FALSE, FALSE, 0);

	lab = gtk_label_new(_("low"));
	gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

	padjAccuracy = GTK_ADJUSTMENT(gtk_adjustment_new(bd->rd->curveAccuracy,
						     8, 60, 4, 12, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjAccuracy ), "value-changed",
			       GTK_SIGNAL_FUNC( option_changed ), NULL );
	pwAccuracy = gtk_hscale_new(padjAccuracy);
	gtk_tooltips_set_tip(ptt, pwAccuracy, _("Change how accurately curves are drawn."
		" If performance is slow try lowering this value."
		" Increasing this value will only have an effect on large displays"), 0);
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
	gtk_signal_connect_object( GTK_OBJECT( padjDiceSize ), "value-changed",
			       GTK_SIGNAL_FUNC( DiceSizeChanged ), 0 );
	pwDiceSize = gtk_hscale_new(padjDiceSize);
	gtk_tooltips_set_tip(ptt, pwDiceSize, _("Vary the size of the dice"), 0);
	gtk_scale_set_draw_value(GTK_SCALE(pwDiceSize), FALSE);
	gtk_box_pack_start(GTK_BOX(hBox), pwDiceSize, TRUE, TRUE, 0);

	lab = gtk_label_new(_("large"));
	gtk_box_pack_start(GTK_BOX(hBox), lab, FALSE, FALSE, 0);

	pwQuickDraw = gtk_check_button_new_with_label (_("Quick drawing"));
	gtk_tooltips_set_tip(ptt, pwQuickDraw, _("Fast drawing option to improve performance"), 0);
	gtk_box_pack_start (GTK_BOX (pw), pwQuickDraw, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwQuickDraw), bd->rd->quickDraw);
	gtk_signal_connect(GTK_OBJECT(pwQuickDraw), "toggled", GTK_SIGNAL_FUNC(toggle_quick_draw), NULL);
	toggle_quick_draw(pwQuickDraw, -1);

	pwTestPerformance = gtk_button_new_with_label(_("Test performance"));
	gtk_widget_set_sensitive(pwTestPerformance, (bd->rd->fDisplayType == DT_3D));
	gtk_box_pack_start(GTK_BOX(pwvbox), pwTestPerformance, FALSE, FALSE, 4);
	gtk_signal_connect(GTK_OBJECT(pwTestPerformance), "clicked",
				   GTK_SIGNAL_FUNC(DoTestPerformance), bdMain);

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
  char *apch[ 2 ];
  gchar *sz, *pch;
  renderdata newPrefs;
#if USE_BOARD3D
  BoardData *bd = BOARD(pwPrevBoard)->board_data;
#endif

  fUpdate = FALSE;

#if USE_BOARD3D
  if (rdPrefs.fDisplayType == DT_3D)
    ClearTextures(bd);
#endif

  newPrefs = rdPrefs;

  pch = sz = g_strdup ( pbdeSelected->szBoardDesign );
  while( ParseKeyValue( &sz, apch ) ) 
    RenderPreferencesParam( &newPrefs, apch[ 0 ], apch[ 1 ] );

  g_free ( pch );

#if USE_BOARD3D

	/* Set only 2D or 3D options */
	if (rdPrefs.fDisplayType == DT_3D)
	{
		Set3dSettings(&rdPrefs, &newPrefs);
		GetTextures(bd);

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

		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(pieceTypeCombo)->entry), pieceTypeStr[newPrefs.pieceType]);
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(textureTypeCombo)->entry), textureTypeStr[newPrefs.pieceTextureType]);

		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwRoundedEdges ),
									newPrefs.roundedEdges );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwBgTrays ),
									newPrefs.bgInTrays );

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
			gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( apwColour[ i ] ),
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

			gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwDiceColour[ i ] ),
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

			gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( apwDiceDotColour[ i ] ), 
											newPrefs.aarDiceDotColour[ i ] );

		}

		/* cube colour */

		gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( pwCubeColour ), 
										newPrefs.arCubeColour );

		/* board + points */

		for ( i = 0; i < 4; ++i ) {

			if ( i != 1 ) 
			gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjBoard[ i ] ),
										newPrefs.aSpeckle[ i ] / 128.0 );

			for ( j = 0; j < 3; j++ )
			ar[ j ] = newPrefs.aanBoardColour[ i ][ j ] / 255.0;

			gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( apwBoard[ i ]),
											ar );
		}

		/* board, border, and points */

		gtk_option_menu_set_history( GTK_OPTION_MENU( pwWoodType ), newPrefs.wt );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( newPrefs.wt != WOOD_PAINT ?
														pwWood : pwWoodF ),
										TRUE );

		gtk_widget_set_sensitive( pwWoodType, newPrefs.wt != WOOD_PAINT );
		gtk_widget_set_sensitive( apwBoard[ 1 ], newPrefs.wt == WOOD_PAINT);

		/* round */

		gtk_adjustment_set_value ( GTK_ADJUSTMENT ( padjRound ),
									1.0f - newPrefs.rRound );

		/* light */

		rElevation = asinf( newPrefs.arLight[ 2 ] ) * 180 / M_PI;
			if ( fabs ( newPrefs.arLight[ 2 ] - 1.0f ) < 1e-5 ) 
			rAzimuth = 0.0;
			else
			rAzimuth = 
				acosf( newPrefs.arLight[ 0 ] / sqrt( 1.0 - newPrefs.arLight[ 2 ] *
												newPrefs.arLight[ 2 ] ) ) * 180 / M_PI;
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
WriteDesign ( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;
  FILE *pf = user_data;

  if ( ! pbde )
    return;

  if ( ! pbde->fDeletable )
    /* predefined board design */
    return;

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
DesignSave ( GtkWidget *pw, gpointer data ) {

  gchar *szFile;
  FILE *pf;
  GList **pplBoardDesigns = (GList **) data;
  time_t t;

  szFile = g_strdup_printf ( "%s/.gnubg/boards.xml", szHomeDirectory );
  BackupFile ( szFile );

  if ( ! ( pf = fopen ( szFile, "w+" ) ) ) {
    outputerr ( szFile );
    g_free ( szFile );
    return;
  }

  fputs ( "<?xml version=\"1.0\" encoding=\"" GNUBG_CHARSET "\"?>\n"
          "\n"
          "<!--\n"
          "\n"
          "    $HOME/.gnubg/boards.xml\n"
          "       generated by GNU Backgammon " VERSION "\n", pf );
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

  g_list_foreach ( *pplBoardDesigns, WriteDesign, pf );

  fputs ( "</board-designs>\n", pf );

  fclose ( pf );
  
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
                           DT_QUESTION,
                           GTK_SIGNAL_FUNC( DesignAddOK ), pbde );

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
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAddTitle, 
                       FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignAddTitle ), "changed",
                       GTK_SIGNAL_FUNC ( DesignAddChanged ), pwDialog );

  /* author */

  pwhbox = gtk_hbox_new ( FALSE, 4 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );


  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Author of new design:") ),
                       FALSE, FALSE, 4 );

  pwDesignAddAuthor = gtk_entry_new ();
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAddAuthor, 
                       FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignAddAuthor ), "changed",
                       GTK_SIGNAL_FUNC ( DesignAddChanged ), pwDialog );

  /* show dialog */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );

  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
  
  gtk_widget_grab_focus( pwDesignAddTitle );
  gtk_widget_show_all( pwDialog );

  DesignAddChanged ( NULL, pwDialog );
  
  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();
}

void WriteDesignString(boarddesign *pbde, renderdata *prd)
{
  float rElevation;
  float rAzimuth;
  char szTemp[2048];

  rElevation = asinf( prd->arLight[ 2 ] ) * 180 / M_PI;
  rAzimuth = ( fabs ( prd->arLight[ 2 ] - 1.0f ) < 1e-5 ) ? 0.0f : 
    acosf( prd->arLight[ 0 ] / sqrt( 1.0 - prd->arLight[ 2 ] *
                                    prd->arLight[ 2 ] ) ) * 180 / M_PI;
  if( prd->arLight[ 1 ] < 0 )
    rAzimuth = 360 - rAzimuth;

  PushLocale( "C" );

	sprintf(szTemp,
            "\n"
            "         board=#%02X%02X%02X;%0.2f\n"
            "         border=#%02X%02X%02X\n"
            "         wood=%s\n"
            "         hinges=%c\n"
            "         light=%0.0f;%0.0f\n"
            "         shape=%0.1f\n"
            "         chequers0=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f\n"
            "         chequers1=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f\n"
            "         dice0=#%02X%02X%02X;%0.2f;%0.2f;%c\n"
            "         dice1=#%02X%02X%02X;%0.2f;%0.2f;%c\n"
            "         dot0=#%02X%02X%02X\n"
            "         dot1=#%02X%02X%02X\n"
            "         cube=#%02X%02X%02X\n"
            "         points0=#%02X%02X%02X;%0.2f\n"
            "         points1=#%02X%02X%02X;%0.2f\n"
#if USE_BOARD3D
            "         hinges3d=%c\n"
            "         piecetype=%d\n"
            "         piecetexturetype=%d\n"
			"         roundededges=%c\n"
			"         bgintrays=%c\n"
            "         lighttype=%c\n"
            "         lightposx=%f lightposy=%f lightposz=%f\n"
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
#endif
            "\n",
            /* board */
            prd->aanBoardColour[ 0 ][ 0 ], prd->aanBoardColour[ 0 ][ 1 ], 
            prd->aanBoardColour[ 0 ][ 2 ], prd->aSpeckle[ 0 ] / 128.0f,
            /* border */
            prd->aanBoardColour[ 1 ][ 0 ], prd->aanBoardColour[ 1 ][ 1 ], 
            prd->aanBoardColour[ 1 ][ 2 ],
            /* wood ... */
            aszWoodName[ prd->wt ],
            prd->fHinges ? 'y' : 'n',
            rAzimuth, rElevation, 1.0 - prd->rRound,
             /* chequers0 */
             (int) ( prd->aarColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarColour[ 0 ][ 2 ] * 0xFF ), 
             prd->aarColour[ 0 ][ 3 ], prd->arRefraction[ 0 ], 
             prd->arCoefficient[ 0 ], prd->arExponent[ 0 ],
             /* chequers1 */
	     (int) ( prd->aarColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarColour[ 1 ][ 2 ] * 0xFF ), 
             prd->aarColour[ 1 ][ 3 ], prd->arRefraction[ 1 ], 
             prd->arCoefficient[ 1 ], prd->arExponent[ 1 ],
             /* dice0 */
             (int) ( prd->aarDiceColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceColour[ 0 ][ 2 ] * 0xFF ), 
             prd->arDiceCoefficient[ 0 ], prd->arDiceExponent[ 0 ],
             prd->afDieColour[ 0 ] ? 'y' : 'n',
             /* dice1 */
	     (int) ( prd->aarDiceColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceColour[ 1 ][ 2 ] * 0xFF ), 
             prd->arDiceCoefficient[ 1 ], prd->arDiceExponent[ 1 ],
             prd->afDieColour[ 1 ] ? 'y' : 'n',
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
	     (int) ( prd->arCubeColour[ 2 ] * 0xFF ), 
             /* points0 */
	     prd->aanBoardColour[ 2 ][ 0 ], prd->aanBoardColour[ 2 ][ 1 ], 
	     prd->aanBoardColour[ 2 ][ 2 ], prd->aSpeckle[ 2 ] / 128.0f,
             /* points1 */
	     prd->aanBoardColour[ 3 ][ 0 ], prd->aanBoardColour[ 3 ][ 1 ], 
	     prd->aanBoardColour[ 3 ][ 2 ], prd->aSpeckle[ 3 ] / 128.0f
#if USE_BOARD3D
			, 
            prd->fHinges3d ? 'y' : 'n',
			prd->pieceType,
			prd->pieceTextureType,
			prd->roundedEdges ? 'y' : 'n',
			prd->bgInTrays ? 'y' : 'n',
			prd->lightType == LT_POSITIONAL ? 'p' : 'd',
			prd->lightPos[0], prd->lightPos[1], prd->lightPos[2],
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
			WriteMaterial(&prd->BackGroundMat)
#endif
	);
    pbde->szBoardDesign = g_malloc(strlen(szTemp) + 1);
	strcpy(pbde->szBoardDesign, szTemp);

	PopLocale();
}

void ShowSelectedRow()
{
	int row = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ), pbdeSelected);
	if( gtk_clist_row_is_visible(GTK_CLIST( pwDesignList ), row) == GTK_VISIBILITY_NONE )
		gtk_clist_moveto(GTK_CLIST( pwDesignList ), row, 0, 1, 0 );
}

#if USE_BOARD3D

void Set2dColour(double newcol[4], Material* pMat)
{
	newcol[0] = (pMat->ambientColour[0] + pMat->diffuseColour[0]) / 2;
	newcol[1] = (pMat->ambientColour[1] + pMat->diffuseColour[1]) / 2;
	newcol[2] = (pMat->ambientColour[2] + pMat->diffuseColour[2]) / 2;
	newcol[3] = (pMat->ambientColour[3] + pMat->diffuseColour[3]) / 2;
}

void Set2dColourChar(unsigned char newcol[4], Material* pMat)
{
	newcol[0] = ((pMat->ambientColour[0] + pMat->diffuseColour[0]) / 2) * 255;
	newcol[1] = ((pMat->ambientColour[1] + pMat->diffuseColour[1]) / 2) * 255;
	newcol[2] = ((pMat->ambientColour[2] + pMat->diffuseColour[2]) / 2) * 255;
	newcol[3] = ((pMat->ambientColour[3] + pMat->diffuseColour[3]) / 2) * 255;
}

void Set3dColour(Material* pMat, double col[4])
{
	pMat->ambientColour[0] = pMat->diffuseColour[0] = col[0];
	pMat->ambientColour[1] = pMat->diffuseColour[1] = col[1];
	pMat->ambientColour[2] = pMat->diffuseColour[2] = col[2];
	pMat->ambientColour[3] = pMat->diffuseColour[3] = 1;
}

void Set3dColourChar(Material* pMat, unsigned char col[4])
{
	pMat->ambientColour[0] = pMat->diffuseColour[0] = ((float)col[0]) / 255;
	pMat->ambientColour[1] = pMat->diffuseColour[1] = ((float)col[1]) / 255;
	pMat->ambientColour[2] = pMat->diffuseColour[2] = ((float)col[2]) / 255;
	pMat->ambientColour[3] = pMat->diffuseColour[3] = 1;
}

void CopyNewSettingsToOtherDimension(renderdata* prd)
{
	if (prd->fDisplayType == DT_3D)
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

  if ( ! ( pbde = (boarddesign *) malloc ( sizeof ( boarddesign ) ) ) ) {
    outputerr ( "allocate boarddesign" );
    return;
  }

  /* name and author of board */

  pbde->szTitle = pbde->szAuthor = pbde->szBoardDesign = NULL;

  DesignAddTitle ( pbde );
  if ( ! pbde->szTitle || ! pbde->szAuthor ) {
    free ( pbde );
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
	if (rdPrefs.fDisplayType == DT_3D)
		sprintf(prompt, _("Permently overwrite 3d settings for design %s?"), pbdeSelected->szTitle);
	else
		sprintf(prompt, _("Permently overwrite 2d settings for design %s?"), pbdeSelected->szTitle);
#else
	sprintf(prompt, _("Permently overwrite settings for design %s?"), pbdeSelected->szTitle);
#endif
	if (!GetInputYN(prompt))
		return;

	gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUpdate ), FALSE );

#if USE_BOARD3D
	if (rdPrefs.fDisplayType == DT_3D)
	{
		char *apch[ 2 ];
		gchar *sz, *pch;
		/* Get current (2d) settings for design */
		newPrefs = rdPrefs;
		pch = sz = g_strdup ( pbdeSelected->szBoardDesign );
		while( ParseKeyValue( &sz, apch ) ) 
			RenderPreferencesParam( &newPrefs, apch[ 0 ], apch[ 1 ] );
		g_free ( pch );

		/* Overwrite 3d settings with current values */
		Set3dSettings(&newPrefs, &rdPrefs);
	}
	else
#endif
	{
		char *apch[ 2 ];
		gchar *sz, *pch;
		/* Get current (3d) design settings */
		renderdata designPrefs;
		designPrefs = rdPrefs;
		pch = sz = g_strdup ( pbdeSelected->szBoardDesign );
		while( ParseKeyValue( &sz, apch ) ) 
			RenderPreferencesParam( &designPrefs, apch[ 0 ], apch[ 1 ] );
		g_free ( pch );

		/* Copy current (2d) settings */
		GetPrefs(&rdPrefs);
		newPrefs = rdPrefs;

#if USE_BOARD3D
		/* Overwrite 3d design settings */
		Set3dSettings(&newPrefs, &designPrefs);
#endif
	}
	/* Save updated design */
	WriteDesignString(pbdeSelected, &newPrefs);
	DesignSave(pw, data);

	SetTitle();
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

    if (GTK_WIDGET_IS_SENSITIVE(pwDesignAdd) &&
    	!GetInputYN(_("Select new design and lose current changes?")))
    	return;

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

  g_list_foreach ( *pplBoardDesigns, AddDesignRow, pwDesignList );
  gtk_container_add ( GTK_CONTAINER ( pwScrolled ), pwDesignList );

  gtk_signal_connect( GTK_OBJECT( pwDesignList ), "select-row",
                      GTK_SIGNAL_FUNC( DesignSelect ), NULL );
  gtk_signal_connect( GTK_OBJECT( pwDesignList ), "unselect-row",
                      GTK_SIGNAL_FUNC( DesignUnselect ), NULL );

  /* button: use design */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwPage ), pwhbox, FALSE, FALSE, 4 );

  /* 
   * buttons 
   */

  /* add current design */

  pwDesignAdd = gtk_button_new_with_label ( _("Add current design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAdd, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignAdd ), "clicked",
                       GTK_SIGNAL_FUNC ( DesignAdd ), pplBoardDesigns );

  /* remove design */

  pwDesignRemove = gtk_button_new_with_label ( _("Remove design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignRemove, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignRemove ), "clicked",
                       GTK_SIGNAL_FUNC ( RemoveDesign ), pplBoardDesigns );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );

  /* update design */

  pwDesignUpdate = gtk_button_new_with_label ( _("Update design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignUpdate, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignUpdate ), "clicked",
                     GTK_SIGNAL_FUNC ( UpdateDesign ), pplBoardDesigns );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUpdate ), FALSE );


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

	gtk_main_quit();
}

static void GetPrefs ( renderdata* prd ) {

    int i, j;
    gdouble ar[ 4 ];

#if USE_BOARD3D
	prd->fDisplayType = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwBoardType ) ) ? DT_2D : DT_3D;
	if (rdPrefs.fDisplayType == DT_3D)
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
		prd->shadowDarkness = padjDarkness->value;
		/* Darkness as percentage of ambient light */
		prd->dimness = ((prd->lightLevels[1] / 100.0f) * (100 - prd->shadowDarkness)) / 100;

		prd->animateRoll = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwAnimateRoll));
		prd->animateFlag = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwAnimateFlag));
		prd->closeBoardOnExit = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwCloseBoard));

		newCurveAccuracy = padjAccuracy->value;
		newCurveAccuracy -= (newCurveAccuracy % 4);
		prd->curveAccuracy = newCurveAccuracy;

		prd->lightType = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwLightSource) ) ? LT_POSITIONAL : LT_DIRECTIONAL;
		prd->lightPos[0] = padjLightPosX->value;
		prd->lightPos[1] = padjLightPosY->value;
		prd->lightPos[2] = padjLightPosZ->value;
		prd->lightLevels[0] = padjLightLevelAmbient->value;
		prd->lightLevels[1] = padjLightLevelDiffuse->value;
		prd->lightLevels[2] = padjLightLevelSpecular->value;

		prd->boardAngle = padjBoardAngle->value;
		prd->skewFactor = padjSkewFactor->value;
		prd->pieceType = getPieceType(lastPieceStr);
		prd->pieceTextureType = getPieceTextureType(lastTextureStr);
		prd->diceSize = padjDiceSize->value;
		prd->roundedEdges = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwRoundedEdges));
		prd->bgInTrays = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(pwBgTrays));
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
	prd->arRefraction[ i ] = apadj[ i ]->value;
	prd->arCoefficient[ i ] = apadjCoefficient[ i ]->value;
	prd->arExponent[ i ] = apadjExponent[ i ]->value;

        prd->arDiceCoefficient[ i ] = apadjDiceCoefficient[ i ]->value;
	prd->arDiceExponent[ i ] = apadjDiceExponent[ i ]->value;
    }
    
    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwColour[ 0 ] ), ar );
    for( i = 0; i < 4; i++ )
	prd->aarColour[ 0 ][ i ] = ar[ i ];

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwColour[ 1 ] ), ar );
    for( i = 0; i < 4; i++ )
	prd->aarColour[ 1 ][ i ] = ar[ i ];

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwDiceColour[ 0 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	prd->aarDiceColour[ 0 ][ i ] = ar[ i ];

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwDiceColour[ 1 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	prd->aarDiceColour[ 1 ][ i ] = ar[ i ];

    for ( j = 0; j < 2; ++j ) {

      gtk_colour_picker_get_colour( 
           GTK_COLOUR_PICKER( apwDiceDotColour[ j ] ), 
           ar );
      for( i = 0; i < 3; i++ )
	prd->aarDiceDotColour[ j ][ i ] = ar[ i ];
    }

    /* cube colour */
    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( pwCubeColour ), ar );

	for( i = 0; i < 3; i++ )
		prd->arCubeColour[ i ] = ar[ i ];

    /* board colour */

    for( j = 0; j < 4; j++ ) {
	gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwBoard[ j ] ),
				      ar );
	for( i = 0; i < 3; i++ )
	    prd->aanBoardColour[ j ][ i ] = ar[ i ] * 0xFF;
    }
    
    prd->aSpeckle[ 0 ] = apadjBoard[ 0 ]->value * 0x80;
/*    prd->aSpeckle[ 1 ] = apadjBoard[ 1 ]->value * 0x80; */
    prd->aSpeckle[ 2 ] = apadjBoard[ 2 ]->value * 0x80;
    prd->aSpeckle[ 3 ] = apadjBoard[ 3 ]->value * 0x80;
    
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pwWood ) ))
	    prd->wt = gtk_option_menu_get_history( GTK_OPTION_MENU(pwWoodType ) );
	else
		prd->wt = WOOD_PAINT;

    prd->rRound = 1.0 - padjRound->value;

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

	gtk_widget_set_sensitive(frame3dOptions, rdPrefs.fDisplayType == DT_3D);

#if !HAVE_GTKGLEXT
	if (rdPrefs.fDisplayType == DT_3D)
		SetupVisual();
#endif
#endif

#if HAVE_LIBXML2
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      DesignPage( &plBoardDesigns, bd ),
			      gtk_label_new(_("Designs") ) );
#endif /* HAVE_LIBXML2 */

#if USE_BOARD3D
	if (rdPrefs.fDisplayType == DT_3D)
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

void ChangePage(GtkNotebook *notebook, GtkNotebookPage *page, 
				guint page_num, gpointer user_data)
{
	BoardData *bd = BOARD(pwPrevBoard)->board_data;
	int dicePage = NUM_NONPREVIEW_PAGES + PI_DICE0;

	if (!fUpdate)
		return;

#if USE_BOARD3D
	if (rdPrefs.fDisplayType == DT_3D)
		dicePage -= 1;
#endif
	/* Make sure correct dice preview visible */
	if (page_num == dicePage)
	{
		if (bd->turn == 1)
		{
			bd->turn = -1;
#if USE_BOARD3D
			if (rdPrefs.fDisplayType == DT_3D)
				updateDiceOccPos(bd);
			else
#endif
				RollDice2d(bd);
			UpdatePreview(0);
			return;
		}
	}
	if (page_num == dicePage + 1)
	{
		if (bd->turn == -1)
		{
			bd->turn = 1;
#if USE_BOARD3D
			if (rdPrefs.fDisplayType == DT_3D)
				updateDiceOccPos(bd);
			else
#endif
				RollDice2d(bd);
			UpdatePreview(0);
			return;
		}
	}
	if (page_num == NUM_NONPREVIEW_PAGES)
		ShowSelectedRow();

#if USE_BOARD3D
	if (rdPrefs.fDisplayType == DT_3D && redrawChange)
	{
		redraw_changed(NULL, NULL);
		redrawChange = FALSE;
	}
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

    pwDialog = GTKCreateDialog( _("GNU Backgammon - Appearance"), DT_QUESTION,
			     GTK_SIGNAL_FUNC( BoardPrefsOK ), pwBoard );

#if USE_BOARD3D
	SetPreviewLightLevel(bd->rd->lightLevels);
	Setup3dColourPicker(pwDialog, ((BoardData*)BOARD(pwBoard)->board_data)->drawing_area->window);
#endif

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwHbox = gtk_hbox_new(FALSE, 0) );

    pwNotebook = gtk_notebook_new();

    gtk_notebook_set_scrollable( GTK_NOTEBOOK( pwNotebook ), TRUE );
    gtk_notebook_popup_enable( GTK_NOTEBOOK( pwNotebook ) );
    
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
#if !USE_BOARD3D
	/* Make sure preview is big enough in 2d mode */
    gtk_widget_set_usize ( GTK_WIDGET ( pwNotebook ), -1, 360 );
#endif

	gtk_box_pack_start( GTK_BOX(pwHbox), pwNotebook, TRUE, TRUE, 0 );
	gtk_box_pack_start( GTK_BOX(pwHbox), pwPrevBoard, TRUE, TRUE, 0 );

    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      GeneralPage( bd, pwBoard ),
			      gtk_label_new( _("General") ) );

#if HAVE_LIBXML2
    plBoardDesigns = read_board_designs ();
#endif
	AddPages(bd, pwNotebook);

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );

    gtk_signal_connect( GTK_OBJECT( pwNotebook ), "switch-page",
			GTK_SIGNAL_FUNC( ChangePage ), 0);

	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( BoardPrefsDestroy ), NULL );

	SetTitle();

    gtk_widget_show_all( pwDialog );

#if USE_BOARD3D
	DisplayCorrectBoardType(bd);
	redrawChange = FALSE;
	bd->rd->quickDraw = FALSE;
#endif

	gtk_notebook_set_page(GTK_NOTEBOOK(pwNotebook), NUM_NONPREVIEW_PAGES);

	fUpdate = TRUE;
    gtk_main();
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

#if USE_BOARD3D
		DisplayCorrectBoardType(bd);
		if (bd->rd->fDisplayType == DT_3D)
			updateOccPos(bd);
		else
			StopIdle3d(bd);

		if (bd->rd->fDisplayType == DT_2D)
#endif
		{
			gtk_widget_queue_draw( bd->drawing_area );
			gtk_widget_queue_draw( bd->dice_area );
		}
		gtk_widget_queue_draw( bd->table );
    }
}

#if USE_BOARD3D

int IsWhiteColour3d(Material* pMat)
{
	return (pMat->ambientColour[0] == 1) && (pMat->ambientColour[1] == 1) && (pMat->ambientColour[2] == 1) &&
		(pMat->diffuseColour[0] == 1) && (pMat->diffuseColour[1] == 1) && (pMat->diffuseColour[2] == 1) &&
		(pMat->specularColour[0] == 1) && (pMat->specularColour[1] == 1) && (pMat->specularColour[2] == 1);
}

int IsBlackColour3d(Material* pMat)
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
  STATE_DESIGN, 
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
  vprintf(msg, args);
  va_end(args);
  
}

static void ScanEndElement( void *pv, const xmlChar *pchName ) {
  
  parsecontext *ppc = pv;

  /* pop element */

  PopState ( ppc );

}



static void ScanCharacters( void *pv, const xmlChar *pchIn, int cch ) {

  parsecontext *ppc = pv;
  char *sz = g_strndup ( pchIn, cch );
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

  if ( ! strcmp ( pchName, "board-designs" ) && ppc->ips == -1 ) {

    PushState ( ppc, STATE_BOARD_DESIGNS );

  }
  else if ( ! strcmp ( pchName, "board-design" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGNS ) {

    /* allocate list if empty */
    if ( ! ppc->pl )
      ppc->pl = g_list_alloc ();

    ppc->pbde = (boarddesign *) malloc ( sizeof ( boarddesign ) );
    memset ( ppc->pbde, 0, sizeof ( boarddesign ) );
    ppc->pbde->fDeletable = ppc->fDeletable;
    ppc->pl = g_list_append ( ppc->pl, (gpointer) ppc->pbde );

    PushState ( ppc, STATE_BOARD_DESIGN );

  }
  else if ( ! strcmp ( pchName, "about" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGN ) {

    PushState ( ppc, STATE_ABOUT );

  }
  else if ( ! strcmp ( pchName, "title" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_TITLE );
    ppc->pbde->szTitle = g_strdup ( "" );

  }
  else if ( ! strcmp ( pchName, "author" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_AUTHOR );
    ppc->pbde->szAuthor = g_strdup ( "" );

  }
  else if ( ! strcmp ( pchName, "design" ) &&
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
  char *pch;

  pc.ips = -1;
  pc.pl = NULL;
  pc.err = FALSE;
  pc.fDeletable = fDeletable;

  /* create parser context */

  if ( ! ( pch = PathSearch ( szFile, szDataDirectory ) ) )
    return NULL;
  if( access( pch, R_OK ) )
    return NULL;

  pxpc = xmlCreateFileParserCtxt ( pch );
  free ( pch );
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
