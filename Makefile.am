if USE_EXT
SUBDIRS = lib doc ext
else
SUBDIRS = lib doc
endif

COMMON_INCS = -I$(srcdir)/lib @GUILE_CFLAGS@
COMMON_LIBS = -Llib -levent @GUILE_LDFLAGS@
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c mt19937int.c \
	mt19937int.h dice.c dice.h rollout.c rollout.h isaac.c isaac.h \
	isaacs.h copying.c kleinman.c md5.c md5.h matchequity.c matchequity.h \
	analysis.h analysis.c sgf.c sgf.h sgfp.c sgfp.h sgfl.c
COMMON_DEPS = lib/libevent.a

bin_PROGRAMS = gnubg

if USE_GTK
## --- GTK
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h \
	gnu.xpm question.xpm
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

pkgdata_DATA = gnubg.bd gnubg.wd

noinst_PROGRAMS = makebearoff makeweights

makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c matchequity.c
makebearoff_LDADD = -Llib -levent
makebearoff_DEPENDENCIES = lib/libevent.a

makeweights_SOURCES = makeweights.c
makeweights_LDADD = -Llib -levent
makeweights_DEPENDENCIES = lib/libevent.a

check_PROGRAMS = md5test

md5test_SOURCES = md5test.c md5.c md5.h

TESTS = gnubgtest md5test
distcheck-hook:
	-cp $(srcdir)/gnubg.bd $(srcdir)/gnubg.weights $(distdir)/

EXTRA_DIST = userrng.c copying.awk gnubgtest sgf.l sgf.y

sgfp.h sgfp.c: sgf.y
	$(BISON) -o sgfp.c -p sgf -d sgf.y

sgfl.c: sgf.l
	$(FLEX) -osgfl.c -Psgf sgf.l

copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $< > $@

gnubg.bd:
	@[ -s $@ ] || if [ -s $(srcdir)/gnubg.bd ]; then \
	$(LN_S) $(srcdir)/gnubg.bd $@; else $(MAKE) bd.message; fi

bd.message:
	@echo ' ** NOTE: For full strength in bearoff moves, GNU'
	@echo ' ** Backgammon requires a "gnubg.bd" file containing a'
	@echo ' ** database of endgame positions.  See the README'
	@echo ' ** file for details.'

bearoff-database: makebearoff
	./makebearoff > $(srcdir)/gnubg.bd; $(MAKE) gnubg.bd

.PHONY: bd.message bearoff-database

weights.message:
	@echo ' ** NOTE: For proper operation, GNU Backgammon requires'
	@echo ' ** a "gnubg.weights" file containing neural net'
	@echo ' ** parameters.  See the README file for details.'

.PHONY: weights.message

gnubg.weights:
	@[ -s $(srcdir)/gnubg.weights ] || $(MAKE) weights.message

gnubg.wd: gnubg.weights makeweights
	./makeweights < $< > $@

.DELETE_ON_ERROR:
