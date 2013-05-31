## $Id$

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DAC_DOCDIR='"$(datadir)/doc/gnubg/"' -DAC_DATADIR='"$(datadir)"' -DAC_PKGDATADIR='"$(datadir)/gnubg"'

GUIRESOURCES = gnubg-gui.rc gnubg-gui.ico
CLIRESOURCES = gnubg-nogui.rc gnubg-nogui.ico

EXTRA_DIST = $(GUIRESOURCES) $(CLIRESOURCES)

if WIN32

if USE_GTK
WINRESOURCES = $(GUIRESOURCES)
else
WINRESOURCES = $(CLIRESOURCES)
endif

BUILT_SOURCES = win32res.o

win32res.o: $(WINRESOURCES) Makefile
	$(WINDRES) $< -o $@

CLEANFILES=win32res.o

endif
