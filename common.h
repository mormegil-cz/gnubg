
#ifndef _COMMON_H_
#define _COMMON_H_

extern double get_time(void);

#if !defined (__GNUC__) && !defined (__attribute__)
#define __attribute__(X)
#endif

#ifdef HUGE_VAL
#define ERR_VAL (-HUGE_VAL)
#else
#define ERR_VAL (-FLT_MAX)
#endif

#undef PI
#define PI 3.14159265358979323846

#ifndef MIN
#define MIN(A,B) (((A) < (B)) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) (((A) > (B)) ? (A) : (B))
#endif

#if HAVE_SIGACTION
typedef struct sigaction psighandler;
#elif HAVE_SIGVEC
typedef struct sigvec psighandler;
#else
typedef RETSIGTYPE (*psighandler)( int );
#endif

#ifdef WIN32
	#include <stdlib.h>
	#define BIG_PATH _MAX_PATH
#else
	#ifdef HAVE_LIMITS_H
		#include <limits.h>
	#endif
	#define BIG_PATH PATH_MAX
#endif
#endif

#define MAX_NAME_LEN 32

#if defined(HAVE_DECL_LRINT) && !HAVE_DECL_LRINT
/* define lrint as macro if not available */    
#define lrint(x) ((long) ((x)+0.5))     
#endif

#if defined(HAVE_DECL_SIGNBIT) && !HAVE_DECL_SIGNBIT
/* define signbit as macro if not available */    
#define signbit(x) ( (x) < 0.0 )     
#endif

