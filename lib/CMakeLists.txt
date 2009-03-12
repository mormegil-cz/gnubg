########### next target ###############

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/lib ${GLIB_INCLUDE_DIRS})

set(event_STAT_SRCS list.c neuralnet.c mt19937ar.c isaac.c md5.c sse.h
	mm_malloc.h cache.c cache.h list.h neuralnet.h mt19937ar.h isaac.h
	isaacs.h md5.h sse.h mm_malloc.h ../eval.h gnubg-types.h sigmoid.h)

add_library(event STATIC ${event_STAT_SRCS})


########### next target ###############

set(neuralnetsse_STAT_SRCS neuralnetsse.c ../common.h sse.h neuralnet.h sigmoid.h)
add_library(neuralnetsse STATIC ${neuralnetsse_STAT_SRCS})

set_target_properties(neuralnetsse PROPERTIES COMPILE_FLAGS ${SSE_CFLAGS})
