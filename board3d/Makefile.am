##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##

INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS) \
	-I$(top_srcdir)/lib @LIBXML2_CFLAGS@ \
	-I$(top_srcdir)/intl -DLOCALEDIR=\"$(localedir)\" \
	@FT2_CFLAGS@ @GTKGLEXT_CFLAGS@

noinst_LTLIBRARIES = libboard3d.la
libboard3d_la_SOURCES = drawboard3d.c matrix.h \
	misc3d.c  widget3d.c gtkcolour3d.c \
        matrix.c model.c shadow.c font3d.c graph.c model.h fun3d.h font3d.h


noinst_HEADERS = shadow.h inc3d.h fun3d.h font3d.h matrix.h
