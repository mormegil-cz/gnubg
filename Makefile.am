if USE_EXT
SUBDIRS = lib doc ext met
else
SUBDIRS = lib doc met
endif

COMMON_INCS = -I$(srcdir)/lib @GUILE_CFLAGS@ @LIBXML2_CFLAGS@
COMMON_LIBS = @LIBOBJS@ -Llib -levent @GUILE_LIBS@ @LIBXML2_LIBS@
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c dice.c dice.h \
	rollout.c rollout.h copying.c kleinman.c matchequity.c matchequity.h \
	analysis.h analysis.c sgf.c sgf.h sgfp.c sgfp.h sgfl.c guile.c \
        guile.h import.c import.h external.c external.h credits.c latex.c \
	postscript.c html.c export.h
COMMON_DEPS = @LIBOBJS@ lib/libevent.a

bin_PROGRAMS = gnubg

if USE_GTK
## --- GTK
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h \
	gdkgetrgb.c gdkgetrgb.h gtkprefs.c gtkprefs.h gtk-multiview.c \
	gtk-multiview.h gnu.xpm question.xpm nextgame.xpm nextmove.xpm \
	prevgame.xpm prevmove.xpm gtktheory.c gtktheory.h gtktexi.c gtktexi.h
gnubg_LDADD = @GTK_LIBS@ $(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
else
if USE_EXT
## --- EXT
INCLUDES = $(COMMON_INCS) -I$(srcdir)/ext @X_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) xboard.c xboard.h xgame.c xgame.h
gnubg_LDADD = -Lext -lext @X_LIBS@ @X_PRE_LIBS@ -lXmu @X_EXTRA_LIBS@ -lX11 \
	$(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS) ext/libext.a
else
## --- Plain
INCLUDES = $(COMMON_INCS)
gnubg_SOURCES = $(COMMON_SOURCES)
gnubg_LDADD = $(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
endif
endif

pkgdata_DATA = gnubg.bd gnubg.wd @GTKDATA@ @GUILEDATA@

man_MANS = gnubg.6

noinst_PROGRAMS = makebearoff makeweights
check_PROGRAMS = erftest

makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c
makebearoff_LDADD = -Llib -levent $(COMMON_LIBS)
makebearoff_DEPENDENCIES = lib/libevent.a

makeweights_SOURCES = makeweights.c
makeweights_LDADD = -Llib -levent
makeweights_DEPENDENCIES = lib/libevent.a

erftest_SOURCES = erftest.c

TESTS = gnubgtest erftest
distcheck-hook:
	-cp $(srcdir)/gnubg.bd $(srcdir)/gnubg.weights $(distdir)/

EXTRA_DIST = userrng.c copying.awk gnubgtest sgf.l sgf.y gnubg.6 gnubg.scm \
	gnubg.gtkrc credits.sh erf.c xboard.c xboard.h xgame.c xgame.h

sgfp.h sgfp.c: sgf.y
	$(BISON) -o $@ -p sgf -d $(srcdir)/sgf.y

sgfl.c: sgf.l
	$(FLEX) -o$@ -Psgf $(srcdir)/sgf.l

credits.c AUTHORS: credits.sh
	cd $(srcdir) && $(SHELL) ./credits.sh

copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@

# Create symlinks to all the necessary parts of $(srcdir), so that gnubg
# can run happily in the build directory.
all-local:
	@for i in gnubg.bd gnubg.gtkrc gnubg.scm gnubg.weights; do \
	[ -s $$i ] || if [ -s $(srcdir)/$$i ]; then $(LN_S) $(srcdir)/$$i .; \
	fi; done
	@for i in gnubg.xml annealing.png rulfig1.png rulfig2.png rulfig3.png \
	rulfig4.png rulfig5.png texinfo.dtd; do [ -s $$i ] || \
	if [ -s $(srcdir)/doc/$$i ]; then $(LN_S) $(srcdir)/doc/$$i .; fi; done

gnubg.bd:
	@echo ' ** NOTE: For full strength in bearoff moves, GNU'
	@echo ' ** Backgammon requires a "gnubg.bd" file containing a'
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
	@echo ' **   makebearoff > gnubg.bd'
	@echo ' ** on the host system.'
else
bearoff-database: makebearoff
	./makebearoff > $(srcdir)/gnubg.bd; $(MAKE) gnubg.bd
endif

.PHONY: bearoff-database

.DELETE_ON_ERROR:
