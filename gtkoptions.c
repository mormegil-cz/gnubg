/*
 * gtkoptions.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
 *
 * Based on Sho Sengoku's Equity Temperature Map
 * http://www46.pair.com/sengoku/TempMap/English/TempMap.html
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

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "backgammon.h"
#include "eval.h"
#include "dice.h"
#include "gtkgame.h"
#include "i18n.h"
#include "sound.h"
#include "drawboard.h"
#include "matchequity.h"
#include "format.h"
#include "gtkboard.h"
#include "renderprefs.h"

typedef struct _optionswidget {

  GtkWidget *pwAutoBearoff, *pwAutoCrawford, *pwAutoGame,
            *pwAutoMove, *pwAutoRoll;
  GtkWidget *pwTutor, *pwTutorCube, *pwTutorChequer, *pwTutorSkill,
            *pwTutorEvalHint, *pwTutorEvalAnalysis;
  GtkAdjustment *padjCubeBeaver, *padjCubeAutomatic, *padjLength;
  GtkWidget *pwCubeUsecube, *pwCubeJacoby, *pwCubeInvert;
  GtkWidget *pwGameClockwise, *pwGameEgyptian;
  GtkWidget *apwVariations[ NUM_VARIATIONS ];
  GtkWidget *pwOutputMWC, *pwOutputGWC, *pwOutputMWCpst;
  GtkWidget *pwConfStart, *pwConfOverwrite;
  GtkWidget *pwDiceManual, *pwDicePRNG, *pwPRNGMenu;

  GtkWidget *pwBeavers, *pwBeaversLabel, *pwAutomatic, *pwAutomaticLabel;
  GtkWidget *pwMETFrame, *pwLoadMET, *pwSeed;

  GtkWidget *pwRecordGames, *pwDisplay;
  GtkAdjustment *padjCache, *padjDelay, *padjSeed;
  GtkAdjustment *padjLearning, *padjAnnealing, *padjThreshold;

  GtkWidget *pwSound, *pwSoundArtsC, *pwSoundCommand, *pwSoundESD,
      *pwSoundNAS, *pwSoundNormal, *pwSoundWindows, *pwSoundQuickTime,
      *pwSoundSettings;

  GtkWidget *pwIllegal, *pwUseDiceIcon, *pwShowIDs, *pwShowPips,
      *pwAnimateNone, *pwAnimateBlink, *pwAnimateSlide, *pwBeepIllegal,
      *pwHigherDieFirst, *pwSetWindowPos, *pwDragTargetHelp;
  GtkAdjustment *padjSpeed;

  GtkWidget *pwCheat, *pwCheatRollBox, *apwCheatRoll[ 2 ];
  GtkWidget *pwGotoFirstGame;
  GtkWidget *pwLangMenu;

  GtkWidget *pwSconyers15x15DVD;
  GtkWidget *pwSconyers15x15Disk;
  GtkWidget *pwPathSconyers15x15DVD;
  GtkWidget *pwPathSconyers15x15Disk;
  GtkWidget *pwPathSconyers15x15DVDModify;
  GtkWidget *pwPathSconyers15x15DiskModify;

  GtkAdjustment *padjDigits;
  GtkWidget *pwDigits;
  int fChanged;
} optionswidget;   


static void
SeedChanged( GtkWidget *pw, int *pf ) {

	*pf = 1;  
}

static void
PathSconyersModify( GtkWidget *pw, optionswidget *pow ) {
  
  gchar *sz = gtk_object_get_user_data( GTK_OBJECT( pw ) );
  gchar *pch = g_strdup_printf( _("NOT IMPLEMENTED!\n"
                                  "Please use the command "
                                  "\"%s\" instead"), sz ? sz : "oops" );

  GTKMessage( pch, DT_ERROR );

  g_free( pch );
  
}

static void UseCubeToggled(GtkWidget *pw, optionswidget *pow){

  gint n = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwCubeUsecube ) ); 

  gtk_widget_set_sensitive( pow->pwCubeJacoby, n );
  gtk_widget_set_sensitive( pow->pwBeavers, n );
  gtk_widget_set_sensitive( pow->pwAutomatic, n );
  gtk_widget_set_sensitive( pow->pwBeaversLabel, n );
  gtk_widget_set_sensitive( pow->pwAutomaticLabel, n );
  gtk_widget_set_sensitive( pow->pwAutoCrawford, n );
}

static void ManualDiceToggled( GtkWidget *pw, optionswidget *pow){

  gint n = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwDicePRNG ) ); 
  gint m = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwCheat ) ); 

  gtk_widget_set_sensitive( pow->pwPRNGMenu, n );
  gtk_widget_set_sensitive( pow->pwSeed, n );
  gtk_widget_set_sensitive( pow->pwCheatRollBox, m );

} 

static void TutorToggled (GtkWidget *pw, optionswidget *pow){

  gint n = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwTutor ) ); 

  gtk_widget_set_sensitive( pow->pwTutorCube, n );
  gtk_widget_set_sensitive( pow->pwTutorChequer, n );
  gtk_widget_set_sensitive( pow->pwTutorSkill, n );
  gtk_widget_set_sensitive( pow->pwTutorEvalHint, n );
  gtk_widget_set_sensitive( pow->pwTutorEvalAnalysis, n );

}

static void SoundToggled( GtkWidget *pw, optionswidget *pow ) {

    gtk_widget_set_sensitive( pow->pwSoundSettings,
			      gtk_toggle_button_get_active(
				  GTK_TOGGLE_BUTTON( pow->pwSound ) ) );
}

static void ToggleAnimation( GtkWidget *pw, GtkWidget *pwSpeed ) {

    gtk_widget_set_sensitive( pwSpeed,
                              !gtk_toggle_button_get_active(
                                  GTK_TOGGLE_BUTTON( pw ) ) );
}

static char *aaszLang[][ 2 ] = {
    { N_("System default"), "system" },
    { N_("Czech"),	    "cs_CZ" },
    { N_("Danish"),	    "da_DA" },
    { N_("English (UK)"),   "en_UK" },
    { N_("English (US)"),   "en_US" },
    { N_("French"),	    "fr_FR" },
    { N_("German"),	    "de_DE" },
    { N_("Icelandic"),      "is_IS" },
    { N_("Italian"),	    "it_IT" },
    { N_("Japanese"),	    "ja_JA" },
    { N_("Turkish"),        "tr_TR" },
    { NULL, NULL }
};

static GtkWidget *OptionsPages( optionswidget *pow ) {

    static char *aszRNG[] = {
	N_("ANSI"), N_("Blum, Blum and Shub"), N_("BSD"), N_("ISAAC"),
	N_("MD5"), N_("Mersenne Twister"), N_("random.org"), N_("User"), 
        N_("File"), NULL
    }, *aszRNGTip[] = {
	N_("The rand() generator specified by ANSI C (typically linear "
	   "congruential)"),
	N_("Blum, Blum and Shub's verifiably strong generator"),
	N_("The random() non-linear additive feedback generator from 4.3BSD "
	   "Unix"),
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
    }, *aszTutor[] = {
	N_("Doubtful"), N_("Bad"), N_("Very bad")
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


    char **ppch, **ppchTip;
    GtkWidget *pw, *pwn, *pwp, *pwvbox, *pwhbox, *pwev, *pwm, *pwf, *pwb,
	*pwAnimBox, *pwFrame, *pwBox, *pwSpeed, *pwScale, *pwhoriz,
	*pwLabelFile;
    int cCache;
    int i, nRandom;

    InitRNG( &nRandom, FALSE, rngCurrent );
    
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
    gtk_tooltips_set_tip( ptt, pow->pwAutoGame,
			  _("Whenever a game is complete, automatically "
			    "start another one in the same match or "
			    "session."), NULL );

    pow->pwAutoRoll = gtk_check_button_new_with_label (
	_("Roll the dice automatically"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoRoll, FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwAutoRoll,
			  _("On a human player's turn, if they are not "
			    "permitted to double, then roll the dice "
			    "immediately."), NULL );
    
    pow->pwAutoMove = gtk_check_button_new_with_label (
	_("Play forced moves automatically"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoMove, FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwAutoMove,
			  _("On a human player's turn, if there are no "
			    "legal moves or only one legal move, then "
			    "finish their turn for them."), NULL );

    pow->pwAutoBearoff = gtk_check_button_new_with_label(
	_("Play bearoff moves automatically"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoBearoff, FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwAutoBearoff,
			  _("On a human player's turn in a non-contact "
			    "bearoff, if there is an unambiguous move which "
			    "bears off as many chequers as possible, then "
			    "choose that move automatically."), NULL );

    pow->pwIllegal = gtk_check_button_new_with_label(
	_("Allow dragging to illegal points"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwIllegal ),
				  fGUIIllegal );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwIllegal, FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pow->pwIllegal,
			  _("If set, when considering your move you may "
			    "temporarily move chequers onto points which "
			    "cannot be reached with the current dice roll.  "
			    "If unset, you may move chequers only onto "
			    "legal points.  Either way, the resulting move "
			    "must be legal when you pick up the dice, or it "
			    "will not be accepted."), NULL );

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
               GTK_TOGGLE_BUTTON ( pow->apwVariations[ i ] ), bgvDefault == i );

      gtk_tooltips_set_tip( ptt, pow->apwVariations[ i ],
                            gettext ( aszVariationsTooltips[ i ] ), NULL );

    }

    /* disable entries if hypergammon databases are not available */

    for ( i = 0; i < 3; ++i )
      gtk_widget_set_sensitive ( 
            GTK_WIDGET ( pow->apwVariations[ i + VARIATION_HYPERGAMMON_1 ] ),
            apbcHyper[ i ] != NULL );

                                                                

    pow->pwGameEgyptian = gtk_check_button_new_with_label(
	_("Forbid more than five chequers on a point"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwGameEgyptian,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwGameEgyptian,
			  _("Don't allow players to place more than five "
			    "chequers on a point.  This is sometimes known "
			    "as the Egyptian rule."), NULL );
    
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
    gtk_tooltips_set_tip( ptt, pow->pwCubeUsecube,
			  _("When the doubling cube is used, under certain "
			    "conditions players may offer to raise the stakes "
			    "of the game by using the \"double\" command."),
			  NULL );
    gtk_signal_connect( GTK_OBJECT ( pow->pwCubeUsecube ), "toggled",
			GTK_SIGNAL_FUNC( UseCubeToggled ), pow );

    pow->pwAutoCrawford = gtk_check_button_new_with_label(
	_("Use Crawford rule"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwAutoCrawford,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwAutoCrawford,
			  _("In match play, the Crawford rule specifies that "
			    "if either player reaches match point (i.e. is "
			    "one point away from winning the match), then "
			    "the doubling cube may not be used for the next "
			    "game only."), NULL );
  
    pow->pwCubeJacoby = gtk_check_button_new_with_label(
	_("Use Jacoby rule") );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwCubeJacoby,
			FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pow->pwCubeJacoby,
			  _("Under the Jacoby rule, players may not score "
			    "double or triple for a gammon or backgammon "
			    "unless the cube has been doubled and accepted.  "
			    "The Jacoby rule is only ever used in money "
			    "games, not matches."), NULL );

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    pow->pwBeaversLabel = gtk_label_new( _("Maximum number of beavers:") );
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwBeaversLabel,
			FALSE, FALSE, 0);

    pow->padjCubeBeaver = GTK_ADJUSTMENT( gtk_adjustment_new (1, 0, 12, 
							      1, 1, 1 ) );
    pow->pwBeavers = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjCubeBeaver),
					  1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwBeavers, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pow->pwBeavers), TRUE);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("When doubled, a player may \"beaver\" (instantly "
			    "redouble).  This option allows you to specify "
			    "how many consecutive redoubles are permitted.  "
			    "Beavers are only ever used in money games, not "
			    "matches."), NULL);

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    pow->pwAutomaticLabel = gtk_label_new (_("Maximum automatic doubles:"));
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwAutomaticLabel,
			FALSE, FALSE, 0);
    
    pow->padjCubeAutomatic = GTK_ADJUSTMENT( gtk_adjustment_new (0, 0, 12, 1,
								 1, 1 ) );
    pow->pwAutomatic = gtk_spin_button_new (GTK_ADJUSTMENT (
						pow->padjCubeAutomatic),
					    1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwAutomatic, TRUE, TRUE, 0);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("If the opening roll is a double, the players "
			    "may choose to increase the cube value and "
			    "reroll (an \"automatic double\").  This option "
			    "allows you to control how many automatic doubles "
			    "may be applied.  Automatic doubles are only "
			    "ever used in money games, not matches." ), NULL);
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
    gtk_tooltips_set_tip( ptt, pow->pwTutor,
			  _("When using the tutor, GNU Backgammon will "
			    "analyse your decisions during play and prompt "
			    "you if it thinks you are making a mistake."),
			  NULL );
    
    gtk_signal_connect ( GTK_OBJECT ( pow->pwTutor ), "toggled",
			 GTK_SIGNAL_FUNC ( TutorToggled ), pow );

    pow->pwTutorCube = gtk_check_button_new_with_label (_("Cube Decisions"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwTutorCube, FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwTutorCube,
			  _("Use the tutor for cube decisions."), NULL );
    
    pow->pwTutorChequer = gtk_check_button_new_with_label (_("Chequer play"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwTutorChequer,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwTutorChequer,
			  _("Use the tutor for chequer play decisions."),
			  NULL );

    pwf = gtk_frame_new (_("Tutor decisions"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pwf, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);

    pow->pwTutorEvalHint = gtk_radio_button_new_with_label (
	NULL, _("Same as Evaluation"));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwTutorEvalHint,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwTutorEvalHint,
			  _("The tutor will consider your decisions using "
			    "the \"Evaluation\" settings."), NULL );
	
    pow->pwTutorEvalAnalysis = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwTutorEvalHint ), _("Same as Analysis"));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwTutorEvalAnalysis,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwTutorEvalAnalysis,
			  _("The tutor will consider your decisions using "
			    "the \"Analysis\" settings."), NULL );

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("Warning level:") ),
			FALSE, FALSE, 0);
    pow->pwTutorSkill = gtk_option_menu_new ();
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwTutorSkill, FALSE, FALSE, 0);
    pwm = gtk_menu_new ();
    for( ppch = aszTutor; *ppch; ppch++ )
	gtk_menu_append( GTK_MENU( pwm ),
			 pw = gtk_menu_item_new_with_label(
			     gettext( *ppch ) ) );
    gtk_widget_show_all( pwm );
    gtk_option_menu_set_menu (GTK_OPTION_MENU (pow->pwTutorSkill), pwm );
    gtk_option_menu_set_history (GTK_OPTION_MENU (pow->pwTutorSkill), 0);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("Specify how bad GNU Backgammon must think a "
			    "decision is before questioning you about a "
			    "possible mistake."), NULL );

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
    gtk_tooltips_set_tip( ptt, pow->pwGameClockwise,
			  _("Orient up the board so that player 1's chequers "
			    "advance clockwise (and player 0 moves "
			    "anticlockwise).  Otherwise, player 1 moves "
			    "anticlockwise and player 0 moves clockwise."),
			  NULL );
  
    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("Move delay:") ),
			FALSE, FALSE, 0);
    pow->padjDelay = GTK_ADJUSTMENT (gtk_adjustment_new (nDelay, 0,
							 3000, 1, 10, 10) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjDelay), 1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("ms") ),
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("Set a delay so that GNU Backgammon "
			    "will pause between each move, to give you a "
			    "chance to see it."), NULL );

    pow->pwUseDiceIcon = 
	gtk_check_button_new_with_label( _("Show dice below board when human "
					   "player on roll") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwUseDiceIcon ),
				  fGUIDiceArea );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwUseDiceIcon, FALSE, FALSE,
			0 );
    gtk_tooltips_set_tip( ptt, pow->pwUseDiceIcon,
			  _("When it is your turn to roll, a pair of dice "
			    "will be shown below the board, and you can "
			    "click on them to roll.  Even if you choose not "
			    "to show the dice, you can always roll by "
			    "clicking the area in the middle of the board "
			    "where the dice will land."), NULL );
    
    pow->pwShowIDs = gtk_check_button_new_with_label(
	_("Show Position ID and Match ID above board") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwShowIDs ),
				  fGUIShowIDs );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwShowIDs, FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pow->pwShowIDs,
			  _("Two entry fields will be shown above the board, "
			    "which can be useful for recording, entering and "
			    "exchanging board positions and match "
			    "situations."), NULL );
    
    pow->pwShowPips = gtk_check_button_new_with_label(
	_("Show pip count below board") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwShowPips ),
				  fGUIShowPips );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwShowPips, FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pow->pwShowPips,
			  _("The \"pip counts\" (number of points each player "
			    "must advance all of their chequers to bear them "
			    "all off) will be shown below the scores."),
			  NULL );
	
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
    gtk_tooltips_set_tip( ptt, pow->pwAnimateNone,
			  _("Do not display any kind of animation for "
			    "automatically moved chequers."), NULL );
    
    pow->pwAnimateBlink = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwAnimateNone ), _("Blink moving chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAnimateBlink ),
				  animGUI == ANIMATE_BLINK );
    gtk_box_pack_start( GTK_BOX( pwBox ), pow->pwAnimateBlink, FALSE, FALSE,
			0 );
    gtk_tooltips_set_tip( ptt, pow->pwAnimateBlink,
			  _("When automatically moving chequers, flash "
			    "them between the original and final points."),
			  NULL );
    
    pow->pwAnimateSlide = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwAnimateNone ), _("Slide moving chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwAnimateSlide ),
				  animGUI == ANIMATE_SLIDE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pow->pwAnimateSlide, FALSE, FALSE,
			0 );
    gtk_tooltips_set_tip( ptt, pow->pwAnimateSlide,
			  _("Show automatically moved chequers moving across "
			    "the board between the points."), NULL );

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwAnimBox ), pwev, FALSE, FALSE, 0 );    
    pwSpeed = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwSpeed );
    
    pow->padjSpeed = GTK_ADJUSTMENT( gtk_adjustment_new( nGUIAnimSpeed, 0, 7,
							 1, 1, 0 ) );
    pwScale = gtk_hscale_new( pow->padjSpeed );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_widget_set_size_request( pwScale, 100, -1 );
#else
    gtk_widget_set_usize ( GTK_WIDGET ( pwScale ), 100, -1 );
#endif
    gtk_scale_set_draw_value( GTK_SCALE( pwScale ), FALSE );
    gtk_scale_set_digits( GTK_SCALE( pwScale ), 0 );

    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Speed:") ),
			FALSE, FALSE, 8 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Slow") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), pwScale, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Fast") ),
			FALSE, FALSE, 4 );
    gtk_tooltips_set_tip( ptt, pwev,
			  _("Control the rate at which blinking or sliding "
			    "chequers are displayed."), NULL );
    
    gtk_signal_connect( GTK_OBJECT( pow->pwAnimateNone ), "toggled",
			GTK_SIGNAL_FUNC( ToggleAnimation ), pwSpeed );
    ToggleAnimation( pow->pwAnimateNone, pwSpeed );

    pow->pwDragTargetHelp = gtk_check_button_new_with_label(
	_("Show target help when dragging a chequer") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwDragTargetHelp ),
				  fGUIDragTargetHelp );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwDragTargetHelp, FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pow->pwDragTargetHelp,
			  _("The possible target points for a move will be "
			    "indicated by coloured rectangles when a chequer "
			    "has been dragged a short distance."), NULL );

    pow->pwDisplay = gtk_check_button_new_with_label (
	_("Display computer moves"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwDisplay, FALSE, FALSE, 0);

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwDisplay ),
				  fDisplay );
    gtk_tooltips_set_tip (ptt, pow->pwDisplay,
			  _("Show each move made by a computer player.  You "
			    "might want to turn this off when playing games "
			    "between computer players, to speed things "
			    "up."), NULL );

    pow->pwSetWindowPos = gtk_check_button_new_with_label(
	_("Restore window positions") );
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwSetWindowPos,
			FALSE, FALSE, 0);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSetWindowPos ),
				  fGUISetWindowPos );
    gtk_tooltips_set_tip( ptt, pow->pwSetWindowPos,
			  _("Restore the previous size and position when "
			    "recreating windows.  This is really the job "
			    "of the session manager and window manager, "
			    "but since some platforms have poor or missing "
			    "window managers, GNU Backgammon tries to "
			    "do the best it can."), NULL );
    
    pow->pwOutputMWC = gtk_check_button_new_with_label (
	_("Match equity as MWC"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwOutputMWC,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwOutputMWC,
			  _("Show match equities as match winning chances.  "
			    "Otherwise, match equities will be shown as "
			    "EMG (equivalent equity in a money game) "
			    "points-per-game."), NULL );
    
    pow->pwOutputGWC = gtk_check_button_new_with_label (
	_("GWC as percentage"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwOutputGWC,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwOutputGWC,
			  _("Show game winning chances as percentages (e.g. "
			    "58.3%).  Otherwise, game winning chances will "
			    "be shown as probabilities (e.g. 0.583)."),
			  NULL );

    pow->pwOutputMWCpst = gtk_check_button_new_with_label (
	_("MWC as percentage"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwOutputMWCpst,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwOutputMWCpst,
			  _("Show match winning chances as percentages (e.g. "
			    "71.2%).  Otherwise, match winning chances will "
			    "be shown as probabilities (e.g. 0.712)."),
			  NULL );

    /* number of digits in output */

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    pw = gtk_label_new( _("Number of digits in output:") );
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, FALSE, FALSE, 0);

    pow->padjDigits = GTK_ADJUSTMENT( gtk_adjustment_new (1, 0, 6, 
							      1, 1, 1 ) );
    pow->pwDigits = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjDigits),
					  1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwDigits, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pow->pwDigits), TRUE);
    gtk_tooltips_set_tip (ptt, pwev,
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
                            "in MWCs being output as 50.33%."),
                          NULL );


    /* Match options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Match") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox),
			gtk_label_new( _("Default match length:") ),
			FALSE, FALSE, 0);

    pow->padjLength = GTK_ADJUSTMENT( gtk_adjustment_new (nDefaultLength, 1,
							  99, 1, 1, 1 ) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjLength),
			      1, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox),
			gtk_label_new( _("points") ),
			FALSE, FALSE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("Specify the default length to use when starting "
			    "new matches."), NULL );
    
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
    pwLabelFile = gtk_label_new( miCurrent.szFileName );
    gtk_box_pack_end(GTK_BOX (pwhoriz), pwLabelFile,
		     FALSE, FALSE, 2);
    pow->pwLoadMET = gtk_button_new_with_label (_("Load..."));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwLoadMET, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pow->pwLoadMET), 2);
    gtk_tooltips_set_tip( ptt, pow->pwLoadMET,
			  _("Read a file containing a match equity table."),
			  NULL );

    gtk_signal_connect ( GTK_OBJECT ( pow->pwLoadMET ), "clicked",
			 GTK_SIGNAL_FUNC ( SetMET ), (gpointer) pwLabelFile );

    pow->pwCubeInvert = gtk_check_button_new_with_label (_("Invert table"));
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwCubeInvert, FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwCubeInvert,
			  _("Use the specified match equity table around "
			    "the other way (i.e., swap the players before "
			    "looking up equities in the table)."),
			  NULL );

    gtk_widget_set_sensitive( pow->pwLoadMET, fCubeUse );
    gtk_widget_set_sensitive( pow->pwCubeInvert, fCubeUse );
  
    /* Sound options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Sound") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pow->pwBeepIllegal = gtk_check_button_new_with_label(
	_("Beep on invalid input") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwBeepIllegal ),
				  fGUIBeep );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pow->pwBeepIllegal,
			FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pow->pwBeepIllegal,
			  _("Emit a warning beep if invalid moves "
			    "are attempted."), NULL );
    
    pow->pwSound = gtk_check_button_new_with_label (
	_("Enable sound effects"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwSound, FALSE, FALSE, 0);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSound ),
                                fSound );
#else
    gtk_widget_set_sensitive( pow->pwSound, FALSE );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSound,
			  _("Have GNU Backgammon make sound effects when "
			    "various events occur."), NULL );
    gtk_signal_connect ( GTK_OBJECT ( pow->pwSound ), "toggled",
			 GTK_SIGNAL_FUNC ( SoundToggled ), pow );

    pow->pwSoundSettings = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwvbox ), pow->pwSoundSettings );
    gtk_widget_set_sensitive( pow->pwSoundSettings,
#if USE_SOUND
			      fSound
#else
			      FALSE
#endif
	);

    pwf = gtk_frame_new (_("Sound system"));
    gtk_box_pack_start (GTK_BOX (pow->pwSoundSettings), pwf, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);
    
    pow->pwSoundArtsC = gtk_radio_button_new_with_label( NULL, _("ArtsC"));
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundArtsC, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundArtsC,
#if HAVE_ARTSC
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundArtsC ),
				  ssSoundSystem == SOUND_SYSTEM_ARTSC );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundArtsC,
			  _("Use the ArtsC sound system."), NULL );
						     
    pow->pwSoundCommand = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwSoundArtsC ), _("External command") );
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundCommand, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundCommand,
#if !WIN32
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundCommand ),
				  ssSoundSystem == SOUND_SYSTEM_COMMAND );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundCommand,
			  _("Use an external program to play sounds."), NULL );
						     
    pow->pwSoundESD = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwSoundArtsC ), _("ESD") );
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundESD, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundESD,
#if HAVE_ESD
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundESD ),
				  ssSoundSystem == SOUND_SYSTEM_ESD );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundESD,
			  _("Play sounds through the Enlightenment Sound "
			    "Daemon."), NULL );
						     
    pow->pwSoundNAS = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwSoundArtsC ), _("NAS") );
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundNAS, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundNAS,
#if HAVE_NAS
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundNAS ),
				  ssSoundSystem == SOUND_SYSTEM_NAS );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundNAS,
			  _("Use the Network Audio System."), NULL );
						     
    pow->pwSoundNormal = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwSoundArtsC ), _("Raw device") );
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundNormal, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundNormal,
#if !WIN32
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundNormal ),
				  ssSoundSystem == SOUND_SYSTEM_NORMAL );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundNormal,
			  _("Use the OSS /dev/dsp device for sound (falling "
			    "back to /dev/audio if necessary)."), NULL );
						     
    pow->pwSoundWindows = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwSoundArtsC ), _("MS Windows") );
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundWindows, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundWindows,
#if WIN32
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundWindows ),
				  ssSoundSystem == SOUND_SYSTEM_WINDOWS );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundWindows,
			  _("Play sounds on the MS Windows platform."), NULL );
						         
    pow->pwSoundQuickTime = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwSoundArtsC ), _("QuickTime") );
    gtk_box_pack_start( GTK_BOX( pwb ), pow->pwSoundQuickTime, FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( pow->pwSoundQuickTime,
#ifdef __APPLE__
			      TRUE
#else
			      FALSE
#endif
	);
#if USE_SOUND
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSoundQuickTime ),
				  ssSoundSystem == SOUND_SYSTEM_QUICKTIME );
#endif
    gtk_tooltips_set_tip( ptt, pow->pwSoundQuickTime,
			  _("Play sounds using QuickTime."), NULL );
						         
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
    gtk_tooltips_set_tip( ptt, pow->pwHigherDieFirst,
			  _("Force the higher of the two dice to be shown "
			    "on the left."), NULL );
    
    pow->pwDiceManual = gtk_radio_button_new_with_label( NULL,
							 _("Manual dice") );
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwDiceManual, FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwDiceManual,
			  _("Enter each dice roll by hand." ), NULL );
    
    pow->pwDicePRNG = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pow->pwDiceManual ),
	_("Random number generator:"));
    gtk_tooltips_set_tip( ptt, pow->pwDicePRNG,
			  _("GNU Backgammon will generate dice rolls itself, "
			    "with a generator selected from the menu below." ),
			  NULL );
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwDicePRNG, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pow->pwDiceManual),
				  (rngCurrent == RNG_MANUAL));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pow->pwDicePRNG),
				  (rngCurrent != RNG_MANUAL));

    gtk_signal_connect ( GTK_OBJECT ( pow->pwDiceManual ), "toggled",
			 GTK_SIGNAL_FUNC ( ManualDiceToggled ), pow );

    gtk_signal_connect ( GTK_OBJECT ( pow->pwDicePRNG ), "toggled",
			 GTK_SIGNAL_FUNC ( ManualDiceToggled ), pow );

    pwhbox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (pwvbox), pwhbox, TRUE, TRUE, 0);

    pow->pwPRNGMenu = gtk_option_menu_new ();
    gtk_box_pack_start (GTK_BOX (pwhbox), pow->pwPRNGMenu, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (pow->pwPRNGMenu), 1);
    pwm = gtk_menu_new ();
    for( ppch = aszRNG, ppchTip = aszRNGTip; *ppch; ppch++, ppchTip++ ) {
	gtk_menu_append( GTK_MENU( pwm ),
			 pw = gtk_menu_item_new_with_label(
			     gettext( *ppch ) ) );
	gtk_tooltips_set_tip( ptt, pw, gettext( *ppchTip ), NULL );
    }
    gtk_widget_show_all( pwm );
    gtk_option_menu_set_menu (GTK_OPTION_MENU (pow->pwPRNGMenu), pwm );

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
		    
    gtk_tooltips_set_tip( ptt, pow->pwSeed,
			  _("Specify the \"seed\" (generator state), which "
			    "can be useful in some circumstances to provide "
			    "duplicate dice sequences."), NULL );

    gtk_widget_set_sensitive( pow->pwPRNGMenu, (rngCurrent != RNG_MANUAL));
    gtk_widget_set_sensitive( pow->pwSeed,  (rngCurrent != RNG_MANUAL));
    pow->fChanged = 0;
    gtk_signal_connect ( GTK_OBJECT ( pw ), "changed",
			 GTK_SIGNAL_FUNC ( SeedChanged ), &pow->fChanged );   
    

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

      pow->apwCheatRoll[ i ] = gtk_option_menu_new ();
      gtk_box_pack_start( GTK_BOX ( pwhbox ), pow->apwCheatRoll[ i ], 
                          FALSE, FALSE, 0);
      gtk_container_set_border_width( GTK_CONTAINER ( pow->apwCheatRoll[ i ]), 
                                      1);

      pwm = gtk_menu_new ();
      for( ppch = aszCheatRoll; *ppch; ++ppch )
	gtk_menu_append( GTK_MENU( pwm ),
			 pw = gtk_menu_item_new_with_label(
			     gettext( *ppch ) ) );
      gtk_widget_show_all( pwm );

      gtk_option_menu_set_menu( GTK_OPTION_MENU ( pow->apwCheatRoll[ i ] ), 
                                pwm );

      sz = g_strdup_printf( _("roll for player %s."), ap[ i ].szName );

      gtk_box_pack_start( GTK_BOX ( pwhbox ), 
                          gtk_label_new( sz ),
                          FALSE, FALSE, 0);

      g_free( sz );

    }

    

    gtk_tooltips_set_tip (ptt, pow->pwCheat,
			  _("Now it's proven! GNU Backgammon is able to "
                            "manipulate the dice. This is meant as a "
                            "learning tool. Examples of use: (a) learn "
                            "how to double aggresively after a good opening "
                            "sequence, (b) learn to control your temper "
                            "while things are going bad, (c) learn to play "
                            "very good or very bad rolls, or "
                            "(d) just have fun. "
                            "Oh, BTW, don't tell the trolls in "
                            "rec.games.backgammon :-)" ), NULL );

    ManualDiceToggled( NULL, pow );

    /* Training options */
    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Training") ) );
    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new(
			    _("Learning rate:") ),
			FALSE, FALSE, 0);
    pow->padjLearning = GTK_ADJUSTMENT (gtk_adjustment_new (rAlpha, 0, 1,
							     0.01, 10, 10) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjLearning), 1 , 2);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("Control the rate at which the neural net weights "
			    "are modified during training.  Higher values "
			    "may speed up learning, but lower values "
			    "will retain more existing knowledge."), NULL );

    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new(
			    _("Annealing rate:") ),
			FALSE, FALSE, 0);
    pow->padjAnnealing = GTK_ADJUSTMENT (gtk_adjustment_new (rAnneal, -5, 5,
							     0.1, 10, 10) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjAnnealing), 
			      1, 2);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("Control the rate at which the learning parameter "
			    "decreases during training.  Higher values "
			    "may speed up learning, but lower values "
			    "will be better at escaping local minima."),
			  NULL );
    
    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new(
			    _("Threshold:") ),
			FALSE, FALSE, 0);
    pow->padjThreshold = GTK_ADJUSTMENT (gtk_adjustment_new (rThreshold, 0, 6,
							     0.01, 0.1, 0.1) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjThreshold), 
			      1, 2);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("Specify how much 0-ply and 1-ply evalations "
			    "must disagree before adding a position to "
			    "the database.  Lower values will admit more "
			    "positions."), NULL );

    /* Bearoff options */

    pwp = gtk_alignment_new( 0, 0, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwn ), pwp,
			      gtk_label_new( _("Bearoff") ) );

    pwvbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

    /* 
     * Sconyers 15x15 (browse only) 
     */

    /* enable */

    pwf = gtk_frame_new( _("Sconyers' Bearoff 15x15 (browse only)") );
    gtk_box_pack_start( GTK_BOX ( pwvbox ), pwf, FALSE, FALSE, 0 );
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);

    pow->pwSconyers15x15DVD = 
      gtk_check_button_new_with_label( _("Enable browsing") );
    gtk_box_pack_start (GTK_BOX (pwb), pow->pwSconyers15x15DVD,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwSconyers15x15DVD,
			  _("Enable use of Hugh Sconyers' full 15x15 bearoff "
                            "database via Analyse->Bearoff. This requires "
                            "that you have the disks available on DVD or on "
                            "your harddisk. "), NULL );

    /* path */
    
    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwb ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
                        gtk_label_new( _("Path to files:") ),
                        FALSE, FALSE, 0 );
    
    gtk_box_pack_start( GTK_BOX( pwhbox ),
                        pow->pwPathSconyers15x15DVD = gtk_label_new( "" ),
                        FALSE, FALSE, 0 );

    gtk_box_pack_end( GTK_BOX( pwhbox ),
                      pow->pwPathSconyers15x15DVDModify = 
                      gtk_button_new_with_label( _("Modify") ),
                      FALSE, FALSE, 0 );

    gtk_object_set_data( GTK_OBJECT( pow->pwPathSconyers15x15DVDModify ),
                         "user_data", "set bearoff sconyers 15x15 dvd path" );

    gtk_signal_connect( GTK_OBJECT( pow->pwPathSconyers15x15DVDModify ), 
                        "clicked",
			GTK_SIGNAL_FUNC( PathSconyersModify ), pow );

