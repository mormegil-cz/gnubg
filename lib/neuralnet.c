/*
 * neuralnet.c
 *
 * by Gary Wong <gtw@gnu.org>, 1998, 1999, 2000, 2001, 2002.
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

#define PROCESSING_UNITS 1

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <assert.h>
#include <errno.h>
#include <isaac.h>
#include <math.h>
#include <neuralnet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef VECTOR
    #define VECTOR 0
#endif
#if VECTOR
    #if VECTOR_VE
        #define VECTOR_MULTIPLY 0
        #define VECTOR_SIGMOID 0
        #define VECTOR_MULTIPLYSIG 0
        #include "vector_ve.h"
    #elif 0
    #else
    #include "vector_none.h"
    #endif
#else
    #include "vector_none.h"
#endif

#if PROCESSING_UNITS

#include "procunits.h"
#include "threadglobals.h"

/* this list replaces the savedBase and savedIBase fields of the
    neuralnet structs; thus, each thread can have its own
    savedbases in separate storages */
DECLARE_THREADGLOBAL(savedBases, gSavedBasesList, NULL)

#endif

static int gVector = VECTOR;

/* e[k] = exp(k/10) / 10 */
static float e[100] = {
0.10000000000000001, 
0.11051709180756478, 
0.12214027581601698, 
0.13498588075760032, 
0.14918246976412702, 
0.16487212707001281, 
0.18221188003905089, 
0.20137527074704767, 
0.22255409284924679, 
0.245960311115695, 
0.27182818284590454, 
0.30041660239464335, 
0.33201169227365473, 
0.36692966676192446, 
0.40551999668446748, 
0.44816890703380646, 
0.49530324243951152, 
0.54739473917271997, 
0.60496474644129461, 
0.66858944422792688, 
0.73890560989306509, 
0.81661699125676512, 
0.90250134994341225, 
0.99741824548147184, 
1.1023176380641602, 
1.2182493960703473, 
1.3463738035001691, 
1.4879731724872838, 
1.6444646771097049, 
1.817414536944306, 
2.0085536923187668, 
2.2197951281441637, 
2.4532530197109352, 
2.7112638920657881, 
2.9964100047397011, 
3.3115451958692312, 
3.6598234443677988, 
4.0447304360067395, 
4.4701184493300818, 
4.9402449105530168, 
5.4598150033144233, 
6.034028759736195, 
6.6686331040925158, 
7.3699793699595784, 
8.1450868664968148, 
9.0017131300521811, 
9.9484315641933776, 
10.994717245212353, 
12.151041751873485, 
13.428977968493552, 
14.841315910257659, 
16.402190729990171, 
18.127224187515122, 
20.033680997479166, 
22.140641620418716, 
24.469193226422039, 
27.042640742615255, 
29.886740096706028, 
33.029955990964865, 
36.503746786532886, 
40.34287934927351, 
44.585777008251675, 
49.274904109325632, 
54.457191012592901, 
60.184503787208222, 
66.514163304436181, 
73.509518924197266, 
81.24058251675433, 
89.784729165041753, 
99.227471560502622, 
109.66331584284585, 
121.19670744925763, 
133.9430764394418, 
148.02999275845451, 
163.59844299959269, 
180.80424144560632, 
199.81958951041173, 
220.83479918872089, 
244.06019776244983, 
269.72823282685101, 
298.09579870417281, 
329.44680752838406, 
364.09503073323521, 
402.38723938223131, 
444.7066747699858, 
491.47688402991344, 
543.16595913629783, 
600.29122172610175, 
663.42440062778894, 
733.19735391559948, 
810.3083927575384, 
895.52927034825075, 
989.71290587439091, 
1093.8019208165192, 
1208.8380730216988, 
1335.9726829661872, 
1476.4781565577266, 
1631.7607198015421, 
1803.3744927828525, 
1993.0370438230298, 
};

/* Calculate an approximation to the sigmoid function 1 / ( 1 + e^x ).
   This is executed very frequently during neural net evaluation, so
   careful optimisation here pays off.

   Statistics on sigmoid(x) calls:
   * >99% of the time, x is positive.
   *  82% of the time, 3 < abs(x) < 8.

   The Intel x87's `f2xm1' instruction makes calculating accurate
   exponentials comparatively fast, but still about 30% slower than
   the lookup table used here. */
static inline float sigmoid(float const xin) {
    
    if( !signbit( xin ) ) { /* signbit() can be faster than a compare to 0.0 */
	/* xin is almost always positive; we place this branch of the `if'
	   first, in the hope that the compiler/processor will predict the
	   conditional branch will not be taken. */
	if( xin < 10.0f ) {
	    /* again, predict the branch not to be taken */
	    const float x1 = 10.0f * xin;
	    const unsigned int i = (int) x1;
	    
	    return 1.0f / ( 1.0f + e[ i ] * ( 10.0f - (int) i + x1 ) );
	} else
	    return 1.0f / 19931.370438230298f;
    } else {
	if( xin > -10.0f ) {
	    const float x1 = -10.0f * xin;
	    const unsigned int i = (int) x1;
	    
	    return 1.0f - 1.0f / ( 1.0f + e[ i ] * ( 10.0f - (int) i + x1 ) );
	} else
	    return 19930.370438230298f / 19931.370438230298f;	
    }
}


