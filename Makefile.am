## $Id$

localedir = $(datadir)/locale

ACLOCAL_AMFLAGS = -I m4

## sounds

if  USE_SOUND 
SOUND_SOURCES = sound.c
SOUND_INCS = @ESD_CFLAGS@ @NAS_CFLAGS@ @ARTSC_CFLAGS@ @AUDIOFILE_CFLAGS@ \
	@WINMMSND_CFLAGS@
SOUND_LIBS = @ESD_LIBS@ @NAS_LIBS@ @ARTSC_LIBS@ @AUDIOFILE_LIBS@ \
	@WINMMSND_LIBS@ @APPLE_QTIME_LIBS@
else
SOUND_INCS = 
SOUND_SOURCES =
SOUND_LIBS = 
endif


## timecontrol
if USE_TIMECONTROL
TIME_SOURCES = timecontrol.h timecontrol.c tctutorial.h
else
TIME_SOURCES =
endif

if COMPILE_BR1
BR1_SOURCES = br1.c
else
BR1_SOURCES =
endif

if EXTERNAL_BR1
BR1DATA = gnubg_os0.bd
else
BR1DATA =
endif

BOARD3D_TEXTURE_FILES = textures.txt

if USE_BOARD3D
BOARD3D_DIR=board3d textures
BOARD3D_DEPS=board3d/libboard3d.a
BOARD3D_LIBS=-L$(srcdir)/board3d -lboard3d @GTKGLEXT_LIBS@
BOARD3D_INCS=@GTKGLEXT_CFLAGS@
BOARD3DDATA=$(BOARD3D_TEXTURE_FILES)
else
BOARD3D_DIR=
BOARD3D_DEPS=
BOARD3D_LIBS=
BOARD3D_INCS=
BOARD3DDATA=
endif


SUBDIRS = lib doc met po m4 xpm sounds $(BOARD3D_DIR) scripts flags fonts non-src

if USE_PYTHON
PYTHON_SOURCES=gnubgmodule.c gnubgmodule.h
else
PYTHON_SOURCES=
endif

COMMON_INCS = -I$(srcdir)/lib @LIBXML2_CFLAGS@ \
              -DLOCALEDIR=\"$(localedir)\" \
              @FT2_CFLAGS@ @PYTHON_CFLAGS@ @GLIB_CFLAGS@
COMMON_LIBS = -Llib -levent @LIBOBJS@ @LTLIBINTL@ @LIBXML2_LIBS@ \
              @FT2_LIBS@ @PYTHON_LIBS@ @GLIB_LIBS@

COMMON_SOURCES = credits.h credits.c copying.c backgammon.h gnubg.c \
		 eval.h eval.c play.c \
		 positionid.h positionid.c drawboard.h drawboard.c \
		 pub_eval.c set.c show.c dice.c dice.h rollout.c \
		 rollout.h kleinman.c matchequity.c matchequity.h \
		 analysis.h analysis.c sgf.c sgf.h sgfp.c sgfp.h \
		 sgfl.c import.c import.h external.c external.h \
		 latex.c postscript.c html.c export.h matchid.c \
		 matchid.h text.c export.c sound.h record.c record.h \
		 osr.c osr.h bearoffgammon.c bearoffgammon.h bearoff.c \
		 bearoff.h onechequer.c onechequer.h path.c path.h \
		 render.c render.h htmlimages.c speed.c \
		 renderprefs.c renderprefs.h boardpos.c boardpos.h \
		 progress.c progress.h format.c format.h formatgs.c \
		 formatgs.h boarddim.h timer.c external_y.y \
		 external_l.l openurl.c openurl.h mec.h \
		 mec.c relational.h relational.c common.h

COMMON_DEPS =  lib/libevent.a @LIBOBJS@

bin_PROGRAMS = gnubg makebearoff makehyper bearoffdump

gnubg_LDFLAGS= @PYTHON_LDFLAGS@

