/*
* gtkgamelist.c
* by Jon Kinsey, 2004
*
* Game list window
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

#include "config.h"
#include <gtk/gtk.h>
#include <assert.h>
#include <stdlib.h>
#include "i18n.h"
#include "backgammon.h"
#include "gtkboard.h"
#include "drawboard.h"
#include "positionid.h"

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_style_get_font(s) ((s)->font)
static void gtk_style_set_font( GtkStyle *ps, GdkFont *pf ) {

    ps->font = pf;
    gdk_font_ref( pf );
}
#endif

GtkWidget *pwGameList;
GtkStyle *psGameList, *psCurrent, *psCubeErrors[3], *psChequerErrors[3], *psLucky[LUCK_VERYGOOD + 1];

extern void SetAnnotation( moverecord *pmr );

typedef struct _gamelistrow {
    moverecord *apmr[ 2 ]; /* moverecord entries for each column */
    int fCombined; /* this message's row is combined across both columns */
} gamelistrow;

extern void GL_Freeze( void ) {

    gtk_clist_freeze( GTK_CLIST( pwGameList ) );
}

extern void GL_Thaw( void ) {

    gtk_clist_thaw( GTK_CLIST( pwGameList ) );
}

extern void GTKClearMoveRecord( void ) {

    gtk_clist_clear( GTK_CLIST( pwGameList ) );
}

void GameListSelectRow(GtkCList *pcl, gint y, gint x, GdkEventButton *pev, gpointer p)
{
#if USE_BOARD3D
    BoardData *bd = BOARD( pwBoard )->board_data;
#endif
    gamelistrow *pglr;
    moverecord *pmr, *pmrPrev = NULL;
    list *pl;
    
    if( x < 1 || x > 2 )
    	return;

    pglr = gtk_clist_get_row_data( pcl, y );
    if (!pglr)
    	pmr = NULL;
    else
    	pmr = pglr->apmr[(pglr->fCombined) ? 0 : x - 1];

    /* Get previous move record */
    if (!pglr->fCombined && x == 2)
    	x = 1;
    else
    {
    	y--;
    	x = 2;
    }
    pglr = gtk_clist_get_row_data( pcl, y );
    if (!pglr)
    	pmrPrev = NULL;
    else
    	pmrPrev = pglr->apmr[(pglr->fCombined) ? 0 : x - 1];

    if (!pmr)
    	return;

    for( pl = plGame->plPrev; pl != plGame; pl = pl->plPrev ) {
    	assert( pl->p );
    	if( pl == plGame->plPrev && pl->p == pmr && pmr->mt == MOVE_SETDICE )
    	break;

    	if( pl->p == pmrPrev && pmr != pmrPrev ) {
    	/* pmr = pmrPrev; */
    	break;
    	} else if( pl->plNext->p == pmr ) {
	    /* pmr = pl->p; */
	    break;
	}
    }

    if( pl == plGame )
    	/* couldn't find the moverecord they selected */
    	return;
    
    plLastMove = pl;
    
    CalculateBoard();

    if ( pmr && pmr->mt == MOVE_NORMAL ) {
       /* roll dice */
       ms.gs = GAME_PLAYING;
       ms.anDice[ 0 ] = pmr->anDice[ 0 ];
       ms.anDice[ 1 ] = pmr->anDice[ 1 ];
   }
#if USE_BOARD3D
	if (bd->rd->fDisplayType == DT_3D)
	{	/* Make sure dice are shown (and not rolled) */
		bd->diceShown = DICE_ON_BOARD;
		bd->diceRoll[0] = !ms.anDice[0];
	}
#endif

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );
    
    SetMoveRecord( pl->p );
    
    ShowBoard();
}

extern void GL_SetNames()
{
    gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 1, 
                                ( ap[0].szName ));
    gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 2, 
                                ( ap[1].szName ));
}

void SetColourIfDifferent(GdkColor cOrg[5], GdkColor cNew[5], GdkColor cDefault[5])
{
	int i;
	for (i = 0; i < 5; i++)
	{
		if (memcmp(&cNew[i], &cDefault[i], sizeof(GdkColor)))
			memcpy(&cOrg[i], &cNew[i], sizeof(GdkColor));
	}
}