/*  #define sigmoid( x ) ( (x) > 0.0f ? \ */
/*  		       1.0f / ( 2.0f + (x) + (1.0f / 2.0f) * (x) * (x) ) : \ */
/*  		       1.0f - 1.0f / ( 2.0f + -(x) + \ */
/*  				       (1.0f / 2.0f) * (x) * (x) ) ) */

static int frc;
static randctx rc; /* for irand */

static void CheckRC( void ) {

    if( !frc ) {
        int i;

        rc.randrsl[ 0 ] = time( NULL );
        for( i = 0; i < RANDSIZ; i++ )
           rc.randrsl[ i ] = rc.randrsl[ 0 ];
        irandinit( &rc, TRUE );

        frc = TRUE;
    }
}


#if PROCESSING_UNITS 

/* returns the savedBase and savedIBase of the pnn neural net
   belonging to the exacuting thread */
savedBases * GetThreadSavedBases (neuralnet *pnn)
{
    savedBases *psb = THREADGLOBAL(gSavedBasesList), **ppsb;
    
    /* look for thread-specific saved bases pertaining to
        that pnn in our thread-specific saved bases list */
    while (psb != NULL) {
        if (psb->pnn == pnn) 
            return psb;
        psb = psb->next;
    }
    
    /* if not found, let's alloc new saved bases for that pnn
        and store them in our thread-specific saved bases list */
    ppsb = (savedBases **) &THREADGLOBAL(gSavedBasesList);
    while (*ppsb != NULL)
        ppsb = &((*ppsb)->next);
        
    *ppsb = malloc (sizeof (savedBases));
    assert (*ppsb);
    (*ppsb)->next = NULL;
    (*ppsb)->pnn = pnn;
    (*ppsb)->savedBase = malloc( pnn->cHidden * sizeof( float ) ); 
    (*ppsb)->savedIBase = malloc( pnn->cInput * sizeof( float ) ); 
    assert ((*ppsb)->savedBase);
    assert ((*ppsb)->savedIBase);
    
    return *ppsb;
}

/* this function must be called by each terminating thread
    that has called at least once GetThreadSavedBases() */
void FreeThreadSavedBases (void)
{
    savedBases *psb = THREADGLOBAL(gSavedBasesList);

    while (psb != NULL) {
        savedBases *next = psb->next;
        if (psb->savedBase != NULL) free (psb->savedBase);
        if (psb->savedIBase != NULL) free (psb->savedIBase);
        free (psb);
        psb = next;
    }
}

#endif

extern int NeuralNetCreate( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput, float rBetaHidden,
			    float rBetaOutput ) {
    int i;
    float *pf;
    
    pnn->cInput = cInput;
    pnn->cHidden = cHidden;
    pnn->cOutput = cOutput;
    pnn->rBetaHidden = rBetaHidden;
    pnn->rBetaOutput = rBetaOutput;
    pnn->nTrained = 0;
    pnn->fDirect = FALSE;
   
    if( !( pnn->arHiddenWeight = malloc( cHidden * cInput *
					 sizeof( float ) ) ) )
	return -1;

    if( !( pnn->arOutputWeight = malloc( cOutput * cHidden *
					 sizeof( float ) ) ) ) {
	free( pnn->arHiddenWeight );
	return -1;
    }
    
    if( !( pnn->arHiddenThreshold = malloc( cHidden * sizeof( float ) ) ) ) {
	free( pnn->arOutputWeight );
	free( pnn->arHiddenWeight );
	return -1;
    }
	   
    if( !( pnn->arOutputThreshold = malloc( cOutput * sizeof( float ) ) ) ) {
	free( pnn->arHiddenThreshold );
	free( pnn->arOutputWeight );
	free( pnn->arHiddenWeight );
	return -1;
    }

    CheckRC();

    #if !PROCESSING_UNITS
    pnn->savedBase = malloc( cHidden * sizeof( float ) ); 
    pnn->savedIBase = malloc( cInput * sizeof( float ) ); 
    #endif
    
    for( i = cHidden * cInput, pf = pnn->arHiddenWeight; i; i-- )
	*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput * cHidden, pf = pnn->arOutputWeight; i; i-- )
	*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cHidden, pf = pnn->arHiddenThreshold; i; i-- )
	*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput, pf = pnn->arOutputThreshold; i; i-- )
	*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;

    #if VECTOR
        pnn->vector.dirty = 1;
        pnn->vector.arHiddenWeight = calloc (VECTOR_COUNTALIGN(cHidden) * VECTOR_COUNTALIGN(cInput),  sizeof( float ) );
        pnn->vector.arOutputWeight = calloc (VECTOR_COUNTALIGN(cOutput) * VECTOR_COUNTALIGN(cHidden), sizeof( float ) );
        pnn->vector.arHiddenThreshold = calloc (VECTOR_COUNTALIGN(cHidden), sizeof (float));
        pnn->vector.arOutputThreshold = calloc (VECTOR_COUNTALIGN(cOutput), sizeof (float));
    #endif
    
    return 0;
}

