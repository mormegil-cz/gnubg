/*
 * acconfig.h
 *
 * by Gary Wong, 1999, 2000
 *
 * $Id$
 */

/* Installation directory (used to determine PKGDATADIR below). */
#undef DATADIR

@BOTTOM@
/* Are we using either GUI (ext or GTK)? */
#if USE_EXT || USE_GTK
#define USE_GUI 1
#endif

/* The directory where the weights and databases will be stored. */
#define PKGDATADIR DATADIR "/" PACKAGE

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
