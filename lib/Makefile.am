##
## All targets have been declared "noinst" for inclusion with gnubg.  As
## a standalone library, it would be better for them to be installed.
##


noinst_LTLIBRARIES = libevent.la
libevent_la_SOURCES = buffer.c dynarray.c event.c fifo.c hash.c heap.c list.c \
	neuralnet.c mt19937ar.c isaac.c md5.c neuralnetsse.c sse.h mm_malloc.h

#neuralnetsse_o_CFLAGS = $(SSE_CFLAGS)
neuralnetsse.o: neuralnetsse.c sse.h mm_malloc.h
	if $(COMPILE) $(SSE_CFLAGS) -MT $@ -MD -MP -MF "$(DEPDIR)/$*.Tpo" -c -o $@ $<; \
	then mv -f "$(DEPDIR)/$*.Tpo" "$(DEPDIR)/$*.Po"; else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; fi

noinst_HEADERS = buffer.h cache.h dynarray.h event.h fifo.h hash.h heap.h \
	list.h neuralnet.h mt19937ar.h isaac.h isaacs.h md5.h ../eval.h sse.h mm_malloc.h

noinst_TEXINFOS = event.texi

check_PROGRAMS = hashtest md5test
hashtest_SOURCES = hashtest.c
hashtest_LDADD = libevent.la

md5test_SOURCES = md5test.c
md5test_LDADD = libevent.la

TESTS = hashtest md5test
