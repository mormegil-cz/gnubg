/*
 * neuralnet.c
 *
 * by Gary Wong, 1998-2000
 */

#include "config.h"

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <errno.h>
#include <isaac.h>
#include <math.h>
#include <neuralnet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	irandinit( &rc, FALSE );
	frc = TRUE;
    }
}

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
    
    for( i = cHidden * cInput, pf = pnn->arHiddenWeight; i; i-- )
	*pf++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput * cHidden, pf = pnn->arOutputWeight; i; i-- )
	*pf++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cHidden, pf = pnn->arHiddenThreshold; i; i-- )
	*pf++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput, pf = pnn->arOutputThreshold; i; i-- )
	*pf++ = ( ( irand( &rc ) & 0xFFFF ) - 0x8000 ) / 131072.0;

    return 0;
}

extern int NeuralNetDestroy( neuralnet *pnn ) {

    free( pnn->arHiddenWeight );
    free( pnn->arOutputWeight );
    free( pnn->arHiddenThreshold );
    free( pnn->arOutputThreshold );

    return 0;
}

static int Evaluate( neuralnet *pnn, float arInput[], float ar[],
		     float arOutput[] ) {
    int i, j;
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
			      float arOutput[] ) {
#if __GNUC__
    float ar[ pnn->cHidden ];
#elif HAVE_ALLOCA
    float *ar = alloca( pnn->cHidden * sizeof( float ) );
#else
    float ar[ 1024 ];
#endif
    
    return Evaluate( pnn, arInput, ar, arOutput );
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
    int i, j;

    Evaluate( pnn, arInput, ar, arOutput );

    memcpy( arIDelta, arInput, sizeof( arIDelta ) );
    
    for( i = 0; i < pnn->cInput; i++ ) {
	arIDelta[ i ] = arInput[ i ] + 0.001f;
	if( i )
	    arIDelta[ i - 1 ] = arInput[ i - 1 ];
	
	Evaluate( pnn, arIDelta, ar, arODelta );

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
#if __GNUC__
    float ar[ pnn->cHidden ], ardOdSigmaI[ pnn->cHidden * pnn->cOutput ];
#elif HAVE_ALLOCA
    float *ar = alloca( pnn->cHidden * sizeof( float ) ),
	*ardOdSigmaI = alloca( pnn->cHidden * pnn->cOutput * sizeof( float ) );
#else
    float ar[ 1024 ], *ardOdSigmaI = malloc( pnn->cHidden * pnn->cOutput *
					sizeof( float ) );
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
    
#if !__GNUC__ && !HAVE_ALLOCA
    free( ardOdSigmaI );
#endif

    return 0;
}
#endif

extern int NeuralNetTrain( neuralnet *pnn, float arInput[], float arOutput[],
			   float arDesired[], float rAlpha ) {
    int i, j;    
    float *pr, *prWeight;
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
    
    Evaluate( pnn, arInput, ar, arOutput );

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

    return 0;
}

extern int NeuralNetLoadBinary( neuralnet *pnn, FILE *pf ) {

    int nTrained;

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
