if NOX
SUBDIRS = lib doc
else
SUBDIRS = lib doc ext
endif

COMMON_INCS = -I$(srcdir)/lib
COMMON_LIBS = -Llib -levent
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c mt19937int.c \
	mt19937int.h dice.c dice.h rollout.c rollout.h isaac.c isaac.h \
	isaacs.h copying.c
COMMON_DEPS = lib/libevent.a

bin_PROGRAMS = gnubg

if NOX
INCLUDES = $(COMMON_INCS)
gnubg_SOURCES = $(COMMON_SOURCES)
gnubg_LDADD = $(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
else
INCLUDES = $(COMMON_INCS) -I$(srcdir)/ext @X_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) xboard.c xboard.h xgame.c xgame.h
gnubg_LDADD = -Lext -lext @X_LIBS@ @X_PRE_LIBS@ -lX11 -lXmu @X_EXTRA_LIBS@ \
	$(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS) ext/libext.a
endif

pkgdata_DATA = gnubg.bd gnubg.wd

noinst_PROGRAMS = makebearoff makeweights

makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c
makebearoff_LDADD = -Llib -levent
makebearoff_DEPENDENCIES = lib/libevent.a

makeweights_SOURCES = makeweights.c
makeweights_LDADD = -Llib -levent
makeweights_DEPENDENCIES = lib/libevent.a

EXTRA_DIST = userrng.c copying.awk

copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $(srcdir)/COPYING > $@

gnubg.bd: makebearoff
	[ -s $@ ] || ./makebearoff > $@

gnubg.weights:
	@echo ' ** NOTE: For proper operation, GNU Backgammon requires'
	@echo ' ** a "gnubg.weights" file containing neural net'
	@echo ' ** parameters.  This file is not included in the'
	@echo ' ** distribution, but you can obtain it from the GNU'
	@echo ' ** Backgammon home page <http://www.gnu.org/software/gnubg/>'
	@echo ' ** or generate one by training a neural net yourself.'

gnubg.wd: gnubg.weights
	if [ -s $(srcdir)/gnubg.weights ]; \
	then ./makeweights < $(srcdir)/gnubg.weights > $@; fi

.DELETE_ON_ERROR:
