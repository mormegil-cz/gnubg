
dnl To rebuild the `configure' script from this, execute the command
dnl     autoconf
dnl in the directory containing this script.
dnl
dnl by Gary Wong <gtw@gnu.org>, 1999, 2000, 2001.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, version 3 or later.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

AC_REVISION($Revision$)
AC_INIT([GNU Backgammon],m4_esyscmd(echo 1.00.0$VERSION_EXT | tr -d '\n'),[bug-gnubg@gnu.org],[gnubg])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR(gnubg.c)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

dnl
dnl parse default options
dnl

if test -f config.options; then
  . ./config.options
fi

dnl
dnl host specifics
dnl

win32=no

AC_MSG_CHECKING([for host])
case "$host" in
  *-*-mingw*)
    win32=yes
    CFLAGS="$CFLAGS -mms-bitfields"
	LDFLAGS="$LDFLAGS -lws2_32 -lwinmm"
    ;;
  *-*-darwin*)
    darwin=yes
    LDFLAGS="$LDFLAGS -dylib_file /System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib `/opt/local/bin/pkg-config --libs glib-2.0`"
    AC_DEFINE(USE_APPLE_OPENGL, 1, [Define if using Apple OpenGL])
    ;;
esac
AM_CONDITIONAL(WIN32, test "x$win32" = "xyes")
AC_MSG_RESULT([$host])

if test "x$win32" = "xyes"; then
AC_CHECK_TOOL(WINDRES, windres)
fi

dnl
dnl Compiler setup
dnl

AC_GNU_SOURCE
AC_PROG_CC
AM_PROG_CC_C_O
AM_CONDITIONAL(CROSS_COMPILING, test "x$cross_compiling" = "xyes")


dnl
dnl Checks for programs.
dnl

AC_PROG_AWK
AC_PROG_LN_S
AC_CHECK_PROGS(DOCBOOK2TEXI, docbook2x-texi db2x_docbook2texi, false)
AC_CHECK_PROGS(DOCBOOK2MAN, docbook2x-man db2x_docbook2man, false)
AC_CHECK_PROGS(DBLATEX, dblatex, false)
AC_CHECK_PROGS(XSLTPROC, xsltproc, false)
AC_ARG_VAR([DOCBOOK2TEXI], [path for docbook2texi from docbook2x package])
AC_ARG_VAR([DOCBOOK2MAN], [path for docbook2man from docbook2x package])
AC_ARG_VAR([DBLATEX], [path for dblatex used to build pdf documentation])
AC_ARG_VAR([XSLTPROC], [path for xsltproc used to build html documentation])

dnl
dnl Setup libtool
dnl

AC_CONFIG_MACRO_DIR([m4])
AC_DISABLE_SHARED
AC_PROG_LIBTOOL

dnl
dnl yacc and lex setup
dnl

AC_PROG_YACC
AM_PROG_LEX
AM_YFLAGS='-d'
AC_SUBST(AM_YFLAGS)


dnl
dnl check for pkg-config controlled libs
dnl

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.6.0], have_glib="yes", AC_MSG_ERROR([You need to have glib2 to compile GNU backgammon]))
PKG_CHECK_MODULES(GTHREAD, [gthread-2.0], have_gthread="yes", AC_MSG_WARN([no thread support in glib]))
PKG_CHECK_MODULES(FREETYPE2, [freetype2], have_freetype="yes", AC_MSG_WARN([no freetype support]))
PKG_CHECK_MODULES(LIBPNG, [libpng >= 1.2], have_libpng="yes", AC_MSG_WARN([no png support]))
PKG_CHECK_MODULES(CAIRO, [cairo >= 1.2], have_cairo="yes", [AC_MSG_WARN([no cairo support])])
PKG_CHECK_MODULES(PANGOCAIRO, [pangocairo >= 1.0], have_pangocairo="yes", [AC_MSG_WARN([no pangocairo support])])
PKG_CHECK_MODULES(SQLITE, [sqlite3], have_sqlite="yes", AC_MSG_WARN([no sqlite support]))

if test "x$win32" != "xyes"; then
    PKG_CHECK_MODULES(CANBERRA, [libcanberra-gtk], have_canberra="yes", AC_MSG_WARN([no libcanberra-gtk support]))
else
    have_canberra="no"
fi

PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.6.0], have_gtk="yes" , AC_MSG_WARN([no gtk support]))
PKG_CHECK_MODULES(GTKGLEXT, [gtkglext-1.0 >= 1.0], have_gtkglext="yes" , AC_MSG_WARN([no gtkglext support]))

if test "x$have_freetype" = "xyes"; then
	AC_DEFINE(HAVE_FREETYPE, 1, [Define if you have freetype2])
fi

if test "x$have_libpng" = "xyes"; then
	AC_DEFINE(HAVE_LIBPNG, 1, [Define if you have libpng])
fi

if test "x$have_cairo" = "xyes"; then
	AC_DEFINE(HAVE_CAIRO,1, [Define if you want to use libcairo])
fi

if test "x$have_pangocairo" = "xyes"; then
	AC_DEFINE(HAVE_PANGOCAIRO,1, [Define if you want to use pangocairo])
fi

AC_ARG_WITH(sqlite,[  --with-sqlite           use sqlite (Default if found)])
if test "$with_sqlite" != "no" && test "x$have_sqlite" = "xyes"; then
	AC_DEFINE(USE_SQLITE,1, [Define if you want to use sqlite])
fi
AM_CONDITIONAL(USE_SQLITE, test "$with_sqlite" != "no" && test "$have_sqlite" != "no" )

dnl If OSX version < 10.6 (Prior to Snow Leopard) then try to build
dnl with Quicktime, otherwise use CoreAudio if it is available
if test "x$darwin" = "xyes"; then
	needquicktime=no
	AS_VERSION_COMPARE("$(uname -r)", "10.0", needquicktime=yes, [], [])

	if test "x$needquicktime" = "xyes"; then
		quicktime=no
	 	AC_CHECK_HEADER([QuickTime/QuickTime.h], [quicktime=yes])

		if test "x$quicktime" = "xyes"; then
			AC_DEFINE(HAVE_APPLE_QUICKTIME,1, [Define if you have Apple QuickTime])
			LDFLAGS="$LDFLAGS -framework QuickTime"
		fi
	else
		coreaudio=no
		AC_CHECK_HEADER([AudioToolbox/AudioToolbox.h], [AC_CHECK_HEADER([CoreAudio/CoreAudioTypes.h],[coreaudio=yes])])

		if test "x$coreaudio" = "xyes"; then
			AC_DEFINE(HAVE_APPLE_COREAUDIO,1, [Define if you have Apple CoreAudio])
			LDFLAGS="$LDFLAGS -framework AudioUnit -framework CoreServices -framework AudioToolBox -framework CoreAudio"
		fi
    fi
fi

if test "x$have_canberra" = "xyes"; then
	AC_DEFINE(HAVE_CANBERRA,1, [Define if you want to use gstreamer])
fi

dnl Enable workaround on recent Ubuntus.

AC_CHECK_PROG(LSB_CHECK,lsb_release,yes)
if test x"$LSB_CHECK" = x"yes" ; then
    if test x`lsb_release -is` = "xUbuntu"; then
        AS_VERSION_COMPARE("$(lsb_release -rs)", "11.04", [], [], [no_ovsb=yes])
        if test "x$no_ovsb" = "xyes"; then
            AC_DEFINE(NO_OVERLAYSCROLLBARS, 1, [Needed for now on recent Ubuntu releases])
        fi
    fi
fi

dnl have_gthread, have_gtk and have_gtk are handled later, not having glib is fatal

dnl
dnl Checks for libraries.
dnl

AC_CHECK_LIB(m,sqrt)
AC_CHECK_LIB(gmp, __gmpz_import)
AC_SEARCH_LIBS(gethostbyname,nsl)
AC_SEARCH_LIBS(inet_aton,resolv)
if test "x$win32" = "xyes"; then
AC_DEFINE(HAVE_SOCKETS,1,Define if the system supports AF_LOCAL sockets.)
else
AC_SEARCH_LIBS(connect,socket,AC_DEFINE(HAVE_SOCKETS,1,Define if the system supports AF_LOCAL sockets.))
fi

dnl
dnl Check for readline
dnl

bcrl=n
AC_CHECK_LIB(ncurses,tparm,TERMLIB=-lncurses, AC_CHECK_LIB(termcap,tgetent,TERMLIB=-ltermcap))
LDSAVE=$LDFLAGS
LDFLAGS="$LDFLAGS $TERMLIB"
AC_CHECK_LIB(readline,readline,
             [AC_CHECK_HEADER(readline/readline.h,
              READLINELIB="-lreadline $TERMLIB";bcrl=y)],
              READLINELIB="")
if test "$bcrl" = "y" ; then
    AC_DEFINE([HAVE_LIBREADLINE],1,Define if you have libreadline)
    fi
LDFLAGS="$LDSAVE $READLINELIB"

dnl
dnl Checks for header files.
dnl

AC_CHECK_HEADERS(sys/resource.h sys/socket.h sys/time.h sys/types.h unistd.h)
AC_CHECK_HEADERS(mcheck.h)

dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl

AC_C_INLINE
AC_TYPE_SIZE_T

dnl
dnl Checks for library functions.
dnl

AC_CHECK_FUNCS(sigaction sigvec,break)
AC_CHECK_FUNCS(strptime random setpriority)
AC_CHECK_FUNCS(mtrace)

dnl 	 
dnl Checks for declarations 	 
dnl

AC_CHECK_DECLS(signbit, , , [#include <math.h>])

dnl *******************
dnl optional components
dnl *******************

AC_MSG_CHECKING([wether deprecated glib functions are enabled])
AC_ARG_ENABLE( gdeprecated, [  --enable-gdeprecated    enable deprecated glib functions], gdeprecated=$enableval)
if test "x$gdeprecated" = "xno"; then
	AC_DEFINE(G_DISABLE_DEPRECATED, 1, Define if you want to disable deprecated glib functions)
fi
AC_MSG_RESULT($gdeprecated)

AC_MSG_CHECKING([wether deprecated gtk functions are enabled])
AC_ARG_ENABLE( gtkdeprecated, [  --enable-gtkdeprecated  enable deprecated gtk functions], gtkdeprecated=$enableval)
if test "x$gtkdeprecated" = "xno"; then
	AC_DEFINE(GTK_DISABLE_DEPRECATED, 1, Define if you want to disable deprecated gtk functions)
fi
AC_MSG_RESULT($gtkprecated)

dnl
dnl Check for gtk and gtkglext
dnl

no_gtk="yes"
no_board3d="yes"
no_gtkglext="yes"
AC_ARG_WITH(gtk,[  --with-gtk              use GTK+ 2.0 (Default if found)])
AC_ARG_WITH(board3d,[  --with-board3d          compile with 3D boards (Default if found)])
if test "$with_gtk" != "no" && test "x$have_gtk" = "xyes"; then
   AC_DEFINE(USE_GTK, 1, [Define if you want to use the gtk gui])
   no_gtk="no"
fi

if test "$with_gtk" = "yes" && test "$no_gtk" = "yes"; then
	AC_MSG_ERROR([gtk requested but not found])
fi

AC_CHECK_HEADERS(GL/glx.h)

if test  "$with_board3d" != "no" && test "$with_gtk" != "no" && test "x$have_gtk" = "xyes" && test "x$have_gtkglext" = "xyes"; then
	AC_DEFINE(USE_BOARD3D,1, [Define if you want to use the 3d boards])
	no_board3d="no"
fi

if test "$with_board3d" = "yes" && test "$no_board3d" = "yes"; then
	AC_MSG_ERROR([board3d requested but gtk or gtkglext not found])
fi

AM_CONDITIONAL(USE_GTK, test "$no_gtk" != "yes" )
AM_CONDITIONAL(USE_BOARD3D, test "$no_board3d" != "yes" )

if test "$no_gtk" = "no"; then
  AC_PATH_PROG(GDK_PIXBUF_CSOURCE, gdk-pixbuf-csource, no)
  if test "x$GDK_PIXBUF_CSOURCE" = "xno"; then
    AC_MSG_WARN([Could not find gdk-pixbuf-csource in your PATH, cannot rebuild pixmaps])
  fi
fi
AM_CONDITIONAL(FOUND_GDK_PIXBUF_CSOURCE, test "x$GDK_PIXBUF_CSOURCE" != "xno" )

dnl
dnl Check for Python
dnl
have_python="no"
AC_ARG_WITH(python,[  --with-python           use python (Default if found)])
AM_PATH_PYTHON(2.2)
AM_CHECK_PYTHON_HEADERS(have_python="yes", have_python="no")
if test "$with_python" != "no" && test "x$have_python" = "xyes"; then
        AC_DEFINE(USE_PYTHON,1,[Define if you want to use Python inside gnubg])
fi
AM_CONDITIONAL(USE_PYTHON, test "$with_python" != "no" && test "$have_python" != "no" )

dnl
dnl Should we enable Extended MatchID test code
dnl

AC_MSG_CHECKING([for extended match ids])
AC_ARG_ENABLE( extmatchid, [  --disable-extmatchid    disable extended MatchIDs ])
AS_IF([test "x$enable_extmatchid" != "xno"], [
        AC_DEFINE(USE_EXTENDEDMATCHID,1,Define if you want extended MatchID support)
])
AS_IF( [test "x$enable_extmatchid" != "xno"], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)] )

dnl
dnl SIMD
dnl

AX_EXT()
AC_MSG_CHECKING([for SIMD CPU instructions])
AC_ARG_ENABLE( simd, [  --enable-simd=TYPE      enable SIMD usage for newer cpus(TYPE=yes,avx,sse2,no)], simdcpu=$enableval, simdcpu="undef")
if test "x$simdcpu" = "xundef" || test "x$simdcpu" = "xyes"; then
    if test "x$ax_cv_have_sse2_ext" = "xyes"; then
        simdcpu="sse2"
    elif test "x$ax_cv_have_sse_ext" = "xyes"; then 
        simdcpu="sse"
    elif test "x$ax_cv_have_avx_ext" = "xyes"; then 
        simdcpu="avx"
    else
        simdcpu="no"
    fi
fi
if test "x$simdcpu" != "xno"; then
	AC_DEFINE(USE_SIMD_INSTRUCTIONS,1,Define if you want to compile with SIMD support)
	if test "x$simdcpu" = "xavx"; then
		AC_DEFINE(USE_AVX, 1, Define if you want to compile with AVX support)
	fi
	if test "x$simdcpu" = "xsse2"; then
		AC_DEFINE(USE_SSE2, 1, Define if you want to compile with SSE2 support)
	fi
	if test x"$GCC" = "xyes"; then
		if test "x$SIMD_CFLAGS" = x; then
			if test "x$simdcpu" = "xavx"; then
				SIMD_CFLAGS="-mavx"
			elif test "x$simdcpu" = "xsse2"; then 
				SIMD_CFLAGS="-msse -msse2"
			else
				SIMD_CFLAGS="-msse"
			fi
		fi
	fi
fi

AC_MSG_RESULT([$host (sse=$simdcpu, SIMD_CFLAGS="$SIMD_CFLAGS")])
AC_ARG_VAR(SIMD_CFLAGS, [CFLAGS needed for compiling in SIMD CPU support])

AC_MSG_CHECKING([for SIMD supported CPU test])
AC_ARG_ENABLE( cputest, [  --disable-cputest       disable runtime SIMD CPU test (Default no) ], cputest=$enableval, cputest="yes")
if test "x$simdcpu" = "xno"; then
	cputest="no"
elif test x"$GCC" = "xno"; then
    AC_MSG_WARN([CPU test disabled, GNUC compatible compiler not being used])
	cputest="no"
fi

AS_IF([test "x$cputest" = "xno"], [
        AC_DEFINE(DISABLE_SIMD_TEST, 1, Define if you want to disable the SIMD CPU instruction test)
])
AS_IF( [test "x$cputest" != "xno"], [AC_MSG_RESULT($cputest)], [AC_MSG_RESULT(no)] )


dnl
dnl Threads
dnl

AC_MSG_CHECKING([for threading])
AC_ARG_ENABLE( threads, [  --enable-threads=TYPE   enable multithread support (TYPE=yes,glib,win32,no) (Default glib)], threads=$enableval, threads="glib")
AM_CONDITIONAL(USE_MULTITHREAD, test "x$enable_threads" != "xno" )
if test "x$enable_threads" == "xyes"; then
	threads=glib
fi
if test "x$enable_threads" != "xno"; then
        if test "x$have_gthread" != "xyes" && test "x$win32" != "xno" ; then
		threads=win32
	fi
        if test "x$have_gthread" != "xyes" && test "x$win32" != "xyes"; then
                AC_MSG_ERROR([Threads enabled but glib doesn't seem to have gthread support])
        fi
        if test "x$enable_threads" == "xwin32" && test "x$win32" != "xyes" ; then
                AC_MSG_ERROR([Win32 Threads only available on Win32 platforms])
	fi
	
        AC_DEFINE(USE_MULTITHREAD, 1, Define if you want to have multithread support)
	case "$threads" in
		glib )
	        AC_DEFINE(GLIB_THREADS, 1, Define if you wish to use glib threads)
		;;
		win32 )
		;;
		*)
                AC_MSG_ERROR([Thread type $threads unsupported])
		;;
	esac
fi
AS_IF( [test "x$enable_threads" != "xno"], [AC_MSG_RESULT($threads)], [AC_MSG_RESULT(no)] )

dnl 
dnl Maximum number of threads
dnl 

AC_ARG_WITH([eval_max_threads],
AC_HELP_STRING([--with-eval-max-threads=size],
		[define the maximum number of evaluation threads allowed: (default=48)]),
[
	EVAL_THREAD_COUNT=$with_eval_max_threads
	AC_DEFINE_UNQUOTED(MAX_NUMTHREADS, $EVAL_THREAD_COUNT, [maximum number of evaluation threads])
])


dnl *************
dnl Misc. setings
dnl *************

dnl internationalization macros
dnl
AC_SUBST(GETTEXT_PACKAGE, gnubg)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The package name, for gettext])
ALL_LINGUAS="da de es en_US ja it fr is tr cs ro ru"
AM_GLIB_GNU_GETTEXT


dnl
dnl config files
dnl

AC_CONFIG_FILES([Makefile doc/Makefile lib/Makefile met/Makefile po/Makefile.in po/Makefile m4/Makefile sounds/Makefile board3d/Makefile textures/Makefile scripts/Makefile flags/Makefile fonts/Makefile non-src/Makefile pixmaps/Makefile win32/Makefile])
dnl
dnl
dnl

AC_OUTPUT
