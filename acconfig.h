/*
 * acconfig.h
 *
 * by Gary Wong, 1999, 2000, 2001
 *
 */

/* Installation directory (used to determine PKGDATADIR below). */
#undef DATADIR

/* Define if you have the gtkextra library (-lgtkextra).  */
#undef HAVE_LIBGTKEXTRA

@BOTTOM@
/* Are we using either GUI (ext or GTK)? */
#if USE_EXT || USE_GTK
#define USE_GUI 1
#endif

/* The directory where the weights and databases will be stored. */
#define PKGDATADIR DATADIR "/" PACKAGE

/* Define the obvious signbit() if the C library doesn't. */
#if !HAVE_SIGNBIT
#define signbit(x) ( (x) < 0.0 )
#endif

/* Use the double versions of the math functions if the float ones aren't
   available. */
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
