/* $Id$ 
 */

#if USE_SSE_VECTORIZE

#include <stdlib.h>

#define ALIGN_SIZE 16

#ifdef _MSC_VER
#define SSE_ALIGN(D) __declspec(align(ALIGN_SIZE)) D
#else
#define SSE_ALIGN(D) D __attribute__ ((aligned(ALIGN_SIZE)))
#endif

extern void *sse_malloc(size_t size);
extern void sse_free(void* ptr);

#else

#define SSE_ALIGN(D) D
#define sse_malloc malloc
#define sse_free free

#endif