extern void *NeuralNetCreateDirect( neuralnet *pnn, void *p ) {
 
   pnn->cInput = *( ( (int *) p )++ );
   pnn->cHidden = *( ( (int *) p )++ );
   pnn->cOutput = *( ( (int *) p )++ );
   pnn->nTrained = *( ( (int *) p )++ );
   pnn->fDirect = TRUE;
   pnn->rBetaHidden = *( ( (float *) p )++ );
   pnn->rBetaOutput = *( ( (float *) p )++ );
 
   if( pnn->cInput < 1 || pnn->cHidden < 1 || pnn->cOutput < 1 ||
     pnn->nTrained < 0 || pnn->rBetaHidden <= 0.0 ||
     pnn->rBetaOutput <= 0.0 ) {
     errno = EINVAL;
     
     return NULL;
   }
 
   pnn->arHiddenWeight = p;
   ( (float *) p ) += pnn->cInput * pnn->cHidden;
   pnn->arOutputWeight = p;
   ( (float *) p ) += pnn->cHidden * pnn->cOutput;
   pnn->arHiddenThreshold = p;
   ( (float *) p ) += pnn->cHidden;
   pnn->arOutputThreshold = p;
   ( (float *) p ) += pnn->cOutput;

   #if !PROCESSING_UNITS
   pnn->savedBase = malloc( pnn->cHidden * sizeof( float ) ); 
   pnn->savedIBase = malloc( pnn->cInput * sizeof( float ) ); 
   #endif
   
    #if VECTOR
        pnn->vector.dirty = 1;
        pnn->vector.arHiddenWeight = calloc (VECTOR_COUNTALIGN(pnn->cHidden) * VECTOR_COUNTALIGN(pnn->cInput),  sizeof( float ) );
        pnn->vector.arOutputWeight = calloc (VECTOR_COUNTALIGN(pnn->cOutput) * VECTOR_COUNTALIGN(pnn->cHidden), sizeof( float ) );
        pnn->vector.arHiddenThreshold = calloc (VECTOR_COUNTALIGN(pnn->cHidden), sizeof (float));
        pnn->vector.arOutputThreshold = calloc (VECTOR_COUNTALIGN(pnn->cOutput), sizeof (float));
    #endif

   return p;
}

extern int
NeuralNetDestroy( neuralnet *pnn )
{
  if( !pnn->fDirect ) {
    free( pnn->arHiddenWeight ); pnn->arHiddenWeight = 0;
    free( pnn->arOutputWeight ); pnn->arOutputWeight = 0;
    free( pnn->arHiddenThreshold ); pnn->arHiddenThreshold = 0;
    free( pnn->arOutputThreshold ); pnn->arOutputThreshold = 0;
  }

  #if !PROCESSING_UNITS
  free(pnn->savedBase); pnn->savedBase = 0;
  free(pnn->savedIBase); pnn->savedIBase = 0;
  #endif
  
    #if VECTOR
        free( pnn->vector.arHiddenWeight ); pnn->vector.arHiddenWeight = 0;
        free( pnn->vector.arOutputWeight ); pnn->vector.arOutputWeight = 0;
        free( pnn->vector.arHiddenThreshold ); pnn->vector.arHiddenThreshold = 0;
        free( pnn->vector.arOutputThreshold ); pnn->vector.arOutputThreshold = 0;
    #endif

  return 0;
}

#if VECTOR
vector float sigmoidv (vector float vx);
float sigmoid2 (const float x);
#endif