#if WIN32
    gtk_tooltips_set_tip( ptt, pwev,
			  _("Set path to the bearoff files, e.g., 'D:\\'"), 
                          NULL );
#else
    gtk_tooltips_set_tip( ptt, pwev,
			  _("Set path to the bearoff files, "
                            "e.g., '/mnt/cdrom'"),
                          NULL );
#endif
    
    /* 
     * Sconyers 15x15 (eval + analysis) 
     */

    /* enable */

    pwf = gtk_frame_new( _("Sconyers' Bearoff 15x15 "
                           "(analysis and evaluations)") );
    gtk_box_pack_start( GTK_BOX ( pwvbox ), pwf, FALSE, FALSE, 0 );
    gtk_container_set_border_width (GTK_CONTAINER (pwf), 4);
    pwb = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (pwf), pwb);

    pow->pwSconyers15x15Disk = 
      gtk_check_button_new_with_label( _("Enable use") );

    /* FIXME: */
    gtk_widget_set_sensitive( GTK_WIDGET( pow->pwSconyers15x15Disk ), FALSE );

    gtk_box_pack_start (GTK_BOX (pwb), pow->pwSconyers15x15Disk,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwSconyers15x15Disk,
			  _("Enable use of Hugh Sconyers' full 15x15 bearoff "
                            "database in play, analysis, and evaluations. "
                            "This requires that you've copied the 44 files "
                            "onto your harddisk."), NULL );
    
    /* path */
    
    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwb ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start( GTK_BOX( pwhbox ),
                        gtk_label_new( _("Path to files:") ),
                        FALSE, FALSE, 0 );
    
    gtk_box_pack_start( GTK_BOX( pwhbox ),
                        pow->pwPathSconyers15x15Disk = gtk_label_new( "" ),
                        FALSE, FALSE, 0 );

    gtk_box_pack_end( GTK_BOX( pwhbox ),
                      pow->pwPathSconyers15x15DiskModify = 
                      gtk_button_new_with_label( _("Modify") ),
                      FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( GTK_WIDGET( pow->pwPathSconyers15x15DiskModify ), 
                              FALSE );

    gtk_object_set_data( GTK_OBJECT( pow->pwPathSconyers15x15DiskModify ),
                         "user_data", "set bearoff sconyers 15x15 disk path" );

    gtk_signal_connect( GTK_OBJECT( pow->pwPathSconyers15x15DiskModify ), 
                        "clicked",
			GTK_SIGNAL_FUNC( PathSconyersModify ), pow );

#if WIN32
    gtk_tooltips_set_tip( ptt, pwev,
			  _("Set path to the bearoff files, e.g., "
                            "'C:\\Sconyers Bearoff Files\\'"), 
                          NULL );
#else
    gtk_tooltips_set_tip( ptt, pwev,
			  _("Set path to the bearoff files, "
                            "e.g., '/huge/disk/sconyers/'"),
                          NULL );
#endif
    

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
    gtk_tooltips_set_tip( ptt, pow->pwConfStart,
			  _("Ask for confirmation when ending a game or "
			    "starting a new game would erase the record "
			    "of the game in progress."), NULL );

    pow->pwConfOverwrite = gtk_check_button_new_with_label (
	_("Confirm when overwriting existing files"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwConfOverwrite,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwConfOverwrite,
			  _("Ask for confirmation when writing to a file "
			    "would overwrite data in an existing file with "
			    "the same name."), NULL );
    
    pow->pwRecordGames = gtk_check_button_new_with_label (
	_("Record all games"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwRecordGames,
			FALSE, FALSE, 0);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwRecordGames ),
                                fRecord );
    gtk_tooltips_set_tip( ptt, pow->pwRecordGames,
			  _("Keep the game records for all previous games in "
			    "the current match or session.  You might want "
			    "to disable this when playing extremely long "
			    "matches or sessions, to save memory."), NULL );
  
    pwev = gtk_event_box_new();
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwev, FALSE, FALSE, 0 );
    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_container_add( GTK_CONTAINER( pwev ), pwhbox );

    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("Cache size:") ),
			FALSE, FALSE, 0);
    EvalCacheStats( NULL, &cCache, NULL, NULL );
    pow->padjCache = GTK_ADJUSTMENT (gtk_adjustment_new (cCache, 0, 1<<30,
							  128, 512, 512) );
    pw = gtk_spin_button_new (GTK_ADJUSTMENT (pow->padjCache), 128, 0);
    gtk_box_pack_start (GTK_BOX (pwhbox), pw, TRUE, TRUE, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pw), TRUE);
    gtk_box_pack_start (GTK_BOX (pwhbox), gtk_label_new( _("entries") ),
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip (ptt, pwev,
			  _("GNU Backgammon uses a cache of previous "
			    "evaluations to speed up processing.  Increasing "
			    "the size may help evaluations complete more "
			    "quickly, but decreasing the size will use "
			    "less memory.  Each entry uses around 50 bytes, "
			    "depending on the platform." ), NULL );

    /* goto first game upon loading option */

    pow->pwGotoFirstGame = gtk_check_button_new_with_label (
	_("Goto first game when loading matches or sessions"));
    gtk_box_pack_start (GTK_BOX (pwvbox), pow->pwGotoFirstGame,
			FALSE, FALSE, 0);
    gtk_tooltips_set_tip( ptt, pow->pwGotoFirstGame,
			  _("This option controls whether GNU Backgammon "
                            "shows the board after the last move in the "
                            "match, game, or session or whether it should "
                            "show the first move in the first game"),
                          NULL );

    /* language preference */

    pwhbox = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwvbox ), pwhbox, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Language:") ),
			FALSE, FALSE, 0 );
    pow->pwLangMenu = gtk_option_menu_new();
    gtk_box_pack_start( GTK_BOX( pwhbox ), pow->pwLangMenu, FALSE, FALSE, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pow->pwLangMenu ), 1 );
    pwm = gtk_menu_new();

    {
	int iCurrentSettingPredifined = FALSE;

	for ( i = 0; aaszLang[ i ][ 0 ]; ++i ) {
	    gtk_menu_append( GTK_MENU( pwm ),
			     pw = gtk_menu_item_new_with_label( 
				    gettext( aaszLang[ i ][ 0 ] ) ) );
	    if ( ! strcmp( szLang, aaszLang[ i ][ 1 ] ) )
		iCurrentSettingPredifined = TRUE;
	}
	if ( ! iCurrentSettingPredifined ) {
	    char* sz = NULL;
	    sz = g_strdup_printf( _("User defined: %s"), szLang );
	    gtk_menu_append( GTK_MENU( pwm ),
			     pw = gtk_menu_item_new_with_label( 
				    sz ) );
	    g_free( sz );
	}
    }
    gtk_widget_show_all( pwm );
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pow->pwLangMenu ), pwm );

    /* return notebook */

    return pwn;    
}

