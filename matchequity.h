/*
 * matchequity.h
 * by Joern Thyssen, 1999-2002
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


#ifndef _MATCHEQUITY_H_
#define _MATCHEQUITY_H_

#include "eval.h"

#define MAXSCORE      64

/* Structure for information about match equity table */

typedef struct _metinfo {

  /* FIXME: use dynamic memory allocation instead? */

  char *szName;          /* Name of match equity table */
  char *szFileName;     /* File name of met */
  char *szDescription;  /* Description of met */
  int nLength;                /* native length of met */
 
} metinfo;

/* macros for getting match equities */

#define GET_MET(i,j,aafMET) ( ( (i) < 0 ) ? 1.0 : ( ( (j) < 0 ) ? 0.0 : \
						 (aafMET [ i ][ j ]) ) )
#define GET_METPostCrawford(i,afBtilde) ( (i) < 0 ? 1.0 : afBtilde [ i ] )

/* current match equity table used by gnubg */

extern float aafMET [ MAXSCORE ][ MAXSCORE ];

extern float afMETPostCrawford [ MAXSCORE ];

extern metinfo miCurrent;

/* Initialise match equity table */

void
InitMatchEquity ( const char *szFileName );

/* Get double points */

extern int
GetPoints ( float arOutput [ 5 ], cubeinfo *pci, float arCP[ 2 ] );

extern float
GetDoublePointDeadCube ( float arOutput [ 5 ], cubeinfo *pci );

#endif

