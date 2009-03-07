/*
 * gtkoptions.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
 *
 * Based on Sho Sengoku's Equity Temperature Map
 * http://www46.pair.com/sengoku/TempMap/English/TempMap.html
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include <gtk/gtk.h>

#include "backgammon.h"
#include "eval.h"
#include "dice.h"
#include "gtkgame.h"
#include "gtkfile.h"
#include "sound.h"
#include "drawboard.h"
#include "matchequity.h"
#include "format.h"
#include "gtkboard.h"
#include "renderprefs.h"
#include "gtkwindows.h"
#include "openurl.h"
#if USE_BOARD3D
#include "fun3d.h"
#endif
#include "multithread.h"
#include "gtkoptions.h"
#include "gtkrelational.h"

typedef struct _optionswidget {

  GtkWidget *pwAutoBearoff, *pwAutoCrawford, *pwAutoGame,
            *pwAutoMove, *pwAutoRoll;
  GtkWidget *pwTutor, *pwTutorCube, *pwTutorChequer, *pwTutorSkill,
            *pwTutorEvalHint, *pwTutorEvalAnalysis;
  GtkAdjustment *padjCubeBeaver, *padjCubeAutomatic, *padjLength;
  GtkWidget *pwCubeUsecube, *pwCubeJacoby, *pwCubeInvert;
  GtkWidget *pwGameClockwise;
  GtkWidget *apwVariations[ NUM_VARIATIONS ];
  GtkWidget *pwOutputMWC, *pwOutputGWC, *pwOutputMWCpst;
  GtkWidget *pwConfStart, *pwConfOverwrite;
  GtkWidget *pwDiceManual, *pwDicePRNG, *pwPRNGMenu;

  GtkWidget *pwBeavers, *pwBeaversLabel, *pwAutomatic, *pwAutomaticLabel;
  GtkWidget *pwMETFrame, *pwLoadMET, *pwSeed;

  GtkWidget *pwRecordGames, *pwDisplay;
  GtkAdjustment *padjCache, *padjDelay, *padjSeed, *padjThreads;

  GtkWidget *pwIllegal, *pwUseDiceIcon, *pwShowIDs, *pwShowPips, *pwShowEPCs, *pwShowWastage,
      *pwAnimateNone, *pwAnimateBlink, *pwAnimateSlide,
      *pwHigherDieFirst, *pwGrayEdit, *pwSetWindowPos, *pwDragTargetHelp;
  GtkAdjustment *padjSpeed;

  GtkWidget *pwCheat, *pwCheatRollBox, *apwCheatRoll[ 2 ];
  GtkWidget *pwGotoFirstGame, *pwGameListStyles;

  GtkWidget *pwDefaultSGFFolder;
  GtkWidget *pwDefaultImportFolder;
  GtkWidget *pwDefaultExportFolder;
  GtkWidget *pwWebBrowser;
  GtkAdjustment *padjDigits;
  GtkWidget *pwDigits;
  int fChanged;
} optionswidget;   

typedef struct _SoundDetail
{
	int Enabled;
	char *Path;
} SoundDeatil;
SoundDeatil soundDetails[NUM_SOUNDS];
static GtkWidget *soundFrame;
static GtkWidget *soundEnabled;
static GtkWidget *soundPath;
static GtkWidget *soundPathButton;
static GtkWidget *soundPlayButton;
static GtkWidget *soundAllDefaultButton;
static GtkWidget *soundDefaultButton;
static GtkWidget *soundBeepIllegal;
static GtkWidget *soundsEnabled;
static GtkWidget *soundList;
static GtkWidget *pwSoundCommand;
static int selSound;
static int SoundSkipUpdate;
static int relPage, relPageActivated;

static void SeedChanged( GtkWidget *UNUSED(pw), int *pf ) {

	*pf = 1;  
}

static void UseCubeToggled(GtkWidget *UNUSED(pw), optionswidget *pow){

  gint n = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwCubeUsecube ) ); 

  gtk_widget_set_sensitive( pow->pwCubeJacoby, n );
  gtk_widget_set_sensitive( pow->pwBeavers, n );
  gtk_widget_set_sensitive( pow->pwAutomatic, n );
  gtk_widget_set_sensitive( pow->pwBeaversLabel, n );
  gtk_widget_set_sensitive( pow->pwAutomaticLabel, n );
  gtk_widget_set_sensitive( pow->pwAutoCrawford, n );
}

static void ManualDiceToggled( GtkWidget *UNUSED(pw), optionswidget *pow){

  gint n = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwDicePRNG ) ); 
  gint m = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwCheat ) ); 

  gtk_widget_set_sensitive( pow->pwPRNGMenu, n );
  gtk_widget_set_sensitive( pow->pwSeed, n );
  gtk_widget_set_sensitive( pow->pwCheatRollBox, m );

} 

static void TutorToggled (GtkWidget *UNUSED(pw), optionswidget *pow){

  gint n = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwTutor ) ); 

  gtk_widget_set_sensitive( pow->pwTutorCube, n );
  gtk_widget_set_sensitive( pow->pwTutorChequer, n );
  gtk_widget_set_sensitive( pow->pwTutorSkill, n );
  gtk_widget_set_sensitive( pow->pwTutorEvalHint, n );
  gtk_widget_set_sensitive( pow->pwTutorEvalAnalysis, n );

}

static void ToggleAnimation( GtkWidget *pw, GtkWidget *pwSpeed ) {

    gtk_widget_set_sensitive( pwSpeed,
                              !gtk_toggle_button_get_active(
                                  GTK_TOGGLE_BUTTON( pw ) ) );
}

static char *aszTutor[] = {
	N_("Doubtful"), N_("Bad"), N_("Very bad"), NULL
};

static void SoundDefaultClicked(GtkWidget *UNUSED(widget), gpointer UNUSED(userdata))
{
	char *defaultSound = GetDefaultSoundFile(selSound);
	SoundSkipUpdate = TRUE;
	gtk_entry_set_text(GTK_ENTRY(soundPath), defaultSound);
	g_free(soundDetails[selSound].Path);
	soundDetails[selSound].Path = defaultSound;
}

static void SoundAllDefaultClicked(GtkWidget *UNUSED(widget), gpointer UNUSED(userdata))
{
	int i;
	for (i = 0; i < NUM_SOUNDS; i++) 
	{
		g_free(soundDetails[i].Path);
		soundDetails[i].Path = GetDefaultSoundFile(i);
	}
	SoundDefaultClicked(NULL, NULL);
}


static void SoundEnabledClicked(GtkWidget *UNUSED(widget), gpointer UNUSED(userdata))
{
	int enabled;
	if (!GTK_WIDGET_REALIZED(soundEnabled) || !GTK_WIDGET_SENSITIVE(soundEnabled))
		return;
	enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(soundEnabled));
	gtk_widget_set_sensitive(soundPath, enabled);
	gtk_widget_set_sensitive(soundPathButton, enabled);
	gtk_widget_set_sensitive(soundPlayButton, enabled);
	gtk_widget_set_sensitive(soundDefaultButton, enabled);
	if (!enabled) {
		g_free(soundDetails[selSound].Path);
		soundDetails[selSound].Path = g_strdup("");
	}
	else if (!*soundDetails[selSound].Path)
		SoundDefaultClicked(0, 0);
}

static void SoundGrabFocus(GtkWidget *pw, void *UNUSED(dummy))
{
	gtk_widget_grab_focus(soundList);
	SoundEnabledClicked(0, 0);
}

static void SoundToggled(GtkWidget *UNUSED(pw), optionswidget *UNUSED(pow))
{
	int enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(soundsEnabled));
    gtk_widget_set_sensitive(soundFrame, enabled);
    gtk_widget_set_sensitive(soundList, enabled);
	gtk_widget_set_sensitive(soundEnabled, enabled);
	gtk_widget_set_sensitive(soundPath, enabled);
	gtk_widget_set_sensitive(soundPathButton, enabled);
	gtk_widget_set_sensitive(soundPlayButton, enabled);
	gtk_widget_set_sensitive(soundDefaultButton, enabled);
	if (enabled)
		SoundGrabFocus(0, 0);
}

static void SoundSelected(GtkTreeView *treeview, gpointer UNUSED(userdata))
{
	GtkTreePath *path;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(treeview), &path, NULL);
	selSound = gtk_tree_path_get_indices(path)[0];

	gtk_frame_set_label(GTK_FRAME(soundFrame), sound_description[selSound]);
	SoundSkipUpdate = TRUE;
	gtk_entry_set_text(GTK_ENTRY(soundPath), soundDetails[selSound].Path);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(soundEnabled), (*soundDetails[selSound].Path) ? TRUE:FALSE);
}

static void SoundTidy(GtkWidget *UNUSED(pw), void *UNUSED(dummy))
{
	unsigned int i;
	for (i = 0; i < NUM_SOUNDS; i++) 
	{	/* Tidy allocated memory */
		g_free(soundDetails[i].Path);
	}
}

