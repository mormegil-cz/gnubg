/*
 * neuralnet.c
 *
 * by Gary Wong <gtw@gnu.org>, 1998, 1999, 2000, 2001, 2002.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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
#include "isaac.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef WIN32
#include <stdlib.h>
#endif

#include "neuralnet.h"
#include "sse.h"
#include "sigmoid.h"

static int frc;
static randctx rc; /* for irand */

static void CheckRC( void )
{
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

static inline NNEvalType NNevalAction(NNState *pnState)
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
	return NNEVAL_NONE;   /* for the picky compiler */
}

extern int NeuralNetCreate( neuralnet *pnn, unsigned int cInput, unsigned int cHidden,
			    unsigned int cOutput, float rBetaHidden,
			    float rBetaOutput )
{
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

extern void NeuralNetDestroy( neuralnet *pnn )
{
  if( !pnn->fDirect ) {
    sse_free( pnn->arHiddenWeight ); pnn->arHiddenWeight = 0;
    sse_free( pnn->arOutputWeight ); pnn->arOutputWeight = 0;
    sse_free( pnn->arHiddenThreshold ); pnn->arHiddenThreshold = 0;
    sse_free( pnn->arOutputThreshold ); pnn->arOutputThreshold = 0;
  }
}

static void Evaluate( const neuralnet *pnn, const float arInput[], float ar[],
                        float arOutput[], float *saveAr )
{

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
		     float arOutput[] )
{

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

extern int NeuralNetResize( neuralnet *pnn, unsigned int cInput, unsigned int cHidden,
			    unsigned int cOutput )
{
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

extern int NeuralNetLoad( neuralnet *pnn, FILE *pf )
{

    unsigned int i;
	int nTrained;
    float *pr;

    if( fscanf( pf, "%u %u %u %d %f %f\n", &pnn->cInput, &pnn->cHidden,
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

extern int NeuralNetLoadBinary( neuralnet *pnn, FILE *pf )
{

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

extern int NeuralNetSaveBinary( const neuralnet *pnn, FILE *pf )
{

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

int SSE_Supported(void)
{
	return 1;
}

#else

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

static int CheckSSE(void)
{
        int result;
#ifdef __APPLE__
    size_t length = sizeof( result );
    int error = sysctlbyname("hw.optional.sse", &result, &length, NULL, 0);
    if ( 0 != error ) result = 0;
    return result;

#else

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
#endif

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

int SSE_Supported(void)
{
	static int state = -1;

	if (state == -1)
		state = CheckSSE();

	return state;
}

#endif
#endif