void GetStyleFromRCFile(GtkStyle** ppStyle, char* name)
{	/* Note gtk 1.3 doesn't seem to have a nice way to do this... */
	BoardData *bd = BOARD(pwBoard)->board_data;
	GtkStyle *psDefault, *psNew;
	GtkWidget *dummy, *temp;
	char styleName[100];
	/* Get default style so only changes to this are applied */
	temp = gtk_button_new();
	gtk_widget_ensure_style(temp);
	psDefault = temp->style;

	/* Get Style from rc file */
	strcpy(styleName, "gnubg-gamelist-");
	strcat(styleName, name);
	dummy = gtk_label_new("");
	gtk_widget_ensure_style(dummy);
	gtk_widget_set_name(dummy, styleName);
	/* Pack in box to make sure style is loaded */
	gtk_box_pack_start(GTK_BOX(bd->vbox_ids), dummy, FALSE, FALSE, 0);
	psNew = gtk_widget_get_style(dummy);

	/* Base new style on default for game list */
	*ppStyle = gtk_style_copy(psGameList);
	/* Only set specified attributes */
	SetColourIfDifferent((*ppStyle)->bg, psNew->bg, psDefault->bg);
	SetColourIfDifferent((*ppStyle)->fg, psNew->fg, psDefault->fg);
	/* Copy some settings, used when row is selected */
	memcpy(&(*ppStyle)->fg[GTK_STATE_SELECTED], &(*ppStyle)->fg[GTK_STATE_NORMAL], sizeof(GdkColor));
	memcpy(&(*ppStyle)->bg[GTK_STATE_SELECTED], &(*ppStyle)->bg[GTK_STATE_NORMAL], sizeof(GdkColor));
	memcpy(&(*ppStyle)->base[GTK_STATE_NORMAL], &(*ppStyle)->bg[GTK_STATE_NORMAL], sizeof(GdkColor));

	if (!gdk_font_equal(gtk_style_get_font(psNew), gtk_style_get_font(psDefault)))
		gtk_style_set_font(*ppStyle, gtk_style_get_font(psNew));

	/* Remove useless widgets */
	gtk_widget_destroy(dummy);
	gtk_widget_destroy(temp);
}

GtkWidget* GL_Create()
{
    GtkStyle *ps;
    gint nMaxWidth; 
    char *asz[] = {_("#"), NULL, NULL};

    pwGameList = gtk_clist_new_with_titles(3, asz);
    GTK_WIDGET_UNSET_FLAGS(pwGameList, GTK_CAN_FOCUS);

    gtk_clist_set_selection_mode( GTK_CLIST( pwGameList ),
				  GTK_SELECTION_BROWSE );
    gtk_clist_column_titles_passive( GTK_CLIST( pwGameList ) );

    GL_SetNames();

    gtk_clist_set_column_justification( GTK_CLIST( pwGameList ), 0,
					GTK_JUSTIFY_RIGHT );
    gtk_clist_set_column_resizeable( GTK_CLIST( pwGameList ), 0, FALSE );
    gtk_clist_set_column_resizeable( GTK_CLIST( pwGameList ), 1, FALSE );
    gtk_clist_set_column_resizeable( GTK_CLIST( pwGameList ), 2, FALSE );
    gtk_widget_ensure_style( pwGameList );
    ps = gtk_style_new();
    ps->base[ GTK_STATE_SELECTED ] =
	ps->base[ GTK_STATE_ACTIVE ] =
	ps->base[ GTK_STATE_NORMAL ] =
	pwGameList->style->base[ GTK_STATE_NORMAL ];
    ps->fg[ GTK_STATE_SELECTED ] =
	ps->fg[ GTK_STATE_ACTIVE ] =
	ps->fg[ GTK_STATE_NORMAL ] =
	pwGameList->style->fg[ GTK_STATE_NORMAL ];
    gtk_style_set_font( ps, gtk_style_get_font( pwGameList->style ) );
    gtk_widget_set_style( pwGameList, ps );
    
    psGameList = gtk_style_copy( ps );
    psGameList->bg[ GTK_STATE_SELECTED ] = psGameList->bg[ GTK_STATE_NORMAL ] =
	ps->base[ GTK_STATE_NORMAL ];

    psCurrent = gtk_style_copy( psGameList );
    psCurrent->bg[ GTK_STATE_SELECTED ] = psCurrent->bg[ GTK_STATE_NORMAL ] =
	psCurrent->base[ GTK_STATE_SELECTED ] =
	psCurrent->base[ GTK_STATE_NORMAL ] =
	psGameList->fg[ GTK_STATE_NORMAL ];
    psCurrent->fg[ GTK_STATE_SELECTED ] = psCurrent->fg[ GTK_STATE_NORMAL ] =
	psGameList->bg[ GTK_STATE_NORMAL ];

    GetStyleFromRCFile(&psCubeErrors[SKILL_VERYBAD], "cube-blunder");
    GetStyleFromRCFile(&psCubeErrors[SKILL_BAD], "cube-error");
    GetStyleFromRCFile(&psCubeErrors[SKILL_DOUBTFUL], "cube-doubtful");
    GetStyleFromRCFile(&psChequerErrors[SKILL_VERYBAD], "chequer-blunder");
    GetStyleFromRCFile(&psChequerErrors[SKILL_BAD], "chequer-error");
    GetStyleFromRCFile(&psChequerErrors[SKILL_DOUBTFUL], "chequer-doubtful");
    GetStyleFromRCFile(&psLucky[LUCK_VERYBAD], "luck-bad");
    GetStyleFromRCFile(&psLucky[LUCK_VERYGOOD], "luck-good");

    nMaxWidth = gdk_string_width( gtk_style_get_font( psCurrent ), _("99") );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 0, nMaxWidth );
    nMaxWidth = gdk_string_width( gtk_style_get_font( psCurrent ),
                                  " (set board AAAAAAAAAAAAAA)");
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 1, nMaxWidth - 30);
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 2, nMaxWidth - 30);
    
    gtk_signal_connect( GTK_OBJECT( pwGameList ), "select-row",
			GTK_SIGNAL_FUNC( GameListSelectRow ), NULL );

    return pwGameList;
}

