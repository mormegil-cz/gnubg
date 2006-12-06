/*
 * format.c
 *
 * by Joern Thyssen  <jth@gnubg.org>, 2003
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

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include "backgammon.h"
#include "eval.h"
#include "format.h"

#include "export.h"


int fOutputMWC = TRUE;
int fOutputWinPC = FALSE;
int fOutputMatchPC = TRUE;
int fOutputDigits = 3;
float rErrorRateFactor = 1000.0f;


extern char *
OutputRolloutResult( const char *szIndent,
		     char asz[][ 1024 ],
		     const  float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
		     const float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
		     const cubeinfo aci[],
		     const int cci,
		     const int fCubeful ) {

  static char sz[ 1024 ];
  int ici;
  char *pc;

  strcpy ( sz, "" );

  for ( ici = 0; ici < cci; ici++ ) {

    /* header */

    if ( asz && *asz[ ici ] ) {
      if ( szIndent && *szIndent )
        strcat ( sz, szIndent );
      sprintf ( pc = strchr ( sz, 0 ), "%s:\n", asz[ ici ] );
    }

    /* output */

    if ( szIndent && *szIndent )
      strcat ( sz, szIndent );

    strcat ( sz, "  " );
    strcat ( sz, OutputPercents ( aarOutput[ ici ], TRUE ) );
    strcat ( sz, " CL " );
    strcat ( sz, OutputEquityScale ( aarOutput[ ici ][ OUTPUT_EQUITY ], 
                                     &aci[ ici ], &aci[ 0 ], TRUE ) );

    if ( fCubeful ) {
      strcat ( sz, " CF " );
      strcat ( sz, OutputMWC ( aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ], 
                               &aci[ 0 ], TRUE ) );
    }

    strcat ( sz, "\n" );

    /* std dev */

    if ( szIndent && *szIndent )
      strcat ( sz, szIndent );

    strcat ( sz, " [" );
    strcat ( sz, OutputPercents ( aarStdDev[ ici ], FALSE ) );
    strcat ( sz, " CL " );
    strcat ( sz, OutputEquityScale ( aarStdDev[ ici ][ OUTPUT_EQUITY ], 
                                     &aci[ ici ], &aci[ 0 ], FALSE ) );

    if ( fCubeful ) {
      strcat ( sz, " CF " );
      strcat ( sz, OutputMWC ( aarStdDev[ ici ][ OUTPUT_CUBEFUL_EQUITY ], 
                               &aci[ 0 ], FALSE ) );
    }

    strcat ( sz, "]\n" );

  }

  return sz;

}


extern char *
OutputEvalContext ( const evalcontext *pec, const int fChequer ) {

  static char sz[ 1024 ];
  char *pc;
  int i;
  
  sprintf ( sz, _("%d-ply %s"), 
            pec->nPlies, 
            ( ! fChequer || pec->fCubeful ) ? _("cubeful") : _("cubeless") );

#if defined( REDUCTION_CODE )
  if ( pec->nPlies == 2 ) 
    sprintf ( pc = strchr ( sz, 0 ),
              " %d%% speed",
              (pec->nReduced) ? 100 / pec->nReduced : 100 );
#else
  if( pec->fUsePrune ) {
    sprintf( pc = strchr ( sz, 0 ), " prune" );
  }
#endif
	    
  if ( fChequer && pec->nPlies ) {
    /* FIXME: movefilters!!! */
  }

  if ( pec->rNoise > 0.0f )
    sprintf ( pc = strchr ( sz, 0 ),
              ", noise %0.3g (%s)",
              pec->rNoise,
              pec->fDeterministic ? "d" : "nd" );

  for ( i = 0; i < NUM_SETTINGS; i++ )

    if ( ! cmp_evalcontext ( &aecSettings[ i ], pec ) ) {
      sprintf ( pc = strchr ( sz, 0 ),
                " [%s]",
                gettext ( aszSettings[ i ] ) );
      break;
    }

  return sz;

}



/* return -1 if this isn't a predefined setting
           n = index of predefined setting if it's a match or
               if the eval context matches and it's 0 ply
 */