#define CHECKUPDATE(button,flag,string) \
   n = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( (button) ) ); \
   if ( n != (flag)){ \
           sprintf(sz, (string), n ? "on" : "off"); \
           UserCommand(sz); \
   }

static void OptionsOK( GtkWidget *pw, optionswidget *pow ){

  char sz[128];
  int n, cCache;
  int i;
  char *pch;

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

  n = gtk_option_menu_get_history(GTK_OPTION_MENU( pow->pwTutorSkill ));
  switch ( n ) {
  case 0:
	if (TutorSkill != SKILL_DOUBTFUL)
	  UserCommand ("set tutor skill doubtful");
	break;

  case 1:
	if (TutorSkill != SKILL_BAD)
	  UserCommand ("set tutor skill bad");
	break;

  case 2:
	if (TutorSkill != SKILL_VERYBAD)
	  UserCommand ("set tutor skill very bad");
	break;

  default:
	UserCommand ("set tutor skill doubtful");
  }
  gtk_widget_set_sensitive( pow->pwTutorSkill, n );

  CHECKUPDATE(pow->pwCubeUsecube,fCubeUse, "set cube use %s")
  CHECKUPDATE(pow->pwCubeJacoby,fJacoby, "set jacoby %s")
  CHECKUPDATE(pow->pwCubeInvert,fInvertMET, "set invert met %s")

  CHECKUPDATE(pow->pwGameClockwise,fClockwise, "set clockwise %s")
  CHECKUPDATE(pow->pwGameEgyptian,fEgyptian, "set egyptian %s")

  for ( i = 0; i < NUM_VARIATIONS; ++i ) 
    if( gtk_toggle_button_get_active( 
              GTK_TOGGLE_BUTTON( pow->apwVariations[ i ] ) ) && bgvDefault != i ) {
      sprintf( sz, "set variation %s", aszVariationCommands[ i ] );
      UserCommand( sz );
      break;
    }
  
  CHECKUPDATE(pow->pwOutputMWC,fOutputMWC, "set output mwc %s")
  CHECKUPDATE(pow->pwOutputGWC,fOutputWinPC, "set output winpc %s")
  CHECKUPDATE(pow->pwOutputMWCpst,fOutputMatchPC, "set output matchpc %s")

  if(( n = pow->padjDigits->value ) != fOutputDigits ){
    sprintf(sz, "set output digits %d", n );
    UserCommand(sz); 
  }
  

  /* bearoff options */

  CHECKUPDATE( pow->pwSconyers15x15DVD, fSconyers15x15DVD,
               "set bearoff sconyers 15x15 dvd enable %s")


  CHECKUPDATE( pow->pwSconyers15x15Disk, fSconyers15x15Disk,
               "set bearoff sconyers 15x15 disk enable %s")

  gtk_label_get( GTK_LABEL( pow->pwPathSconyers15x15Disk ), &pch );
  if ( pch && *pch ) {
    if ( ! szPathSconyers15x15Disk || 
         strcmp( pch, szPathSconyers15x15Disk ) ) {
      sprintf( sz, "set bearoff sconyers 15x15 disk path \"%s\"",
               pch );
      UserCommand( sz );
    }
  }

  gtk_label_get( GTK_LABEL( pow->pwPathSconyers15x15DVD ), &pch );
  if ( pch && *pch ) {
    if ( ! szPathSconyers15x15DVD || 
         strcmp( pch, szPathSconyers15x15DVD ) ) {
      sprintf( sz, "set bearoff sconyers 15x15 dvd path \"%s\"",
               pch );
      UserCommand( sz );
    }
  }

  /* ... */
  
  CHECKUPDATE(pow->pwConfStart,fConfirm, "set confirm new %s")
  CHECKUPDATE(pow->pwConfOverwrite,fConfirmSave, "set confirm save %s")
  
  if(( n = pow->padjCubeAutomatic->value ) != cAutoDoubles){
    sprintf(sz, "set automatic doubles %d", n );
    UserCommand(sz); 
  }

  if(( n = pow->padjCubeBeaver->value ) != nBeavers){
    sprintf(sz, "set beavers %d", n );
    UserCommand(sz); 
  }
  
  if(( n = pow->padjLength->value ) != nDefaultLength){
    sprintf(sz, "set matchlength %d", n );
    UserCommand(sz); 
  }
  
  if(gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pow->pwDiceManual))) {
    if (rngCurrent != RNG_MANUAL)
      UserCommand("set rng manual");  
  } else {

    n = gtk_option_menu_get_history(GTK_OPTION_MENU( pow->pwPRNGMenu ));

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
      if (rngCurrent != RNG_USER)
        UserCommand("set rng user");
      break;
    case 8:
      GTKFileCommand( _("Select file with dice"), NULL, 
                      "set rng file", NULL, 0 );
      break;
    default:
      break;
    }

  }       
  
  CHECKUPDATE(pow->pwRecordGames,fRecord, "set record %s" )   
  CHECKUPDATE(pow->pwDisplay,fDisplay, "set display %s" )   

  EvalCacheStats( NULL, &cCache, NULL, NULL );

  if((n = pow->padjCache->value) != cCache) {
    sprintf(sz, "set cache %d", n );
    UserCommand(sz); 
  }

  if((n = pow->padjDelay->value) != nDelay) {
    sprintf(sz, "set delay %d", n );
    UserCommand(sz); 
  }

  if( pow->padjLearning->value != rAlpha ) 
  { 
     lisprintf(sz, "set training alpha %0.3f", pow->padjLearning->value); 
     UserCommand(sz); 
  }

  if( pow->padjAnnealing->value != rAnneal ) 
  { 
     lisprintf(sz, "set training anneal %0.3f", pow->padjAnnealing->value); 
     UserCommand(sz); 
  }

  if( pow->padjThreshold->value != rThreshold ) 
  { 
     lisprintf(sz, "set training threshold %0.3f", pow->padjThreshold->value); 
     UserCommand(sz); 
  }
  
  if( pow->fChanged == 1 ) 
  { 
     n = pow->padjSeed->value;
     sprintf(sz, "set seed %d", n); 
     UserCommand(sz); 
  }
  
