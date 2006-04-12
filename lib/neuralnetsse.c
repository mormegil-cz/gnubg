/*
 * neuralnetsse.c
 * by Jon Kinsey, 2006
 *
 * SSE (Intel) specific code
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

#if USE_SSE_VECTORIZE

#include "sse.h"
#include "eval.h"
#include "neuralnet.h"
#include <string.h>
#include <assert.h>

#include <xmmintrin.h>
#include <mm_malloc.h>

#define sse_aligned(ar) (!(((int)ar) % ALIGN_SIZE))

#define HIDDEN_NODES 128

void *sse_malloc(size_t size)
{
	if (SSE_Supported())
		return _mm_malloc(size, ALIGN_SIZE);
	else
		return malloc(size);
}

void sse_free(void* ptr)
{
	if (SSE_Supported())
		_mm_free(ptr);
	else
		free(ptr);
}

static int
Evaluate128( neuralnet *pnn, float arInput[], float ar[],
                        float arOutput[], float *saveAr ) {

    int i, j;
    float *prWeight;
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
    
    for( i = 0; i < HIDDEN_NODES; i++ )
	ar[ i ] = sigmoid( -pnn->rBetaHidden * ar[ i ] );
    
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

    return 0;
}

static int EvaluateFromBase128( neuralnet *pnn, float arInputDif[], float ar[],
		     float arOutput[] ) {

    int i, j;
    float *prWeight;
    __m128 vec0, vec1, vec3, scalevec, sum;

    prWeight = pnn->arHiddenWeight;
    
    for( i = 0; i < pnn->cInput; ++i ) {
	float const ari = arInputDif[ i ];

	if( ari ) {
	    float *pr = ar;

	    if( ari == 1.0f )
		for( j = 32; j; j--, pr += 4, prWeight += 4 ){
                   vec0 = _mm_load_ps( pr );  
                   vec1 = _mm_load_ps( prWeight );  
                   sum =  _mm_add_ps(vec0, vec1);
                   _mm_store_ps(pr, sum);
		}
	    else
            if(ari == -1.0f)
		for( j = 32; j; j--, pr += 4, prWeight += 4 ){
                   vec0 = _mm_load_ps( pr );  
                   vec1 = _mm_load_ps( prWeight );  
                   sum =  _mm_sub_ps(vec0, vec1);
                   _mm_store_ps(pr, sum);
		}
	    else {
		scalevec = _mm_set1_ps( ari );
		for( j = 32; j; j--, pr += 4, prWeight += 4 ){
                   vec0 = _mm_load_ps( pr );  
	           vec1 = _mm_load_ps( prWeight ); 
		   vec3 = _mm_mul_ps( vec1, scalevec );
                   sum =  _mm_add_ps( vec0, vec3 );
                   _mm_store_ps ( pr, sum );
		}
	    }
	} else
	    prWeight += HIDDEN_NODES;
    }
    
    for( i = 0; i < HIDDEN_NODES; i++ )
	ar[ i ] = sigmoid( -pnn->rBetaHidden * ar[ i ] );

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

    return 0;

}

extern int NeuralNetEvaluate128( neuralnet *pnn, float arInput[],
			      float arOutput[], NNEvalType t ) {

    SSE_ALIGN(float ar[HIDDEN_NODES]);
    assert(sse_aligned(ar));
    assert(sse_aligned(arInput));

    assert(pnn->cHidden == HIDDEN_NODES);

    switch( t ) {
      case NNEVAL_NONE:
      {
        Evaluate128(pnn, arInput, ar, arOutput, 0);
        break;
      }
      case NNEVAL_SAVE:
      {
        memcpy(pnn->savedIBase, arInput, pnn->cInput * sizeof(*ar));
        Evaluate128(pnn, arInput, ar, arOutput, pnn->savedBase);
        break;
      }
      case NNEVAL_FROMBASE:
      {
        int i;
        
        memcpy(ar, pnn->savedBase, HIDDEN_NODES * sizeof(*ar));
  
        {
          float* r = arInput;
          float* s = pnn->savedIBase;
         
          for(i = 0; i < pnn->cInput; ++i, ++r, ++s) {
            if( *r != *s ) {
              *r -= *s;
            } else {
              *r = 0.0;
            }
          }
        }
        EvaluateFromBase128(pnn, arInput, ar, arOutput);
        break;
      }
    }
    return 0;
}

#endif
