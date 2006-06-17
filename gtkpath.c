/*
 * gtkpath.c
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
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "backgammon.h"
#include "gtkgame.h"
#include <glib/gi18n.h>




typedef struct _pathdata {

  GtkWidget *apwPath[ NUM_PATHS ];

} pathdata;


static void
SetPathAsDefault ( GtkWidget *pw, pathdata *ppd ) {
  
  int *pi;

  pi = gtk_object_get_data ( GTK_OBJECT ( pw ), "user_data" );

  gtk_label_set_text ( GTK_LABEL ( ppd->apwPath[ *pi ] ), 
                       aaszPaths[ *pi ][ 1 ] );

}


static void
ModifyPath ( GtkWidget *pw, pathdata *ppd ) {

  /*
    
    int *pi;
    
    pi = gtk_object_get_data ( GTK_OBJECT ( pw ), "user_data" );

    FIXME: implement SelectPath 

    gtk_label_set_text ( GTK_LABEL ( ppd->apwPath[ *pi ] ), 
    pc = SelectPath ( "Select Path", 
    aaszPaths[ *pi ][ 0 ] ) );
  */

  GTKMessage ( _("NOT IMPLEMENTED!\n"
            "Use the \"set path\" command instead!"), DT_ERROR );

}


static void 
SetPath ( GtkWidget *pw, pathdata *ppd, int fOK ) {

  int i;
  gchar *pc;

  for ( i = 0; i < NUM_PATHS; i++ ) {
    gtk_label_get ( GTK_LABEL ( ppd->apwPath[ i ] ), &pc );
    strcpy ( aaszPaths[ i ][ 0 ], pc );
  }


  if( fOK )
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}


static void 
PathOK ( GtkWidget *pw, void *p ) {
  SetPath ( pw, p, TRUE );
}


static void 
PathApply ( GtkWidget *pw, void *p ) {
  SetPath ( pw, p, FALSE );
}


extern void
GTKShowPath ( void ) {

  GtkWidget *pwDialog;
  GtkWidget *pwApply;
  GtkWidget *pwVBox;
  GtkWidget *pw;
  GtkWidget *pwHBox;
  GtkWidget *pwNotebook;

  pathdata pd;

  int i;
  int *pi;

  char *aaszPathNames[][ NUM_PATHS ] = {
    { N_("Export of Encapsulated PostScript .eps files") , 
      N_("Encapsulated PostScript") },
    { N_("Import or export of Jellyfish .gam files") , 
      N_("Jellyfish .gam") },
    { N_("Export of HTML files") , 
      N_("HTML") },
    { N_("Export of LaTeX files") , 
      N_("LaTeX") },
    { N_("Import or export of Jellyfish .mat files") , 
      N_("Jellyfish .mat") },
    { N_("Import of FIBS oldmoves files") , 
      N_("FIBS oldmoves") },
    { N_("Export of PDF files") , 
      N_("PDF") },
    { N_("Export of PNG positions") , 
      N_("PNG") },
    { N_("Import of Jellyfish .pos files") , 
      N_("Jellyfish .pos") },
    { N_("Export of PostScript files") , 
      N_("PostScript") },
    { N_("Load and save of SGF files") , 
      N_("SGF (gnubg)") },
    { N_("Import of GamesGrid SGG files") , 
      N_("GamesGrid SGG") },
    { N_("Export of text files"), 
      N_("Text") },
    { N_("Loading of match equity files (.xml)"), 
      N_("Match Equity Tables") },
    { N_("Loading of TrueMoneyGames files (.tmg)"), 
      N_("TrueMoneyGames TMG") },
    { N_("Loading of BKG files"),
      N_("BKG") },
    { N_("Loading of Snowie .txt files"),
      N_("Snowie .txt") }
  };

  
  pwDialog = GTKCreateDialog( _("GNU Backgammon - Paths"), DT_QUESTION,
                           GTK_SIGNAL_FUNC ( PathOK ), &pd );
    
  pwApply = gtk_button_new_with_label( _("Apply") );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
                     pwApply );
  gtk_signal_connect( GTK_OBJECT( pwApply ), "clicked",
                      GTK_SIGNAL_FUNC( PathApply ), &pd );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     pwNotebook = gtk_notebook_new() );
  gtk_notebook_set_scrollable( GTK_NOTEBOOK( pwNotebook ), TRUE );
  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );

  /* content of widget */

  for ( i = 0; i < NUM_PATHS; i++ ) {

    pwVBox = gtk_vbox_new ( FALSE, 4 );

    pw = gtk_label_new ( gettext ( aaszPathNames[ i ][ 0 ] ) );
    gtk_box_pack_start ( GTK_BOX ( pwVBox ), pw, FALSE, TRUE, 4 );

    /* default */
    
    pi = malloc ( sizeof ( int ) );
    *pi = i;

    pwHBox = gtk_hbox_new ( FALSE, 4 );

    pw = gtk_label_new ( _("Default: ") );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = pd.apwPath[ i ] = gtk_label_new ( aaszPaths[ i ][ 0 ] );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = gtk_button_new_with_label ( _("Modify...") );
    gtk_box_pack_end ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );
    gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                         GTK_SIGNAL_FUNC ( ModifyPath ),
                         &pd );
    gtk_object_set_data_full( GTK_OBJECT( pw ), "user_data", pi, free );

    gtk_box_pack_start ( GTK_BOX ( pwVBox ), pwHBox, FALSE, TRUE, 4 );

    /* current */

    pi = malloc ( sizeof ( int ) );
    *pi = i;

    pwHBox = gtk_hbox_new ( FALSE, 4 );

    pw = gtk_label_new ( _("Current: ") );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = gtk_label_new ( aaszPaths[ i ][ 1 ] );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = gtk_button_new_with_label ( _("Set as default") );
    gtk_box_pack_end ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                         GTK_SIGNAL_FUNC ( SetPathAsDefault ),
                         &pd );
    gtk_object_set_data_full( GTK_OBJECT( pw ), "user_data", pi, free );


    gtk_box_pack_start ( GTK_BOX ( pwVBox ), pwHBox, FALSE, TRUE, 4 );

    /* add page */

    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
                              pwVBox,
			      gtk_label_new( 
                                 gettext ( aaszPathNames[ i ][ 1 ] ) ) ) ;

  }

  /* signals, modality, etc */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
		      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
  gtk_widget_show_all( pwDialog );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}