static void PathChanged(GtkWidget *widget, gpointer UNUSED(userdata))
{
	if (!SoundSkipUpdate)
	{
		g_free(soundDetails[selSound].Path);
		soundDetails[selSound].Path = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
	}
	else
		SoundSkipUpdate = FALSE;
}

static void SoundChangePathClicked(GtkWidget *UNUSED(widget), gpointer UNUSED(userdata))
{
	static char *lastSoundFolder = NULL;
	char *filename = GTKFileSelect(_("Select soundfile"), "*.wav", lastSoundFolder, NULL, GTK_FILE_CHOOSER_ACTION_OPEN);
	if (filename)
	{
		lastSoundFolder = g_path_get_dirname(filename);
		SoundSkipUpdate = TRUE;
		gtk_entry_set_text(GTK_ENTRY(soundPath), filename);
		g_free(soundDetails[selSound].Path);
		soundDetails[selSound].Path = filename;
	}
}

static void SoundPlayClicked(GtkWidget *UNUSED(widget), gpointer UNUSED(userdata))
{
        playSoundFile(soundDetails[selSound].Path, TRUE);
}

static gchar* CacheSizeString(GtkScale *UNUSED(scale), gdouble value)
{
  return g_strdup_printf ("%imb", GetCacheMB(value));
}


static void
AddSoundWidgets (GtkWidget * container)
{
    GtkWidget *pwvboxMain, *pwhboxTop, *pwvboxTop;
    gint i;
    GtkWidget *pwScrolled, *pwhbox, *pwvboxDetails, *pwLabel;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);
    GtkTreeIter iter;

    pwvboxMain = gtk_vbox_new (FALSE, 0);
    pwhboxTop = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (pwvboxMain), pwhboxTop, TRUE, TRUE, 0);
    pwvboxTop = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (pwhboxTop), pwvboxTop, TRUE, TRUE, 0);

    pwhbox = gtk_hbox_new (FALSE, 0);
    pwLabel = gtk_label_new (_("Sound command:"));
    gtk_box_pack_start (GTK_BOX (pwhbox), pwLabel, FALSE, FALSE, 0);
    pwSoundCommand = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (pwSoundCommand), sound_get_command ());
    gtk_box_pack_start (GTK_BOX (pwhbox), pwSoundCommand, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (pwvboxTop), pwhbox, FALSE, FALSE, 0);

    soundBeepIllegal =
	gtk_check_button_new_with_label (_("Beep on invalid input"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (soundBeepIllegal),
				  fGUIBeep);
    gtk_box_pack_start (GTK_BOX (pwvboxTop), soundBeepIllegal, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(soundBeepIllegal, _ ("Emit a warning beep if invalid moves are attempted."));

    soundsEnabled =
	gtk_check_button_new_with_label (_("Enable sound effects"));
    gtk_box_pack_start (GTK_BOX (pwvboxTop), soundsEnabled, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text( soundsEnabled,
			  _("Have GNU Backgammon make sound effects when various events occur."));
    g_signal_connect(G_OBJECT (soundsEnabled), "toggled", G_CALLBACK (SoundToggled), NULL);
#define SOUND_COL 0
    for (i = 0; i < NUM_SOUNDS; i++)
      {
	  /* Copy sound path data to be used in dialog */
	  soundDetails[i].Path = GetSoundFile (i);

	  gtk_list_store_append(store, &iter);
	  gtk_list_store_set(store, &iter, SOUND_COL,
			      gettext(sound_description[i]), -1);
      }

    soundList = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref (G_OBJECT (store));	/* The view now holds a reference.  We can get rid of our own reference */
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(soundList)), GTK_SELECTION_BROWSE);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (soundList), -1, _("Sound Event"), renderer, "text", SOUND_COL, NULL);
    gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (soundList), FALSE);
    g_signal_connect (soundList, "cursor-changed", G_CALLBACK (SoundSelected), NULL);
    g_signal_connect (soundList, "map_event", G_CALLBACK (SoundGrabFocus), NULL);
    g_signal_connect (soundList, "destroy", G_CALLBACK (SoundTidy), NULL);


    pwScrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pwScrolled),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (pwScrolled), soundList);

    gtk_box_pack_start (GTK_BOX (pwvboxTop), pwScrolled, TRUE, TRUE, 0);

    soundFrame = gtk_frame_new (NULL);
    gtk_box_pack_start (GTK_BOX (pwvboxMain), soundFrame, FALSE, FALSE, 0);

    pwvboxDetails = gtk_vbox_new (FALSE, 4);
    gtk_container_set_border_width (GTK_CONTAINER (pwvboxDetails), 4);
    soundEnabled = gtk_check_button_new_with_label ("Enabled");
    g_signal_connect (soundEnabled, "clicked",
		      G_CALLBACK (SoundEnabledClicked), NULL);
    gtk_box_pack_start (GTK_BOX (pwvboxDetails), soundEnabled, FALSE, FALSE, 0);

    pwhbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new (_("Path:")), FALSE, FALSE, 0);
    soundPath = gtk_entry_new ();
    g_signal_connect (soundPath, "changed", G_CALLBACK (PathChanged), NULL);
    gtk_box_pack_start (GTK_BOX (pwhbox), soundPath, TRUE, TRUE, 0);
    soundPathButton = gtk_button_new_with_label ("Browse");
    g_signal_connect (soundPathButton, "clicked",
		      G_CALLBACK (SoundChangePathClicked), NULL);
    gtk_box_pack_start (GTK_BOX (pwhbox), soundPathButton, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (pwvboxDetails), pwhbox, FALSE, FALSE, 0);

    pwhbox = gtk_hbox_new (FALSE, 4);
    soundPlayButton = gtk_button_new_with_label ("Play Sound");
    g_signal_connect (soundPlayButton, "clicked",
		      G_CALLBACK (SoundPlayClicked), NULL);
    gtk_box_pack_start (GTK_BOX (pwhbox), soundPlayButton, FALSE, FALSE, 0);
    /*sound default button*/
    soundDefaultButton = gtk_button_new_with_label ("Reset Default");
    g_signal_connect (soundDefaultButton, "clicked",
		      G_CALLBACK (SoundDefaultClicked), NULL);
    gtk_box_pack_start (GTK_BOX (pwhbox), soundDefaultButton, FALSE, FALSE, 0);

    /*sound all defaults button*/
    soundAllDefaultButton = gtk_button_new_with_label ("Reset All to Defaults");
    g_signal_connect (soundAllDefaultButton, "clicked",
		      G_CALLBACK (SoundAllDefaultClicked), NULL);
    gtk_box_pack_start (GTK_BOX (pwhbox), soundAllDefaultButton, FALSE, FALSE, 0);



    gtk_box_pack_start (GTK_BOX (pwvboxDetails), pwhbox, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (soundFrame), pwvboxDetails);

    gtk_container_add (GTK_CONTAINER (container), pwvboxMain);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (soundsEnabled), TRUE);	/* Set true so event fired */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (soundsEnabled), fSound);
}