static int AddMoveRecordRow( void )
{
    gamelistrow *pglr;
    static char *aszData[] = {NULL, NULL, NULL};
    char szIndex[5];
    int row;

    sprintf(szIndex, "%d", GTK_CLIST(pwGameList)->rows);
    aszData[0] = szIndex;
    row = gtk_clist_append(GTK_CLIST(pwGameList), aszData);
    gtk_clist_set_row_style(GTK_CLIST(pwGameList), row, psGameList);

    pglr = malloc(sizeof(*pglr));
    pglr->fCombined = FALSE;
    pglr->apmr[0] = pglr->apmr[1] = NULL;
    gtk_clist_set_row_data_full(GTK_CLIST(pwGameList), row, pglr, free);

    return row;
}

void SetCellColour(int row, int col, moverecord *pmr)
{
	GtkStyle *pStyle;

	if (!fStyledGamelist)
		pStyle = psGameList;
	else if (pmr->n.stMove == SKILL_VERYBAD)
		pStyle = psChequerErrors[SKILL_VERYBAD];
	else if (pmr->stCube == SKILL_VERYBAD)
		pStyle = psCubeErrors[SKILL_VERYBAD];
	else if (pmr->n.stMove == SKILL_BAD)
		pStyle = psChequerErrors[SKILL_BAD];
	else if (pmr->stCube == SKILL_BAD)
		pStyle = psCubeErrors[SKILL_BAD];
	else if (pmr->n.stMove == SKILL_DOUBTFUL)
		pStyle = psChequerErrors[SKILL_DOUBTFUL];
	else if (pmr->stCube == SKILL_DOUBTFUL)
		pStyle = psCubeErrors[SKILL_DOUBTFUL];
	else if (pmr->lt == LUCK_VERYGOOD)
		pStyle = psLucky[LUCK_VERYGOOD];
	else if (pmr->lt == LUCK_VERYBAD)
		pStyle = psLucky[LUCK_VERYBAD];
	else
		pStyle = psGameList;

	gtk_clist_set_cell_style(GTK_CLIST(pwGameList), row, col, pStyle);
}

/* Add a moverecord to the game list window.  NOTE: This function must be
   called _before_ applying the moverecord, so it can be displayed
   correctly. */
