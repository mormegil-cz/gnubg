include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/board3d
 	${CMAKE_SOURCE_DIR}/lib ${GLIB_INCLUDE_DIRS} ${GTKGLEXT_INCLUDE_DIRS})

ADD_DEFINITIONS("-DPKGDATADIR=\"${PKGDATADIR}\"")

set(board3d_STAT_SRCS drawboard3d.c font3d.c fun3d.h graph.c gtkcolour3d.c
	inc3d.h matrix.c matrix.h misc3d.c misc3d.h model.c model.h shadow.c
	tr.c tr.h types3d.h widget3d.c)
add_library(board3d STATIC ${board3d_STAT_SRCS})
