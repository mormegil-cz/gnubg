##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##

INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS) \
	-I$(top_srcdir)/lib @LIBXML2_CFLAGS@ \
	-I$(top_srcdir)/intl -DLOCALEDIR=\"$(localedir)\" \
	@FT2_CFLAGS@ @GTKGLEXT_CFLAGS@

noinst_LIBRARIES = libboard3d.a
libboard3d_a_SOURCES = drawboard3d.c matrix.h \
	misc3d.c  widget3d.c gtkcolour3d.c \
        matrix.c model.c shadow.c font3d.c graph.c


noinst_HEADERS = shadow.h inc3d.h
