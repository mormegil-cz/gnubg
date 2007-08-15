## $Id$

localedir = $(datadir)/locale

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib doc met po m4 xpm sounds board3d textures scripts flags fonts non-src .

bin_PROGRAMS = gnubg makebearoff makehyper bearoffdump
noinst_PROGRAMS = makeweights makebearoff1

INCLUDES = -I$(srcdir)/lib -I$(srcdir)/board3d @LIBXML2_CFLAGS@ \
	   -DLOCALEDIR=\"$(localedir)\" @FREETYPE2_CFLAGS@ @PYTHON_CSPEC@ @GLIB_CFLAGS@ \
	   @LIBPNG_CFLAGS@ @GTK_CFLAGS@ @CAIRO_CFLAGS@ @GTHREAD_CFLAGS@ @ESD_CFLAGS@ \
	   @WINMMSND_CFLAGS@ @GTKGLEXT_CFLAGS@

gnubg_SOURCES = credits.h credits.c copying.c backgammon.h gnubg.c \
		 eval.h eval.c play.c \
		 positionid.h positionid.c drawboard.h drawboard.c \
		 pub_eval.c set.c show.c dice.c dice.h rollout.c \
		 rollout.h matchequity.c matchequity.h \
		 analysis.h analysis.c sgf.c sgf.h sgfp.c sgfp.h \
		 sgfl.c import.c import.h external.c external.h \
		 latex.c postscript.c html.c export.h matchid.c \
		 matchid.h text.c export.c sound.h record.c record.h \
		 osr.c osr.h bearoffgammon.c bearoffgammon.h bearoff.c \
		 bearoff.h \
		 render.c render.h htmlimages.c speed.c \
		 renderprefs.c renderprefs.h boardpos.c boardpos.h \
		 progress.c progress.h format.c format.h formatgs.c \
		 formatgs.h boarddim.h timer.c external_y.y \
		 external_l.l openurl.c openurl.h mec.h \
		 mec.c relational.h relational.c common.h sound.c file.c file.h

if USE_MULTITHREAD
gnubg_SOURCES +=  multithread.c multithread.h
endif

if COMPILE_BR1
gnubg_SOURCES += br1.c
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
		 gtkwindows.c gtkwindows.h gtkrelational.c gtkrelational.h gtkpanels.h
endif

gnubg_LDADD = -Llib lib/libevent.la @GTK_LIBS@ @LIBPNG_LIBS@ @CAIRO_LIBS@ \
	      @GTHREAD_LIBS@ @ESD_LIBS@ \
	      @WINMMSND_LIBS@ @LTLIBOBJS@ @LTLIBINTL@ @LIBXML2_LIBS@ \
	      @FREETYPE2_LIBS@ @PYTHON_LSPEC@ @GLIB_LIBS@ @LIBPNG_LIBS@


if USE_BOARD3D
gnubg_LDADD += -L$(srcdir)/board3d board3d/libboard3d.la @GTKGLEXT_LIBS@
endif


UTILSOURCES = eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
	osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h \
	format.c format.h mec.h mec.c util.c util.h

makebearoff_SOURCES = makebearoff.c $(UTILSOURCES)
makebearoff_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ @LIBXML2_LIBS@

makehyper_SOURCES = makehyper.c $(UTILSOURCES) $(BR1_SOURCES)
if COMPILE_BR1
makehyper_SOURCES += br1.c
endif
makehyper_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ @LIBXML2_LIBS@

bearoffdump_SOURCES = bearoffdump.c $(UTILSOURCES)
bearoffdump_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ @LIBXML2_LIBS@

makebearoff1_SOURCES = makebearoff1.c
makebearoff1_LDADD = -Llib lib/libevent.la  @GLIB_LIBS@ 

makeweights_SOURCES = makeweights.c
makeweights_LDADD = -Llib lib/libevent.la @GLIB_LIBS@ 

if USE_BOARD3D
BOARD3DDATA=textures.txt
endif

if EXTERNAL_BR1
BR1DATA = gnubg_os0.bd
endif

pkgdata_DATA = gnubg_ts0.bd gnubg.wd boards.xml \
	$(BR1DATA) $(BOARD3DDATA) gnubg.game.sql gnubg.gtkrc gnubg.svg gnubg.png


distcheck-hook:
	chmod -R a+w $(distdir)
	-cp $(srcdir)/gnubg.weights $(distdir)/

EXTRA_DIST = config.rpath  userrng.c copying.awk gnubg.6 \
	gnubg.gtkrc credits.sh \
	external_l.c external_y.c external_y.h sgfp.c sgfp.h sgfl.c \
	external_l.l external_y.y sgf.l sgf.y \
	ABOUT-NLS boards.xml gnubg.game.sql autogen.sh gnubg.weights\
	textures.txt gnubg.png gnubg.svg


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
