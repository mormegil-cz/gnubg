
#if USE_SSE_VECTORIZE

#include <xmmintrin.h>
#include <mm_malloc.h>

#define ALIGN_SIZE 16

#define sse_malloc(A) _mm_malloc(A,ALIGN_SIZE)
#define sse_free _mm_free

#define sse_aligned(ar) (!(((int)ar) % ALIGN_SIZE))

#ifdef _MSC_VER
#define SSE_ALIGN(D) __declspec(align(ALIGN_SIZE)) D
#else
#define SSE_ALIGN(D) D __attribute__ ((aligned(ALIGN_SIZE)))
#endif

#else

#define sse_malloc malloc
#define sse_free free

#define SSE_ALIGN(D) D

#endif

#define HIDDEN_NODES 128
