if NOX
SUBDIRS = lib
else
SUBDIRS = lib ext
endif

COMMON_INCS = -I$(srcdir)/lib
COMMON_LIBS = -Llib -levent
COMMON_SOURCES = backgammon.h gnubg.c getopt.h getopt.c getopt1.c eval.h \
	eval.c play.c positionid.h positionid.c drawboard.h drawboard.c \
	pub_eval.c set.c show.c database.h database.c mt19937int.c \
	mt19937int.h dice.c dice.h rollout.c rollout.h isaac.c isaac.h \
	isaacs.h
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

pkgdata_DATA = gnubg.bd gnubg.weights

noinst_PROGRAMS = makebearoff
makebearoff_SOURCES = makebearoff.c eval.h eval.c positionid.h positionid.c \
	pub_eval.c
makebearoff_LDADD = -Llib -levent
makebearoff_DEPENDENCIES = lib/libevent.a

info_TEXINFOS = gnubg.texi

gnubg.bd: makebearoff
	[ -s gnubg.bd ] || ./makebearoff > gnubg.bd

gnubg.weights:
	@echo 'NOTE: For proper operation, GNU Backgammon requires'
	@echo 'a "gnubg.weights" file containing neural net'
	@echo 'parameters.  This file is not included in the'
	@echo 'distribution, but you can obtain it from the GNU'
	@echo 'Backgammon home page <http://gnubg.sourceforge.net/>'
	@echo 'or generate one by training a neural net yourself.'