static int Evaluate( neuralnet *pnn, float arInput[], float ar[],
		     float arOutput[], float *saveAr ) {
    int i, j;
    float *prWeight;
    
    /* Calculate activity at hidden nodes */
    
    for( i = 0; i < pnn->cHidden; i++ )
        ar[ i ] = pnn->arHiddenThreshold[ i ];

    if (gVector && VECTOR_MULTIPLYSIG && !saveAr) {
        #if VECTOR
        Vector_MultiplySig (pnn, pnn->cInput, pnn->cHidden, arInput, ar, pnn->vector.arHiddenWeight, -pnn->rBetaHidden);
        #endif
    }
    else {
            
        if (gVector && VECTOR_MULTIPLY) {
            #if VECTOR
            Vector_Multiply (pnn, pnn->cInput, pnn->cHidden, arInput, ar, pnn->vector.arHiddenWeight);
            #endif
        }    
        else {
            prWeight = pnn->arHiddenWeight;
            
            for( i = 0; i < pnn->cInput; i++ ) {
                float const ari = arInput[ i ];
        
                if( ari ) {
                    float *pr = ar;
        
                    if( ari == 1.0f )
                        for( j = pnn->cHidden; j; j-- )
                            *pr++ += *prWeight++;
                    else
                        for( j = pnn->cHidden; j; j-- )
                            *pr++ += *prWeight++ * ari;
                } else
                    prWeight += pnn->cHidden;
            }
        }
    
        if( saveAr)
        memcpy( saveAr, ar, pnn->cHidden * sizeof( *saveAr));
    
        if (gVector && VECTOR_SIGMOID) {
            #if VECTOR
            Vector_Sigmoid (pnn->cHidden, ar, -pnn->rBetaHidden);
            #endif
        }    
        else {
            for( i = 0; i < pnn->cHidden; i++ )
                ar[ i ] = sigmoid ( -pnn->rBetaHidden * ar[ i ] );
        }
        
    }

    /* Calculate activity at output nodes */

    if (gVector && VECTOR_MULTIPLYSIG) {
        for( i = 0; i < pnn->cOutput; i++ ) 
            arOutput[i] = pnn->arOutputThreshold[ i ];
        #if VECTOR
        Vector_MultiplySig (pnn, pnn->cHidden, pnn->cOutput, ar, arOutput, pnn->vector.arOutputWeight, -pnn->rBetaOutput);
        #endif
    }
    else {
        
        if (gVector && VECTOR_MULTIPLY) {
            #if VECTOR
                for( i = 0; i < pnn->cOutput; i++ ) 
                    arOutput[i] = pnn->arOutputThreshold[ i ];
                Vector_Multiply (pnn, pnn->cHidden, pnn->cOutput, ar, arOutput, pnn->vector.arOutputWeight);
                #if VECTOR_SIGMOID
                Vector_Sigmoid (pnn->cOutput, arOutput, -pnn->rBetaOutput);
                #else
                for( i = 0; i < pnn->cOutput; i++ )
                    arOutput[ i ] = sigmoid ( -pnn->rBetaOutput * arOutput[ i ] );
                #endif
            #endif
        }
        else {
            prWeight = pnn->arOutputWeight;
    
            for( i = 0; i < pnn->cOutput; i++ ) {
                float r = pnn->arOutputThreshold[ i ];
                
                for( j = 0; j < pnn->cHidden; j++ )
                    r += ar[ j ] * *prWeight++;
        
                #if VECTOR_SIGMOID
                arOutput[ i ] = r;                
                #else
                arOutput[ i ] = sigmoid ( -pnn->rBetaOutput * r );                
                #endif
            }
            #if VECTOR_SIGMOID
            Vector_Sigmoid (pnn->cOutput, arOutput, -pnn->rBetaOutput);
            #endif
        }
        
    }
    
    return 0;
}

static int EvaluateFromBase( neuralnet *pnn, float arInputDif[], float ar[],
		     float arOutput[] ) {
    int i, j;
    float *prWeight;

    /* Calculate activity at hidden nodes */
/*    for( i = 0; i < pnn->cHidden; i++ )
	ar[ i ] = pnn->arHiddenThreshold[ i ]; */

    prWeight = pnn->arHiddenWeight;
    
    for( i = 0; i < pnn->cInput; ++i ) {
	float const ari = arInputDif[ i ];

	if( ari ) {
	    float *pr = ar;

	    if( ari == 1.0f )
		for( j = pnn->cHidden; j; j-- )
		    *pr++ += *prWeight++;
	    else
            if(ari == -1.0f)
              for(j = pnn->cHidden; j; j-- ) 
                *pr++ -= *prWeight++;
            else
		for( j = pnn->cHidden; j; j-- )
		    *pr++ += *prWeight++ * ari;
	} else
	    prWeight += pnn->cHidden;
    }
    
    for( i = 0; i < pnn->cHidden; i++ )
	ar[ i ] = sigmoid( -pnn->rBetaHidden * ar[ i ] );

    /* Calculate activity at output nodes */
    prWeight = pnn->arOutputWeight;

    for( i = 0; i < pnn->cOutput; i++ ) {
	float r = pnn->arOutputThreshold[ i ];
	
	for( j = 0; j < pnn->cHidden; j++ )
	    r += ar[ j ] * *prWeight++;

	arOutput[ i ] = sigmoid( -pnn->rBetaOutput * r );
    }

    return 0;
}

