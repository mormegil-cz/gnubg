/*
 * bearoff.h
 *
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
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

#ifndef _BEAROFF_H_
#define _BEAROFF_H_


typedef enum _bearofftype {
  BEAROFF_GNUBG,
  BEAROFF_EXACT_BEAROFF,
  BEAROFF_UNKNOWN,
  NUM_BEAROFFS
} bearofftype;

typedef struct _bearoffcontext {

  int h;          /* file handle */
  bearofftype bt; /* type of bearoff database */
  int nPoints;    /* number of points covered by database */
  int nChequers;  /* number of chequers for one-sided database */
  int fInMemory;  /* Is database entirely read into memory? */
  int fTwoSided;  /* one sided or two sided db? */
  int fMalloc;    /* is data malloc'ed? */

  /* one sided dbs */
  int fCompressed; /* is database compressed? */
  int fGammon;     /* gammon probs included */
  int fND;         /* normal distibution instead of exact dist? */
  int fHeuristic;  /* heuristic database? */
  /* two sided dbs */
  int fCubeful;    /* cubeful equities included */

  void *p;        /* pointer to data */

  unsigned long int nReads; /* number of reads */

} bearoffcontext;


enum _bearoffoptions {
  BO_NONE = 0,
  BO_IN_MEMORY = 1,
  BO_MUST_BE_ONE_SIDED = 2,
  BO_MUST_BE_TWO_SIDED = 4,
  BO_HEURISTIC = 8
};

extern bearoffcontext *
BearoffInit ( const char *szFilename, const char *szDir,
              const int bo, void (*pfProgress) ( int ) );

extern bearoffcontext *
BearoffInitBuiltin ( void );

extern int
BearoffEval ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], float arOutput[] );

extern void
BearoffStatus ( bearoffcontext *pbc, char *sz );

extern void
BearoffDump ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz );

extern void
BearoffDist ( bearoffcontext *pbc, const unsigned int nPosID,
              float arProb[ 32 ], float arGammonProb[ 32 ],
              float ar[ 4 ],
              unsigned short int ausProb[ 32 ], 
              unsigned short int ausGammonProb[ 32 ] );

extern int
BearoffCubeful ( bearoffcontext *pbc,
                 const unsigned int iPos,
                 float ar[ 4 ], unsigned short int aus[ 4 ] );

extern void
BearoffClose ( bearoffcontext *pbc );

extern int
isBearoff ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ] );

extern float
fnd ( const float x, const float mu, const float sigma  );

#endif /* _BEAROFF_H_ */