#if USE_SOUND
  CHECKUPDATE(pow->pwSound, fSound, "set sound enable %s")

  if( fSound ) {
      if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
					    pow->pwSoundArtsC ) ) &&
	  ssSoundSystem != SOUND_SYSTEM_ARTSC )
	  UserCommand( "set sound system artsc" );
      else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
						 pow->pwSoundCommand ) ) &&
	       ssSoundSystem != SOUND_SYSTEM_COMMAND )
	  UserCommand( "set sound system command /bin/true" ); /* FIXME */
      else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
						 pow->pwSoundESD ) ) &&
	       ssSoundSystem != SOUND_SYSTEM_ESD )
	  UserCommand( "set sound system esd" );
      else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
						 pow->pwSoundNAS ) )
	       && ssSoundSystem != SOUND_SYSTEM_NAS )
	  UserCommand( "set sound system nas" );
      else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
						 pow->pwSoundNormal ) ) &&
	       ssSoundSystem != SOUND_SYSTEM_NORMAL )
	  UserCommand( "set sound system normal" );
      else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
						 pow->pwSoundWindows ) ) &&
	       ssSoundSystem != SOUND_SYSTEM_WINDOWS )
	  UserCommand( "set sound system windows" );
      else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
						 pow->pwSoundQuickTime ) ) &&
	       ssSoundSystem != SOUND_SYSTEM_QUICKTIME )
	  UserCommand( "set sound system quicktime" );
  }
