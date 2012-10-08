##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##

AM_CPPFLAGS = -I$(srcdir)/.. $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS) \
	-I$(srcdir)/../lib \
	-DLOCALEDIR=\"$(localedir)\" \
	-DAC_DATADIR='"$(datadir)"' -DAC_PKGDATADIR=='"$(datadir)/gnubg"'\
	@FREETYPE2_CFLAGS@ @GTKGLEXT_CFLAGS@

if USE_BOARD3D
noinst_LTLIBRARIES = libboard3d.la
endif

EXTRA_DIST = wglBuffer.c wglBuffer.h

libboard3d_la_SOURCES = drawboard3d.c font3d.c fun3d.h graph.c gtkcolour3d.c \
			inc3d.h matrix.c matrix.h misc3d.c model.c model.h \
			shadow.c types3d.h widget3d.c tr.c tr.h

if WIN32
libboard3d_la_SOURCES += $(EXTRA_DIST)
endif

noinst_HEADERS = fun3d.h inc3d.h matrix.h model.h types3d.h 