static GtkWidget *OptionsPages( optionswidget *pow )
{
    char *aszRNGTip[] = {
		NULL,	/* Manual setting */
	N_("The rand() generator specified by ANSI C (typically linear "
	   "congruential)"),
	N_("Blum, Blum and Shub's verifiably strong generator"),
	N_("The random() non-linear additive feedback generator from 4.3 BSD Unix"),
	N_("Bob Jenkin's Indirection, Shift, Accumulate, Add and Count "
	   "cryptographic generator"),
	N_("A generator based on the Message Digest 5 algorithm"),
	N_("Makoto Matsumoto and Takuji Nishimura's generator"),
	N_("The online non-deterministic generator from random.org"),
	N_("A user-provided external shared library"),
	N_("Read dice from a file: "
           "GNU Backgammon will use all characters from the file "
           "in the range '1' to '6' "
           "as dice. When the entire file has been read, it will be "
           "rewinded. You will be prompted for a file "
           "when closing the options dialog.")
    };

  static char *aszCheatRoll[] = {
    N_("best"), N_("second best"), N_("third best"), 
    N_("4th best"), N_("5th best"), N_("6th best"),
    N_("7th best"), N_("8th best"), N_("9th best"),
    N_("10th best"),
    N_("median"),
    N_("10th worst"),
    N_("9th worst"), N_("8th worst"), N_("7th worst"),
    N_("6th worst"), N_("5th worst"), N_("4th worst"),
    N_("third worst"), N_("second worst"), N_("worst"), 
    NULL
  };
    
    static char *aszVariationsTooltips[ NUM_VARIATIONS ] = {
      N_("Use standard backgammon starting position"),
      N_("Use Nick \"Nack\" Ballard's Nackgammon starting position "
         "with standard rules."),
      N_("Play 1-chequer hypergammon (i.e., gammon and backgammons possible)"),
      N_("Play 2-chequer hypergammon (i.e., gammon and backgammons possible)"),
      N_("Play 3-chequer hypergammon (i.e., gammon and backgammons possible)")
    };

    char **ppch;
    GtkWidget *pw, *pwn, *pwp, *pwvbox, *pwhbox, *pwev, *pwf, *pwb,
	*pwAnimBox, *pwFrame, *pwBox, *pwSpeed, *pwScale, *pwhoriz,
	*pwLabelFile, *table, *label;
    unsigned int i;
    unsigned long nRandom;

    BoardData *bd = BOARD( pwBoard )->board_data;

    InitRNG( &nRandom, NULL, FALSE, rngCurrent );
    
    pwn = gtk_notebook_new();
    gtk_container_set_border_width( GTK_CONTAINER( pwn ), 8 );
    
    /* Game options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Game") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pow->pwAutoGame = gtk_check_button_new_with_label (
	_("Start new games immediately"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoGame, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwAutoGame,
			  _("Whenever a game is complete, automatically "
			    "start another one in the same match or "
			    "session."));

    pow->pwAutoRoll = gtk_check_button_new_with_label (
	_("Roll the dice automatically"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoRoll, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwAutoRoll,
			  _("On a human player's turn, if they are not "
			    "permitted to double, then roll the dice "
			    "immediately."));
    
    pow->pwAutoMove = gtk_check_button_new_with_label (
	_("Play forced moves automatically"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoMove, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwAutoMove,
			  _("On a human player's turn, if there are no "
			    "legal moves or only one legal move, then "
			    "finish their turn for them."));

    pow->pwAutoBearoff = gtk_check_button_new_with_label(
	_("Play bearoff moves automatically"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoBearoff, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwAutoBearoff,
			  _("On a human player's turn in a non-contact "
			    "bearoff, if there is an unambiguous move which "
			    "bears off as many chequers as possible, then "
			    "choose that move automatically."));

    pow->pwIllegal = gtk_check_button_new_with_label(
	_("Allow dragging to illegal points"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwIllegal ),
				  fGUIIllegal );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwIllegal, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwIllegal,
			  _("If set, when considering your move you may "
			    "temporarily move chequers onto points which "
			    "cannot be reached with the current dice roll.  "
			    "If unset, you may move chequers only onto "
			    "legal points.  Either way, the resulting move "
			    "must be legal when you pick up the dice, or it "
			    "will not be accepted."));

    pwf = gtk_frame_new( _("Variations") );
    gtk_box_pack_start( GTK_BOX ( pwvbox ), pwf, FALSE, FALSE, 0 );
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);

    for ( i = 0; i < NUM_VARIATIONS; ++i ) {
    
      pow->apwVariations[ i ] = 
        i ? gtk_radio_button_new_with_label_from_widget ( 
               GTK_RADIO_BUTTON( pow->apwVariations[ 0 ] ), 
               gettext ( aszVariations[ i ] ) ) :
        gtk_radio_button_new_with_label ( NULL, 
                                          gettext ( aszVariations[ i ] ) );

      gtk_box_pack_start( GTK_BOX( pwb ), pow->apwVariations[ i ], 
                          FALSE, FALSE, 0 );

      gtk_toggle_button_set_active ( 
               GTK_TOGGLE_BUTTON ( pow->apwVariations[ i ] ), bgvDefault == (bgvariation)i );

      gtk_widget_set_tooltip_text(pow->apwVariations[ i ],
                            gettext ( aszVariationsTooltips[ i ] ));

    }

    /* disable entries if hypergammon databases are not available */

    for ( i = 0; i < 3; ++i )
      gtk_widget_set_sensitive ( 
            GTK_WIDGET ( pow->apwVariations[ i + VARIATION_HYPERGAMMON_1 ] ),
            apbcHyper[ i ] != NULL );

                                                                

    /* Cube options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Cube") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );
    
    pow->pwCubeUsecube = gtk_check_button_new_with_label(
	_("Use doubling cube") );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwCubeUsecube,
			FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwCubeUsecube,
			  _("When the doubling cube is used, under certain "
			    "conditions players may offer to raise the stakes "
			    "of the game by using the \"double\" command."));
    g_signal_connect( G_OBJECT ( pow->pwCubeUsecube ), "toggled",
			G_CALLBACK( UseCubeToggled ), pow );

    pow->pwAutoCrawford = gtk_check_button_new_with_label(
	_("Use Crawford rule"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoCrawford,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwAutoCrawford,
			  _("In match play, the Crawford rule specifies that "
			    "if either player reaches match point (i.e. is "
			    "one point away from winning the match), then "
			    "the doubling cube may not be used for the next "
			    "game only."));
  
    pow->pwCubeJacoby = gtk_check_button_new_with_label(
	_("Use Jacoby rule") );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwCubeJacoby,
			FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwCubeJacoby,
			  _("Under the Jacoby rule, players may not score "
			    "double or triple for a gammon or backgammon "
			    "unless the cube has been doubled and accepted.  "
			    "The Jacoby rule is only ever used in money "
			    "games, not matches."));

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    pow->pwBeaversLabel = gtk_label_new( _("Maximum number of beavers:") );
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwBeaversLabel,
			FALSE, FALSE, 0);

    pow->padjCubeBeaver = GTK_ADJUSTMENT( gtk_adjustment_new (1, 0, 12, 
							      1, 1, 0 ) );
    pow->pwBeavers = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjCubeBeaver),
					  1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwBeavers, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pow->pwBeavers), TRUE);
    gtk_widget_set_tooltip_text( pwev,
			  _("When doubled, a player may \"beaver\" (instantly "
			    "redouble).  This option allows you to specify "
			    "how many consecutive redoubles are permitted.  "
			    "Beavers are only ever used in money games, not "
			    "matches."));

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    pow->pwAutomaticLabel = gtk_label_new (_("Maximum automatic doubles:"));
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwAutomaticLabel,
			FALSE, FALSE, 0);
    
    pow->padjCubeAutomatic = GTK_ADJUSTMENT( gtk_adjustment_new (0, 0, 12, 1,
								 1, 0 ) );
    pow->pwAutomatic = gtk_spin_button_new (GTK_ADJUSTMENT (
						pow->padjCubeAutomatic),
					    1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwAutomatic, TRUE, TRUE, 0);
    gtk_widget_set_tooltip_text( pwev,
			  _("If the opening roll is a double, the players "
			    "may choose to increase the cube value and "
			    "reroll (an \"automatic double\").  This option "
			    "allows you to control how many automatic doubles "
			    "may be applied.  Automatic doubles are only "
			    "ever used in money games, not matches." ));
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pow->pwAutomatic), TRUE);

    /* Tutor options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Tutor") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pow->pwTutor = gtk_check_button_new_with_label (_("Tutor mode"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwTutor, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwTutor,
			  _("When using the tutor, GNU Backgammon will "
			    "analyse your decisions during play and prompt "
			    "you if it thinks you are making a mistake."));
    
    g_signal_connect( G_OBJECT ( pow->pwTutor ), "toggled",
			 G_CALLBACK ( TutorToggled ), pow );

    pow->pwTutorCube = gtk_check_button_new_with_label (_("Cube Decisions"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwTutorCube, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwTutorCube,
			  _("Use the tutor for cube decisions."));
    
    pow->pwTutorChequer = gtk_check_button_new_with_label (_("Chequer play"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwTutorChequer,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwTutorChequer,
			  _("Use the tutor for chequer play decisions."));

    pwf = gtk_frame_new (_("Tutor decisions"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pwf, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);

    pow->pwTutorEvalHint = gtk_radio_button_new_with_label (
	NULL, _("Same as Evaluation"));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwTutorEvalHint,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwTutorEvalHint,
			  _("The tutor will consider your decisions using "
			    "the \"Evaluation\" settings."));
	
    pow->pwTutorEvalAnalysis = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwTutorEvalHint ), _("Same as Analysis"));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwTutorEvalAnalysis,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwTutorEvalAnalysis,
			  _("The tutor will consider your decisions using "
			    "the \"Analysis\" settings."));

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("Warning level:") ),
			FALSE, FALSE, 0);
	pow->pwTutorSkill = gtk_combo_box_new_text();
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwTutorSkill, FALSE, FALSE, 0);
	for( ppch = aszTutor; *ppch; ppch++ ) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (pow->pwTutorSkill), gettext( *ppch ));
	}
	g_assert(nTutorSkillCurrent >= 0 && nTutorSkillCurrent <= 2) ;
    gtk_widget_set_tooltip_text( pwev,
			  _("Specify how bad GNU Backgammon must think a "
			    "decision is before questioning you about a "
			    "possible mistake."));

    gtk_widget_set_sensitive (pow->pwTutorSkill, fTutor);
    gtk_widget_set_sensitive( pow->pwTutorCube, fTutor );
    gtk_widget_set_sensitive( pow->pwTutorChequer, fTutor );
    gtk_widget_set_sensitive( pow->pwTutorEvalHint, fTutor );
    gtk_widget_set_sensitive( pow->pwTutorEvalAnalysis, fTutor );
  
    /* Display options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Display") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pow->pwGameClockwise = gtk_check_button_new_with_label (
	_("Clockwise movement"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwGameClockwise,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwGameClockwise,
			  _("Orient up the board so that player 1's chequers "
			    "advance clockwise (and player 0 moves "
			    "anticlockwise).  Otherwise, player 1 moves "
			    "anticlockwise and player 0 moves clockwise."));
  
    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("Move delay:") ),
			FALSE, FALSE, 0);
    pow->padjDelay = GTK_ADJUSTMENT (gtk_adjustment_new (nDelay, 0,
							 3000, 1, 10, 0) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjDelay), 1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("ms") ),
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text( pwev,
			  _("Set a delay so that GNU Backgammon "
			    "will pause between each move, to give you a "
			    "chance to see it."));

    pow->pwUseDiceIcon = 
	gtk_check_button_new_with_label( _("Show dice below board when human "
					   "player on roll") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwUseDiceIcon ),
				  bd->rd->fDiceArea );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwUseDiceIcon, FALSE, FALSE,
			0 );
    gtk_widget_set_tooltip_text(pow->pwUseDiceIcon,
			  _("When it is your turn to roll, a pair of dice "
			    "will be shown below the board, and you can "
			    "click on them to roll.  Even if you choose not "
			    "to show the dice, you can always roll by "
			    "clicking the area in the middle of the board "
			    "where the dice will land."));
    
    pow->pwShowIDs = gtk_check_button_new_with_label(
	_("Show Position ID and Match ID above board") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwShowIDs ),
				  bd->rd->fShowIDs);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwShowIDs, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwShowIDs,
			  _("One entry field will be shown above the board, "
			    "which can be useful for recording, entering and "
			    "exchanging board positions and match "
			    "situations."));
    
    pow->pwShowPips = gtk_check_button_new_with_label(
	_("Show pip count below board") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwShowPips ),
				  fGUIShowPips );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwShowPips, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwShowPips,
			  _("The \"pip counts\" (number of points each player "
			    "must advance all of their chequers to bear them "
			    "all off) will be shown below the scores."));
	
    pow->pwShowWastage = gtk_check_button_new_with_label(
	_("Show EPC wastage below board") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwShowWastage ),
				  fGUIShowWastage );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwShowWastage, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwShowWastage,
			  _("The \"effective pip count wastage\" "
                            "will be shown below the scores."));
	
    pow->pwShowEPCs = gtk_check_button_new_with_label(
	_("Show EPCs below board") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwShowEPCs ),
				  fGUIShowEPCs );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwShowEPCs, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwShowEPCs,
			  _("The \"effective pip counts\" "
                            "will be shown below the scores."));
	
    pwAnimBox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwAnimBox, FALSE, FALSE, 0 );
    
    pwFrame = gtk_frame_new( _("Animation") );
    gtk_box_pack_start( GTK_BOX( pwAnimBox ), pwFrame, FALSE, FALSE, 4 );

    pwBox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwFrame ), pwBox );

    pow->pwAnimateNone = gtk_radio_button_new_with_label( NULL, _("None") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAnimateNone ),
				  animGUI == ANIMATE_NONE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pow->pwAnimateNone, FALSE, FALSE,
			0 );
    gtk_widget_set_tooltip_text(pow->pwAnimateNone,
			  _("Do not display any kind of animation for "
			    "automatically moved chequers."));
    
    pow->pwAnimateBlink = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwAnimateNone ), _("Blink moving chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAnimateBlink ),
				  animGUI == ANIMATE_BLINK );
    gtk_box_pack_start( GTK_BOX( pwBox ), pow->pwAnimateBlink, FALSE, FALSE,
			0 );
    gtk_widget_set_tooltip_text(pow->pwAnimateBlink,
			  _("When automatically moving chequers, flash "
			    "them between the original and final points."));
    
    pow->pwAnimateSlide = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwAnimateNone ), _("Slide moving chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAnimateSlide ),
				  animGUI == ANIMATE_SLIDE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pow->pwAnimateSlide, FALSE, FALSE,
			0 );
    gtk_widget_set_tooltip_text(pow->pwAnimateSlide,
			  _("Show automatically moved chequers moving across "
			    "the board between the points."));

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwAnimBox ), pwev, FALSE, FALSE, 0 );    
    pwSpeed = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwSpeed );
    
    pow->padjSpeed = GTK_ADJUSTMENT( gtk_adjustment_new( nGUIAnimSpeed, 0, 7,
							 1, 1, 0 ) );
    pwScale = gtk_hscale_new( pow->padjSpeed );
    gtk_widget_set_size_request( pwScale, 100, -1 );
    gtk_scale_set_draw_value( GTK_SCALE( pwScale ), FALSE );
    gtk_scale_set_digits( GTK_SCALE( pwScale ), 0 );

    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Speed:") ),
			FALSE, FALSE, 8 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Slow") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), pwScale, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Fast") ),
			FALSE, FALSE, 4 );
    gtk_widget_set_tooltip_text(pwev,
			  _("Control the rate at which blinking or sliding "
			    "chequers are displayed."));
    
    g_signal_connect( G_OBJECT( pow->pwAnimateNone ), "toggled",
			G_CALLBACK( ToggleAnimation ), pwSpeed );
    ToggleAnimation( pow->pwAnimateNone, pwSpeed );

    pow->pwGrayEdit = gtk_check_button_new_with_label(
	_("Gray board in edit mode") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwGrayEdit ),
				  fGUIGrayEdit );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwGrayEdit, FALSE, FALSE,
			0 );
    gtk_widget_set_tooltip_text(pow->pwGrayEdit,
			  _("Gray board in edit mode to make it clearer"));
    
    pow->pwDragTargetHelp = gtk_check_button_new_with_label(
	_("Show target help when dragging a chequer") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwDragTargetHelp ),
				  fGUIDragTargetHelp );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwDragTargetHelp, FALSE, FALSE, 0 );
    gtk_widget_set_tooltip_text(pow->pwDragTargetHelp,
			  _("The possible target points for a move will be "
			    "indicated by coloured rectangles when a chequer "
			    "has been dragged a short distance."));

    pow->pwDisplay = gtk_check_button_new_with_label (
	_("Display computer moves"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwDisplay, FALSE, FALSE, 0);

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwDisplay ),
				  fDisplay );
    gtk_widget_set_tooltip_text( pow->pwDisplay,
			  _("Show each move made by a computer player.  You "
			    "might want to turn this off when playing games "
			    "between computer players, to speed things "
			    "up."));

    pow->pwSetWindowPos = gtk_check_button_new_with_label(
	_("Restore window positions") );
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwSetWindowPos,
			FALSE, FALSE, 0);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSetWindowPos ),
				  fGUISetWindowPos );
    gtk_widget_set_tooltip_text(pow->pwSetWindowPos,
			  _("Restore the previous size and position when "
			    "recreating windows.  This is really the job "
			    "of the session manager and window manager, "
			    "but since some platforms have poor or missing "
			    "window managers, GNU Backgammon tries to "
			    "do the best it can."));
    
    pow->pwOutputMWC = gtk_check_button_new_with_label (
	_("Match equity as MWC"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwOutputMWC,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwOutputMWC,
			  _("Show match equities as match winning chances.  "
			    "Otherwise, match equities will be shown as "
			    "EMG (equivalent equity in a money game) "
			    "points-per-game."));
    
    pow->pwOutputGWC = gtk_check_button_new_with_label (
	_("GWC as percentage"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwOutputGWC,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwOutputGWC,
			  _("Show game winning chances as percentages (e.g. "
			    "58.3%).  Otherwise, game winning chances will "
			    "be shown as probabilities (e.g. 0.583)."));

    pow->pwOutputMWCpst = gtk_check_button_new_with_label (
	_("MWC as percentage"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwOutputMWCpst,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwOutputMWCpst,
			  _("Show match winning chances as percentages (e.g. "
			    "71.2%).  Otherwise, match winning chances will "
			    "be shown as probabilities (e.g. 0.712)."));

    /* number of digits in output */

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    pw = gtk_label_new( _("Number of digits in output:") );
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, FALSE, FALSE, 0);

    pow->padjDigits = GTK_ADJUSTMENT( gtk_adjustment_new (1, 0, 6, 
							      1, 1, 0 ) );
    pow->pwDigits = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjDigits),
					  1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwDigits, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pow->pwDigits), TRUE);
    gtk_widget_set_tooltip_text( pwev,
			  _("Control the number of digits to be shown after "
                            "the decimal point in "
                            "probabilities and equities. "
                            "This value is used throughout GNU Backgammon, "
                            "i.e., in exported files, hints, analysis etc."
                            "The default value of 3 results "
                            "in equities output as +0.123. "
                            "The equities and probabilities are internally "
                            "stored with 7-8 digits, so it's possible to "
                            "change the value "
                            "after an analysis if you want more digits "
                            "shown in the output. The output of match "
                            "winning chances are derived from this value "
                            "to produce numbers with approximately the same "
                            "number of digits. The default value of 3 results "
                            "in MWCs being output as 50.33%."));


    /* Match options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Match") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox),
			gtk_label_new( _("Default match length:") ),
			FALSE, FALSE, 0);

    pow->padjLength = GTK_ADJUSTMENT( gtk_adjustment_new (nDefaultLength, 0,
							  99, 1, 1, 0 ) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjLength),
			      1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox),
			gtk_label_new( _("length|points") ),
			FALSE, FALSE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_widget_set_tooltip_text( pwev,
			  _("Specify the default length to use when starting "
			    "new matches."));
    
    pwf = gtk_frame_new (_("Match equity table"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pwf, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);

    pwhoriz = gtk_hbox_new (FALSE, 4);
    gtk_container_add (GTK_CONTAINER (pwb), pwhoriz);
    gtk_box_pack_start (GTK_BOX (pwhoriz),
			gtk_label_new( _("Current:") ),
			FALSE, FALSE, 2);
    pwLabelFile = gtk_label_new( (char*) miCurrent.szFileName );
    gtk_box_pack_end(GTK_BOX (pwhoriz), pwLabelFile,
		     FALSE, FALSE, 2);
    pow->pwLoadMET = gtk_button_new_with_label (_("Load..."));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwLoadMET, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pow->pwLoadMET), 2);
    gtk_widget_set_tooltip_text(pow->pwLoadMET,
			  _("Read a file containing a match equity table."));

    g_signal_connect( G_OBJECT ( pow->pwLoadMET ), "clicked",
			 G_CALLBACK ( SetMET ), (gpointer) pwLabelFile );

    pow->pwCubeInvert = gtk_check_button_new_with_label (_("Invert table"));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwCubeInvert, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwCubeInvert,
			  _("Use the specified match equity table around "
			    "the other way (i.e., swap the players before "
			    "looking up equities in the table)."));

    gtk_widget_set_sensitive( pow->pwLoadMET, fCubeUse );
    gtk_widget_set_sensitive( pow->pwCubeInvert, fCubeUse );
  
    /* Sound options */
    pwp = gtk_vbox_new (FALSE, 0);
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Sound") ) );
    AddSoundWidgets(pwp);
	/* Dice options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Dice") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pow->pwHigherDieFirst = gtk_check_button_new_with_label(
	_("Show higher die on left") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwHigherDieFirst ),
				  fGUIHighDieFirst );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwHigherDieFirst, FALSE, FALSE,
			0 );
    gtk_widget_set_tooltip_text(pow->pwHigherDieFirst,
			  _("Force the higher of the two dice to be shown "
			    "on the left."));
    
    pow->pwDiceManual = gtk_radio_button_new_with_label( NULL,
							 _("Manual dice") );
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwDiceManual, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwDiceManual,
			  _("Enter each dice roll by hand." ));
    
    pow->pwDicePRNG = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwDiceManual ),
	_("Random number generator:"));
    gtk_widget_set_tooltip_text(pow->pwDicePRNG,
			  _("GNU Backgammon will generate dice rolls itself, "
			    "with a generator selected from the menu below." ));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwDicePRNG, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pow->pwDiceManual),
				  (rngCurrent == RNG_MANUAL));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pow->pwDicePRNG),
				  (rngCurrent != RNG_MANUAL));

    g_signal_connect( G_OBJECT ( pow->pwDiceManual ), "toggled",
			 G_CALLBACK ( ManualDiceToggled ), pow );

    g_signal_connect( G_OBJECT ( pow->pwDicePRNG ), "toggled",
			 G_CALLBACK ( ManualDiceToggled ), pow );

    pwhbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (pwvbox), pwhbox, TRUE, TRUE, 0);

    pow->pwPRNGMenu = gtk_combo_box_new_text();
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwPRNGMenu, FALSE, FALSE, 0);
    for( i = 1; i < NUM_RNGS; i++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(pow->pwPRNGMenu), gettext( aszRNG[i] ));
		gtk_widget_set_tooltip_text(pw, gettext( aszRNGTip[i] ));
    }

    pow->pwSeed = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwSeed, TRUE,TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pow->pwSeed), gtk_label_new(_("Seed: ")),FALSE,FALSE, 1);

    pow->padjSeed = GTK_ADJUSTMENT (gtk_adjustment_new (nRandom, 0, 16000000,
		    1, 1, 0));
    
    /* a bug in gtk? The adjustment does not work if I choose 
     * maxvalues above 16e6, INT_MAX does not work!  (GTK-1.3 Win32) */
    
    pw = gtk_spin_button_new(GTK_ADJUSTMENT(pow->padjSeed), 1, 0);
    gtk_box_pack_start (GTK_BOX (pow->pwSeed), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
		    
    gtk_widget_set_tooltip_text(pow->pwSeed,
			  _("Specify the \"seed\" (generator state), which "
			    "can be useful in some circumstances to provide "
			    "duplicate dice sequences."));

    gtk_widget_set_sensitive( pow->pwPRNGMenu, (rngCurrent != RNG_MANUAL));
    gtk_widget_set_sensitive( pow->pwSeed,  (rngCurrent != RNG_MANUAL));
    pow->fChanged = 0;
    g_signal_connect( G_OBJECT ( pw ), "changed",
			 G_CALLBACK ( SeedChanged ), &pow->fChanged );   
    

    /* dice manipulation */
    

    /* Enable dice manipulation-widget */

    pow->pwCheat = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwDiceManual ),
        _("Dice manipulation") );

    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwCheat, FALSE, FALSE, 0 );

    /* select rolls for player */

    pow->pwCheatRollBox = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwCheatRollBox, FALSE, FALSE, 0 );

    for ( i = 0; i < 2; ++i ) {

      char *sz;

      pwhbox = gtk_hbox_new (FALSE, 4);
      gtk_box_pack_start( GTK_BOX ( pow->pwCheatRollBox ), 
                          pwhbox, TRUE, TRUE, 0 );

      gtk_box_pack_start( GTK_BOX ( pwhbox ), 
                          gtk_label_new( _("Always roll the ") ),
                          FALSE, FALSE, 0);

      pow->apwCheatRoll[ i ] = gtk_combo_box_new_text ();
      gtk_box_pack_start( GTK_BOX ( pwhbox ), pow->apwCheatRoll[ i ], 
                          FALSE, FALSE, 0);
      gtk_container_set_border_width( GTK_CONTAINER ( pow->apwCheatRoll[ i ]), 
                                      1);

      for( ppch = aszCheatRoll; *ppch; ++ppch )
		  gtk_combo_box_append_text( GTK_COMBO_BOX(pow->apwCheatRoll[i]), gettext( *ppch ));

      sz = g_strdup_printf( _("roll for player %s."), ap[ i ].szName );

      gtk_box_pack_start( GTK_BOX ( pwhbox ), 
                          gtk_label_new( sz ),
                          FALSE, FALSE, 0);

      g_free( sz );

    }

    

    gtk_widget_set_tooltip_text( pow->pwCheat,
			  _("Now it's proven! GNU Backgammon is able to "
                            "manipulate the dice. This is meant as a "
                            "learning tool. Examples of use: (a) learn "
                            "how to double aggressively after a good opening "
                            "sequence, (b) learn to control your temper "
                            "while things are going bad, (c) learn to play "
                            "very good or very bad rolls, or "
                            "(d) just have fun. " ));

    ManualDiceToggled( NULL, pow );

    /* Database options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    relPage = gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp, gtk_label_new( _("Database") ) );
	relPageActivated = FALSE;
    gtk_container_add( GTK_CONTAINER( pwp ), RelationalOptions() );

    /* Other options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Other") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pow->pwConfStart = gtk_check_button_new_with_label (
	_("Confirm when aborting game"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwConfStart,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwConfStart,
			  _("Ask for confirmation when ending a game or "
			    "starting a new game would erase the record "
			    "of the game in progress."));

    pow->pwConfOverwrite = gtk_check_button_new_with_label (
	_("Confirm when overwriting existing files"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwConfOverwrite,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwConfOverwrite,
	_("Confirm when overwriting existing files"));
    
    pow->pwRecordGames = gtk_check_button_new_with_label (
	_("Record all games"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwRecordGames,
			FALSE, FALSE, 0);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwRecordGames ),
                                fRecord );
    gtk_widget_set_tooltip_text(pow->pwRecordGames,
			  _("Keep the game records for all previous games in "
			    "the current match or session.  You might want "
			    "to disable this when playing extremely long "
			    "matches or sessions, to save memory."));
  
    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    /* goto first game upon loading option */

    pow->pwGotoFirstGame = gtk_check_button_new_with_label (
	_("Goto first game when loading matches or sessions"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwGotoFirstGame,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwGotoFirstGame,
			  _("This option controls whether GNU Backgammon "
                            "shows the board after the last move in the "
                            "match, game, or session or whether it should "
                            "show the first move in the first game"));

    /* display styles in game list */

    pow->pwGameListStyles = gtk_check_button_new_with_label (
	_("Display colours for marked moves in game list"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwGameListStyles,
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(pow->pwGameListStyles,
			  _("This option controls whether moves in the "
                            "game list window are shown in different "
                            "colours depending on their analysis"));

    table = gtk_table_new (2, 3, FALSE);
    
    label = gtk_label_new (_("Default SGF folder:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
    pow->pwDefaultSGFFolder =
    gtk_file_chooser_button_new (NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    if (default_sgf_folder)
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (pow->pwDefaultSGFFolder),
    				 default_sgf_folder);
    gtk_table_attach_defaults (GTK_TABLE (table), pow->pwDefaultSGFFolder, 1, 2,
    			   0, 1);
    
    label = gtk_label_new (_("Default Import folder:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
    pow->pwDefaultImportFolder =
    gtk_file_chooser_button_new (NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    if (default_import_folder)
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER
    				 (pow->pwDefaultImportFolder),
    				 default_import_folder);
    gtk_table_attach_defaults (GTK_TABLE (table), pow->pwDefaultImportFolder, 1,
    			   2, 1, 2);
    
    label = gtk_label_new (_("Default Export folder:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
    pow->pwDefaultExportFolder =
    gtk_file_chooser_button_new (NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    if (default_export_folder)
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER
    				 (pow->pwDefaultExportFolder),
    				 default_export_folder);
    gtk_table_attach_defaults (GTK_TABLE (table), pow->pwDefaultExportFolder, 1,
    			   2, 2, 3);

    gtk_box_pack_start (GTK_BOX (pwvbox), table, FALSE, FALSE, 3);

    pwhbox = gtk_hbox_new( FALSE, 4 );
    label = gtk_label_new (_("Web browser:"));
    gtk_box_pack_start (GTK_BOX (pwhbox), label, FALSE, FALSE, 0);

    pow->pwWebBrowser = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (pow->pwWebBrowser), get_web_browser());
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwWebBrowser, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (pwvbox), pwhbox, FALSE, FALSE, 3);


    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );    
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwev ), pw );

	pow->padjCache = GTK_ADJUSTMENT( gtk_adjustment_new( GetEvalCacheSize(), 0, 6, 1, 1, 0 ) );
    pwScale = gtk_hscale_new( pow->padjCache );
    gtk_widget_set_size_request( pwScale, 100, -1 );
    gtk_scale_set_draw_value( GTK_SCALE( pwScale ), TRUE );
	g_signal_connect(G_OBJECT(pwScale), "format-value", G_CALLBACK(CacheSizeString), NULL);
    gtk_scale_set_digits( GTK_SCALE( pwScale ), 0 );

    gtk_box_pack_start( GTK_BOX( pw ), gtk_label_new( _("Cache:") ),
			FALSE, FALSE, 8 );
    gtk_box_pack_start( GTK_BOX( pw ), gtk_label_new( _("Small") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pw ), pwScale, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pw ), gtk_label_new( _("Large") ),
			FALSE, FALSE, 4 );
    gtk_widget_set_tooltip_text( pwev,
			  _("GNU Backgammon uses a cache of previous evaluations to speed up processing.  "
			    "Increasing the size may help evaluations complete more quickly, "
				"but decreasing the size will use less memory." ));

#if USE_MULTITHREAD
    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("Eval Threads:") ),
			FALSE, FALSE, 0);
    pow->padjThreads = GTK_ADJUSTMENT(gtk_adjustment_new (MT_GetNumThreads(), 1, MAX_NUMTHREADS, 1, 1, 0));
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjThreads), 1, 0);
	gtk_widget_set_size_request(GTK_WIDGET(pw), 50, -1);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, FALSE, FALSE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("threads") ),
			FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text( pwev,
			  _("The number of threads to use in multi-threaded operations,"
			  " this should be set to the number of logical processing units available"));
#endif
    /* return notebook */

    return pwn;    
}

char checkupdate_sz[128];
int checkupdate_n;

#define CHECKUPDATE(button,flag,string) \
   checkupdate_n = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( (button) ) ); \
   if ( checkupdate_n != (flag)){ \
           sprintf(checkupdate_sz, (string), checkupdate_n ? "on" : "off"); \
           UserCommand(checkupdate_sz); \
   }

static void SetSoundSettings(void)
{
	int i;
	outputoff();
	fGUIBeep = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(soundBeepIllegal));
	CHECKUPDATE(soundBeepIllegal, fGUIBeep, "set gui beep %s");
	CHECKUPDATE(soundsEnabled, fSound, "set sound enable %s");
	for (i = 0; i < NUM_SOUNDS; i++) 
	{
		if (*soundDetails[i].Path)
			SetSoundFile(i, soundDetails[i].Path);
		else
			SetSoundFile(i, "");
	}
  sound_set_command(gtk_entry_get_text(GTK_ENTRY(pwSoundCommand)));
	outputon();
}

static void OptionsOK(GtkWidget *pw, optionswidget *pow)
{
  char sz[128];
  unsigned int n;
  unsigned int i;
  gchar *filename, *command, *tmp, *newfolder;
  const gchar *new_browser;
  BoardData *bd = BOARD( pwBoard )->board_data;

  gtk_widget_hide( gtk_widget_get_toplevel( pw ) );

  CHECKUPDATE(pow->pwAutoBearoff,fAutoBearoff, "set automatic bearoff %s")
  CHECKUPDATE(pow->pwAutoCrawford,fAutoCrawford, "set automatic crawford %s")
  CHECKUPDATE(pow->pwAutoGame,fAutoGame, "set automatic game %s")
  CHECKUPDATE(pow->pwAutoRoll,fAutoRoll, "set automatic roll %s")
  CHECKUPDATE(pow->pwAutoMove,fAutoMove, "set automatic move %s")
  CHECKUPDATE(pow->pwTutor, fTutor, "set tutor mode %s")
  CHECKUPDATE(pow->pwTutorCube, fTutorCube, "set tutor cube %s")
  CHECKUPDATE(pow->pwTutorChequer, fTutorChequer, "set tutor chequer %s")
  if(gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON (pow->pwTutorEvalHint))) {
	if (fTutorAnalysis)
	  UserCommand( "set tutor eval off");
  } else {
	if (!fTutorAnalysis)
	  UserCommand( "set tutor eval on");
  }

  {
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (pow->pwTutorSkill));
	
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX(pow->pwTutorSkill), &iter)) {
		gchar *selection;
		gtk_tree_model_get (model, &iter, 0, &selection, -1);

		if (! strcmp(selection, gettext(aszTutor[0]))) {			/* N_("Doubtful") */
			if (TutorSkill != SKILL_DOUBTFUL)
			  UserCommand ("set tutor skill doubtful");
		} else if (! strcmp(selection, gettext(aszTutor[1]))) {	/* N_("Bad") */
			if (TutorSkill != SKILL_BAD)
			  UserCommand ("set tutor skill bad");
		} else if (! strcmp(selection, gettext(aszTutor[2]))) {	/* N_("Very Bad") */
			if (TutorSkill != SKILL_VERYBAD)
				UserCommand ("set tutor skill very bad");
		} else {
			/* g_assert(FALSE); Unknown Selection, defaulting */
			if (TutorSkill != SKILL_DOUBTFUL)
				UserCommand ("set tutor skill doubtful");
		}
		g_free(selection);
	}	
  }

  CHECKUPDATE(pow->pwCubeUsecube,fCubeUse, "set cube use %s")
  CHECKUPDATE(pow->pwCubeJacoby,fJacoby, "set jacoby %s")
  CHECKUPDATE(pow->pwCubeInvert,fInvertMET, "set invert met %s")

  CHECKUPDATE(pow->pwGameClockwise,fClockwise, "set clockwise %s")

  for ( i = 0; i < NUM_VARIATIONS; ++i ) 
    if( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pow->apwVariations[ i ] ) ) && bgvDefault != (bgvariation)i )
	{
      sprintf( sz, "set variation %s", aszVariationCommands[ i ] );
      UserCommand( sz );
      break;
    }
  
  CHECKUPDATE(pow->pwOutputMWC,fOutputMWC, "set output mwc %s")
  CHECKUPDATE(pow->pwOutputGWC,fOutputWinPC, "set output winpc %s")
  CHECKUPDATE(pow->pwOutputMWCpst,fOutputMatchPC, "set output matchpc %s")

  if(( n = (unsigned int)pow->padjDigits->value ) != fOutputDigits ){
    sprintf(sz, "set output digits %d", n );
    UserCommand(sz); 
  }
  

  /* ... */
  
  CHECKUPDATE(pow->pwConfStart,fConfirmNew, "set confirm new %s")
  CHECKUPDATE(pow->pwConfOverwrite,fConfirmSave, "set confirm save %s")
  
  if(( n = (unsigned int)pow->padjCubeAutomatic->value ) != cAutoDoubles){
    sprintf(sz, "set automatic doubles %d", n );
    UserCommand(sz); 
  }

  if(( n = (unsigned int)pow->padjCubeBeaver->value ) != nBeavers){
    sprintf(sz, "set beavers %d", n );
    UserCommand(sz); 
  }
  
  if(( n = (unsigned int)pow->padjLength->value ) != nDefaultLength){
    sprintf(sz, "set matchlength %d", n );
    UserCommand(sz); 
  }
  
  if(gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwDiceManual))) {
    if (rngCurrent != RNG_MANUAL)
      UserCommand("set rng manual");  
  } else {

    n = gtk_combo_box_get_active(GTK_COMBO_BOX( pow->pwPRNGMenu ));

    switch ( n ) {
    case 0:
      if (rngCurrent != RNG_ANSI)
        UserCommand("set rng ansi");
      break;
    case 1:
      if (rngCurrent != RNG_BBS)
        UserCommand("set rng bbs");
      break;
    case 2:
      if (rngCurrent != RNG_BSD)
        UserCommand("set rng bsd");
      break;
    case 3:
      if (rngCurrent != RNG_ISAAC)
        UserCommand("set rng isaac");
      break;
    case 4:
      if (rngCurrent != RNG_MD5)
        UserCommand("set rng md5");
      break;
    case 5:
      if (rngCurrent != RNG_MERSENNE)
        UserCommand("set rng mersenne");
      break;
    case 6:
      if (rngCurrent != RNG_RANDOM_DOT_ORG)
        UserCommand("set rng random.org");
      break;
    case 7:
      filename =
	      GTKFileSelect (_("Select file with dice"), NULL, NULL, NULL,
			      GTK_FILE_CHOOSER_ACTION_OPEN);
      if (filename)
      {
	      command = g_strconcat ("set rng file \"", filename, "\"", NULL);
	      UserCommand (command);
	      g_free (command);
	      g_free (filename);
      }
      break;
    default:
      break;
    }

  }       
  
  CHECKUPDATE(pow->pwRecordGames,fRecord, "set record %s" )   
  CHECKUPDATE(pow->pwDisplay,fDisplay, "set display %s" )   

  if((n = (unsigned int)pow->padjCache->value) != GetEvalCacheSize()) {
    SetEvalCacheSize(n);
  }

