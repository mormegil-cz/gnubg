##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##

INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS) @LIBART_CFLAGS@ \
           -I$(srcdir)/../lib @GUILE_CFLAGS@ @LIBXML2_CFLAGS@ \
              -I$(srcdir)/../intl -DLOCALEDIR=\"$(localedir)\" \
              @FT2_CFLAGS@ @GTKEXTRA_CFLAGS@ @GTKGLEXT_CFLAGS@

noinst_LIBRARIES = libboard3d.a
libboard3d_a_SOURCES = drawboard3d.c matrix.h mylist.c \
	timer.c misc3d.c  widget3d.c gtkcolour3d.c \
        matrix.c model.c shadow.c texture3d.c  font3d.cpp


noinst_HEADERS = shadow.h mylist.h inc3d.h