extern int NeuralNetEvaluate( neuralnet *pnn, float arInput[],
			      float arOutput[], NNEvalType t ) {
#if VECTOR
    VECTOR_FLOATARRAY(ar,1024);
#else
#if __GNUC__
    float ar[ pnn->cHidden ];
#elif HAVE_ALLOCA
    float *ar = alloca( pnn->cHidden * sizeof( float ) );
#else
    float ar[ 1024 ];
#endif
#endif
    switch( t ) {
      case NNEVAL_NONE:
      {
        //fprintf (stderr, "<nneval-none>");
        Evaluate(pnn, arInput, ar, arOutput, 0);
        break;
      }
      case NNEVAL_SAVE:
      {
        //fprintf (stderr, "<nneval-save>");
        #if PROCESSING_UNITS
            savedBases	*psb = GetThreadSavedBases (pnn);
            assert (psb);
            memcpy(psb->savedIBase, arInput, pnn->cInput * sizeof(*ar));
            Evaluate(pnn, arInput, ar, arOutput, psb->savedBase);
        #else
            memcpy(pnn->savedIBase, arInput, pnn->cInput * sizeof(*ar));
            Evaluate(pnn, arInput, ar, arOutput, pnn->savedBase);
        #endif
        break;
      }
      case NNEVAL_FROMBASE:
      {
        int i;
        //fprintf (stderr, "<nneval-frombase>");        
        #if PROCESSING_UNITS
            savedBases	*psb = GetThreadSavedBases (pnn);
            assert (psb);
            memcpy(ar, psb->savedBase, pnn->cHidden * sizeof(*ar));
        #else
            memcpy(ar, pnn->savedBase, pnn->cHidden * sizeof(*ar));
        #endif
  
        {
          float* r = arInput;
          #if PROCESSING_UNITS
          float* s = psb->savedIBase;          
          #else
          float* s = pnn->savedIBase;
          #endif
          
          for(i = 0; i < pnn->cInput; ++i, ++r, ++s) {
            if( *r != *s ) {
              *r -= *s;
            } else {
              *r = 0.0;
            }
          }
        }
        EvaluateFromBase(pnn, arInput, ar, arOutput);
        break;
      }
    }
    return 0;
}

/*
 * Calculate the partial derivate of the output neurons with respect to
 * each of the inputs.
 *
 * Note: this is a numerical approximation to the derivative.  There is
 * an analytical solution below, with an explanation of why it is not used.
 */
extern int NeuralNetDifferentiate( neuralnet *pnn, float arInput[],
				   float arOutput[], float arDerivative[] ) {
#if VECTOR
    VECTOR_FLOATARRAY(ar,1024);
    VECTOR_FLOATARRAY(arIDelta,1024);
    VECTOR_FLOATARRAY(arODelta,1024);
#else
#if __GNUC__
    float ar[ pnn->cHidden ], arIDelta[ pnn->cInput ],
	arODelta[ pnn->cOutput ];
#elif HAVE_ALLOCA
    float *ar = alloca( pnn->cHidden * sizeof( float ) ),
	*arIDelta = alloca( pnn->cInput * sizeof( float ) ),
	*arODelta = alloca( pnn->cOutput * sizeof( float ) );
#else
    float ar[ 1024 ], arIDelta[ 1024 ], arODelta[ 1024 ];
#endif
#endif

    int i, j;

    Evaluate( pnn, arInput, ar, arOutput, 0 );

    memcpy( arIDelta, arInput, sizeof( arIDelta ) );
    
    for( i = 0; i < pnn->cInput; i++ ) {
	arIDelta[ i ] = arInput[ i ] + 0.001f;
	if( i )
	    arIDelta[ i - 1 ] = arInput[ i - 1 ];
	
	Evaluate( pnn, arIDelta, ar, arODelta, 0 );

	for( j = 0; j < pnn->cOutput; j++ )
	    arDerivative[ j * pnn->cInput + i ] =
		( arODelta[ j ] - arOutput[ j ] ) * 1000.0f;
    }

    return 0;
}

/*
 * Here is an analytical solution to the partial derivative.  Normally this
 * algorithm would be preferable (for its efficiency and numerical stability),
 * but unfortunately it relies on the relation:
 *
 *  d sigmoid(x)
 *  ------------ = sigmoid( x ) . sigmoid( -x )
 *       dx
 *                                                              x  -1
 * This relation is true for the function sigmoid( x ) = ( 1 + e  ), but
 * the inaccuracy of the polynomial approximation to the exponential
 * function used here leads to significant error near x=0.  There
 * are numerous possible solutions, but in the meantime the numerical
 * algorithm above will do.
 */