#if USE_MULTITHREAD
  if((n = (unsigned int)pow->padjThreads->value) != MT_GetNumThreads()) {
    sprintf(sz, "set threads %d", n );
    UserCommand(sz); 
  }
#endif

  if((n = (unsigned int)pow->padjDelay->value) != nDelay) {
    sprintf(sz, "set delay %d", n );
    UserCommand(sz); 
  }

  if( pow->fChanged == 1 ) 
  { 
     n = (unsigned int)pow->padjSeed->value;
     sprintf(sz, "set seed %d", n); 
     UserCommand(sz); 
  }
  
	SetSoundSettings();

  CHECKUPDATE( pow->pwIllegal, fGUIIllegal, "set gui illegal %s" )
  CHECKUPDATE( pow->pwUseDiceIcon, bd->rd->fDiceArea, "set gui dicearea %s" )
  CHECKUPDATE( pow->pwShowIDs, bd->rd->fShowIDs, "set gui showids %s" )
  CHECKUPDATE( pow->pwShowPips, fGUIShowPips, "set gui showpips %s" )
  CHECKUPDATE( pow->pwShowEPCs, fGUIShowEPCs, "set gui showepcs %s" )
  CHECKUPDATE( pow->pwShowWastage, fGUIShowWastage, "set gui showwastage %s" )
  CHECKUPDATE( pow->pwHigherDieFirst, fGUIHighDieFirst, "set gui highdiefirst %s" )
  CHECKUPDATE( pow->pwSetWindowPos, fGUISetWindowPos,
	       "set gui windowpositions %s" )
  CHECKUPDATE( pow->pwGrayEdit, fGUIGrayEdit, "set gui grayedit %s" )
  CHECKUPDATE( pow->pwDragTargetHelp, fGUIDragTargetHelp,
	       "set gui dragtargethelp %s" )