extern void GTKAddMoveRecord( moverecord *pmr )
{
    gamelistrow *pglr;
    int i, numRows, fPlayer = 0;
    char *pch = 0;
    char sz[ 40 ];
    doubletype dt;
    
    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	/* no need to list this */
	return;

#if USE_TIMECONTROL
    case MOVE_TIME:
	sprintf(pch = sz, _("(%s out of time [%d])"),
		     ap[ ms.fTurn ].szName , pmr->t.nPoints);
	fPlayer = -1;
	break;
#endif

    case MOVE_NORMAL:
	fPlayer = pmr->fPlayer;
	pch = sz;
	sz[ 0 ] = pmr->anDice[ 0 ] + '0';
	sz[ 1 ] = pmr->anDice[ 1 ] + '0';
	sz[ 2 ] = ':';
	sz[ 3 ] = ' ';
	FormatMove( sz + 4, ms.anBoard, pmr->n.anMove );
	strcat( sz, aszSkillTypeAbbr[ pmr->n.stMove ] );
	strcat( sz, aszSkillTypeAbbr[ pmr->stCube ] );
	break;

    case MOVE_DOUBLE:
	fPlayer = pmr->fPlayer;
	pch = sz;

        switch ( ( dt = DoubleType ( ms.fDoubled, ms.fMove, ms.fTurn ) ) ) {
        case DT_NORMAL:
          sprintf( sz, ( ms.fCubeOwner == -1 )? 
                   _("Double to %d") : _("Redouble to %d"), 
                   ms.nCube << 1 );
          break;
        case DT_BEAVER:
        case DT_RACCOON:
          sprintf( sz, ( dt == DT_BEAVER ) ? 
                   _("Beaver to %d") : _("Raccoon to %d"), ms.nCube << 2 );
          break;
        default:
          assert ( FALSE );
          break;
        }
	strcat( sz, aszSkillTypeAbbr[ pmr->stCube ] );
	break;
	
    case MOVE_TAKE:
	fPlayer = pmr->fPlayer;
	strcpy( pch = sz, _("Take") );
	strcat( sz, aszSkillTypeAbbr[ pmr->stCube ] );
	break;
	
    case MOVE_DROP:
	fPlayer = pmr->fPlayer;
	strcpy( pch = sz, _("Drop") );
	strcat( sz, aszSkillTypeAbbr[ pmr->stCube ] );
	break;
	
    case MOVE_RESIGN:
	fPlayer = pmr->fPlayer;
	pch = _(" Resigns"); /* FIXME show value */
	break;

    case MOVE_SETDICE:
	fPlayer = pmr->fPlayer;
	sprintf( pch = sz, _("Rolled %d%d"), pmr->anDice[ 0 ],
		 pmr->anDice[ 1 ] );
	break;

    case MOVE_SETBOARD:
	fPlayer = -1;
	sprintf( pch = sz, " (set board %s)",
		 PositionIDFromKey( pmr->sb.auchKey ) );
	break;

    case MOVE_SETCUBEPOS:
	fPlayer = -1;
	if( pmr->scp.fCubeOwner < 0 )
	    pch = " (set cube centre)";
	else
	    sprintf( pch = sz, " (set cube owner %s)",
		     ap[ pmr->scp.fCubeOwner ].szName );
	break;

    case MOVE_SETCUBEVAL:
	fPlayer = -1;
	sprintf( pch = sz, " (set cube value %d)", pmr->scv.nCube );
	break;
	
    default:
	assert( FALSE );
    }

	i = -1;
	numRows = GTK_CLIST( pwGameList )->rows;
	if (numRows > 0)
	{
		pglr = gtk_clist_get_row_data(GTK_CLIST(pwGameList), numRows - 1);
		if (pglr &&
			!(pglr->fCombined || pglr->apmr[ 1 ] || ( fPlayer != 1 && pglr->apmr[ 0 ] )))
			/* Add to second half of current row */
			i = numRows - 1;
	}
	if (i == -1)
		/* Add new row */
		i = AddMoveRecordRow();

    pglr = gtk_clist_get_row_data( GTK_CLIST( pwGameList ), i );

	if (fPlayer == -1)
	{
		pglr->fCombined = TRUE;
		fPlayer = 0;
	}
	else
		pglr->fCombined = FALSE;

    pglr->apmr[fPlayer] = pmr;
    
    gtk_clist_set_text(GTK_CLIST(pwGameList), i, fPlayer + 1, pch);

    SetCellColour(i, fPlayer + 1, pmr);
}