#if 0
extern int NeuralNetDifferentiate( neuralnet *pnn, float arInput[],
				   float arOutput[], float arDerivative[] ) {
#if VECTOR
    VECTOR_FLOATARRAY(ar,1024);
    *ardOdSigmaI = malloc( pnn->cHidden * pnn->cOutput * sizeof( float ) );
#else
#if __GNUC__
    float ar[ pnn->cHidden ], ardOdSigmaI[ pnn->cHidden * pnn->cOutput ];
#elif HAVE_ALLOCA
    float *ar = alloca( pnn->cHidden * sizeof( float ) ),
	*ardOdSigmaI = alloca( pnn->cHidden * pnn->cOutput * sizeof( float ) );
#else
    float ar[ 1024 ], *ardOdSigmaI = malloc( pnn->cHidden * pnn->cOutput *
					sizeof( float ) );
#endif
#endif
    int i, j, k;
    float rdOdSigmaH, *prWeight, *prdOdSigmaI, *prdOdI;
    
    Evaluate( pnn, arInput, ar, arOutput );

    for( i = 0; i < pnn->cHidden; i++ )
	ar[ i ] = ar[ i ] * ( 1.0f - ar[ i ] ) * pnn->rBetaHidden;

    prWeight = pnn->arOutputWeight;
    prdOdSigmaI = ardOdSigmaI;
    
    for( i = 0; i < pnn->cOutput; i++ ) {
	rdOdSigmaH = arOutput[ i ] * ( 1.0f - arOutput[ i ] ) *
	    pnn->rBetaOutput;
	for( j = 0; j < pnn->cHidden; j++ )
	    *prdOdSigmaI++ = rdOdSigmaH * ar[ j ] * *prWeight++;
    }

    prdOdI = arDerivative;
    
    for( i = 0; i < pnn->cOutput; i++ ) {
	prWeight = pnn->arHiddenWeight;
	for( j = 0; j < pnn->cInput; j++ ) {
	    *prdOdI = 0.0f;
	    prdOdSigmaI = ardOdSigmaI + i * pnn->cHidden;
	    for( k = 0; k < pnn->cHidden; k++ )
		*prdOdI += *prdOdSigmaI++ * *prWeight++;
	    prdOdI++;
	}
    }

#if VECTOR
    free( ardOdSigmaI );
#else
#if !__GNUC__ && !HAVE_ALLOCA
    free( ardOdSigmaI );
#endif
#endif

    return 0;
}
#endif

extern int NeuralNetTrain( neuralnet *pnn, float arInput[], float arOutput[],
			   float arDesired[], float rAlpha ) {
    int i, j;    
    float *pr, *prWeight;
#if VECTOR
    VECTOR_FLOATARRAY(ar,1024);
    VECTOR_FLOATARRAY(arOutputError,1024);
    VECTOR_FLOATARRAY(arHiddenError,1024);
#else
#if __GNUC__
    float ar[ pnn->cHidden ], arOutputError[ pnn->cOutput ],
	arHiddenError[ pnn->cHidden ];
#elif HAVE_ALLOCA
    float *ar = alloca( pnn->cHidden * sizeof( float ) ),
	*arOutputError = alloca( pnn->cOutput * sizeof( float ) ),
	*arHiddenError = alloca( pnn->cHidden * sizeof( float ) );
#else
    float ar[ 1024 ], arOutputError[ 128 ], arHiddenError[ 1024 ];
#endif
#endif;
    
    Evaluate( pnn, arInput, ar, arOutput, 0 );

    /* Calculate error at output nodes */
    for( i = 0; i < pnn->cOutput; i++ )
	arOutputError[ i ] = ( arDesired[ i ] - arOutput[ i ] ) *
	    pnn->rBetaOutput * arOutput[ i ] * ( 1 - arOutput[ i ] );

    /* Calculate error at hidden nodes */
    for( i = 0; i < pnn->cHidden; i++ )
	arHiddenError[ i ] = 0.0;

    prWeight = pnn->arOutputWeight;
    
    for( i = 0; i < pnn->cOutput; i++ )
	for( j = 0; j < pnn->cHidden; j++ )
	    arHiddenError[ j ] += arOutputError[ i ] * *prWeight++;

    for( i = 0; i < pnn->cHidden; i++ )
	arHiddenError[ i ] *= pnn->rBetaHidden * ar[ i ] * ( 1 - ar[ i ] );

    /* Adjust weights at output nodes */
    prWeight = pnn->arOutputWeight;
    
    for( i = 0; i < pnn->cOutput; i++ ) {
	for( j = 0; j < pnn->cHidden; j++ )
	    *prWeight++ += rAlpha * arOutputError[ i ] * ar[ j ];

	pnn->arOutputThreshold[ i ] += rAlpha * arOutputError[ i ];
    }

    /* Adjust weights at hidden nodes */
    for( i = 0; i < pnn->cInput; i++ )
	if( arInput[ i ] == 1.0 )
	    for( prWeight = pnn->arHiddenWeight + i * pnn->cHidden,
		     j = pnn->cHidden, pr = arHiddenError; j; j-- )
		*prWeight++ += rAlpha * *pr++;
	else if( arInput[ i ] )
	    for( prWeight = pnn->arHiddenWeight + i * pnn->cHidden,
		     j = pnn->cHidden, pr = arHiddenError; j; j-- )
		*prWeight++ += rAlpha * *pr++ * arInput[ i ];

    for( i = 0; i < pnn->cHidden; i++ )
	pnn->arHiddenThreshold[ i ] += rAlpha * arHiddenError[ i ];

    pnn->nTrained++;
    
    return 0;
}

