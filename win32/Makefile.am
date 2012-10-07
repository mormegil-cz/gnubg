## $Id$

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DAC_DOCDIR='"$(datadir)/doc/gnubg/"' -DAC_DATADIR='"$(datadir)"' -DAC_PKGDATADIR='"$(datadir)/gnubg"'

if WIN32

if USE_GTK
WINRESOURCES = gnubg-gui.rc gnubg-gui.ico
else
WINRESOURCES = gnubg-nogui.rc gnubg-nogui.ico
endif

BUILT_SOURCES = win32res.o

win32res.o: $(WINRESOURCES) Makefile
	$(WINDRES) $< -o $@

CLEANFILES=win32res.o

endif

