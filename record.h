/*
 * record.h
 *
 * by Gary Wong <gtw@gnu.org>, 2002.
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

#ifndef RECORD_H
#define RECORD_H

typedef enum _expaverage {
    EXPAVG_TOTAL, EXPAVG_20, EXPAVG_100, EXPAVG_500, NUM_AVG
} expaverage;

typedef struct _playerrecord {
    char szName[ 32 ];
    int cGames;
    float arErrorChequerplay[ NUM_AVG ];
    float arErrorCube[ NUM_AVG ];
    float arErrorCombined[ NUM_AVG ];
    float arLuck[ NUM_AVG ];
} playerrecord;

extern int RecordReadItem( FILE *pf, char *pch, playerrecord *ppr );

extern playerrecord *
GetPlayerRecord( char *szPlayer );

#define GNUBGPR ".gnubg/gnubgpr"

#endif