/* Select a moverecord as the "current" one.  NOTE: This function must be
   called _after_ applying the moverecord. */
extern void GTKSetMoveRecord( moverecord *pmr ) {

    /* highlighted row/col in game record */
    static int yCurrent = -1, xCurrent = -1;

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr;
    int i;

    SetAnnotation( pmr );

    if( woPanel[WINDOW_HINT].pwWin ) {

#ifdef UNDEF
	hintdata *phd = gtk_object_get_user_data( GTK_OBJECT( woPanel[WINDOW_HINT].pwWin ) );

	phd->fButtonsValid = FALSE;
	CheckHintButtons( phd );
#endif
    }
    
	if (yCurrent != -1 && xCurrent != -1)
	{
		moverecord *pmrLast = NULL;
		pglr = gtk_clist_get_row_data(pcl, yCurrent);
		if (pglr)
		{
			pmrLast = pglr->apmr[xCurrent - 1];
			if (pmrLast)
				SetCellColour(yCurrent, xCurrent, pmrLast);
		}
		if (!pmrLast)
		   gtk_clist_set_cell_style(pcl, yCurrent, xCurrent, psGameList);
	}

    yCurrent = xCurrent = -1;

    if( !pmr )
	return;
    
    if( pmr == plGame->plNext->p ) {
	assert( pmr->mt == MOVE_GAMEINFO );
	yCurrent = 0;
	
	if( plGame->plNext->plNext->p ) {
	    moverecord *pmrNext = plGame->plNext->plNext->p;

	    if( pmrNext->mt == MOVE_NORMAL && pmrNext->fPlayer == 1 )
		xCurrent = 2;
	    else
		xCurrent = 1;
	} else
	    xCurrent = 1;
    } else {
	for( i = pcl->rows - 1; i >= 0; i-- ) {
	    pglr = gtk_clist_get_row_data( pcl, i );
	    if( pglr->apmr[ 1 ] == pmr ) {
		xCurrent = 2;
		break;
	    } else if( pglr->apmr[ 0 ] == pmr ) {
		xCurrent = 1;
		break;
	    }
	}
	
	yCurrent = i;
	
	if( yCurrent >= 0 && !( pmr->mt == MOVE_SETDICE &&
				yCurrent == pcl->rows - 1 ) ) {
	    do {
		if( ++xCurrent > 2 ) {
		    xCurrent = 1;
		    yCurrent++;
		}

		pglr = gtk_clist_get_row_data( pcl, yCurrent );
	    } while( yCurrent < pcl->rows - 1 && !pglr->apmr[ xCurrent - 1 ] );
	    
	    if( yCurrent >= pcl->rows )
		AddMoveRecordRow();
	}
    }

	/* Highlight current move */
	gtk_clist_set_cell_style(pcl, yCurrent, xCurrent, psCurrent);

	/* Wait for screen to resize to make sure move will be shown */
	while( gtk_events_pending() )
		gtk_main_iteration();

	if( gtk_clist_row_is_visible( pcl, yCurrent ) != GTK_VISIBILITY_FULL )
		gtk_clist_moveto( pcl, yCurrent, xCurrent, 0.8, 0.5 );
}

extern void GTKPopMoveRecord( moverecord *pmr ) {

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr = NULL;
    
    gtk_clist_freeze( pcl );

    while( pcl->rows ) {
	pglr = gtk_clist_get_row_data( pcl, pcl->rows - 1 );
	
	if( pglr->apmr[ 0 ] != pmr && pglr->apmr[ 1 ] != pmr )
	    gtk_clist_remove( pcl, pcl->rows - 1);
	else
	    break;
    }

    if( pcl->rows ) {
	if( pglr->apmr[ 0 ] == pmr )
	    /* the left column matches; delete the row */
	    gtk_clist_remove( pcl, pcl->rows - 1 );
	else {
	    /* the right column matches; delete that column only */
	    gtk_clist_set_text( pcl, pcl->rows - 1, 2, NULL );
	    pglr->apmr[ 1 ] = NULL;
	}
    }
    
    gtk_clist_thaw( pcl );
}
