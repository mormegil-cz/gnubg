/*
 * extwin.h
 *
 * by Gary Wong, 1997
 *
 */

#ifndef _EXTWIN_H_
#define _EXTWIN_H_

#include <ext.h>

extern extwindowclass ewcText;

extern extwindowclass ewcPushButton;
extern extwindowclass ewcCheckButton;
extern extwindowclass ewcRadioButton;

/* Text properties */
#define TP_TEXT 1

/* Button notifies */
#define BN_CLICK 1

/* Button properties */
#define BP_LABEL 1

extern extwindowclass ewcScrollBar;

/* Scroll bar notifies */

#define SBN_POSITION 1
#define SBN_TRACK 2

/* Scroll bar properties */

#define SBP_CONFIG 1

extern extwindowclass ewcListBox;

/* List box notifies */

#define LBN_SELECT 1
#define LBN_ENTER 2

extern extwindowclass ewcEntryField;

/* Entry field notifies */

#define EFN_CHANGE 1

/* Entry field properties */

#define EFP_SIZE 1
#define EFP_TEXT 2

/* FIXME not a good interface! */
extern char *ExtEntryFieldString( extwindow *pewnd );

#endif
