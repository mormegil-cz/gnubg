// Optional defines...

#define USE_GTK 1
#define USE_GTK2 1

#if USE_GTK
#define USE_BOARD3D 1
#endif

#define HAVE_LIBPNG 1
#define HAVE_LIBART 1
#define HAVE_FREETYPE 1
//#define USE_PYTHON 1
#define USE_SOUND 1
#define USE_TIMECONTROL 1
#define HAVE_SOCKETS 1
#define HAVE_LIBXML2 1
//#define HAVE_LIBGDBM 1

//#define USE_SSE_VECTORIZE 1

#define REDUCTION_CODE 1

// disabled becuase of compile errors in gtk files
//#define ENABLE_NLS 1

///////// End of options - other defines follow...

#define HAVE_LIBREADLINE 0

#if USE_GTK2
	#define HAVE_GTKGLEXT 1
#endif

#if HAVE_LIBGDBM
	#define HAVE_GDBM_ERRNO 0
	#define HAVE_IMP_GDBM_ERRNO 1
#endif

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#define stricmp stricmp_error_use_strcasecmp
#define strnicmp stricmp_error_use_strncasecmp
#endif

/* next line to prevent use of BR1.C */
#undef USE_BUILTIN_BEAROFF

#define HAVE_LOCALE_H 1
#define HAVE_MALLOC_H 1
#define HAVE_SELECT 1
#define HAVE_STRDUP 1
#define HAVE_MEMCPY 1

#define HAVE_IMP_GDBM_ERRNO 1
#define HAVE_SETLOCALE 1

#define HAVE_SETVBUF 0

#define HAVE_SIGPROCMASK 1
#ifndef _MSC_VER
#define HAVE_SYS_TIME_H 1
#define TIME_WITH_SYS_TIME 1
#define HAVE_SYS_FILE_H 1
#define HAVE_UNISTD_H 1
#endif

#define HAVE_ALLOCA 1 
#ifndef HAVE_FSTAT
#define HAVE_FSTAT 1
#endif
#define HAVE_FCNTL_H 1 
#define HAVE_ISATTY 1
#define HAVE_LIMITS_H 1
#define HAVE_STRING_H 1

#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1

#define RETSIGTYPE void
#define STDC_HEADERS 1

#if USE_EXT || USE_GTK
#define USE_GUI 1
#endif

#define PACKAGE "gnubg"
#ifdef _MSC_VER
#define VERSION "0.14-msdev"
#else
#define VERSION "0.14-mingw"
#endif
#define PKGDATADIR ".\\""/" PACKAGE
#ifndef LOCALEDIR
#define LOCALEDIR ""
#endif

#ifdef _MSC_VER
#undef inline
#define inline __inline
#define __inline__ __inline
#define __asm__ __asm
#endif

#if defined(__cplusplus) || !defined(_MSC_VER)

#define HAVE_SINF 1
#define HAVE_TANF 1
#define HAVE_COSF 1
#define HAVE_ASINF 1
#define HAVE_ATANF 1
#define HAVE_ACOSF 1
#define HAVE_LRINT 1
#define HAVE_SIGNBIT 1

#endif

#if !HAVE_SIGNBIT
#define signbit(x) ( (x) < 0.0 )
#endif

#if !HAVE_ACOSF
#define acosf acos
#endif

#if !HAVE_ASINF
#define asinf asin
#endif

#if !HAVE_ATANF
#define atanf atan
#endif

#if !HAVE_COSF
#define cosf cos
#endif

#if !HAVE_LRINT
#define lrint(x) ((long) ((x)+0.5))
#endif

#if !HAVE_SINF
#define sinf sin
#endif

#if !HAVE_TANF
#define tanf tan
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#ifdef _MSC_VER
/* Prevent Windows from defining unused and colliding stuff */
#ifndef USE_GDI
#define NOGDI 1
#endif
#ifndef __cplusplus
#define WIN32_LEAN_AND_MEAN 1
#endif

/* Windows missing defines */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1

#define F_OK 00
#define R_OK 04

#ifdef _MSC_VER
	#define alloca _alloca
#endif
#endif
