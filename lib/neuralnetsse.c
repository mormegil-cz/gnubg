/*
 * neuralnetsse.c
 * by Jon Kinsey, 2006
 *
 * SSE (Intel) specific code
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

#if USE_SSE_VECTORIZE

#define DEBUG_SSE 0

#include "sse.h"
#include "neuralnet.h"
#include <string.h>
#if DEBUG_SSE
#include <assert.h>
#endif

#ifdef USE_SSE2 
#include <emmintrin.h> 
#else
#include <xmmintrin.h> 
#endif

#include <glib.h>
#include "sigmoid.h"

#define HIDDEN_NODES 128

float *sse_malloc(size_t size)
{
	return (float *)_mm_malloc(size, ALIGN_SIZE);
}

void sse_free(float* ptr)
{
	_mm_free(ptr);
}

#if USE_SSE2
#include <stdint.h>

static const union {
	float f[4];
	__m128 ps;
} ones = {{1.0f, 1.0f, 1.0f, 1.0f}};

static const union {
	float f[4];
	__m128 ps;
} tens = {{10.0f, 10.0f, 10.0f, 10.0f}};

static const union {
	int32_t i32[4];
	__m128 ps;
} abs_mask = {{0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF}};

static inline __m128 sigmoid_positive_ps( __m128 xin )
{
	union {
		__m128i i;
		int32_t i32[4];
	} i;
	__m128 ex;
	float *ex_elem = (float*) &ex;
	__m128 x1 = _mm_min_ps( xin, tens.ps );

	x1 = _mm_mul_ps( x1, tens.ps );
	i.i  = _mm_cvtps_epi32( x1 );
 	ex_elem[0] = e[i.i32[0]];
	ex_elem[1] = e[i.i32[1]];
	ex_elem[2] = e[i.i32[2]];
	ex_elem[3] = e[i.i32[3]];
	x1 = _mm_sub_ps( x1, _mm_cvtepi32_ps( i.i ) ); 
	x1 = _mm_add_ps( x1, tens.ps );
	x1 = _mm_mul_ps( x1, ex );
	x1 = _mm_add_ps( x1, ones.ps ); 
	return _mm_rcp_ps( x1 );
}

static inline __m128 sigmoid_ps( __m128 xin )
{	
	__m128 mask = _mm_cmplt_ps( xin, _mm_setzero_ps() );
	__m128 c; 
	xin = _mm_and_ps (xin , abs_mask.ps ); /* Abs. value by clearing signbit */
	c = sigmoid_positive_ps(xin);
	return _mm_or_ps( _mm_and_ps(  mask, c ) , _mm_andnot_ps ( mask , _mm_sub_ps( ones.ps, c )));
}

#endif  // USE_SSE2

static void
Evaluate128( const neuralnet *pnn, const float arInput[], float ar[],
                        float arOutput[], float *saveAr ) {

    unsigned int i, j;
    float *prWeight;
#if USE_SSE2
    float *par;
#endif
    __m128 vec0, vec1, vec3, scalevec, sum;
    
    /* Calculate activity at hidden nodes */
    memcpy(ar, pnn->arHiddenThreshold, HIDDEN_NODES * sizeof(float));

    prWeight = pnn->arHiddenWeight;
    
	for (i = 0; i < pnn->cInput; i++)
	{
		float const ari = arInput[i];

		if (ari)
		{
			float *pr = ar;
			if (ari == 1.0f)
			{
				for( j = 32; j; j--, pr += 4, prWeight += 4 )
				{
                   vec0 = _mm_load_ps( pr );  
                   vec1 = _mm_load_ps( prWeight );  
                   sum =  _mm_add_ps(vec0, vec1);
                   _mm_store_ps(pr, sum);
				}
			}
			else
			{
                scalevec = _mm_set1_ps( ari );
				for( j = 32; j; j--, pr += 4, prWeight += 4 )
				{
					vec0 = _mm_load_ps( pr );  
					vec1 = _mm_load_ps( prWeight ); 
					vec3 = _mm_mul_ps( vec1, scalevec );
					sum =  _mm_add_ps( vec0, vec3 );
					_mm_store_ps ( pr, sum );
				}
			}
		}
		else
			prWeight += HIDDEN_NODES;
    }

    if( saveAr)
      memcpy( saveAr, ar, HIDDEN_NODES * sizeof( *saveAr));
    
#if USE_SSE2
	scalevec = _mm_set1_ps(pnn->rBetaHidden);
	for (par = ar, i = 32; i; i--, par += 4) {
		__m128 vec = _mm_load_ps(par);
		vec = _mm_mul_ps(vec, scalevec);
		vec = sigmoid_ps(vec);
		_mm_store_ps(par, vec);
	}
#else
	for (i = 0; i < HIDDEN_NODES; i++)
		ar[i] = sigmoid(-pnn->rBetaHidden * ar[i]);
#endif
    
    /* Calculate activity at output nodes */
    prWeight = pnn->arOutputWeight;

    for( i = 0; i < pnn->cOutput; i++ ) {

       float r;
       float *pr = ar;
       sum = _mm_setzero_ps();
       for( j = 32; j ; j--, prWeight += 4, pr += 4 ){
         vec0 = _mm_load_ps( pr );           /* Four floats into vec0 */
         vec1 = _mm_load_ps( prWeight );     /* Four weights into vect1 */ 
         vec3 = _mm_mul_ps ( vec0, vec1 );   /* Multiply */
         sum = _mm_add_ps( sum, vec3 );  /* Add */
       }

       vec0 = _mm_shuffle_ps(sum, sum,_MM_SHUFFLE(2,3,0,1));
       vec1 = _mm_add_ps(sum, vec0);
       vec0 = _mm_shuffle_ps(vec1,vec1,_MM_SHUFFLE(1,1,3,3));
       sum = _mm_add_ps(vec1,vec0);
       _mm_store_ss(&r, sum); 

       arOutput[ i ] = sigmoid( -pnn->rBetaOutput * (r + pnn->arOutputThreshold[ i ]));
    }
}


extern int NeuralNetEvaluate128(const neuralnet *pnn, /*lint -e{818}*/ float arInput[],
			      float arOutput[], NNState * UNUSED(pnState))
{
    SSE_ALIGN(float ar[HIDDEN_NODES]);

#if DEBUG_SSE
	/* Not 64bit robust (pointer truncation) - causes strange crash */
    assert(sse_aligned(ar));
    assert(sse_aligned(arInput));
    assert(pnn->cHidden == HIDDEN_NODES);
#endif

	Evaluate128(pnn, arInput, ar, arOutput, 0);
    return 0;
}

#endif
