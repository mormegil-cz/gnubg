if USE_EXT
SUBDIRS = lib doc ext
else
SUBDIRS = lib doc
endif

COMMON_INCS = -I$(srcdir)/lib
COMMON_LIBS = -Llib -levent
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c mt19937int.c \
	mt19937int.h dice.c dice.h rollout.c rollout.h isaac.c isaac.h \
	isaacs.h copying.c kleinman.c md5.c md5.h
COMMON_DEPS = lib/libevent.a

bin_PROGRAMS = gnubg

if USE_GTK
## --- GTK
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h
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
	pub_eval.c
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

EXTRA_DIST = userrng.c copying.awk gnubgtest

copying.c: COPYING copying.awk
	$(AWK) -f $(srcdir)/copying.awk < $< > $@

gnubg.bd: makebearoff
	[ -s $@ ] || if [ -s $(srcdir)/gnubg.bd ]; then \
	$(LN_S) $(srcdir)/gnubg.bd $@; else ./makebearoff > $@; fi

weights.message:
	@echo ' ** NOTE: For proper operation, GNU Backgammon requires'
	@echo ' ** a "gnubg.weights" file containing neural net'
	@echo ' ** parameters.  This file is not included in the'
	@echo ' ** distribution, but you can obtain it from the GNU'
	@echo ' ** Backgammon home page <http://www.gnu.org/software/gnubg/>'
	@echo ' ** or generate one by training a neural net yourself.'

.PHONY: weights.message

gnubg.weights:
	[ -s $(srcdir)/gnubg.weights ] || $(MAKE) weights.message

gnubg.wd: gnubg.weights makeweights
	[ -s $@ ] || ./makeweights < $< > $@

.DELETE_ON_ERROR:
