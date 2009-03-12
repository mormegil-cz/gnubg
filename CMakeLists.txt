project(GNUBG)

cmake_minimum_required(VERSION 2.6)

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake) # for FindReadline.cmake

Include(FindPkgConfig)
Include(CheckFunctionExists)
Include(CheckLibraryExists)
Include(CheckIncludeFile)
Include(CheckCCompilerFlag)
Include(CheckCSourceCompiles)
Include(FindGettext)

if (CMAKE_SYSTEM_NAME MATCHES Linux)
	add_definitions(-D_GNU_SOURCE)
endif (CMAKE_SYSTEM_NAME MATCHES Linux)

find_program(EXE_DOCBOOK2TEXI NAMES docbook2x-texi db2x_docbook2texi)
find_program(EXE_DOCBOOK2MAN NAMES docbook2x-man db2x_docbook2man)
find_program(EXE_DBLATEX dblatex)
find_program(EXE_XSLTPROC xsltproc)
find_program(EXE_BISON NAMES bison PATHS /usr/bin DOC "Bison executable")
find_program(EXE_FLEX NAMES flex PATHS /usr/bin DOC "Flex executable")
find_program(EXE_GDK_PIXBUF_CSOURCE NAME gdk-pixbuf-csource PATHS /usr/bin DOC "CSource executable")

check_function_exists(sqrt SQRT_IN_LIBC)
if (NOT SQRT_IN_LIBC)
	check_library_exists(m sqrt "" SQRT_IN_M)
	if (SQRT_IN_M)
		set (M_LIB m)
	endif (SQRT_IN_M)
endif (NOT SQRT_IN_LIBC)

check_library_exists(gmp __gmpz_import "" LIBGMP_FOUND)
if (LIBGMP_FOUND)
	option(HAVE_LIBGMP  "Enable support for long seeds." ON)
endif (LIBGMP_FOUND)

check_function_exists(gethostbyname GETHOSTBYNAME_IN_LIBC)
if (NOT GETHOSTBYNAME_IN_LIBC)
	check_library_exists(nsl gethostbyname "" GETHOSTBYNAME_IN_NSL)
	if (GETHOSTBYNAME_IN_NSL)
		set (NSL_LIB nsl)
	endif (GETHOSTBYNAME_IN_NSL)
endif (NOT GETHOSTBYNAME_IN_LIBC)

check_function_exists(inet_aton INET_ATON_IN_LIBC)
if (NOT_INIT_ATON_IN_LIBC)
	check_library_exists(resolv inet_aton "" INET_ATON_IN_RESOLV)
endif (NOT_INIT_ATON_IN_LIBC)

check_function_exists(connect CONNECT_IN_LIBC)
if (NOT CONNECT_IN_LIBC)
	check_library_exists(socket connect "" CONNECT_IN_SOCKET)
	if (CONNECT_IN_SOCKET)
		set (CONNECT_LIB nsl)
	endif (CONNECT_IN_SOCKET)
endif (NOT CONNECT_IN_LIBC)

if (CONNECT_IN_LIBC OR CONNECT_IN_SOCKET)
	option(HAVE_SOCKETS  "Enable support for external players." ON)
endif (CONNECT_IN_LIBC OR CONNECT_IN_SOCKET)

pkg_check_modules(GLIB REQUIRED glib-2.0>=2.6.0)

pkg_check_modules(GTHREAD gthread-2.0)
if (GTHREAD_FOUND)
	option(USE_MULTITHREAD  "Enable support for multiple threads." ON)
endif (GTHREAD_FOUND)


pkg_check_modules(LIBXML2 libxml-2.0)
if (LIBXML2_FOUND)
	option(HAVE_LIBXML2  "Enable support for xml in MET." ON)
endif (LIBXML2_FOUND)

pkg_check_modules(FREETYPE freetype2)
if (FREETYPE_FOUND)
	option(HAVE_FREETYPE  "Enable support for Freetype" ON)
endif (FREETYPE_FOUND)

pkg_check_modules(LIBPNG libpng12)
if (LIBPNG_FOUND)
	option(HAVE_LIBPNG  "Enable support for PNG" ON)
endif (LIBPNG_FOUND)

pkg_check_modules(CAIRO cairo>=1.2)
if (CAIRO_FOUND)
	option(HAVE_CAIRO  "Enable support for Cairo" ON)
endif (CAIRO_FOUND)

pkg_check_modules(PANGOCAIRO pangocairo>=1.0)
if (PANGOCAIRO_FOUND)
	option(HAVE_PANGOCAIRO  "Enable support for PangoCairo" ON)
endif (PANGOCAIRO_FOUND)

pkg_check_modules(SQLITE sqlite3)
if (SQLITE_FOUND)
	option(HAVE_SQLITE  "Enable support for sqlite" ON)
endif (SQLITE_FOUND)

pkg_check_modules(GSTREAMER gstreamer-0.10)
if (GSTREAMER_FOUND)
	option(HAVE_GSTREAMER  "Enable support for Pango Cairo" ON)
endif (GSTREAMER_FOUND)

