/*
 * formatgs.c
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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "backgammon.h"
#include "formatgs.h"
#include "analysis.h"

#include "i18n.h"
#include "export.h"

static char **
numberEntry( const char *sz, const int n0, const int n1 ) {

  char **aasz;

  aasz = g_malloc( 3 * sizeof ( *aasz ) );

  aasz[ 0 ] = g_strdup( sz );
  aasz[ 1 ] = g_strdup_printf( "%3d", n0 );
  aasz[ 2 ] = g_strdup_printf( "%3d", n1 );

  return aasz;

}

static char *
errorRate( const float rn, const float ru, const matchstate *pms ) {

  if ( rn != 0.0f ) {

    if ( pms->nMatchTo ) 
      return g_strdup_printf( "%+6.3f (%+7.3f%%)",
                              rn, ru * 100.0f );
    else
      return g_strdup_printf( "%+6.3f (%+7.3f)",
                              rn, ru );

  }
  else
    return g_strdup_printf( "%+6.3f", 0.0f );

}

static char *
errorRateMP( const float rn, const float ru, const matchstate *pms ) {

  if ( rn != 0.0f ) {

    if ( pms->nMatchTo ) 
      return g_strdup_printf( "%+6.2f (%+7.3f%%)",
                              1000.0f * rn, ru * 100.0f );
    else
      return g_strdup_printf( "%+6.2f (%+7.3f)",
                              1000.0f * rn, ru );

  }
  else
    return g_strdup_printf( "%+6.3f", 0.0f );

}

static char *
cubeEntry( const int n, const float rn, const float ru, 
           const matchstate *pms ) {

  if ( ! n )
    return g_strdup( "  0" );
  else {
    if ( pms->nMatchTo ) 
      return g_strdup_printf( "%3d (%+6.3f (%+7.3f%%))", 
                              n, rn, 100.0f * ru );
    else
      return g_strdup_printf( "%3d (%+6.3f (%+7.3f))", 
                              n, rn, ru );
  }
    
  g_assert_not_reached();
  return NULL;

}


static char **
luckAdjust( const char *sz, const float ar[ 2 ], const matchstate *pms ) {

  char **aasz;
  int i;

  aasz = g_malloc( 3 * sizeof ( *aasz ) );

  aasz[ 0 ] = g_strdup( sz );
  
  for ( i = 0; i < 2; ++i )
    if ( pms->nMatchTo )
      aasz[ i + 1 ] = g_strdup_printf( "%+7.2f%%", 100.0f * ar[ i ] );
    else
      aasz[ i + 1 ] = g_strdup_printf( "%+7.3f", ar[ i ] );

  return aasz;

}

extern GList *
formatGS( const statcontext *psc, const matchstate *pms,
          const int fIsMatch, const enum _formatgs fg ) {

  GList *list = NULL;
  char **aasz;
  float aaaar[ 3 ][ 2 ][ 2 ][ 2 ];

  getMWCFromError ( psc, aaaar );

  switch ( fg ) {
  case FORMATGS_CHEQUER: 
    {

      static int ai[ 5 ] = { SKILL_NONE, SKILL_GOOD, SKILL_DOUBTFUL,
                             SKILL_BAD, SKILL_VERYBAD };
      static char *asz[] = {
        N_("Unmarked moves"),
        N_("Moves marked good"),
        N_("Moves marked doubtful"),
        N_("Moves marked bad"),
        N_("Moves marked very bad") };
      int i;

      /* chequer play part */
      
      list = g_list_append( list, numberEntry( _("Total moves"),
                                               psc->anTotalMoves[ 0 ],
                                               psc->anTotalMoves[ 1 ] ) );
      
      list = g_list_append( list, numberEntry( _("Unforced moves"),
                                               psc->anUnforcedMoves[ 0 ],
                                               psc->anUnforcedMoves[ 1 ] ) );
      for ( i = 0; i < 5; ++i )
        list = g_list_append( list, 
                              numberEntry( gettext( asz[ i ] ),
                                           psc->anMoves[ 0 ][ ai[ i ] ],
                                           psc->anMoves[ 1 ][ ai[ i ] ] ) );

      /* total error rate */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Error rate (total)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRate( 
              -aaaar[ CHEQUERPLAY ][ TOTAL ][ i ][ NORMALISED ],
              -aaaar[ CHEQUERPLAY ][ TOTAL ][ i ][ UNNORMALISED ],
              pms );

      list = g_list_append( list, aasz );

      /* error rate per move */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Error rate (per move)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRateMP( 
              -aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ NORMALISED ],
              -aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ UNNORMALISED ],
              pms );

      list = g_list_append( list, aasz );

      /* chequer play rating */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Chequerplay rating") );

      for ( i = 0; i < 2; ++i )
        if ( psc->anUnforcedMoves[ i ] )
          aasz[ i + 1 ] =
            g_strdup( gettext ( aszRating[ GetRating( aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ NORMALISED ] ) ] ) );
        else 
          aasz[ i + 1 ] = g_strdup( _("n/a") );

      list = g_list_append( list, aasz );

    }

    break;

  case FORMATGS_CUBE:
    {
      static char *asz[] = {
        N_("Total cube decisions"),
        N_("Close or actual cube decisions"),
        N_("Doubles"),
        N_("Takes"),
        N_("Passes") };
      const int *ai[ 5 ] = { psc->anTotalCube, psc->anCloseCube,
                             psc->anDouble, psc->anTake, psc->anPass };

      static char *asz2[] = {
        N_("Missed doubles around DP"),
        N_("Missed doubles around CP"),
        N_("Missed doubles around TG"),
        N_("Wrong doubles around DP"),
        N_("Wrong doubles around CP"),
        N_("Wrong doubles around TG"),
        N_("Wrong takes"),
        N_("Wrong passes") };

      const int *ai2[ 8 ] = { psc->anCubeMissedDoubleDP,
                              psc->anCubeMissedDoubleCP,
                              psc->anCubeMissedDoubleTG,
                              psc->anCubeWrongDoubleDP,
                              psc->anCubeWrongDoubleCP,
                              psc->anCubeWrongDoubleTG,
                              psc->anCubeWrongTake,
                              psc->anCubeWrongPass };
      const float *af2[ 2 ][ 8 ] = {
        { psc->arErrorMissedDoubleDP[ 0 ],
          psc->arErrorMissedDoubleCP[ 0 ],
          psc->arErrorMissedDoubleTG[ 0 ],
          psc->arErrorWrongDoubleDP[ 0 ],
          psc->arErrorWrongDoubleCP[ 0 ],
          psc->arErrorWrongDoubleTG[ 0 ],
          psc->arErrorWrongTake[ 0 ],
          psc->arErrorWrongPass[ 0 ] },
        { psc->arErrorMissedDoubleDP[ 1 ],
          psc->arErrorMissedDoubleCP[ 1 ],
          psc->arErrorMissedDoubleTG[ 1 ],
          psc->arErrorWrongDoubleDP[ 1 ],
          psc->arErrorWrongDoubleCP[ 1 ],
          psc->arErrorWrongDoubleTG[ 1 ],
          psc->arErrorWrongTake[ 1 ],
          psc->arErrorWrongPass[ 1 ] } };

      int i, j;

      for ( i = 0; i < 5; ++i )
        list = g_list_append( list, 
                              numberEntry( gettext( asz[ i ] ),
                                           ai[ i ][ 0 ],
                                           ai[ i ][ 1 ] ) );
      for ( i = 0; i < 8; ++i ) {
        aasz = g_malloc( 3 * sizeof ( *aasz ) );

        aasz[ 0 ] = g_strdup( gettext( asz2[ i ] ) );

        for ( j = 0; j < 2; ++j ) 
          aasz[ j + 1 ] = cubeEntry( ai2[ i ][ j ],
                                     -af2[ j ][ i ][ 0 ],
                                     -af2[ j ][ i ][ 1 ],
                                     pms );

        list = g_list_append( list, aasz );

      }

      /* total error rate */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Error rate (total)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRate( 
              -aaaar[ CUBEDECISION ][ TOTAL ][ i ][ NORMALISED ],
              -aaaar[ CUBEDECISION ][ TOTAL ][ i ][ UNNORMALISED ],
              pms );

      list = g_list_append( list, aasz );

      /* error rate per cube decision */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Error rate (per cube decision)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRateMP( 
              -aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ],
              -aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ UNNORMALISED ],
              pms );

      list = g_list_append( list, aasz );

      /* cube decision rating */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Cube decision rating") );

      for ( i = 0; i < 2; ++i )
        if ( psc->anCloseCube[ i ] )
          aasz[ i + 1 ] =
            g_strdup( gettext ( aszRating[ GetRating( aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ] ) ] ) );
        else 
          aasz[ i + 1 ] = g_strdup( _("n/a") );

      list = g_list_append( list, aasz );



    }
    break;

  case FORMATGS_LUCK: 
    {

      static char *asz[] = {
        N_("Rolls marked very lucky"),
        N_("Rolls marked lucky"),
        N_("Rolls unmarked"),
        N_("Rolls marked unlucky"),
        N_("Rolls marked very unlucky") };
      int i;

      for ( i = 0; i < 5; ++i )
        list = g_list_append( list, 
                              numberEntry( gettext( asz[ i ] ),
                                           psc->anLuck[ 0 ][ 4 - i ],
                                           psc->anLuck[ 1 ][ 4 - i ] ) );


      /* total luck */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Luck rate (total)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRate( psc->arLuck[ i ][ 0 ],
                                   psc->arLuck[ i ][ 1 ], pms );

      list = g_list_append( list, aasz );

      /* luck rate per move */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Luck rate (per move)") );

      for ( i = 0; i < 2; ++i )
        if ( psc->anTotalMoves[ i ] ) 
          aasz[ i + 1 ] = errorRateMP( psc->arLuck[ i ][ 0 ] / 
                                     psc->anTotalMoves[ i ], 
                                     psc->arLuck[ i ][ 1 ] / 
                                     psc->anTotalMoves[ i ], 
                                     pms );
        else
          aasz[ i + 1 ] = g_strdup( _("n/a") );

      list = g_list_append( list, aasz );

      /* chequer play rating */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Luck rating") );

      for ( i = 0; i < 2; ++i )
        if ( psc->anTotalMoves[ i ] )
          aasz[ i + 1 ] =
            g_strdup( gettext ( aszLuckRating[ getLuckRating( psc->arLuck[ i ][ 0 ] / psc->anTotalMoves[ i ] ) ] ) );
        else 
          aasz[ i + 1 ] = g_strdup( _("n/a") );

      list = g_list_append( list, aasz );

    }
    break;

  case FORMATGS_OVERALL:

    {
      int i, n;

      /* total error rate */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Error rate (total)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRate( 
              -aaaar[ COMBINED ][ TOTAL ][ i ][ NORMALISED ],
              -aaaar[ COMBINED ][ TOTAL ][ i ][ UNNORMALISED ],
              pms );

      list = g_list_append( list, aasz );

      /* error rate per decision */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Error rate (per decision)") );

      for ( i = 0; i < 2; ++i )
        aasz[ i + 1 ] = errorRateMP( 
              -aaaar[ COMBINED ][ PERMOVE ][ i ][ NORMALISED ],
              -aaaar[ COMBINED ][ PERMOVE ][ i ][ UNNORMALISED ],
              pms );

      list = g_list_append( list, aasz );

      /* eq. snowie error rate */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Equiv. Snowie error rate") );

      for ( i = 0; i < 2; ++i )
        if ( ( n = psc->anTotalMoves[ 0 ] + psc->anTotalMoves[ 1 ] ) )
          aasz[ i + 1 ] = g_strdup_printf( "%+6.2f",
                                           -aaaar[ COMBINED ][ TOTAL ][ i ][ NORMALISED ] / n  *
                                           1000.0f );
        else
          aasz[ i + 1 ] = g_strdup( _("n/a") );

      list = g_list_append( list, aasz );

      /* rating */

      aasz = g_malloc( 3 * sizeof ( *aasz ) );

      aasz[ 0 ] = g_strdup( _("Overall rating") );

      for ( i = 0; i < 2; ++i )
        if ( psc->anCloseCube[ i ] + psc->anUnforcedMoves[ i ] )
          aasz[ i + 1 ] =
            g_strdup( gettext ( aszRating[ GetRating( aaaar[ COMBINED ][ PERMOVE ][ i ][ NORMALISED ] ) ] ) );
        else 
          aasz[ i + 1 ] = g_strdup( _("n/a") );

      list = g_list_append( list, aasz );

      /* luck adj. result */

      if ( ( psc->arActualResult[ 0 ] > 0.0f || 
             psc->arActualResult[ 1 ] > 0.0f ) && psc->fDice ) {

        list = g_list_append( list, 
                              luckAdjust( _("Actual result"),
                                          psc->arActualResult,
                                          pms ) );
        
        list = g_list_append( list, 
                              luckAdjust( _("Luck adjusted result"),
                                          psc->arLuckAdj,
                                          pms ) );

        if ( fIsMatch && pms->nMatchTo ) {

          /* luck based fibs rating */
          
          float r = 0.5f + psc->arActualResult[ 0 ] - 
            psc->arLuck[ 0 ][ 1 ] + psc->arLuck[ 1 ][ 1 ];
          
          aasz = g_malloc( 3 * sizeof ( *aasz ) );
          
          aasz[ 0 ] = g_strdup( _("Luck based FIBS rating diff.") );
          aasz[ 2 ] = g_strdup( "" );
          
          if ( r > 0.0f && r < 1.0f )
            aasz[ 1 ] = g_strdup_printf( "%+7.2f",
                                         relativeFibsRating( r, 
                                                             ms.nMatchTo ) );
          else
            aasz[ 1 ] = g_strdup_printf( _("n/a") );

          list = g_list_append( list, aasz );
          
        }
        
      }

      /* error based fibs rating */

      if ( fIsMatch && pms->nMatchTo ) {

        aasz = g_malloc( 3 * sizeof ( *aasz ) );
        aasz[ 0 ] = g_strdup( _("Error based abs. FIBS rating") );
      
        for ( i = 0; i < 2; ++i ) 
          if ( psc->anCloseCube[ i ] + psc->anUnforcedMoves[ i ] )
            aasz[ i + 1 ] = g_strdup_printf( "%6.1f",
                                             absoluteFibsRating( aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ NORMALISED ], 
                                                                 aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ], 
                                                                 pms->nMatchTo, rRatingOffset ) );
          else
            aasz[ i + 1 ] = g_strdup_printf( _("n/a") );

        list = g_list_append( list, aasz );

        /* chequer error fibs rating */

        aasz = g_malloc( 3 * sizeof ( *aasz ) );
        aasz[ 0 ] = g_strdup( _("Chequerplay errors rating loss") );
      
        for ( i = 0; i < 2; ++i ) 
          if ( psc->anUnforcedMoves[ i ] )
            aasz[ i + 1 ] = g_strdup_printf( "%6.1f",
                                             absoluteFibsRatingChequer( aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ NORMALISED ], 
                                                                        pms->nMatchTo ) );
          else
            aasz[ i + 1 ] = g_strdup_printf( _("n/a") );

        list = g_list_append( list, aasz );

        /* cube error fibs rating */

        aasz = g_malloc( 3 * sizeof ( *aasz ) );
        aasz[ 0 ] = g_strdup( _("Cube errors rating loss") );
      
        for ( i = 0; i < 2; ++i ) 
          if ( psc->anCloseCube[ i ] )
            aasz[ i + 1 ] = g_strdup_printf( "%6.1f",
                                             absoluteFibsRatingCube( aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ], 
                                                                     pms->nMatchTo ) );
          else
            aasz[ i + 1 ] = g_strdup_printf( _("n/a") );

        list = g_list_append( list, aasz );

      }

      if( fIsMatch && !pms->nMatchTo && psc->nGames > 1 ) {

	static char *asz[ 2 ][ 2 ] = {
          { N_("Advantage (actual) in ppg"),
            N_("95% confidence interval (ppg)") },
          { N_("Advantage (luck adjusted) in ppg"),
            N_("95% confidence interval (ppg)") }
	};
        const float *af[ 2 ][ 2 ] = { 
          { psc->arActualResult, psc->arVarianceActual },
          { psc->arLuckAdj, psc->arVarianceLuckAdj } };
        int i, j;

        for ( i = 0; i < 2; ++i ) {

          /* ppg */

          aasz = g_malloc( 3 * sizeof ( *aasz ) );
          aasz[ 0 ] = g_strdup( gettext( asz[ i ][ 0 ] ) );
      
          for ( j = 0; j < 2; ++j ) 
            aasz[ j + 1 ] = 
              g_strdup_printf( "%6.1f",
                               af[ i ][ 0 ][ j ] / psc->nGames );

          list = g_list_append( list, aasz );

          /* std dev. */

          aasz = g_malloc( 3 * sizeof ( *aasz ) );
          aasz[ 0 ] = g_strdup( gettext( asz[ i ][ 1 ] ) );
      
          for ( j = 0; j < 2; ++j ) 
            aasz[ j + 1 ] = 
              g_strdup_printf( "%6.1f",
                               1.95996f * af[ i ][ 1 ][ j ] / psc->nGames );

          list = g_list_append( list, aasz );

        }


      }


    }
    
    break;

  default:

    g_assert_not_reached();
    break;

  }

  return list;


}

static void
_freeGS( gpointer data, gpointer userdata ) {

  char **aasz = data;
  int i;

  for ( i = 0; i < 3; ++i )
    g_free( aasz[ i ] );

  g_free( aasz );

}


extern void
freeGS( GList *list ) {

  g_list_foreach( list, _freeGS, NULL );

  g_list_free( list );

}
