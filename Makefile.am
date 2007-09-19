## $Id$

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib doc met po m4 xpm sounds board3d textures scripts flags fonts non-src .

bin_PROGRAMS = gnubg makebearoff makehyper bearoffdump makeweights

#
##include path
#
INCLUDES = -I$(srcdir)/lib -I$(srcdir)/board3d @LIBXML2_CFLAGS@ \
	   -DLOCALEDIR=\"$(localedir)\" @FREETYPE2_CFLAGS@ @PYTHON_CSPEC@ @GLIB_CFLAGS@ \
	   @LIBPNG_CFLAGS@ @GTK_CFLAGS@ @CAIRO_CFLAGS@ @GTHREAD_CFLAGS@ @ESD_CFLAGS@ \
	   @WINMMSND_CFLAGS@ @GTKGLEXT_CFLAGS@

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
		dice.c \
		dice.h \
		drawboard.c \
		drawboard.h \
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
		multithread.h \
		openurl.c \
		openurl.h \
		osr.c \
		osr.h \
		play.c \
		positionid.c \
		positionid.h \
		postscript.c \
		progress.c \
		progress.h \
		pub_eval.c \
		record.c \
		record.h \
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
		sgfl.c \
		sgfp.c \
		sgfp.h \
		show.c \
		sound.c \
		sound.h \
		speed.c \
		speed.h \
		text.c \
		timer.c

if USE_MULTITHREAD
gnubg_SOURCES +=  multithread.c
endif

if USE_PYTHON
gnubg_SOURCES += gnubgmodule.c gnubgmodule.h
endif

if USE_GTK
gnubg_SOURCES += gtkboard.c gtkboard.h gtkgame.c gtkgame.h gtkfile.c gtkfile.h \
		 gtkprefs.c gtkprefs.h gtk-multiview.c gtk-multiview.h gtktheory.c \
		 gtktheory.h gtkexport.c gtkexport.h gtkcube.c gtkcube.h \
		 gtkchequer.c gtkchequer.h gtkrace.c gtkrace.h gtkmovefilter.c \
		 gtkmovefilter.h gtkmet.c gtkmet.h gtkcolour.c gtkcolour.h gtksplash.c \
		 gtksplash.h gtkrolls.c gtkrolls.h gtktempmap.c gtktempmap.h \
		 gtkoptions.h gtkoptions.c gtktoolbar.h gtktoolbar.c \
		 gtkgamelist.c gtkpanels.c gtkmovelist.c gtkmovelistctrl.c gtkmovelistctrl.h \
		 gtkwindows.c gtkwindows.h
if USE_PYTHON
gnubg_SOURCES += gtkrelational.c gtkrelational.h
endif
endif

#
##linkpath for gnubg
#
gnubg_LDADD = -Llib lib/libevent.la @GTK_LIBS@ @LIBPNG_LIBS@ @CAIRO_LIBS@ \
	      @GTHREAD_LIBS@ @ESD_LIBS@ \
	      @WINMMSND_LIBS@ @LTLIBOBJS@ @LTLIBINTL@ @LIBXML2_LIBS@ \
	      @FREETYPE2_LIBS@ @PYTHON_LSPEC@ @GLIB_LIBS@ @LIBPNG_LIBS@
if USE_BOARD3D
gnubg_LDADD += -L$(srcdir)/board3d board3d/libboard3d.la @GTKGLEXT_LIBS@
endif


#
##common sources for building the utility programs
#
UTILSOURCES = eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
	osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h \
	format.c format.h mec.h mec.c util.c util.h

makebearoff_SOURCES = makebearoff.c $(UTILSOURCES)
makebearoff_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ @LIBXML2_LIBS@

makehyper_SOURCES = makehyper.c $(UTILSOURCES)
makehyper_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ @LIBXML2_LIBS@

bearoffdump_SOURCES = bearoffdump.c $(UTILSOURCES)
bearoffdump_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ @LIBXML2_LIBS@

makeweights_SOURCES = makeweights.c
makeweights_LDADD = -Llib lib/libevent.la @GLIB_LIBS@ 


#
##files to be installed in the datadir
#
pkgdata_DATA = gnubg_ts0.bd gnubg.wd boards.xml \
	gnubg_os0.bd textures.txt gnubg.game.sql gnubg.gtkrc gnubg.svg gnubg.png

#
##files to add to the tarball when 'make dist'
#
EXTRA_DIST = config.rpath  userrng.c copying.awk gnubg.6 \
	gnubg.gtkrc credits.sh \
	external_l.c external_y.c external_y.h sgfp.c sgfp.h sgfl.c \
	external_l.l external_y.y sgf.l sgf.y \
	ABOUT-NLS boards.xml gnubg.game.sql autogen.sh gnubg.weights\
	textures.txt gnubg.png gnubg.svg

#
##sgf.y and sgf.l ought to be renamed sgf_y.y and sgf_l.l to avoid conflicts with sgf.c
#
sgfp.c sgfp.h: sgf.y
	$(YACC) -o sgfp.c -p sgf -d $(srcdir)/sgf.y

sgfl.c: sgf.l
	$(LEX) -o$@ -Psgf $(srcdir)/sgf.l

sgf.c: sgfp.c sgfp.h sgfl.c
	@true

#
##external_l.c includes external_y.h and automake doesn't track this dependency
#
external_l.c: external_l.l external_y.c
	$(LEXCOMPILE) $<
	sed '/^#/ s|$(LEX_OUTPUT_ROOT)\.c|$@|' $(LEX_OUTPUT_ROOT).c >$@
	rm -f $(LEX_OUTPUT_ROOT).c

#
##credits.c target
#
credits.c credits.h: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

#
##copying.c target
#
copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@

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
	@echo ' **   makebearoff -o 6 -s 7999999 > gnubg_os0.bd'
	@echo ' **   makebearoff -t 6x6 > gnubg_ts0.bd'
	@echo ' ** on the host system.'
else
gnubg.wd: gnubg.weights makeweights$(EXEEXT)
	[ -s $@ ] || \
	./makeweights < $< > $@
gnubg_os0.bd: makebearoff$(EXEEXT)
	[ -s $@ ] || \
	./makebearoff -o 6 -s 7999999 -f $@
gnubg_ts0.bd: makebearoff$(EXEEXT)
	[ -s $@ ] || \
	./makebearoff -t 6x6 -f $@
endif

MOSTLYCLEANFILES=sgfp.c sgfp.h sgfl.c external_l.c external_l.h external_y.c external_y.h