if USE_GTK
## --- GTK
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS) $(BOARD3D_INCS) @CAIRO_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h gtkfile.c gtkfile.h \
	gtkprefs.c gtkprefs.h gtk-multiview.c \
	gtk-multiview.h gtktheory.c gtktheory.h gtktexi.c gtktexi.h \
	gtkexport.c gtkexport.h gtkcube.c gtkcube.h gtkchequer.c gtkchequer.h \
	gtkrace.c gtkrace.h \
	gtkmovefilter.c gtkmovefilter.h gtkmet.c gtkmet.h gtkcolour.c \
	gtkcolour.h gtksplash.c gtksplash.h gtkrolls.c gtkrolls.h \
        gtktempmap.c gtktempmap.h \
	gtkbearoff.c gtkbearoff.h gtkoptions.h gtkoptions.c \
	gtktoolbar.h gtktoolbar.c gtkgamelist.c gtkpanels.c \
	gtkmovelist.c gtkmovelistctrl.c gtkmovelistctrl.h gtkwindows.c gtkwindows.h gtkrelational.c gtkrelational.h\
	$(BR1_SOURCES) $(SOUND_SOURCES) $(PYTHON_SOURCES) $(TIME_SOURCES)
gnubg_LDADD = $(COMMON_LIBS) @GTK_LIBS@ $(SOUND_LIBS) $(PNG_LIBS) \
        $(BOARD3D_LIBS) @THREAD_LIBS@ @CAIRO_LIBS@
gnubg_DEPENDENCIES = $(COMMON_DEPS) $(BOARD3D_DEPS)
else
## --- Plain
INCLUDES = $(COMMON_INCS) $(SOUND_INCS)
gnubg_SOURCES = $(COMMON_SOURCES) $(BR1_SOURCES) $(SOUND_SOURCES) \
	$(PYTHON_SOURCES) $(TIME_SOURCES)
gnubg_LDADD = $(COMMON_LIBS) $(SOUND_LIBS) $(PNG_LIBS) @THREAD_LIBS@


gnubg_DEPENDENCIES = $(COMMON_DEPS)
endif


pkgdata_DATA = gnubg_ts0.bd gnubg.wd boards.xml @GTKDATA@ \
	$(BR1DATA) $(BOARD3DDATA) gnubg.game.sql gnubg.gtkrc

man_MANS = gnubg.6

noinst_PROGRAMS = makeweights makebearoff1
check_PROGRAMS = erftest

makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
	getopt.c getopt.h getopt1.c osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h path.c path.h \
	format.c format.h mec.h mec.c
makebearoff_LDADD = $(COMMON_LIBS) @GTK_LIBS@
makebearoff_DEPENDENCIES = lib/libevent.a


makebearoff1_SOURCES = makebearoff1.c getopt.c getopt.h getopt1.c
makebearoff1_LDADD = @GTK_LIBS@ 
makebearoff1_DEPENDENCIES = 


makeweights_SOURCES = makeweights.c
makeweights_LDADD = $(COMMON_LIBS) @GTK_LIBS@
makeweights_DEPENDENCIES = lib/libevent.a

makehyper_SOURCES = makehyper.c eval.h eval.c positionid.h positionid.c \
        pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
        getopt.c getopt.h getopt1.c osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h path.c path.h \
        drawboard.c drawboard.h format.c format.h mec.h mec.c \
	$(BR1_SOURCES)

makehyper_LDADD = $(COMMON_LIBS) @GTK_LIBS@
makehyper_DEPENDENCIES = lib/libevent.a

bearoffdump_SOURCES = bearoffdump.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
        getopt.c getopt.h getopt1.c osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h path.c path.h \
	format.c format.h mec.h mec.c
bearoffdump_LDADD = $(COMMON_LIBS) @GTK_LIBS@
bearoffdump_DEPENDENCIES = lib/libevent.a


erftest_SOURCES = erftest.c

TESTS = gnubgtest erftest
distcheck-hook:
	chmod -R a+w $(distdir)
	-cp $(srcdir)/gnubg.weights $(distdir)/

EXTRA_DIST = config.rpath  userrng.c copying.awk gnubgtest gnubg.6 \
	gnubg.gtkrc credits.sh \
	external_l.c external_y.c external_y.h sgfp.c sgfp.h sgfl.c \
	external_l.l external_y.y sgf.l sgf.y \
	ABOUT-NLS boards.xml gnubg.game.sql autogen.sh gnubg.weights\
	$(BOARD3D_TEXTURE_FILES)