extern int
GetPredefinedChequerplaySetting( const evalcontext *pec,
                                 movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  int nEval;
  int nFilter;
  int nPlies = pec->nPlies;
  int i;
  int fSame;
  int nPreset;
  int Accept;

  for (nEval = 0; nEval < NUM_SETTINGS; ++nEval) {
    if (cmp_evalcontext( aecSettings + nEval, pec) == 0) {
      /* eval matches and it's 0 ply, we have a predefined one */
      if (nPlies == 0)
	return nEval;

      /* see if there's a filter set which matches and uses the
	 same eval context (world class and supremo use the same
         eval context and different filters) */
      for (nFilter = 0; nFilter < NUM_SETTINGS; ++nFilter) {
	/* see what filter set goes with the predefined settings */
	if ((nPreset = aiSettingsMoveFilter[nFilter]) < 0)
	  continue;

	/* nPreset = 0/tiny, 1/normal, 2/large, 3/huge */
	fSame = 1;
	for (i = 0; i < nPlies - 1; ++i) {
	  if ((Accept = aamf[ nPlies - 1 ][ i ].Accept) !=
	      aaamfMoveFilterSettings[nPreset][nPlies - 1 ][ i ].Accept) {
	    fSame = 0;
	    break;
	  }

	  /* if they are both ignore this level, don't check 
	     the extra and threshold */
	  if (Accept < 0)
	    continue;

	  if (aamf[ nPlies - 1 ][ i ].Extra !=
	      aaamfMoveFilterSettings[nPreset][nPlies - 1 ][ i ].Extra) {
	    fSame = 0;
	    break;
	  }

	  if (fabs (aamf[ nPlies - 1 ][ i ].Threshold - 
	   aaamfMoveFilterSettings[nPreset][nPlies - 1 ][ i ].Threshold)
	      > 0.1e-6) {
	    fSame = 0;
	    break;
	  }
	}
	/* the filters match (for this nPlies, so we may have a 
	   preset */
	if (fSame && (nFilter == nEval))
	  return nEval;
      }
    }
  }

  return -1;
}

extern char *
OutputMoveFilterPly( const char *szIndent, 
                     const int nPlies,
                     movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  static char sz[ 1024 ];
  int i;

  strcpy( sz, "" );

  if ( ! nPlies ) 
    return sz;
    
  for ( i = 0; i < nPlies; ++i ) {

    movefilter *pmf = &aamf[ nPlies - 1 ][ i ];

    if ( szIndent && *szIndent )
      strcat( sz, szIndent );

    if ( pmf->Accept < 0 ) {
      sprintf( strchr( sz, 0 ), 
               _("Skip pruning for %d-ply moves.\n"),
               i );
      continue;
    }

    if ( pmf->Accept == 1 )
      sprintf( strchr( sz, 0 ),
               _("keep the best %d-ply move"), i );
    else
      sprintf( strchr( sz, 0 ),
               _("keep the first %d %d-ply moves"),
               pmf->Accept, i );

    if ( pmf->Extra )
      sprintf( strchr( sz, 0 ),
               _(" and up to %d more moves within equity %0.3g\n"),
               pmf->Extra, pmf->Threshold );
    else
      strcat( sz, "\n" );

  }

  return sz;

}
                                 
static void
OutputEvalContextsForRollout( char *sz, const char *szIndent,
                              const evalcontext aecCube[ 2 ],
                              const evalcontext aecChequer[ 2 ],
                              movefilter aaamf[ 2 ][ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  int fCube = ! cmp_evalcontext ( &aecCube[ 0 ], &aecCube[ 1 ] );
  int fChequer = ! cmp_evalcontext ( &aecChequer[ 0 ], &aecChequer[ 1 ] );
  int fMovefilter = fChequer && 
    ( !aecChequer[ 0 ].nPlies || 
      equal_movefilter( aecChequer[ 0 ].nPlies - 1,
                        aaamf[ 0 ][ aecChequer[ 0 ].nPlies - 1 ],
                        aaamf[ 1 ][ aecChequer[ 0 ].nPlies - 1 ] ) );
  int fIdentical = fCube && fChequer && fMovefilter;
  int i;
  int j;

  for ( i = 0; i < 1 + ! fIdentical; i++ ) {

    if ( ! fIdentical ) {

      if ( szIndent && *szIndent )
        strcat ( sz, szIndent );

      sprintf ( strchr ( sz, 0 ), _("Player %d:\n" ), i );
    }

    /* chequer play */

    j = GetPredefinedChequerplaySetting( &aecChequer[ i ], aaamf[ i ] );

    if ( szIndent && *szIndent )
      strcat ( sz, szIndent );

    if ( aecChequer[ i ].nPlies ) {

      sprintf( strchr( sz, 0 ),
               _("Play: %s "), 
               ( j < 0 ) ? "" : gettext ( aszSettings[ j ] ) );

      strcat( sz, OutputEvalContext ( &aecChequer[ i ], TRUE ) );
      strcat( sz, "\n" );
      strcat( sz, OutputMoveFilterPly( szIndent, aecChequer[ i ].nPlies, 
                                       aaamf[ i ] ) );

    }
    else {
      strcat ( sz, _("Play: ") );
      strcat ( sz, OutputEvalContext ( &aecChequer[ i ], FALSE ) );
      strcat ( sz, "\n" );
    }

    if ( szIndent && *szIndent )
      strcat ( sz, szIndent );

    strcat ( sz, _("Cube: ") );
    strcat ( sz, OutputEvalContext ( &aecCube[ i ], FALSE ) );
    strcat ( sz, "\n" );

  }

}



extern char *
OutputRolloutContext ( const char *szIndent, const evalsetup *pes ) {

  const rolloutcontext *prc = &pes->rc;
  static char sz[ 1024 ];

  strcpy ( sz, "" );

  if ( szIndent && *szIndent )
    strcat ( sz, szIndent );

  if ( prc->nTruncate && prc->fDoTruncate)
    sprintf ( strchr ( sz, 0 ),
              prc->fCubeful ?
              _("Truncated cubeful rollout (depth %d)") :
              _("Truncated cubeless rollout (depth %d)"), 
              prc->nTruncate );
  else
    sprintf ( strchr ( sz, 0 ),
              prc->fCubeful ? 
              _("Full cubeful rollout") :
              _("Full cubeless rollout") );

  if ( prc->fTruncBearoffOS && ! prc->fCubeful )
    strcat ( sz, _(" (trunc. at one-sided bearoff)") );
  else if ( prc->fTruncBearoff2 && ! prc->fCubeful )
    strcat ( sz, _(" (trunc. at exact bearoff)") );

  sprintf ( strchr ( sz, 0 ),
            prc->fVarRedn ? _(" with var.redn.") : _(" without var.redn.") );

  strcat ( sz, "\n" );
  
  if ( szIndent && *szIndent )
    strcat ( sz, szIndent );

  sprintf ( strchr ( sz, 0 ),
            "%d games",
            prc->nGamesDone );

  if ( prc->fInitial )
    strcat ( sz, ", rollout as initial position" );

  if ( prc->fRotate ) 
    sprintf ( strchr ( sz, 0 ),
              _(", %s dice gen. with seed %d and quasi-random dice\n"),
              gettext( aszRNG[ prc->rngRollout ] ),
              prc->nSeed );
  else
    sprintf ( strchr ( sz, 0 ),
              _(", %s dice generator with seed %d\n"),
              gettext( aszRNG[ prc->rngRollout ] ),
              prc->nSeed );

   if ( ( prc->fStopOnJsd || prc->fStopMoveOnJsd || prc->fStopOnSTD )
        && szIndent && *szIndent )
     strcat ( sz, szIndent );

    /* stop on std.err */

   if ( prc->fStopOnSTD && !prc->fStopMoveOnJsd )
    sprintf( strchr( sz, 0 ),
             _("Stop when std.errs. are small enough: ratio "
               "%.4g (min. %d games)\n"),
             prc->rStdLimit, prc->nMinimumGames );

   /* stop on JSD */
   if ( prc->fStopOnJsd || prc->fStopMoveOnJsd )
     sprintf( strchr( sz, 0 ),
              _("Stop when best play is enough JSDs ahead: limit "
                "%.4g (min. %d games)\n"),
              prc->rJsdLimit, prc->nMinimumJsdGames );

  /* first play */

  OutputEvalContextsForRollout( sz, szIndent, 
                                prc->aecCube, prc->aecChequer,
                                ((rolloutcontext *)prc)->aaamfChequer );

  /* later play */

  if ( prc->fLateEvals ) {

    if ( szIndent && *szIndent )
      strcat ( sz, szIndent );
    
    sprintf( strchr( sz, 0 ), 
             _("Different evaluations after %d plies:\n"),
             prc->nLate );

    OutputEvalContextsForRollout( sz, szIndent, 
                                  prc->aecCubeLate, prc->aecChequerLate,
                                  ((rolloutcontext *)prc)->aaamfLate );


  }

  return sz;

}

/*
 * Return formatted string with equity or MWC.
 *
 * Input:
 *    r: equity (either money equity for normalised money eq. for match play
 *    pci: cubeinfo
 *    f: indicates equity (TRUE) or std. error (FALSE)
 *    
 *
 * Important: function is not re-entrant. Caller must save output
 * if needed.
 */

extern char *
OutputEquity ( const float r, const cubeinfo *pci, const int f ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {
    /* fmt: "%+7.3f" or "%7.3f" */
    sprintf( fmt, f ? "%%+%d.%df" : "%%%d.%df", 
             fOutputDigits + 4, fOutputDigits );
    sprintf ( sz, fmt, r );
  }
  else {
    if ( fOutputMatchPC ) {
      /* fmt: "%6.2f%%" */
      sprintf( fmt, "%%%d.%df%%%%", fOutputDigits + 3, fOutputDigits > 1 ? fOutputDigits - 1 : 0 );
      sprintf ( sz, fmt,
                100.0f * ( f ? eq2mwc ( r, pci ) : se_eq2mwc ( r, pci ) ) );
    }
    else {
      /* fmt: "%6.4f" */
      sprintf( fmt, "%%+%d.%df", fOutputDigits + 3, fOutputDigits + 1 );
      sprintf ( sz, fmt, f ? eq2mwc ( r, pci ) : se_eq2mwc ( r, pci ) );
    }
  }

  return sz;

}


extern char *
OutputMoneyEquity ( const float ar[], const int f ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  /* fmt: "%+7.3f" or "%7.3f" */
  sprintf( fmt, f ? "%%+%d.%df" : "%%%d.%df", 
           fOutputDigits + 4, fOutputDigits );

  sprintf ( sz, fmt,
            2.0 * ar[ OUTPUT_WIN ] - 1.0
            + ar[ OUTPUT_WINGAMMON ] + ar[ OUTPUT_WINBACKGAMMON] 
            - ar[ OUTPUT_LOSEGAMMON ] - ar[ OUTPUT_LOSEBACKGAMMON] );

  return sz;

}



/*
 * Return formatted string with equity or MWC.
 *
 * Input:
 *    r: equity (either money equity for normalised money eq. for match play
 *    pci: cubeinfo
 *    f: indicates equity (TRUE) or std. error (FALSE)
 *    
 *
 * Important: function is not re-entrant. Caller must save output
 * if needed.
 */

extern char *
OutputEquityScale ( const float r, const cubeinfo *pci, 
                    const cubeinfo *pciBase, const int f ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  if ( ! pci->nMatchTo ) {
    /* fmt: "%+7.3f" or "%7.3f" */
    sprintf( fmt, f ? "%%+%d.%df" : "%%%d.%df", 
             fOutputDigits + 4, fOutputDigits );
    sprintf ( sz, fmt, pci->nCube / pciBase->nCube * r );
  }
  else {

    if ( fOutputMWC ) {

      if ( fOutputMatchPC ) {
        /* fmt: "%6.2f%%" */
        sprintf( fmt, "%%%d.%df%%%%", fOutputDigits + 3, fOutputDigits > 1 ? fOutputDigits - 1 : 0 );
        sprintf ( sz, fmt,
                  100.0f * ( f ? eq2mwc ( r, pci ) : se_eq2mwc ( r, pci ) ) );
      }
      else {
        /* fmt: "%6.4f" */
        sprintf( fmt, "%%+%d.%df", fOutputDigits + 3, fOutputDigits + 1 );
        sprintf ( sz, fmt, f ? eq2mwc ( r, pci ) : se_eq2mwc ( r, pci ) );
      }

    }
    else {
      /* fmt: "%+7.3f" or "%7.3f" */
      sprintf( fmt, f ? "%%+%d.%df" : "%%%d.%df", 
               fOutputDigits + 4, fOutputDigits );
      sprintf ( sz, fmt, f ? 
                mwc2eq ( eq2mwc ( r, pci ),pciBase ) : 
                se_mwc2eq( se_eq2mwc( r, pci ), pciBase ) );
    }
      

  }

  return sz;

}


/*
 * Return formatted string with equity or MWC for an equity difference.
 *
 * Input:
 *    r: equity (either money equity for normalised money eq. for match play
 *    pci: cubeinfo
 *    f: indicates equity (TRUE) or std. error (FALSE)
 *    
 *
 * Important: function is not re-entrant. Caller must save output
 * if needed.
 */

extern char *
OutputEquityDiff ( const float r1, const float r2, const cubeinfo *pci ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {
    /* fmt: "%+7.3f" */
    sprintf( fmt, "%%+%d.%df", fOutputDigits + 4, fOutputDigits );
    sprintf ( sz, fmt, r1 - r2 );
  }
  else {
    if ( fOutputMatchPC ) {
      /* fmt: "%6.2f%%" */
      sprintf( fmt, "%%%d.%df%%%%", fOutputDigits + 3, fOutputDigits > 1 ? fOutputDigits - 1 : 0 );
      sprintf ( sz, fmt,
                100.0f * eq2mwc ( r1, pci ) - 100.0f * eq2mwc ( r2, pci ) );
    }
    else {
      /* fmt: "%6.4f" */
      sprintf( fmt, "%%+%d.%df", fOutputDigits + 3, fOutputDigits + 1 );
      sprintf ( sz, fmt, eq2mwc ( r1, pci ) - eq2mwc ( r2, pci ) );
    }
  }

  return sz;

}


/*
 * Return formatted string with equity or MWC for an equity difference.
 *
 * Input:
 *    r: equity (either money equity or MWC for match play
 *    pci: cubeinfo
 *    f: indicates equity (TRUE) or std. error (FALSE)
 *    
 *
 * Important: function is not re-entrant. Caller must save output
 * if needed.
 */

extern char *
OutputMWCDiff ( const float r1, const float r2, const cubeinfo *pci ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {
    /* fmt: "%+7.3f" */
    sprintf( fmt, "%%+%d.%df", fOutputDigits + 4, fOutputDigits );
    if ( pci->nMatchTo ) 
      sprintf ( sz, fmt, r1 - r2 );
    else
      sprintf ( sz, fmt, mwc2eq( r1, pci ) - mwc2eq( r2, pci ) );
  }
  else {
    if ( fOutputMatchPC ) {
      /* fmt: "%6.2f%%" */
      sprintf( fmt, "%%%d.%df%%%%", fOutputDigits + 3, fOutputDigits > 1 ? fOutputDigits - 1 : 0 );
      sprintf ( sz, fmt,
                100.0f * r1 - 100.0f * r2 );
    }
    else {
      /* fmt: "%6.4f" */
      sprintf( fmt, "%%+%d.%df", fOutputDigits + 3, fOutputDigits + 1 );
      sprintf ( sz, fmt, r1 - r2 );
    }
  }

  return sz;

}

/*
 * Return formatted string with equity or MWC.
 *
 * Input is: equity for money game/MWC for match play.
 *
 * Important: function is not re-entrant. Caller must save output
 * if needed.
 */


extern char *
OutputMWC ( const float r, const cubeinfo *pci, const int f ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  if ( ! pci->nMatchTo ) {
    /* fmt: "%+7.3f" */
    sprintf( fmt, f ? "%%+%d.%df" : "%%%d.%df", 
             fOutputDigits + 4, fOutputDigits );
    sprintf ( sz, fmt, r );
  }
  else {
    
    if ( ! fOutputMWC ) {
      /* fmt: "%+7.3f" */
      sprintf( fmt, f ? "%%+%d.%df" : "%%%d.%df", 
               fOutputDigits + 4, fOutputDigits );
      sprintf ( sz, fmt, 
                f ? mwc2eq ( r, pci ) : se_mwc2eq ( r, pci ) );
    }
    else if ( fOutputMatchPC ) {
      /* fmt: "%6.2f%%" */
      sprintf( fmt, "%%%d.%df%%%%", fOutputDigits + 3, fOutputDigits > 1 ? fOutputDigits - 1 : 0 );
      sprintf ( sz, fmt, 100.0f * r );
    }
    else {
      /* fmt: "%6.4f" */
      sprintf( fmt, f? "%%+%d.%df" : "%%%d.%df", 
               fOutputDigits + 3, fOutputDigits + 1 );
      sprintf ( sz, fmt, r );
    }
  }

  return sz;

}


extern char *
OutputPercent ( const float r ) {

  static char sz[ 9 ];
  char fmt[ 32 ];

  if ( fOutputWinPC ) {
    /* fmt: "%5.1f" */
	  sprintf( fmt, "%%%d.%df%%%%", fOutputDigits + 2, fOutputDigits > 2 ? fOutputDigits - 2 : 0 );
    sprintf ( sz, fmt, 100.0 * r );
  }
  else {
    /* fmt: "%5.3f" */
    sprintf( fmt, "%%%d.%df", fOutputDigits + 2, fOutputDigits  );
    sprintf ( sz, fmt, r );
  }

  return sz;

}

extern char *
OutputPercents ( const float ar[], const int f ) {

  static char sz[ 80 ];

  strcpy ( sz, "" );

  strcat ( sz, OutputPercent ( ar [ OUTPUT_WIN ] ) );
  strcat ( sz, " " );
  strcat ( sz, OutputPercent ( ar [ OUTPUT_WINGAMMON ] ) );
  strcat ( sz, " " );
  strcat ( sz, OutputPercent ( ar [ OUTPUT_WINBACKGAMMON ] ) );
  strcat ( sz, " - " );
  if ( f )
    strcat ( sz, OutputPercent ( 1.0f - ar [ OUTPUT_WIN ] ) );
  else
    strcat ( sz, OutputPercent ( ar [ OUTPUT_WIN ] ) );
  strcat ( sz, " " );
  strcat ( sz, OutputPercent ( ar [ OUTPUT_LOSEGAMMON ] ) );
  strcat ( sz, " " );
  strcat ( sz, OutputPercent ( ar [ OUTPUT_LOSEBACKGAMMON ] ) );

  return sz;

}



/*
 * Print cube analysis
 *
 * Input:
 *  pf: output file
 *  arDouble: equitites for cube decisions
 *  fPlayer: player who doubled
 *  esDouble: eval setup
 *  pci: cubeinfo
 *  fDouble: double/no double
 *  fTake: take/drop
 *
 */

extern char *
OutputCubeAnalysisFull ( const float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                         const float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                         const evalsetup* pes, const cubeinfo* pci,
                         int fDouble, int fTake,
                         skilltype stDouble,
                         skilltype stTake ) {

  float r;

  int fMissed;
  int fAnno = FALSE;

  float arDouble[ 4 ];

  static char sz[ 4096 ];
  char *pc;


  strcpy ( sz, "" );

  /* check if cube analysis should be printed */

  if ( pes->et == EVAL_NONE ) return NULL; /* no evaluation */

  FindCubeDecision ( arDouble, aarOutput, pci );

  /* print alerts */

  fMissed = 
    fDouble > -1 &&
    isMissedDouble ( arDouble, aarOutput, fDouble, pci );

  /* print alerts */

  if ( fMissed ) {

    fAnno = TRUE;

    /* missed double */

    sprintf ( pc = strchr ( sz, 0 ), "%s (%s)!",
              _("Alert: missed double"),
              OutputEquityDiff ( arDouble[ OUTPUT_NODOUBLE ], 
                                 ( arDouble[ OUTPUT_TAKE ] > 
                                   arDouble[ OUTPUT_DROP ] ) ? 
                                 arDouble[ OUTPUT_DROP ] : 
                                 arDouble[ OUTPUT_TAKE ],
                                 pci ) );
    
    if ( badSkill(stDouble) )
      sprintf ( pc = strchr ( sz, 0 ), " [%s]", 
                gettext ( aszSkillType[ stDouble ] ) );
    
  }

  r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_DROP ];

  if ( fTake > 0 && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong take */

    sprintf ( pc = strchr ( sz, 0 ), "%s (%s)!",
              _("Alert: wrong take"),
              OutputEquityDiff ( arDouble[ OUTPUT_DROP ],
                                 arDouble[ OUTPUT_TAKE ],
                                 pci ) );

    if ( badSkill(stTake) )
      sprintf ( pc = strchr ( sz, 0 ), " [%s]", 
                gettext ( aszSkillType[ stTake ] ) );
    
  }

  r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble > 0 && ! fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong pass */

    sprintf ( pc = strchr ( sz, 0 ), "%s (%s)!",
              _("Alert: wrong pass"),
              OutputEquityDiff ( arDouble[ OUTPUT_TAKE ],
                                 arDouble[ OUTPUT_DROP ],
                                 pci ) );

    if ( badSkill(stTake) )
      sprintf ( pc = strchr ( sz, 0 ), " [%s]", 
                gettext ( aszSkillType[ stTake ] ) );
    
  }


  if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_DROP ];
  else
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble > 0 && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong double */

    sprintf ( pc = strchr ( sz, 0 ), "%s (%s)!",
              _("Alert: wrong double"),
              OutputEquityDiff ( ( arDouble[ OUTPUT_TAKE ] > 
                                   arDouble[ OUTPUT_DROP ] ) ? 
                                 arDouble[ OUTPUT_DROP ] : 
                                 arDouble[ OUTPUT_TAKE ],
                                 arDouble[ OUTPUT_NODOUBLE ], 
                                 pci ) );

    if ( badSkill(stDouble) )
      sprintf ( pc = strchr ( sz, 0 ), " [%s]", 
                gettext ( aszSkillType[ stDouble ] ) );

  }

  if ( ( badSkill(stDouble) || badSkill(stTake) ) && ! fAnno ) {
    
    if ( badSkill(stDouble) ) {
      sprintf ( pc = strchr ( sz, 0 ), _("Alert: double decision marked %s"),
                gettext ( aszSkillType[ stDouble ] ) );
      strcat ( sz, "\n" );
    }

    if ( badSkill(stTake) ) {
      sprintf ( pc = strchr ( sz, 0 ), _("Alert: take decision marked %s"),
                gettext ( aszSkillType[ stTake ] ) );
      strcat ( sz, "\n" );
    }

  }

  strcat( sz, OutputCubeAnalysis( aarOutput, aarStdDev, pes, pci ) );

  return sz;

}

