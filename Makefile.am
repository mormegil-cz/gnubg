if USE_EXT
SUBDIRS = intl lib doc ext met po m4 xpm sounds
else
SUBDIRS = intl lib doc met po m4 xpm sounds
endif

localedir = $(datadir)/locale

ACLOCAL_AMFLAGS = -I m4

## sounds

if USE_SOUND
SOUND_INCS = @ESD_CFLAGS@ @NAS_CFLAGS@ @ARTSC_CFLAGS@
SOUND_SOURCES = sound.c
SOUND_LIBS = @ESD_LIBS@ @NAS_LIBS@ @ARTSC_LIBS@
else
SOUND_INCS = 
SOUND_SOURCES =
SOUND_LIBS = 
endif

COMMON_INCS = -I$(srcdir)/lib @GUILE_CFLAGS@ @LIBXML2_CFLAGS@ \
              -I$(srcdir)/intl -DLOCALEDIR=\"$(localedir)\"
COMMON_LIBS = @LIBOBJS@ -Llib -levent @GUILE_LIBS@ @INTLLIBS@ @LIBXML2_LIBS@
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c dice.c dice.h \
	rollout.c rollout.h copying.c kleinman.c matchequity.c matchequity.h \
	analysis.h analysis.c sgf.c sgf.h sgfp.c sgfp.h sgfl.c guile.c \
        guile.h import.c import.h external.c external.h credits.c latex.c \
	postscript.c html.c export.h matchid.c matchid.h i18n.c i18n.h text.c \
	export.c sound.h record.c record.h osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h br1.c
COMMON_DEPS = @LIBOBJS@ lib/libevent.a

bin_PROGRAMS = gnubg

if USE_GTK
## --- GTK
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@ $(SOUND_INCS)
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h \
	gdkgetrgb.c gdkgetrgb.h gtkprefs.c gtkprefs.h gtk-multiview.c \
	gtk-multiview.h gnu.xpm question.xpm nextgame.xpm nextmove.xpm \
	prevgame.xpm prevmove.xpm gtktheory.c gtktheory.h gtktexi.c gtktexi.h \
	gtkexport.c gtkexport.h gtkcube.c gtkcube.h gtkchequer.c gtkchequer.h \
	$(SOUND_SOURCES)
gnubg_LDADD = @GTK_LIBS@ $(COMMON_LIBS) $(SOUND_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
else
if USE_EXT
## --- EXT
INCLUDES = $(COMMON_INCS) -I$(srcdir)/ext @X_CFLAGS@  $(SOUND_INCS)
gnubg_SOURCES = $(COMMON_SOURCES) xboard.c xboard.h xgame.c xgame.h \
	$(SOUND_SOURCES)
gnubg_LDADD = -Lext -lext @X_LIBS@ @X_PRE_LIBS@ -lXmu @X_EXTRA_LIBS@ -lX11 \
	$(COMMON_LIBS) $(SOUND_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS) ext/libext.a
else
## --- Plain
INCLUDES = $(COMMON_INCS) $(SOUND_INCS)
gnubg_SOURCES = $(COMMON_SOURCES) $(SOUND_SOURCES)
gnubg_LDADD = $(COMMON_LIBS) $(SOUND_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
endif
endif


pkgdata_DATA = gnubg_ts0.bd gnubg.wd @GTKDATA@ @GUILEDATA@

man_MANS = gnubg.6

noinst_PROGRAMS = makebearoff makeweights makebearoff1
check_PROGRAMS = erftest

makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c matchequity.h matchid.h matchid.c \
	i18n.c i18n.h getopt.c getopt.h getopt1.c osr.c osr.h \
        bearoffgammon.c bearoffgammon.h bearoff.c bearoff.h
makebearoff_LDADD = -Llib -levent $(COMMON_LIBS)
makebearoff_DEPENDENCIES = lib/libevent.a


makebearoff1_SOURCES = makebearoff1.c 
makebearoff1_LDADD = 
makebearoff1_DEPENDENCIES = 


makeweights_SOURCES = makeweights.c i18n.c i18n.h
makeweights_LDADD = -Llib -levent $(COMMON_LIBS)
makeweights_DEPENDENCIES = lib/libevent.a

erftest_SOURCES = erftest.c

TESTS = gnubgtest erftest
distcheck-hook:
	-cp $(srcdir)/gnubg_ts0.bd $(srcdir)/gnubg.weights $(distdir)/

EXTRA_DIST = userrng.c copying.awk gnubgtest sgf.l sgf.y gnubg.6 gnubg.scm \
	gnubg.gtkrc credits.sh erf.c xboard.c xboard.h xgame.c xgame.h \
	ABOUT-NLS

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
	@for i in gnubg_ts0.bd gnubg.gtkrc gnubg.scm gnubg.weights; do \
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
gnubg.wd: gnubg.weights makeweights
	@echo ' ** NOTE: Since you are cross-compiling GNU Backgammon,'
	@echo ' ** it is not possible to generate the gnubg.wd file'
	@echo ' ** on the build system.  To create this file manually,'
	@echo ' ** use a command like:'
	@echo ' **   makeweights < gnubg.weights > gnubg.wd'
	@echo ' ** on the host system.'
else
gnubg.wd: gnubg.weights makeweights
	-./makeweights < $< > $@
endif

if CROSS_COMPILING
bearoff-database: makebearoff
	@echo ' ** NOTE: Since you are cross-compiling GNU Backgammon,'
	@echo ' ** it is not possible to generate the bearoff database'
	@echo ' ** on the build system.  To create this file manually,'
	@echo ' ** use a command like:'
	@echo ' **   makebearoff -t 6x6 > gnubg_ts0.bd'
	@echo ' ** on the host system.'
else
bearoff-database: makebearoff
	@echo ' ** NOTE: you can generate huge one sided and two sided'
	@echo ' ** bearoff databases for usage with gnubg'
	£echo ' ** See README for more details'
	./makebearoff -t 6x6 > $(srcdir)/gnubg_ts0.bd; $(MAKE) gnubg.bd
endif

.PHONY: bearoff-database

.DELETE_ON_ERROR:
