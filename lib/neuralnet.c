/*
 * neuralnet.c
 *
 * by Gary Wong, 1998
 */

#include <errno.h>
#include <math.h>
#include <neuralnet.h>
#include <stdio.h>
#include <stdlib.h>

#define sigmoid( x ) ( (x) > 0.0f ? \
		       1.0f / ( 2.0f + (x) + 77.0f / 60.0f * (x) * (x) ) : \
		       1.0f - 1.0f / ( 2.0f + -(x) + \
				       77.0f / 60.0f * (x) * (x) ) )

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

    for( i = cHidden * cInput, pf = pnn->arHiddenWeight; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput * cHidden, pf = pnn->arOutputWeight; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cHidden, pf = pnn->arHiddenThreshold; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
    
    for( i = cOutput, pf = pnn->arOutputThreshold; i; i-- )
	*pf++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;

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
    float r, *pr, *prWeight;

    /* Calculate activity at hidden nodes */
    for( i = 0; i < pnn->cHidden; i++ )
	ar[ i ] = pnn->arHiddenThreshold[ i ];

    for( i = 0; i < pnn->cInput; i++ )
	if( arInput[ i ] == 1.0f )
	    for( prWeight = pnn->arHiddenWeight + i * pnn->cHidden,
		     j = pnn->cHidden, pr = ar; j; j-- )
		*pr++ += *prWeight++;
	else if( arInput[ i ] )
	    for( prWeight = pnn->arHiddenWeight + i * pnn->cHidden,
		     j = pnn->cHidden, pr = ar; j; j-- )
		*pr++ += *prWeight++ * arInput[ i ];
    
    for( i = 0; i < pnn->cHidden; i++ )
	ar[ i ] = sigmoid( -pnn->rBetaHidden * ar[ i ] );

    /* Calculate activity at output nodes */
    prWeight = pnn->arOutputWeight;

    for( i = 0; i < pnn->cOutput; i++ ) {
	r = pnn->arOutputThreshold[ i ];
	for( j = 0; j < pnn->cHidden; j++ )
	    r += ar[ j ] * *prWeight++;

	arOutput[ i ] = sigmoid( -pnn->rBetaOutput * r );
    }

    return 0;
}

extern int NeuralNetEvaluate( neuralnet *pnn, float arInput[],
			      float arOutput[] ) {
    float ar[ pnn->cHidden ];

    return Evaluate( pnn, arInput, ar, arOutput );
}

extern int NeuralNetTrain( neuralnet *pnn, float arInput[], float arOutput[],
			   float arDesired[], float rAlpha ) {
    int i, j;    
    float ar[ pnn->cHidden ], arOutputError[ pnn->cOutput ],
	arHiddenError[ pnn->cHidden ], *pr, *prWeight;

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

    if( cHidden != pnn->cHidden ) {
	if( !( pnn->arHiddenThreshold = realloc( pnn->arHiddenThreshold,
		cHidden * sizeof( float ) ) ) )
	    return -1;

	for( i = pnn->cHidden; i < cHidden; i++ )
	    pnn->arHiddenThreshold[ i ] = ( ( random() & 0xFFFF ) - 0x8000 ) /
		131072.0;
    }
    
    if( cHidden != pnn->cHidden || cInput != pnn->cInput ) {
	if( !( pr = malloc( cHidden * cInput * sizeof( float ) ) ) )
	    return -1;

	prNew = pr;
	
	for( i = 0; i < cInput; i++ )
	    for( j = 0; j < cHidden; j++ )
		if( j >= pnn->cHidden )
		    *prNew++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
		else if( i >= pnn->cInput )
		    *prNew++ = ( ( random() & 0x0FFF ) - 0x0800 ) / 131072.0;
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
	    pnn->arOutputThreshold[ i ] = ( ( random() & 0xFFFF ) - 0x8000 ) /
		131072.0;
    }
    
    if( cOutput != pnn->cOutput || cHidden != pnn->cHidden ) {
	if( !( pr = malloc( cOutput * cHidden * sizeof( float ) ) ) )
	    return -1;

	prNew = pr;
	
	for( i = 0; i < cHidden; i++ )
	    for( j = 0; j < cOutput; j++ )
		if( j >= pnn->cOutput )
		    *prNew++ = ( ( random() & 0xFFFF ) - 0x8000 ) / 131072.0;
		else if( i >= pnn->cHidden )
		    *prNew++ = ( ( random() & 0x0FFF ) - 0x0800 ) / 131072.0;
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