extern int NeuralNetResize( neuralnet *pnn, int cInput, int cHidden,
			    int cOutput ) {
    int i, j;
    float *pr, *prNew;

    CheckRC();
    
    if( cHidden != pnn->cHidden ) {
	if( !( pnn->arHiddenThreshold = realloc( pnn->arHiddenThreshold,
		cHidden * sizeof( float ) ) ) )
	    return -1;

	for( i = pnn->cHidden; i < cHidden; i++ )
	    pnn->arHiddenThreshold[ i ] = ( ( irand( &rc ) & 0xFFFF ) -
					    0x8000 ) / 131072.0;
    }
    
    if( cHidden != pnn->cHidden || cInput != pnn->cInput ) {
	if( !( pr = malloc( cHidden * cInput * sizeof( float ) ) ) )
	    return -1;

	prNew = pr;
	
	for( i = 0; i < cInput; i++ )
	    for( j = 0; j < cHidden; j++ )
		if( j >= pnn->cHidden )
		    *prNew++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) /
			131072.0;
		else if( i >= pnn->cInput )
		    *prNew++ = ( ( irand( &rc ) & 0x0FFF ) - 0x0800 ) /
			131072.0;
		else
		    *prNew++ = pnn->arHiddenWeight[ i * pnn->cHidden + j ];
		    
	free( pnn->arHiddenWeight );

	pnn->arHiddenWeight = pr;
    }
	
    if( cOutput != pnn->cOutput ) {
	if( !( pnn->arOutputThreshold = realloc( pnn->arOutputThreshold,
		cOutput * sizeof( float ) ) ) )
	    return -1;

	for( i = pnn->cOutput; i < cOutput; i++ )
	    pnn->arOutputThreshold[ i ] = ( ( irand( &rc ) & 0xFFFF ) -
					    0x8000 ) / 131072.0;
    }
    
    if( cOutput != pnn->cOutput || cHidden != pnn->cHidden ) {
	if( !( pr = malloc( cOutput * cHidden * sizeof( float ) ) ) )
	    return -1;

	prNew = pr;
	
	for( i = 0; i < cHidden; i++ )
	    for( j = 0; j < cOutput; j++ )
		if( j >= pnn->cOutput )
		    *prNew++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) /
			131072.0;
		else if( i >= pnn->cHidden )
		    *prNew++ = ( ( irand( &rc ) & 0x0FFF ) - 0x0800 ) /
			131072.0;
		else
		    *prNew++ = pnn->arOutputWeight[ i * pnn->cOutput + j ];

	free( pnn->arOutputWeight );

	pnn->arOutputWeight = pr;
    }

    pnn->cInput = cInput;
    pnn->cHidden = cHidden;
    pnn->cOutput = cOutput;
    
    #if VECTOR
        free (pnn->vector.arHiddenWeight);
        pnn->vector.arHiddenWeight = calloc (VECTOR_COUNTALIGN(cHidden) * VECTOR_COUNTALIGN(cInput),  sizeof( float ) );
        free (pnn->vector.arOutputWeight);
        pnn->vector.arOutputWeight = calloc (VECTOR_COUNTALIGN(cOutput) * VECTOR_COUNTALIGN(cHidden), sizeof( float ) );
        free (pnn->vector.arHiddenThreshold);
        pnn->vector.arHiddenThreshold = calloc (VECTOR_COUNTALIGN(cHidden), sizeof (float));
        free (pnn->vector.arOutputThreshold);
        pnn->vector.arOutputThreshold = calloc (VECTOR_COUNTALIGN(cOutput), sizeof (float));
        pnn->vector.dirty = 1;
    #endif
    
    return 0;
}

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf ) {

    int i, nTrained;
    float *pr;

    if( fscanf( pf, "%d %d %d %d %f %f\n", &pnn->cInput, &pnn->cHidden,
		&pnn->cOutput, &nTrained, &pnn->rBetaHidden,
		&pnn->rBetaOutput ) < 5 || pnn->cInput < 1 ||
	pnn->cHidden < 1 || pnn->cOutput < 1 || nTrained < 0 ||
	pnn->rBetaHidden <= 0.0 || pnn->rBetaOutput <= 0.0 ) {
	errno = EINVAL;

	return -1;
    }

    if( NeuralNetCreate( pnn, pnn->cInput, pnn->cHidden, pnn->cOutput,
			 pnn->rBetaHidden, pnn->rBetaOutput ) )
	return -1;

    pnn->nTrained = nTrained;
    
    for( i = pnn->cInput * pnn->cHidden, pr = pnn->arHiddenWeight; i; i-- )
	if( fscanf( pf, "%f\n", pr++ ) < 1 )
	    return -1;

    for( i = pnn->cHidden * pnn->cOutput, pr = pnn->arOutputWeight; i; i-- )
	if( fscanf( pf, "%f\n", pr++ ) < 1 )
	    return -1;

    for( i = pnn->cHidden, pr = pnn->arHiddenThreshold; i; i-- )
	if( fscanf( pf, "%f\n", pr++ ) < 1 )
	    return -1;

    for( i = pnn->cOutput, pr = pnn->arOutputThreshold; i; i-- )
	if( fscanf( pf, "%f\n", pr++ ) < 1 )
	    return -1;

    #if !PROCESSING_UNITS
    
    for(i = 0; i < pnn->cHidden; ++i)
      pnn->savedBase[i] = 0.0;

    for(i = 0; i < pnn->cInput; ++i) 
      pnn->savedIBase[i] = 0.0;
      
    #endif
    
    #if VECTOR
    pnn->vector.dirty = 1;
    #endif

    return 0;
}