pkg_check_modules(GTK gtk+-2.0>=2.6.0)
if (GTK_FOUND)
	option(USE_GTK  "Enable support for GTK ui" ON)
endif (GTK_FOUND)

pkg_check_modules(GTKGLEXT gtkglext-1.0>=1.0)
if (GTKGLEXT_FOUND)
	option(USE_BOARD3D  "Enable support for 3d boards" ON)
endif (GTKGLEXT_FOUND)

find_package(Readline)
IF(READLINE_FOUND)
	option(HAVE_LIBREADLINE  "Enable support for linking gnubg with libreadline." ON)
ENDIF(READLINE_FOUND)
IF(HAVE_LIBREADLINE)
	include_directories(${READLINE_INCLUDE_DIR})
	set(GNUBG_LIBS ${READLINE_LIBRARY})
ENDIF(HAVE_LIBREADLINE)

check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)

check_function_exists(sigaction HAVE_SIGACTION)
check_function_exists(sigvec HAVE_SIGVEC)
check_function_exists(strptime HAVE_STRPTIME)
check_function_exists(random HAVE_RANDOM)
check_function_exists(setpriority HAVE_SETPRIORITY)

find_package(PythonLibs)
if (PYTHONLIBS_FOUND)
	option(USE_PYTHON  "Enable support for python inside gnubg." ON)
endif (PYTHONLIBS_FOUND)

check_c_compiler_flag(-msse HAVE_SSE_FLAG)
if (HAVE_SSE_FLAG)
	set(SSE_CFLAGS -msse)
endif (HAVE_SSE_FLAG)

set(CMAKE_REQUIRED_FLAGS ${SSE_CFLAGS})
check_c_source_compiles("#include <xmmintrin.h>\nint main(){}" SSE_COMPILES)
if (SSE_COMPILES)
	option(USE_SSE_VECTORIZE  "Enable support for sse speedup on i586 supported cpus." ON)
endif (SSE_COMPILES)

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

if (EXE_BISON)
	add_custom_command( OUTPUT sgf_y.c DEPENDS sgf_y.y
	       	COMMAND ${EXE_BISON} -y --defines=sgf_y.h -osgf_y.c sgf_y.y)
	add_custom_command( OUTPUT external_y.c DEPENDS external_y.y
	       	COMMAND ${EXE_BISON} -y --defines=external_y.h -oexternal_y.c external_y.y)
	add_custom_target(RerunBison DEPENDS sgf_y.c external_y.c)
endif (EXE_BISON)

if (EXE_FLEX)
      add_custom_command(OUTPUT sgf_l.c DEPENDS sgf_l.l 
	      COMMAND ${EXE_FLEX} sgf_l.l
	      COMMAND cmake -E copy lex.yy.c sgf_l.c)
      #depend on sgf_l.c so that lex.yy.c isn't clobbered
      add_custom_command(OUTPUT external_l.c DEPENDS external_l.l sgf_l.c
	      COMMAND ${EXE_FLEX} external_l.l
	      COMMAND cmake -E copy lex.yy.c external_l.c)
      add_custom_target(RerunFlex DEPENDS sgf_l.c external_l.c)
endif (EXE_FLEX)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/lib
	${CMAKE_SOURCE_DIR}/board3d ${GLIB_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS}
	${PYTHON_INCLUDE_PATH} ${GSTREAMER_INCLUDE_DIRS})

set(DATADIR ${CMAKE_INSTALL_PREFIX}/share)
set(DOCDIR ${CMAKE_INSTALL_PREFIX}/share/doc/gnubg)
set(LOCALEDIR ${DATADIR}/locale)
set(PKGDATADIR ${DATADIR}/gnubg)
ADD_DEFINITIONS("-DDATADIR=\"${DATADIR}\"")
ADD_DEFINITIONS("-DDOCDIR=\"${DOCDIR}\"")
ADD_DEFINITIONS("-DLOCALEDIR=\"${LOCALEDIR}\"")
ADD_DEFINITIONS("-DPKGDATADIR=\"${PKGDATADIR}\"")

add_library(eval bearoff.c bearoff.h positionid.c
	positionid.h eval.c eval.h matchequity.c matchequity.h util.c util.h
	bearoffgammon.h bearoffgammon.c matchid.c matchid.h mec.c mec.h)
target_link_libraries(eval event neuralnetsse)

add_executable(gnubg analysis.c bearoffgammon.c boardpos.c 
	dbprovider.c dice.c drawboard.c eval.c export.c external.c
	file.c format.c formatgs.c gnubg.c gnubgmodule.c gnubgstock.c
	gtkboard.c gtkchequer.c gtkcube.c gtkexport.c gtkfile.c gtkgame.c
	gtkgamelist.c gtkmet.c gtkmovefilter.c gtkmovelist.c gtkmovelistctrl.c
	gtk-multiview.c gtkoptions.c gtkpanels.c gtkprefs.c gtkrace.c
	gtkrelational.c gtkrolls.c gtksplash.c gtktempmap.c gtktheory.c
	gtktoolbar.c gtkwindows.c html.c htmlimages.c import.c latex.c
	matchequity.c matchid.c mec.c multithread.c openurl.c osr.c play.c
	progress.c record.c relational.c render.c renderprefs.c
	rollout.c set.c sgf.c show.c simpleboard.c sound.c speed.c text.c
	timer.c util.c copying.c credits.c credits.h external_l.c external_y.c
	external_y.h sgf_l.c sgf_y.c sgf_y.h analysis.h backgammon.h
	boarddim.h boardpos.h common.h config.h
	credits.h dbprovider.h dice.h drawboard.h export.h external.h
	external_y.h file.h formatgs.h format.h gnubgmodule.h gnubgstock.h
	gtkboard.h gtkchequer.h gtkcube.h gtkexport.h gtkfile.h gtkgame.h
	gtkmet.h gtkmovefilter.h gtkmovelistctrl.h gtk-multiview.h
	gtkoptions.h gtkpanels.h gtkprefs.h gtkrace.h gtkrelational.h
	gtkrolls.h gtksplash.h gtktempmap.h gtktheory.h gtktoolbar.h
	gtkwindows.h import.h multithread.h
	openurl.h osr.h progress.h record.h relational.h render.h
	renderprefs.h rollout.h sgf.h sgf_y.h simpleboard.h sound.h speed.h
	)

target_link_libraries(gnubg eval board3d ${GLIB_LIBRARIES} ${GTK_LIBRARIES}
	${PYTHON_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GTKGLEXT_LIBRARIES}
	${SQLITE_LIBRARIES} gmp ${READLINE_LIBRARY})


add_executable(makeweights makeweights.c)
target_link_libraries(makeweights eval ${GLIB_LIBRARIES} )

add_executable(makebearoff makebearoff.c)

target_link_libraries(makebearoff eval ${GLIB_LIBRARIES} ${LIBXML2_LIBRARIES} )

add_executable(makehyper makehyper.c)
target_link_libraries(makehyper eval ${GLIB_LIBRARIES} ${LIBXML2_LIBRARIES} )

add_executable(bearoffdump bearoffdump.c)
target_link_libraries(bearoffdump eval ${GLIB_LIBRARIES} ${LIBXML2_LIBRARIES} )

add_custom_command(OUTPUT gnubg.wd DEPENDS makeweights gnubg.weights COMMAND
	makeweights <gnubg.weights >gnubg.wd)

add_custom_command(OUTPUT gnubg_os0.bd DEPENDS makebearoff COMMAND makebearoff
	./makebearoff -o 6 -s 7999999 -f gnubg_os0.bd)

add_custom_command(OUTPUT gnubg_ts0.bd DEPENDS makebearoff COMMAND makebearoff
	./makebearoff -t 6x6 -f gnubg_ts0.bd)

add_custom_target(RebuildEvalData DEPENDS gnubg.wd gnubg_os0.bd gnubg_ts0.bd)


install(PROGRAMS gnubg makehyper makebearoff makeweights bearoffdump
	DESTINATION bin/)

install(DIRECTORY flags DESTINATION ${PKGDATADIR} REGEX "CVS|Makefile|.cvsignore" EXCLUDE)
install(DIRECTORY fonts DESTINATION ${PKGDATADIR} REGEX "CVS|Makefile|.cvsignore" EXCLUDE)
install(DIRECTORY met DESTINATION ${PKGDATADIR} REGEX
	"CVS|Makefile|.cvsignore|TODO|ChangeLog" EXCLUDE)
install(DIRECTORY met DESTINATION ${PKGDATADIR} REGEX
	"CVS|Makefile|.cvsignore|TODO|ChangeLog" EXCLUDE)
install(FILES scripts/batch.py scripts/batch_win.py scripts/database.py
	scripts/db_import.py scripts/gnubg.py scripts/matchseries.py
	scripts/query_player.sh DESTINATION ${PKGDATADIR}/scripts)

install(FILES sounds/chequer.wav sounds/dance.wav sounds/double.wav
	sounds/drop.wav sounds/fanfare.wav sounds/gameover.wav
	sounds/haere-ra.wav sounds/matchover.wav sounds/move.wav
	sounds/resign.wav sounds/roll.wav sounds/take.wav DESTINATION
	${PKGDATADIR}/sounds)

install(FILES textures/maple.png textures/alder.png textures/Grass01.png
	textures/suede.png textures/wavesand01.png textures/hinge2.png
	textures/stain.png textures/EarthSand01.png textures/ash.png
	textures/Water01.png textures/swirl.png textures/hinge.png
	textures/logo.png textures/redwood.png textures/wood.png
	textures/oak.png textures/pine.png textures/felt.png
	textures/pine2.png textures/berch.png textures/marble.png DESTINATION
	${PKGDATADIR}/textures)

install(FILES boards.xml gnubg.gtkrc gnubg_os0.bd gnubg.sql gnubg_ts0.bd gnubg.wd textures.txt DESTINATION
	${PKGDATADIR})

add_subdirectory(doc)
add_subdirectory(lib)
add_subdirectory(po)
add_subdirectory(board3d)
add_subdirectory(pixmaps)

