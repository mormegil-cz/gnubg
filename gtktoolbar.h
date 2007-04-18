/*
 * gtktoolbar.h
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
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

#ifndef _GTKTOOLBAR_H_
#define _GTKTOOLBAR_H_

#include "gtkboard.h"

typedef enum _toolbarcontrol { 
  C_NONE, 
  C_ROLLDOUBLE, 
  C_TAKEDROP, 
  C_AGREEDECLINE,
  C_PLAY } toolbarcontrol;

extern GtkWidget *
ToolbarNew ( void );

extern GtkWidget *
ToolbarGetStopParent ( GtkWidget *pwToolbar );

extern toolbarcontrol
ToolbarUpdate ( GtkWidget *pwToolbar,
                const matchstate *pms,
                const DiceShown diceShown,
                const int fComputerTurn,
                const int fPlaying );

extern int
ToolbarIsEditing( GtkWidget *pwToolbar );

extern void
ToolbarActivateEdit( GtkWidget *pwToolbar );

extern void
ToolbarSetPlaying( GtkWidget *pwToolbar, const int f );

extern void
ToolbarSetClockwise( GtkWidget *pwToolbar, const int f );

extern GtkWidget *
image_from_xpm_d ( char **xpm, GtkWidget *pw );

extern void click_edit();

#endif /* _GTKTOOLBAR_H_ */