DISTCLEANFILES=gnubg.wd

br1.c:	makebearoff$(EXEEXT) makebearoff1$(EXEEXT) gnubg_os0.bd
	if [ ! -f br1.c ]; then \
	   ./makebearoff1 -o br1.c < gnubg_os0.bd; \
	fi

sgfp.c sgfp.h: sgf.y
	$(YACC) -o sgfp.c -p sgf -d $(srcdir)/sgf.y

sgfl.c: sgf.l
	$(LEX) -o$@ -Psgf $(srcdir)/sgf.l

#LEX must not clobber sgf.c, but make sure the sources are in place
sgf.c: sgfp.c sgfp.h sgfl.c
	@true

credits.c credits.h: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@

# Create symlinks to all the necessary parts of $(srcdir), so that gnubg
# can run happily in the build directory.
all-local:
	@for i in gnubg_os0.bd gnubg_ts0.bd gnubg.gtkrc gnubg.weights; do \
	[ -s $$i ] || if [ -s $(srcdir)/$$i ]; then $(LN_S) $(srcdir)/$$i .; \
	fi; done
	@for i in gnubg.xml annealing.png rulfig1.png rulfig2.png rulfig3.png \
	rulfig4.png rulfig5.png texinfo.dtd; do [ -s $$i ] || \
	if [ -s $(srcdir)/doc/$$i ]; then $(LN_S) $(srcdir)/doc/$$i .; fi; done

gnubg_ts0.bd:
	@echo ' ** NOTE: For full strength in bearoff moves, GNU'
	@echo ' ** Backgammon requires a "gnubg_ts0.bd" file containing a'
	@echo ' ** database of endgame positions.  See the README'
	@echo ' ** file for details.'

gnubg.weights:
	@echo ' ** NOTE: For proper operation, GNU Backgammon requires'
	@echo ' ** a "gnubg.weights" file containing neural net'
	@echo ' ** parameters.  See the README file for details.'

if CROSS_COMPILING
gnubg.wd: gnubg.weights makeweights$(EXEEXT)
	@echo ' ** NOTE: Since you are cross-compiling GNU Backgammon,'
	@echo ' ** it is not possible to generate the gnubg.wd file'
	@echo ' ** on the build system.  To create this file manually,'
	@echo ' ** use a command like:'
	@echo ' **   makeweights < gnubg.weights > gnubg.wd'
	@echo ' ** on the host system.'
else
gnubg.wd: gnubg.weights makeweights$(EXEEXT)
	-./makeweights < $< > $@
endif

if CROSS_COMPILING
gnubg_os0.bd:
	@echo ' ** NOTE: Since you are cross-compiling GNU Backgammon,'
	@echo ' ** it is not possible to generate the bearoff database'
	@echo ' ** on the build system.  To create this file manually,'
	@echo ' ** use a command like:'
	@echo ' **   makebearoff -o 6 -s 7999999 > gnubg_os0.bd'
	@echo ' ** on the host system.'

bearoff-database: makebearoff$(EXEEXT)
	@echo ' ** NOTE: Since you are cross-compiling GNU Backgammon,'
	@echo ' ** it is not possible to generate the bearoff database'
	@echo ' ** on the build system.  To create this file manually,'
	@echo ' ** use a command like:'
	@echo ' **   makebearoff -t 6x6 > gnubg_ts0.bd'
	@echo ' ** on the host system.'
else
gnubg_os0.bd: makebearoff$(EXEEXT)
	[ -s $(srcdir)/gnubg_os0.bd ] || \
	./makebearoff -o 6 -s 7999999 -f $(srcdir)/gnubg_os0.bd

bearoff-database: makebearoff$(EXEEXT)
	@echo ' ** NOTE: you can generate huge one sided and two sided'
	@echo ' ** bearoff databases for usage with gnubg'
	@echo ' ** See README for more details'
	./makebearoff -t 6x6 -f  $(srcdir)/gnubg_ts0.bd
endif

.PHONY: bearoff-database

.DELETE_ON_ERROR:
