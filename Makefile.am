## $Id$
if USE_EXT
EXT_DIR=ext
else
EXT_DIR=
endif

localedir = $(datadir)/locale

ACLOCAL_AMFLAGS = -I m4

## libgen

if  HAVE_LIBGEN
LIBGEN_SOURCES = 
else
LIBGEN_SOURCES = simplelibgen.c simplelibgen.h
endif

## sounds

if  USE_SOUND 
SOUND_SOURCES = sound.c
SOUND_INCS = @ESD_CFLAGS@ @NAS_CFLAGS@ @ARTSC_CFLAGS@ @AUDIOFILE_CFLAGS@ \
	@WINMMSND_CFLAGS@
SOUND_LIBS = @ESD_LIBS@ @NAS_LIBS@ @ARTSC_LIBS@ @AUDIOFILE_LIBS@ \
	@WINMMSND_LIBS@
else
SOUND_INCS = 
SOUND_SOURCES =
SOUND_LIBS = 
endif

## timecontrol
if USE_TIMECONTROL
TIME_SOURCES = timecontrol.h timecontrol.c
else
TIME_SOURCES =
endif

if COMPILE_BR1
BR1_SOURCES = br1.c
else
BR1_SOURCES =
endif

if USE_FREETYPE
FONT_SOURCES = luxi.c
else
FONT_SOURCES =
endif

if EXTERNAL_BR1
BR1DATA = gnubg_os0.bd
else
BR1DATA =
endif

if USE_BOARD3D
BOARD3D_DIR=board3d textures
BOARD3D_DEPS=board3d/libboard3d.a
BOARD3D_LIBS=-L$(srcdir)/board3d -lboard3d @GTKGLEXT_LIBS@ @FTGL_LIBS@
BOARD3D_INCS=@GTKGLEXT_CFLAGS@
BOARD3DDATA=textures.txt 
else
BOARD3D_DIR=
BOARD3D_DEPS=
BOARD3D_LIBS=
BOARD3D_INCS=
BOARD3DDATA=
endif


SUBDIRS = intl lib doc met po m4 xpm sounds $(BOARD3D_DIR) $(EXT_DIR)

if USE_PYTHON
PYTHON_SOURCES=gnubgmodule.c gnubgmodule.h
else
PYTHON_SOURCES=
endif

COMMON_INCS = -I$(srcdir)/lib @GUILE_CFLAGS@ @LIBXML2_CFLAGS@ \
              -I$(srcdir)/intl -DLOCALEDIR=\"$(localedir)\" \
              @FT2_CFLAGS@ @GTKEXTRA_CFLAGS@ @PYTHON_CFLAGS@ 
COMMON_LIBS = @LIBOBJS@ -Llib -levent @GUILE_LIBS@ @INTLLIBS@ @LIBXML2_LIBS@ \
              @FT2_LIBS@ @GTKEXTRA_LIBS@ @PYTHON_LIBS@ 
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c dice.c dice.h \
	rollout.c rollout.h copying.c kleinman.c matchequity.c matchequity.h \
	analysis.h analysis.c sgf.c sgf.h sgfp.c sgfp.h sgfl.c guile.c \
        guile.h import.c import.h external.c external.h credits.c latex.c \
	postscript.c html.c export.h matchid.c matchid.h i18n.c i18n.h text.c \
	export.c sound.h record.c record.h osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h \
	onechequer.c onechequer.h path.c path.h render.c render.h \
	htmlimages.c speed.c renderprefs.c renderprefs.h \
	boardpos.c boardpos.h progress.c progress.h format.c format.h \
	formatgs.c formatgs.h boarddim.h $(LIBGEN_SOURCES)
COMMON_DEPS = @LIBOBJS@ lib/libevent.a

bin_PROGRAMS = gnubg makebearoff makehyper

gnubg_LDFLAGS= @PYTHON_LDFLAGS@

if USE_GTK
## --- GTK
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS) @LIBART_CFLAGS@ \
           $(BOARD3D_INCS)
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h \
	gtkprefs.c gtkprefs.h gtk-multiview.c \
	gtk-multiview.h gnu.xpm question.xpm nextgame.xpm nextmove.xpm \
	prevgame.xpm prevmove.xpm gtktheory.c gtktheory.h gtktexi.c gtktexi.h \
	gtkexport.c gtkexport.h gtkcube.c gtkcube.h gtkchequer.c gtkchequer.h \
	gtkrace.c gtkrace.h nextmarked.xpm prevmarked.xpm \
	gtkmovefilter.c gtkmovefilter.h gtkmet.c gtkmet.h gtkcolour.c \
	gtkcolour.h gtksplash.c gtksplash.h gtkrolls.c gtkrolls.h \
        openurl.c openurl.h gtktempmap.c gtktempmap.h \
	gtkbearoff.c gtkbearoff.h gtkoptions.h gtkoptions.c \
	gtkpath.c gtkpath.h gtktoolbar.h gtktoolbar.c \
	$(BR1_SOURCES) $(SOUND_SOURCES) $(FONT_SOURCES) $(PYTHON_SOURCES) $(TIME_SOURCES)
gnubg_LDADD = @GTK_LIBS@ $(COMMON_LIBS) $(SOUND_LIBS) $(PNG_LIBS) \
        @LIBART_LIBS@ $(BOARD3D_LIBS) @THREAD_LIBS@
gnubg_DEPENDENCIES = $(COMMON_DEPS) $(BOARD3D_DEPS)
else
if USE_EXT
## --- EXT
INCLUDES = $(COMMON_INCS) -I$(srcdir)/ext @X_CFLAGS@  $(SOUND_INCS)
gnubg_SOURCES = $(COMMON_SOURCES) xboard.c xboard.h xgame.c xgame.h \
	$(BR1_SOURCES) $(SOUND_SOURCES) $(FONT_SOURCES) $(PYTHON_SOURCES) $(TIME_SOURCES)
gnubg_LDADD = -Lext -lext @X_LIBS@ @X_PRE_LIBS@ -lXmu @X_EXTRA_LIBS@ -lX11 \
	$(COMMON_LIBS) $(SOUND_LIBS) @THREAD_LIBS@
gnubg_DEPENDENCIES = $(COMMON_DEPS) ext/libext.a
else
## --- Plain
INCLUDES = $(COMMON_INCS) $(SOUND_INCS) $(PNG_LIBS)
gnubg_SOURCES = $(COMMON_SOURCES) $(BR1_SOURCES) $(SOUND_SOURCES) \
	$(FONT_SOURCES)  $(PYTHON_SOURCES) $(TIME_SOURCES)
gnubg_LDADD = $(COMMON_LIBS) $(SOUND_LIBS) $(PNG_LIBS) @THREAD_LIBS@


gnubg_DEPENDENCIES = $(COMMON_DEPS)
endif
endif


pkgdata_DATA = gnubg_ts0.bd gnubg.wd boards.xml @GTKDATA@ @GUILEDATA@ \
	$(BR1DATA) $(BOARD3DDATA)

man_MANS = gnubg.6

noinst_PROGRAMS = makeweights makebearoff1
check_PROGRAMS = erftest

makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
	i18n.c i18n.h getopt.c getopt.h getopt1.c osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h path.c path.h \
	format.c format.h $(LIBGEN_SOURCES) 
makebearoff_LDADD = -Llib -levent $(COMMON_LIBS) @GTK_LIBS@
makebearoff_DEPENDENCIES = lib/libevent.a


makebearoff1_SOURCES = makebearoff1.c getopt.c getopt.h getopt1.c \
	$(LIBGEN_SOURCES)
makebearoff1_LDADD = @GTK_LIBS@ 
makebearoff1_DEPENDENCIES = 


makeweights_SOURCES = makeweights.c i18n.c i18n.h $(LIBGEN_SOURCES)
makeweights_LDADD = -Llib -levent $(COMMON_LIBS) @GTK_LIBS@
makeweights_DEPENDENCIES = lib/libevent.a

makehyper_SOURCES = makehyper.c eval.h eval.c positionid.h positionid.c \
        pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
        i18n.c i18n.h getopt.c getopt.h getopt1.c osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h path.c path.h \
        drawboard.c drawboard.h format.c format.h $(LIBGEN_SOURCES)

makehyper_LDADD = -Llib -levent $(COMMON_LIBS) @GTK_LIBS@
makehyper_DEPENDENCIES = lib/libevent.a

erftest_SOURCES = erftest.c

TESTS = gnubgtest erftest
distcheck-hook:
	chmod -R a+w $(distdir)
	-cp $(srcdir)/gnubg.weights $(distdir)/

EXTRA_DIST = userrng.c copying.awk gnubgtest sgf.l sgf.y gnubg.6 gnubg.scm \
	gnubg.gtkrc credits.sh erf.c xboard.c xboard.h xgame.c xgame.h \
	ABOUT-NLS boards.xml gnubg.py

DISTCLEANFILES=gnubg.wd

br1.c:	makebearoff makebearoff1
	if [ ! -f br1.c ]; then \
	   ./makebearoff -o 6 -s 7999999 | ./makebearoff1 -o br1.c; \
	fi

sgfp.c sgfp.h: sgf.y
	$(BISON) -o sgfp.c -p sgf -d $(srcdir)/sgf.y

sgfl.c: sgf.l
	$(FLEX) -o$@ -Psgf $(srcdir)/sgf.l

credits.c AUTHORS: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@

# Create symlinks to all the necessary parts of $(srcdir), so that gnubg
# can run happily in the build directory.
all-local:
	@for i in gnubg_os0.bd gnubg_ts0.bd gnubg.gtkrc gnubg.scm \
	gnubg.weights; do \
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