extern int NeuralNetLoadBinary( neuralnet *pnn, FILE *pf ) {

    int nTrained, i;

#define FREAD( p, c ) \
    if( fread( (p), sizeof( *(p) ), (c), pf ) < (c) ) return -1;

    FREAD( &pnn->cInput, 1 );
    FREAD( &pnn->cHidden, 1 );
    FREAD( &pnn->cOutput, 1 );
    FREAD( &nTrained, 1 );
    FREAD( &pnn->rBetaHidden, 1 );
    FREAD( &pnn->rBetaOutput, 1 );

    if( pnn->cInput < 1 || pnn->cHidden < 1 || pnn->cOutput < 1 ||
	nTrained < 0 || pnn->rBetaHidden <= 0.0 || pnn->rBetaOutput <= 0.0 ) {
	errno = EINVAL;

	return -1;
    }

    if( NeuralNetCreate( pnn, pnn->cInput, pnn->cHidden, pnn->cOutput,
			 pnn->rBetaHidden, pnn->rBetaOutput ) )
	return -1;

    pnn->nTrained = nTrained;
    
    FREAD( pnn->arHiddenWeight, pnn->cInput * pnn->cHidden );
    FREAD( pnn->arOutputWeight, pnn->cHidden * pnn->cOutput );
    FREAD( pnn->arHiddenThreshold, pnn->cHidden );
    FREAD( pnn->arOutputThreshold, pnn->cOutput );
#undef FREAD
    
    #if !PROCESSING_UNITS
    
    for(i = 0; i < pnn->cHidden; ++i)
      pnn->savedBase[i] = 0.0;

    for(i = 0; i < pnn->cInput; ++i) 
      pnn->savedIBase[i] = 0.0;
    
    #endif
 
    #if VECTOR
    pnn->vector.dirty = 1;
    #endif

    return 0;
}

extern int NeuralNetSave( neuralnet *pnn, FILE *pf ) {

    int i;
    float *pr;
    
    if( fprintf( pf, "%d %d %d %d %11.7f %11.7f\n", pnn->cInput, pnn->cHidden,
		 pnn->cOutput, pnn->nTrained, pnn->rBetaHidden,
		 pnn->rBetaOutput ) < 0 )
	return -1;

    for( i = pnn->cInput * pnn->cHidden, pr = pnn->arHiddenWeight; i; i-- )
	if( fprintf( pf, "%11.7f\n", *pr++ ) < 0 )
	    return -1;

    for( i = pnn->cHidden * pnn->cOutput, pr = pnn->arOutputWeight; i; i-- )
	if( fprintf( pf, "%11.7f\n", *pr++ ) < 0 )
	    return -1;

    for( i = pnn->cHidden, pr = pnn->arHiddenThreshold; i; i-- )
	if( fprintf( pf, "%11.7f\n", *pr++ ) < 0 )
	    return -1;

    for( i = pnn->cOutput, pr = pnn->arOutputThreshold; i; i-- )
	if( fprintf( pf, "%11.7f\n", *pr++ ) < 0 )
	    return -1;

    return 0;
}

extern int NeuralNetSaveBinary( neuralnet *pnn, FILE *pf ) {

#define FWRITE( p, c ) \
    if( fwrite( (p), sizeof( *(p) ), (c), pf ) < (c) ) return -1;

    FWRITE( &pnn->cInput, 1 );
    FWRITE( &pnn->cHidden, 1 );
    FWRITE( &pnn->cOutput, 1 );
    FWRITE( &pnn->nTrained, 1 );
    FWRITE( &pnn->rBetaHidden, 1 );
    FWRITE( &pnn->rBetaOutput, 1 );

    FWRITE( pnn->arHiddenWeight, pnn->cInput * pnn->cHidden );
    FWRITE( pnn->arOutputWeight, pnn->cHidden * pnn->cOutput );
    FWRITE( pnn->arHiddenThreshold, pnn->cHidden );
    FWRITE( pnn->arOutputThreshold, pnn->cOutput );
#undef FWRITE
    
    return 0;
}


