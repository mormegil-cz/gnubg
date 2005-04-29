
#if USE_SSE_VECTORIZE

#include <xmmintrin.h>
#include <mm_malloc.h>

#define ALIGN_SIZE 16

#define sse_malloc(A) _mm_malloc(A,ALIGN_SIZE)
#define sse_free _mm_free

#define _mm_load_ps _mm_loadu_ps
#define _mm_store_ps _mm_storeu_ps

#else

#define sse_malloc malloc
#define sse_free free

#endif

#define HIDDEN_NODES 128