extern char *
OutputCubeAnalysis( const float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                    const float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                    const evalsetup* pes, const cubeinfo* pci ) {

  static char sz[ 4096 ];
  int i;
  float arDouble[ 4 ];
  const char *aszCube[] = {
    NULL, 
    N_("No double"), 
    N_("Double, take"), 
    N_("Double, pass") 
  };

  int ai[ 3 ];
  cubedecision cd;
  float r;

  FindCubeDecision ( arDouble, aarOutput, pci );

  /* header */

  strcpy ( sz, _("\nCube analysis\n") );

  /* ply & cubeless equity */

  switch ( pes->et ) {
  case EVAL_NONE:
    strcat ( sz, _("n/a") );
    break;
  case EVAL_EVAL:
    sprintf ( strchr ( sz, 0 ), 
              _("%d-ply"), 
              pes->ec.nPlies );
    break;
  case EVAL_ROLLOUT:
    strcat ( sz, _("Rollout") );
    break;
  }

  if ( pci->nMatchTo ) 
    sprintf ( strchr ( sz, 0 ), " %s %s (%s: %s)\n",
              ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) ?
              _("cubeless equity") : _("cubeless MWC"),
              OutputEquity ( aarOutput[ 0 ][ OUTPUT_EQUITY ], pci, TRUE ),
              _("Money"), 
              OutputMoneyEquity ( aarOutput[ 0 ], TRUE ) );
  else
    sprintf ( strchr ( sz, 0 ), _(" cubeless equity %s\n"),
              OutputMoneyEquity ( aarOutput[ 0 ], TRUE ) );




  /* Output percentags for evaluations */

  if ( exsExport.fCubeDetailProb && pes->et == EVAL_EVAL ) {

    strcat ( sz, "  " );
    strcat ( sz, OutputPercents ( aarOutput[ 0 ], TRUE ) );

  }

  strcat( sz, "\n" );

  /* equities */

  strcat( sz, _("Cubeful equities:\n") );
  if ( pes->et == EVAL_EVAL && exsExport.afCubeParameters[ 0 ] ) {
    strcat( sz, "  " );
    strcat( sz, OutputEvalContext( &pes->ec, FALSE ) );
    strcat( sz, "\n" );
  }

  getCubeDecisionOrdering ( ai, arDouble, aarOutput, pci );

  for ( i = 0; i < 3; i++ ) {

    sprintf ( strchr ( sz, 0 ),
              "%d. %-20s", i + 1, 
              gettext ( aszCube[ ai[ i ] ] ) );

    strcat ( sz, OutputEquity ( arDouble[ ai [ i ] ], pci, TRUE ) );

    if ( i )
      sprintf ( strchr ( sz, 0 ),
                "  (%s)",
                OutputEquityDiff ( arDouble[ ai [ i ] ], 
                                   arDouble[ OUTPUT_OPTIMAL ], pci ) );
    strcat ( sz, "\n" );

  }

  /* cube decision */

  cd = FindBestCubeDecision ( arDouble, aarOutput, pci );

  sprintf ( strchr ( sz, 0 ),
            "%s %s",
            _("Proper cube action:"),
            GetCubeRecommendation ( cd ) );

  if ( ( r = getPercent ( cd, arDouble ) ) >= 0.0 )
    sprintf ( strchr ( sz, 0 ), " (%.1f%%)", 100.0f * r );

  strcat ( sz, "\n" );

  /* dump rollout */

  if ( pes->et == EVAL_ROLLOUT && exsExport.fCubeDetailProb ) {

    char asz[ 2 ][ 1024 ];
    cubeinfo aci[ 2 ];

    for ( i = 0; i < 2; i++ ) {

      memcpy ( &aci[ i ], pci, sizeof ( cubeinfo ) );

      if ( i ) {
        aci[ i ].fCubeOwner = ! pci->fMove;
        aci[ i ].nCube *= 2;
      }

      FormatCubePosition ( asz[ i ], &aci[ i ] );

    }

    strcat ( sz, _("Rollout details:\n") );

    strcat ( sz, 
             OutputRolloutResult ( NULL, asz, aarOutput, aarStdDev,
                                   aci, 2, pes->rc.fCubeful ) );
             

  }

  if ( pes->et == EVAL_ROLLOUT && exsExport.afCubeParameters[ 1 ] )
    strcat ( sz, OutputRolloutContext ( NULL, pes ) );
    
  return sz;
}


extern void
FormatCubePositions( const cubeinfo *pci, char asz[ 2 ][ 40 ] ) {

  cubeinfo aci[ 2 ];

  SetCubeInfo ( &aci[ 0 ], pci->nCube, pci->fCubeOwner, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, 
                pci->fJacoby, pci->fBeavers, pci->bgv );

  FormatCubePosition ( asz[ 0 ], &aci[ 0 ] );

  SetCubeInfo ( &aci[ 1 ], 2 * pci->nCube, ! pci->fMove, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, 
                pci->fJacoby, pci->fBeavers, pci->bgv );

  FormatCubePosition ( asz[ 1 ], &aci[ 1 ] );

}

extern char *FormatCubePosition ( char *sz, cubeinfo *pci ) {

  if ( pci->fCubeOwner == -1 )
    sprintf ( sz, _("Centered %d-cube"), pci->nCube );
  else 
    sprintf ( sz, _("Player %s owns %d-cube"),
              ap[ pci->fCubeOwner ].szName, pci->nCube );

  return sz;

}

