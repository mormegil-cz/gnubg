##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##

noinst_LIBRARIES = libevent.a
libevent_a_SOURCES = buffer.c dynarray.c event.c fifo.c hash.c heap.c list.c \
	neuralnet.c
libevent_a_LIBADD = @LIBOBJS@
noinst_HEADERS = buffer.h cache.h dynarray.h event.h fifo.h hash.h heap.h \
	list.h neuralnet.h
noinst_TEXINFOS = event.texi

EXTRA_DIST = rand_r.c rand_r.h

EXTRA_PROGRAMS = hashtest
hashtest_SOURCES = hashtest.c
hashtest_LDADD = -L. -levent

TESTS = hashtest