{
#if USE_BOARD3D
	if (display_is_3d(bd->rd))
		updateDiceOccPos(bd, bd->bd3d);
	else
#endif
	if( GTK_WIDGET_REALIZED( pwBoard ) )
	{
		board_create_pixmaps( pwBoard, bd );
		gtk_widget_queue_draw( bd->drawing_area );
	}
}
  if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwAnimateNone ) )
      && animGUI != ANIMATE_NONE )
      UserCommand( "set gui animation none" );
  else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
					     pow->pwAnimateBlink ) )
      && animGUI != ANIMATE_BLINK )
      UserCommand( "set gui animation blink" );
  else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
					     pow->pwAnimateSlide ) )
      && animGUI != ANIMATE_SLIDE )
      UserCommand( "set gui animation slide" );

  if( ( n = (unsigned int)pow->padjSpeed->value ) != nGUIAnimSpeed ) {
      sprintf( sz, "set gui animation speed %d", n );
      UserCommand( sz );
  }

  /* dice manipulation */

  n = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( pow->pwCheat ) );

  if ( (int)n != fCheat ) {
    sprintf( sz, "set cheat enable %s", n ? "on" : "off" );
    UserCommand( sz );
  }

  for ( i = 0; i < 2; ++i ) {
    n = gtk_combo_box_get_active(GTK_COMBO_BOX( pow->apwCheatRoll[ i ] ));
    if ( n != afCheatRoll[ i ] ) {
      sprintf( sz, "set cheat player %d roll %d", i, n + 1 );
      UserCommand( sz );
    }
  }

  CHECKUPDATE( pow->pwGotoFirstGame, fGotoFirstGame, "set gotofirstgame %s" )
  CHECKUPDATE( pow->pwGameListStyles, fStyledGamelist, "set styledgamelist %s" )

  newfolder =
  gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pow->pwDefaultSGFFolder));
  if (newfolder && (!default_sgf_folder || strcmp (newfolder, default_sgf_folder)))
    {
      tmp = g_strdup_printf ("set sgf folder \"%s\"", newfolder);
      UserCommand (tmp);
      g_free (tmp);
    }
  g_free (newfolder);
  
  newfolder =
  gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pow->pwDefaultImportFolder));
  if (newfolder && (!default_import_folder || strcmp (newfolder, default_import_folder)))
    {
      tmp = g_strdup_printf ("set import folder \"%s\"", newfolder);
      UserCommand (tmp);
      g_free (tmp);
    }
  g_free (newfolder);
  
  newfolder =
  gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pow->pwDefaultExportFolder));
  if (newfolder && (!default_export_folder || strcmp (newfolder, default_export_folder)))
    {
      tmp = g_strdup_printf ("set export folder \"%s\"", newfolder);
      UserCommand (tmp);
      g_free (tmp);
    }
  g_free (newfolder);

  new_browser = gtk_entry_get_text(GTK_ENTRY(pow->pwWebBrowser));
  if (new_browser && (!get_web_browser() || strcmp (new_browser, get_web_browser())))
    {
      tmp = g_strdup_printf ("set browser \"%s\"", new_browser);
      UserCommand (tmp);
      g_free (tmp);
    }

	if (relPageActivated)
		RelationalSaveOptions();

  /* Destroy widget on exit */
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void 
OptionsSet( optionswidget *pow) {

  unsigned int i;

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAutoBearoff ),
                                fAutoBearoff );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAutoCrawford ),
                                fAutoCrawford );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAutoGame ),
                                fAutoGame );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAutoMove ),
                                fAutoMove );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAutoRoll ),
                                fAutoRoll );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwTutor ),
                                fTutor );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwTutorCube ),
                                fTutorCube );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwTutorChequer ),
                                fTutorChequer );
  if (!fTutorAnalysis)
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwTutorEvalHint ),
								  TRUE );
  else
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON( pow->pwTutorEvalAnalysis ),
								 TRUE );

	gtk_combo_box_set_active (GTK_COMBO_BOX (pow->pwTutorSkill), nTutorSkillCurrent);
  gtk_adjustment_set_value ( pow->padjCubeBeaver, nBeavers );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwCubeUsecube ),
                                fCubeUse );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwCubeJacoby ),
                                fJacoby );
  gtk_adjustment_set_value ( pow->padjCubeAutomatic, cAutoDoubles );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwCubeInvert ),
                                fInvertMET );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwGameClockwise ),
                                fClockwise );

  for ( i = 0; i < NUM_VARIATIONS; ++i )
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( pow->apwVariations[ i ] ),
                                   bgvDefault == (bgvariation)i );

  if (rngCurrent == RNG_MANUAL)
     gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwDiceManual ),
                                TRUE );
  else
     gtk_combo_box_set_active(GTK_COMBO_BOX( pow->pwPRNGMenu ), (rngCurrent > RNG_MANUAL) ? rngCurrent - 1 : rngCurrent );
 
  /* dice manipulation */

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwCheat ), fCheat );

  for ( i = 0; i< 2; ++i )
    gtk_combo_box_set_active( GTK_COMBO_BOX( pow->apwCheatRoll[ i ] ), afCheatRoll[ i ] );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwOutputMWC ),
                                fOutputMWC );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwOutputGWC ),
                                fOutputWinPC );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwOutputMWCpst ),
                                fOutputMatchPC );

  gtk_adjustment_set_value ( pow->padjDigits, fOutputDigits );


  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwConfStart ),
                                fConfirmNew );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwConfOverwrite ),
                                fConfirmSave );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwGotoFirstGame ),
                                fGotoFirstGame );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwGameListStyles ),
                                fStyledGamelist );
}

