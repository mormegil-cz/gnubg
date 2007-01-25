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
#include "common.h"

#include <glib.h>
#include <errno.h>
#include <isaac.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "neuralnet.h"
#include "sse.h"

/* e[k] = exp(k/10) / 10 */
static float e[100] = {
0.10000000000000001f, 
0.11051709180756478f, 
0.12214027581601698f, 
0.13498588075760032f, 
0.14918246976412702f, 
0.16487212707001281f, 
0.18221188003905089f, 
0.20137527074704767f, 
0.22255409284924679f, 
0.245960311115695f, 
0.27182818284590454f, 
0.30041660239464335f, 
0.33201169227365473f, 
0.36692966676192446f, 
0.40551999668446748f, 
0.44816890703380646f, 
0.49530324243951152f, 
0.54739473917271997f, 
0.60496474644129461f, 
0.66858944422792688f, 
0.73890560989306509f, 
0.81661699125676512f, 
0.90250134994341225f, 
0.99741824548147184f, 
1.1023176380641602f, 
1.2182493960703473f, 
1.3463738035001691f, 
1.4879731724872838f, 
1.6444646771097049f, 
1.817414536944306f, 
2.0085536923187668f, 
2.2197951281441637f, 
2.4532530197109352f, 
2.7112638920657881f, 
2.9964100047397011f, 
3.3115451958692312f, 
3.6598234443677988f, 
4.0447304360067395f, 
4.4701184493300818f, 
4.9402449105530168f, 
5.4598150033144233f, 
6.034028759736195f, 
6.6686331040925158f, 
7.3699793699595784f, 
8.1450868664968148f, 
9.0017131300521811f, 
9.9484315641933776f, 
10.994717245212353f, 
12.151041751873485f, 
13.428977968493552f, 
14.841315910257659f, 
16.402190729990171f, 
18.127224187515122f, 
20.033680997479166f, 
22.140641620418716f, 
24.469193226422039f, 
27.042640742615255f, 
29.886740096706028f, 
33.029955990964865f, 
36.503746786532886f, 
40.34287934927351f, 
44.585777008251675f, 
49.274904109325632f, 
54.457191012592901f, 
60.184503787208222f, 
66.514163304436181f, 
73.509518924197266f, 
81.24058251675433f, 
89.784729165041753f, 
99.227471560502622f, 
109.66331584284585f, 
121.19670744925763f, 
133.9430764394418f, 
148.02999275845451f, 
163.59844299959269f, 
180.80424144560632f, 
199.81958951041173f, 
220.83479918872089f, 
244.06019776244983f, 
269.72823282685101f, 
298.09579870417281f, 
329.44680752838406f, 
364.09503073323521f, 
402.38723938223131f, 
444.7066747699858f, 
491.47688402991344f, 
543.16595913629783f, 
600.29122172610175f, 
663.42440062778894f, 
733.19735391559948f, 
810.3083927575384f, 
895.52927034825075f, 
989.71290587439091f, 
1093.8019208165192f, 
1208.8380730216988f, 
1335.9726829661872f, 
1476.4781565577266f, 
1631.7607198015421f, 
1803.3744927828525f, 
1993.0370438230298f, 
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

/* NB. can't be inlined as now used in sse file as well */
extern /*inline*/ float sigmoid_original(float const xin) {
    
    if( !signbit( xin ) ) { /* signbit() can be faster than a compare to 0.0 */
	/* xin is almost always positive; we place this branch of the `if'
	   first, in the hope that the compiler/processor will predict the
	   conditional branch will not be taken. */
	if( xin < 10.0f ) {
	    /* again, predict the branch not to be taken */
	    const float x1 = 10.0f * xin;
	    const int i = (int)x1;
	    
		return 1 / (1 + e[i] * ((10 - i) + x1));
 	} else
	    return 1.0f / 19931.370438230298f;
    } else {
	if( xin > -10.0f ) {
	    const float x1 = -10.0f * xin;
	    const int i = (int)x1;
	    
		return 1 - 1 / (1 + e[i] * ((10 - i) + x1));
	} else
	    return 19930.370438230298f / 19931.370438230298f;	
    }
}

#if SIGMOID_BAUR

#define SIG_Q 10.0f     /* reciprocal of precision step */                      
#define SIG_MAX 100     /* half number of entries in lookup table */
/* note: the lookup table covers the interval [ -SIG_MAX/SIG_Q  to
+SIG_MAX/SIG_Q ] */
static float Sig[2*SIG_MAX+1];

#endif /* SIGMOID_BAUR */

#if SIGMOID_JTH

#define SIG_Q 10.0f
#define SIG_MAX 100  /* half number of entries */

static float SigD0[ 2 * SIG_MAX + 1 ]; /* sigmoid(x0) */
static float SigD1[ 2 * SIG_MAX + 1 ]; /* sigmoid'(x0) */

#endif /* SIGMOID_JTH */

extern void 
ComputeSigTable (void) {

#if SIGMOID_BAUR

  int i;

  for (i = -SIG_MAX; i < SIG_MAX+1; i++) {
    float x = (float) i / SIG_Q;
    /* Sig[SIG_MAX+i] = 1.0f / (1.0f + exp (x));  more accurate, 
                                                  but fails gnubgtest */
    /* use the current sigmoid function instead :-)  */
    Sig[SIG_MAX+i] = sigmoid_original(x);  
  }

  printf( "Table with %d elements initialised\n", 2 * SIG_MAX + 1 );

#endif /* SIGMOID_BAUR */

#if SIGMOID_JTH

  int i;
  for ( i = -SIG_MAX; i < SIG_MAX + 1; ++i ) {
    float x = (float) i / SIG_Q;
    SigD0[ SIG_MAX + i ] = sigmoid_original(x);
    SigD1[ SIG_MAX + i ] = 
      - exp(x) * SigD0[ SIG_MAX + i ] * SigD0[ SIG_MAX + i ] / SIG_Q;
  }

  printf( "SIGMOID_JTH: table with %d elements initialised.\n", 
          2 * SIG_MAX + 1 );

#endif /* SIGMOID_JTH */

}

#if SIGMOID_BAUR

extern float 
sigmoid_baur (float const x) {

  float x2 = x * SIG_Q;
  int i = x2;

  if (i > - SIG_MAX) {
    if (i < SIG_MAX) {
      float a = Sig[i+SIG_MAX];
      float b = Sig[i+SIG_MAX+1];
      return a + (b - a) * (x2 - i);
    }
    else
      /* warning: this is 1.0f/(1.0f+exp(9.9f)) */
      return 1.0f / 19931.370438230298f;  
  }
  else
    /* warning: this is 1.0f/(1.0f+exp(-9.9f)) */
    return 19930.370438230298f / 19931.370438230298f;  
}

#endif /* SIGMOID_BAUR */

#if SIGMOID_JTH

extern float
sigmoid_jth( const float x ) {

  float x2 = x * SIG_Q;
  int i = x2;

  if (i > - SIG_MAX) {
    if (i < SIG_MAX) {
      float fx0   = SigD0[i+SIG_MAX];
      float dfx0 = SigD1[i+SIG_MAX];
      float s = fx0 + dfx0 * ( x2 - i );
      /*
      float d = sigmoid_original(x);
      if ( fabs(s-d) > 2.0e-3 ) {
        printf( "%f %f %f %f %f %f %f \n", x, x2, x0, fx0, dfx0, s, d );
        }*/
      return s;
    }
    else
      /* warning: this is 1.0f/(1.0f+exp(9.9f)) */
      return 1.0f / 19931.370438230298f;  
  }
  else
    /* warning: this is 1.0f/(1.0f+exp(-9.9f)) */
    return 19930.370438230298f / 19931.370438230298f;  
}

#endif /* SIGMOID_JTH */


/*  #define sigmoid( x ) ( (x) > 0.0f ? \ */
/*  		       1.0f / ( 2.0f + (x) + (1.0f / 2.0f) * (x) * (x) ) : \ */
/*  		       1.0f - 1.0f / ( 2.0f + -(x) + \ */
/*  				       (1.0f / 2.0f) * (x) * (x) ) ) */

static int frc;
static randctx rc; /* for irand */

static void CheckRC( void ) {

    if( !frc ) {
        int i;

        rc.randrsl[ 0 ] = (ub4)time( NULL );
        for( i = 0; i < RANDSIZ; i++ )
           rc.randrsl[ i ] = rc.randrsl[ 0 ];
        irandinit( &rc, TRUE );

        frc = TRUE;
    }
}

/* separate context for race, crashed, contact
   -1: regular eval
    0: save base
    1: from base
 */

static inline NNEvalType
NNevalAction(NNState *pnState)
{
	if (!pnState)
		return NNEVAL_NONE;

	switch(pnState->state)
	{
    case NNSTATE_NONE:
    {
      /* incremental evaluation not useful */
      return NNEVAL_NONE;
    }
    case NNSTATE_INCREMENTAL:
    {
      /* next call should return FROMBASE */
		pnState->state = NNSTATE_DONE;
      
      /* starting a new context; save base in the hope it will be useful */
      return NNEVAL_SAVE;
    }
    case NNSTATE_DONE:
    {
      /* context hit!  use the previously computed base */
      return NNEVAL_FROMBASE;
    }
  }

  /* never reached */
  g_assert(0);
  return 0;   /* for the picky compiler */
}

extern int NeuralNetCreate( neuralnet *pnn, unsigned int cInput, unsigned int cHidden,
			    unsigned int cOutput, float rBetaHidden,
			    float rBetaOutput ) {
    unsigned int i;
    float *pf;
    
    pnn->cInput = cInput;
    pnn->cHidden = cHidden;
    pnn->cOutput = cOutput;
    pnn->rBetaHidden = rBetaHidden;
    pnn->rBetaOutput = rBetaOutput;
    pnn->nTrained = 0;
    pnn->fDirect = FALSE;
   
    if( ( pnn->arHiddenWeight = sse_malloc( cHidden * cInput *
					 sizeof( float ) ) ) == NULL )
		return -1;

    if( ( pnn->arOutputWeight = sse_malloc( cOutput * cHidden *
					 sizeof( float ) ) ) == NULL ) {
		sse_free( pnn->arHiddenWeight );
		return -1;
    }
    
    if( ( pnn->arHiddenThreshold = sse_malloc( cHidden * sizeof( float ) ) ) == NULL ) {
		sse_free( pnn->arOutputWeight );
		sse_free( pnn->arHiddenWeight );
		return -1;
    }
	   
    if( ( pnn->arOutputThreshold = sse_malloc( cOutput * sizeof( float ) ) ) == NULL ) {
		sse_free( pnn->arHiddenThreshold );
		sse_free( pnn->arOutputWeight );
		sse_free( pnn->arHiddenWeight );
		return -1;
    }

    CheckRC();

    for( i = cHidden * cInput, pf = pnn->arHiddenWeight; i; i-- )
		*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0f;
    
    for( i = cOutput * cHidden, pf = pnn->arOutputWeight; i; i-- )
		*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0f;
    
    for( i = cHidden, pf = pnn->arHiddenThreshold; i; i-- )
		*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0f;
    
    for( i = cOutput, pf = pnn->arOutputThreshold; i; i-- )
		*pf++ = ( (int) ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0f;

    return 0;
}
#if HAVE_MMAP
extern void *NeuralNetCreateDirect( neuralnet *pnn, void *p ) {
 
   float *fp;
   int *ip = (int *)p;
   pnn->cInput = *ip;
   ip++;
   pnn->cHidden = *ip;
   ip++;
   pnn->cOutput = *ip;
   ip++;
   pnn->nTrained = *ip;
   ip++;
   pnn->fDirect = TRUE;
   fp = (float*)ip;
   pnn->rBetaHidden = *fp;
   fp++;
   pnn->rBetaOutput = *fp;
   fp++;
 
   if( pnn->cInput < 1 || pnn->cHidden < 1 || pnn->cOutput < 1 ||
     pnn->nTrained < 0 || pnn->rBetaHidden <= 0.0 ||
     pnn->rBetaOutput <= 0.0 ) {
     errno = EINVAL;
     
     return NULL;
   }
 
   pnn->arHiddenWeight = fp;
   fp += pnn->cInput * pnn->cHidden;
   pnn->arOutputWeight = fp;
   fp += pnn->cHidden * pnn->cOutput;
   pnn->arHiddenThreshold = fp;
   fp += pnn->cHidden;
   pnn->arOutputThreshold = fp;
   fp += pnn->cOutput;

   return fp;
}
#endif

extern void
NeuralNetDestroy( neuralnet *pnn )
{
  if( !pnn->fDirect ) {
    sse_free( pnn->arHiddenWeight ); pnn->arHiddenWeight = 0;
    sse_free( pnn->arOutputWeight ); pnn->arOutputWeight = 0;
    sse_free( pnn->arHiddenThreshold ); pnn->arHiddenThreshold = 0;
    sse_free( pnn->arOutputThreshold ); pnn->arOutputThreshold = 0;
  }
}

static void Evaluate( const neuralnet *pnn, const float arInput[], float ar[],
                        float arOutput[], float *saveAr ) {

    unsigned int i, j;
    float *prWeight;

    /* Calculate activity at hidden nodes */
    for( i = 0; i < pnn->cHidden; i++ )
		ar[ i ] = pnn->arHiddenThreshold[ i ];

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

    if( saveAr)
      memcpy( saveAr, ar, pnn->cHidden * sizeof( *saveAr));


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
}

static void EvaluateFromBase( const neuralnet *pnn, const float arInputDif[], float ar[],
		     float arOutput[] ) {

    unsigned int i, j;
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
}

extern int NeuralNetEvaluate( const neuralnet *pnn, float arInput[],
			      float arOutput[], NNState *pnState )
{
    float *ar = (float*) g_alloca(pnn->cHidden * sizeof(float));
    switch( NNevalAction(pnState) ) {
      case NNEVAL_NONE:
      {
        Evaluate(pnn, arInput, ar, arOutput, 0);
        break;
      }
      case NNEVAL_SAVE:
      {
        memcpy(pnState->savedIBase, arInput, pnn->cInput * sizeof(*ar));
        Evaluate(pnn, arInput, ar, arOutput, pnState->savedBase);
        break;
      }
      case NNEVAL_FROMBASE:
      {
        unsigned int i;
        
        memcpy(ar, pnState->savedBase, pnn->cHidden * sizeof(*ar));
  
        {
          float* r = arInput;
          float* s = pnState->savedIBase;
         
          for(i = 0; i < pnn->cInput; ++i, ++r, ++s) {
            if( *r != *s /*lint --e(777) */) {
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
extern int NeuralNetDifferentiate( const neuralnet *pnn, const float arInput[],
				   float arOutput[], float arDerivative[] ) {
    float *ar = (float*) g_alloca(pnn->cHidden * sizeof(float));
    float *arIDelta = (float*) g_alloca(pnn->cInput * sizeof(float));
    float *arODelta = (float*) g_alloca(pnn->cOutput * sizeof(float));
    unsigned int i, j;

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
    float *ar = (float*) g_alloca(pnn->cHidden * sizeof(float));
    float *ardOdSigmaI = (float*) g_alloca(pnn->cHidden * pnn->cOutput * sizeof(float));
    unsigned int i, j, k;
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

    return 0;
}
#endif

extern int NeuralNetTrain( neuralnet *pnn, const float arInput[], float arOutput[],
			   const float arDesired[], float rAlpha ) {
    unsigned int i, j;    
    float *pr, *prWeight;
    float *ar = (float*) g_alloca(pnn->cHidden * sizeof(float));
    float *arOutputError = (float*) g_alloca(pnn->cOutput * sizeof(float));
    float *arHiddenError = (float*) g_alloca(pnn->cHidden * sizeof(float));
    
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

extern int NeuralNetResize( neuralnet *pnn, unsigned int cInput, unsigned int cHidden,
			    unsigned int cOutput ) {
    unsigned int i, j;
    float *pr, *prNew;

    CheckRC();
    
    if( cHidden != pnn->cHidden ) {
		if( ( pnn->arHiddenThreshold = (float*)realloc( pnn->arHiddenThreshold,
				cHidden * sizeof( float ) ) ) == NULL )
			return -1;

	for( i = pnn->cHidden; i < cHidden; i++ )
	    pnn->arHiddenThreshold[ i ] = ( ( irand( &rc ) & 0xFFFF ) -
					    0x8000 ) / 131072.0f;
    }
    
    if( cHidden != pnn->cHidden || cInput != pnn->cInput ) {
		if( ( pr = sse_malloc( cHidden * cInput * sizeof( float ) ) ) == NULL )
			return -1;

		prNew = pr;
		
		for( i = 0; i < cInput; i++ )
			for( j = 0; j < cHidden; j++ )
			if( j >= pnn->cHidden )
				*prNew++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) /
				131072.0f;
			else if( i >= pnn->cInput )
				*prNew++ = ( ( irand( &rc ) & 0x0FFF ) - 0x0800 ) /
				131072.0f;
			else
				*prNew++ = pnn->arHiddenWeight[ i * pnn->cHidden + j ];
			    
		sse_free( pnn->arHiddenWeight );

		pnn->arHiddenWeight = pr;
    }
	
    if( cOutput != pnn->cOutput ) {
		if( ( pnn->arOutputThreshold = (float*)realloc( pnn->arOutputThreshold,
			cOutput * sizeof( float ) ) ) == NULL )
			return -1;

		for( i = pnn->cOutput; i < cOutput; i++ )
			pnn->arOutputThreshold[ i ] = ( ( irand( &rc ) & 0xFFFF ) -
							0x8000 ) / 131072.0f;
    }
    
    if( cOutput != pnn->cOutput || cHidden != pnn->cHidden ) {
		if( ( pr = sse_malloc( cOutput * cHidden * sizeof( float ) ) ) == NULL )
			return -1;

		prNew = pr;
		
		for( i = 0; i < cHidden; i++ )
			for( j = 0; j < cOutput; j++ )
			if( j >= pnn->cOutput )
				*prNew++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) /
				131072.0f;
			else if( i >= pnn->cHidden )
				*prNew++ = ( ( irand( &rc ) & 0x0FFF ) - 0x0800 ) /
				131072.0f;
			else
				*prNew++ = pnn->arOutputWeight[ i * pnn->cOutput + j ];

		sse_free( pnn->arOutputWeight );

		pnn->arOutputWeight = pr;
    }

    pnn->cInput = cInput;
    pnn->cHidden = cHidden;
    pnn->cOutput = cOutput;
    
    return 0;
}

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf ) {

    unsigned int i;
	int nTrained;
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

    return 0;
}

extern int NeuralNetLoadBinary( neuralnet *pnn, FILE *pf ) {

	int nTrained;

#define FREAD( p, c ) \
    if( fread( (p), sizeof( *(p) ), (c), pf ) < (unsigned int)(c) ) return -1;

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

    return 0;
}

extern int NeuralNetSave( const neuralnet *pnn, FILE *pf ) {

    unsigned int i;
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

extern int NeuralNetSaveBinary( const neuralnet *pnn, FILE *pf ) {

#define FWRITE( p, c ) \
    if( fwrite( (p), sizeof( *(p) ), (c), pf ) < (unsigned int)(c) ) return -1;

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
#if USE_SSE_VECTORIZE

#if defined(_MSC_VER) || defined(DISABLE_SSE_TEST)

int SSE_Supported()
{
	return 1;
}

#else

int CheckSSE()
{
	int result = 0;

	asm (
		/* Check if cpuid is supported (can bit 21 of flags be changed) */
		"mov $1, %%eax\n\t"
		"shl $21, %%eax\n\t"
		"mov %%eax, %%edx\n\t"

		"pushfl\n\t"
		"popl %%eax\n\t"
		"mov %%eax, %%ecx\n\t"
		"xor %%edx, %%eax\n\t"
		"pushl %%eax\n\t"
		"popfl\n\t"

		"pushfl\n\t"
		"popl %%eax\n\t"
		"xor %%ecx, %%eax\n\t"
		"test %%edx, %%eax\n\t"
		"jnz 1f\n\t"
		/* Failed (non-pentium compatible machine) */
		"mov $-1, %%ebx\n\t"
		"jp 4f\n\t"

"1:"
		/* Check feature test is supported */
		"mov $0, %%eax\n\t"
		"cpuid\n\t"
		"cmp $1, %%eax\n\t"
		"jge 2f\n\t"
		/* Unlucky - somehow cpuid 1 isn't supported */
		"mov $-2, %%ebx\n\t"
		"jp 4f\n\t"

"2:"
		/* Check if sse is supported (bit 25 in edx from cpuid 1) */
		"mov $1, %%eax\n\t"
		"cpuid\n\t"
		"mov $1, %%eax\n\t"
		"shl $25, %%eax\n\t"
		"test %%eax, %%edx\n\t"
		"jnz 3f\n\t"
		/* Not supported */
		"mov $0, %%ebx\n\t"
		"jp 4f\n\t"
"3:"
		/* Supported */
		"mov $1, %%ebx\n\t"
"4:"

			: "=b"(result) : : "%eax", "%ecx", "%edx");
	
	switch (result)
	{
	case -1:
		printf("Can't check for SSE - non pentium cpu\n");
		break;
	case -2:
		printf("No sse cpuid check available\n");
		break;
	case 0:
		/* No SSE support */
		break;
	case 1:
		/* SSE support */
		return 1;
	default:
		printf("Unknown return testing for SSE\n");
	}

	return 0;
}

int SSE_Supported()
{
	static int state = -1;

	if (state == -1)
		state = CheckSSE();

	return state;
}

#endif
#endif
