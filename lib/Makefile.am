##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##

INCLUDES = @GLIB_CFLAGS@
LIBADD = @GLIB_LIBS@

noinst_LTLIBRARIES = libevent.la libneuralnetsse.la

libneuralnetsse_la_SOURCES = neuralnetsse.c
libneuralnetsse_la_CFLAGS = $(SSE_CFLAGS)

libevent_la_SOURCES = dynarray.c event.c hash.c heap.c list.c \
	neuralnet.c mt19937ar.c isaac.c md5.c sse.h mm_malloc.h cache.c
libevent_la_LIBADD = libneuralnetsse.la


noinst_HEADERS = cache.h dynarray.h event.h hash.h heap.h \
	list.h neuralnet.h mt19937ar.h isaac.h isaacs.h md5.h ../eval.h sse.h mm_malloc.h

noinst_TEXINFOS = event.texi

check_PROGRAMS = hashtest md5test
hashtest_SOURCES = hashtest.c
hashtest_LDADD = libevent.la

md5test_SOURCES = md5test.c
md5test_LDADD = libevent.la

TESTS = hashtest md5test
