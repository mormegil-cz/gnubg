##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##


noinst_LIBRARIES = libevent.a
libevent_a_SOURCES = buffer.c dynarray.c event.c fifo.c hash.c heap.c list.c \
	neuralnet.c mt19937ar.c isaac.c md5.c neuralnetsse.c

neuralnetsse.o: neuralnetsse.c
	$(COMPILE) $(SSE_CFLAGS) neuralnetsse.c -c -o neuralnetsse.o

noinst_HEADERS = buffer.h cache.h dynarray.h event.h fifo.h hash.h heap.h \
	list.h neuralnet.h mt19937ar.h isaac.h isaacs.h md5.h ../eval.h

noinst_TEXINFOS = event.texi

check_PROGRAMS = hashtest md5test
hashtest_SOURCES = hashtest.c hash.c

md5test_SOURCES = md5test.c md5.c md5.h

TESTS = hashtest md5test