#endif

  CHECKUPDATE( pow->pwIllegal, fGUIIllegal, "set gui illegal %s" )
  CHECKUPDATE( pow->pwUseDiceIcon, fGUIDiceArea, "set gui dicearea %s" )
  CHECKUPDATE( pow->pwShowIDs, fGUIShowIDs, "set gui showids %s" )
  CHECKUPDATE( pow->pwShowPips, fGUIShowPips, "set gui showpips %s" )
  CHECKUPDATE( pow->pwBeepIllegal, fGUIBeep, "set gui beep %s" )
  CHECKUPDATE( pow->pwHigherDieFirst, fGUIHighDieFirst,
	       "set gui highdiefirst %s" )
  CHECKUPDATE( pow->pwSetWindowPos, fGUISetWindowPos,
	       "set gui windowpositions %s" )
  CHECKUPDATE( pow->pwDragTargetHelp, fGUIDragTargetHelp,
	       "set gui dragtargethelp %s" )

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_2D)
#endif
	if( GTK_WIDGET_REALIZED( pwBoard ) )
	{
		BoardData* bd = BOARD( pwBoard )->board_data;
		board_create_pixmaps( pwBoard, bd );
		gtk_widget_queue_draw( bd->drawing_area );
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

  if( ( n = pow->padjSpeed->value ) != nGUIAnimSpeed ) {
      sprintf( sz, "set gui animation speed %d", n );
      UserCommand( sz );
  }

  /* dice manipulation */

  n = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( pow->pwCheat ) );

  if ( n != fCheat ) {
    sprintf( sz, "set cheat enable %s", n ? "on" : "off" );
    UserCommand( sz );
  }

  for ( i = 0; i < 2; ++i ) {
    n = 
      gtk_option_menu_get_history(GTK_OPTION_MENU( pow->apwCheatRoll[ i ] ));
    if ( n != afCheatRoll[ i ] ) {
      sprintf( sz, "set cheat player %d roll %d", i, n + 1 );
      UserCommand( sz );
    }
  }

  CHECKUPDATE( pow->pwGotoFirstGame, fGotoFirstGame, "set gotofirstgame %s" )
      
  /* language preference */

  n = gtk_option_menu_get_history( GTK_OPTION_MENU( pow->pwLangMenu ) );
  if ( n >= 0 && ( n < sizeof( aaszLang ) / sizeof( aaszLang[0] ) ) &&
       aaszLang[1]) {
    if (aaszLang[ n ][ 1 ] &&  strcmp( szLang, aaszLang[ n ][ 1 ] ) ) {
      sprintf( sz, "set lang %s", aaszLang[ n ][ 1 ] );
      UserCommand( sz );
    }
  }

  /* Destroy widget on exit */
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
  
}

#undef CHECKUPDATE

static void 
OptionsSet( optionswidget *pow) {

  int i;

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

  gtk_option_menu_set_history(GTK_OPTION_MENU( pow->pwTutorSkill ), 
                                 nTutorSkillCurrent );

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
                                   bgvDefault == i );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwGameEgyptian ),
                                fEgyptian );

  if (rngCurrent == RNG_MANUAL)
     gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwDiceManual ),
                                TRUE );
  else
     gtk_option_menu_set_history(GTK_OPTION_MENU( pow->pwPRNGMenu ), 
                                (rngCurrent > RNG_MANUAL) ? rngCurrent - 1 :
                                 rngCurrent );
 
  /* dice manipulation */

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwCheat ), fCheat );

  for ( i = 0; i< 2; ++i )
    gtk_option_menu_set_history( GTK_OPTION_MENU( pow->apwCheatRoll[ i ] ),
                                 afCheatRoll[ i ] );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwOutputMWC ),
                                fOutputMWC );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwOutputGWC ),
                                fOutputWinPC );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwOutputMWCpst ),
                                fOutputMatchPC );

  gtk_adjustment_set_value ( pow->padjDigits, fOutputDigits );


  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwConfStart ),
                                fConfirm );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwConfOverwrite ),
                                fConfirmSave );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwGotoFirstGame ),
                                fGotoFirstGame );

  /* language preference */

  for ( i = 0; aaszLang[ i ][ 0 ]; ++i )
    if ( ! strcmp( szLang, aaszLang[ i ][ 1 ] ) )
      break;
  gtk_option_menu_set_history( GTK_OPTION_MENU( pow->pwLangMenu ), i );

  /* bearoff options */

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSconyers15x15DVD ),
                                fSconyers15x15DVD );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pow->pwSconyers15x15Disk ),
                                fSconyers15x15Disk );

  gtk_label_set( GTK_LABEL( pow->pwPathSconyers15x15DVD ),
                 szPathSconyers15x15DVD ? szPathSconyers15x15DVD : "" );

  gtk_label_set( GTK_LABEL( pow->pwPathSconyers15x15Disk ),
                 szPathSconyers15x15Disk ? szPathSconyers15x15Disk : "" );


}

extern void 
GTKSetOptions( void ) {

  GtkWidget *pwDialog, *pwOptions;
  optionswidget ow;

  pwDialog = GTKCreateDialog( _("GNU Backgammon - General options"), DT_QUESTION,
			     GTK_SIGNAL_FUNC( OptionsOK ), &ow );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
 		        pwOptions = OptionsPages( &ow ) );
  gtk_widget_show_all( pwDialog );

  OptionsSet ( &ow );
 
  gtk_main();

}
