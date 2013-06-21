## $Id$

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = win32 lib doc met po m4 sounds board3d textures scripts flags fonts non-src pixmaps .

bin_PROGRAMS = gnubg makebearoff makehyper bearoffdump makeweights

#
##include path
#
if WIN32
localeloc=-DLOCALEDIR=\"locale\" 
else
localeloc=-DLOCALEDIR=\"$(localedir)\"
endif 

AM_CPPFLAGS = -I$(srcdir)/lib -I$(srcdir)/board3d -I$(srcdir)/pixmaps \
	   $(localeloc) @FREETYPE2_CFLAGS@ @GLIB_CFLAGS@ \
	   @LIBPNG_CFLAGS@ @GTK_CFLAGS@ @CAIRO_CFLAGS@ @PANGOCAIRO_CFLAGS@ \
	   @GTHREAD_CFLAGS@ @GTKGLEXT_CFLAGS@ @CANBERRA_CFLAGS@

if USE_SQLITE
AM_CPPFLAGS += @SQLITE_CFLAGS@
endif
if USE_PYTHON
AM_CPPFLAGS += @PYTHON_INCLUDES@ 
endif

AM_CPPFLAGS += -DAC_DOCDIR='"$(datadir)/doc/gnubg/"' -DAC_DATADIR='"$(datadir)"' -DAC_PKGDATADIR='"$(datadir)/gnubg"'

OTHER_LIBS =
if WIN32
OTHER_LIBS += win32/win32res.o
endif

BUILT_SOURCES = sgf_y.c sgf_l.c copying.c credits.c external_l.c external_y.c

#
## sources for building the main executable
#

gnubg_SOURCES = \
		analysis.c \
		analysis.h \
		backgammon.h \
		bearoff.c \
		bearoffgammon.c \
		bearoffgammon.h \
		bearoff.h \
		boarddim.h \
		boardpos.c \
		boardpos.h \
		common.h \
		copying.c \
		credits.c \
		credits.h \
		dbprovider.c \
		dbprovider.h \
		dice.c \
		dice.h \
		drawboard.c \
		drawboard.h \
		evallock.c \
		eval.c \
		eval.h \
		export.c \
		export.h \
		external.c \
		external.h \
		external_l.l \
		external_y.y \
		file.c \
		file.h \
		format.c \
		formatgs.c \
		formatgs.h \
		format.h \
		gnubg.c \
		gnubgmodule.c \
		gnubgmodule.h \
		html.c \
		htmlimages.c \
		import.c \
		import.h \
		latex.c \
		matchequity.c \
		matchequity.h \
		matchid.c \
		matchid.h \
		mec.c \
		mec.h \
		multithread.c \
		multithread.h \
		openurl.c \
		openurl.h \
		osr.c \
		osr.h \
		output.c \
		output.h \
		play.c \
		positionid.c \
		positionid.h \
		progress.c \
		progress.h \
		relational.c \
		relational.h \
		render.c \
		render.h \
		renderprefs.c \
		renderprefs.h \
		rollout.c \
		rollout.h \
		set.c \
		sgf.c \
		sgf.h \
		sgf_l.l \
		sgf_y.y \
		show.c \
		simpleboard.c \
		simpleboard.h \
		sound.c \
		sound.h \
		speed.c \
		speed.h \
		text.c \
		timer.c \
		util.h \
		util.c 

if USE_GTK
gnubg_SOURCES += gtkboard.c gtkboard.h gtkgame.c gtkgame.h gtkfile.c gtkfile.h \
		 gtkprefs.c gtkprefs.h gtk-multiview.c gtk-multiview.h gtktheory.c \
		 gtktheory.h gtkexport.c gtkexport.h gtkcube.c gtkcube.h \
		 gtkchequer.c gtkchequer.h gtkrace.c gtkrace.h gtkmovefilter.c \
		 gtkmovefilter.h gtkmet.c gtkmet.h gtksplash.c \
		 gtksplash.h gtkrolls.c gtkrolls.h gtktempmap.c gtktempmap.h \
		 gtkoptions.h gtkoptions.c gtktoolbar.h gtktoolbar.c \
		 gtkgamelist.c gtkpanels.c gtkpanels.h gtkmovelist.c gtkmovelistctrl.c gtkmovelistctrl.h \
		 gtkwindows.c gtkwindows.h gtkrelational.c gtkrelational.h gnubgstock.c gnubgstock.h \
		 gtkuidefs.h gtklocdefs.c gtklocdefs.h