static void OptionsPageChange(GtkNotebook *UNUSED(notebook), GtkNotebookPage *UNUSED(page), gint tabNumber, gpointer UNUSED(data))
{
	if (tabNumber == relPage && !relPageActivated)
	{
		RelationalOptionsShown();
		relPageActivated = TRUE;
	}
}

extern void GTKSetOptions( void ) 
{
  GtkWidget *pwDialog, *pwOptions;
  optionswidget ow;

  pwDialog = GTKCreateDialog( _("GNU Backgammon - General options"), DT_QUESTION,
			     NULL, DIALOG_FLAG_MODAL, G_CALLBACK( OptionsOK ), &ow );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
 		        pwOptions = OptionsPages( &ow ) );
  g_signal_connect(G_OBJECT(pwOptions), "switch-page", G_CALLBACK(OptionsPageChange), NULL);

  OptionsSet ( &ow );
 
  GTKRunDialog(pwDialog);
}

static void SoundOK(GtkWidget *pw, void *UNUSED(dummy))
{
	SetSoundSettings();
	gtk_widget_destroy(gtk_widget_get_toplevel(pw));
}

extern void GTKSound(void)
{
	GtkWidget *pwDialog;
	pwDialog = GTKCreateDialog(_("GNU Backgammon - Sound options"), DT_QUESTION,
			NULL, DIALOG_FLAG_MODAL, G_CALLBACK(SoundOK), NULL);

	AddSoundWidgets(DialogArea(pwDialog, DA_MAIN));

	GTKRunDialog(pwDialog);
}
