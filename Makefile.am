SUBDIRS = lib doc

COMMON_INCS = -I$(srcdir)/lib
COMMON_LIBS = -Llib -levent
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c mt19937int.c \
	mt19937int.h dice.c dice.h rollout.c rollout.h isaac.c isaac.h \
	isaacs.h copying.c kleinman.c
COMMON_DEPS = lib/libevent.a

bin_PROGRAMS = gnubg

if NOX
INCLUDES = $(COMMON_INCS)
gnubg_SOURCES = $(COMMON_SOURCES)
gnubg_LDADD = $(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
else
INCLUDES = $(COMMON_INCS) @GTK_CFLAGS@
gnubg_SOURCES = $(COMMON_SOURCES) gtkboard.c gtkboard.h gtkgame.c gtkgame.h
gnubg_LDADD = @GTK_LIBS@ $(COMMON_LIBS)
gnubg_DEPENDENCIES = $(COMMON_DEPS)
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

TESTS = gnubgtest
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