endif

#
##linkpath for gnubg
#

gnubg_LDADD = -L$(srcdir)/lib lib/libevent.la @GTK_LIBS@ @LIBPNG_LIBS@ @CAIRO_LIBS@ @PANGOCAIRO_LIBS@ \
	      @GTHREAD_LIBS@ \
	      @LTLIBOBJS@ \
	      @FREETYPE2_LIBS@ @GLIB_LIBS@ @LIBPNG_LIBS@ @CANBERRA_LIBS@ \
           $(OTHER_LIBS)

if USE_SQLITE
gnubg_LDADD += @SQLITE_LIBS@
endif
if USE_PYTHON
gnubg_LDADD += @PYTHON_LIBS@ 
endif
if USE_BOARD3D
gnubg_LDADD += -L$(srcdir)/board3d board3d/libboard3d.la @GTKGLEXT_LIBS@
endif

if WIN32
if USE_GTK
gnubg_LDFLAGS = -mwindows
endif
endif



#
##common sources for building the utility programs
#
UTILSOURCES = eval.h eval.c positionid.h positionid.c \
	matchequity.c matchequity.h matchid.h matchid.c \
	osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h \
	mec.h mec.c util.c util.h

makebearoff_SOURCES = makebearoff.c $(UTILSOURCES)
makebearoff_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@

makehyper_SOURCES = makehyper.c $(UTILSOURCES)
makehyper_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@

bearoffdump_SOURCES = bearoffdump.c $(UTILSOURCES)
bearoffdump_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@

makeweights_SOURCES = makeweights.c
makeweights_LDADD = -Llib lib/libevent.la @GLIB_LIBS@ 


#
##files to be installed in the datadir
#
pkgdata_DATA = gnubg_ts0.bd gnubg.wd boards.xml \
	gnubg_os0.bd textures.txt gnubg.sql gnubg.gtkrc

#
##files to add to the tarball when 'make dist'
#
EXTRA_DIST = config.rpath  copying.awk gnubg.gtkrc credits.sh AUTHORS \
	$(BUILT_SOURCES) ABOUT-NLS boards.xml gnubg.sql autogen.sh \
	gnubg.weights textures.txt \
	external_y.h sgf_y.h commands.inc movefilters.inc

#
# targets created by credits.sh
#
credits.c: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

credits.h: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

AUTHORS: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

#
##copying.c target
#
if WIN32
copying.c: copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@

copying.$(OBJEXT): copying.c
	$(CC) -c $(CFLAGS) $< -o $@
else
copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@
endif


#
##databases
#
if CROSS_COMPILING
gnubg.wd:
	@echo ' ** NOTE: Since you are cross-compiling GNU Backgammon,'
	@echo ' ** it is not possible to generate weight and database files'
	@echo ' ** on the build system.  To create these files manually,'
	@echo ' ** use commands like:'
	@echo ' **   makeweights < gnubg.weights > gnubg.wd'
	@echo ' **   makebearoff -o 6 -s 7999999 -f gnubg_os0.bd'
	@echo ' **   makebearoff -t 6x6 -f gnubg_ts0.bd'
	@echo ' ** on the host system.'
else
gnubg.wd: gnubg.weights makeweights$(EXEEXT)
	[ $@ -nt $< ] || \
	./makeweights -f $@ $< 
gnubg_os0.bd: makebearoff$(EXEEXT)
	[ -s $@ ] || \
	./makebearoff -o 6 -s 7999999 -f $@
gnubg_ts0.bd: makebearoff$(EXEEXT)
	[ -s $@ ] || \
	./makebearoff -t 6x6 -f $@
endif

MOSTLYCLEANFILES=sgf_y.c sgf_y.h sgf_l.c external_l.c external_l.h external_y.c external_y.h copying.c credits.c credits.h AUTHORS
DISTCLEANFILES=gnubg_os0.bd gnubg_ts0.bd gnubg.wd
