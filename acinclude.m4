dnl @synopsis AC_DEFINE_DIR(VARNAME, DIR [, DESCRIPTION])
dnl
dnl This macro defines (with AC_DEFINE) VARNAME to the expansion of the DIR
dnl variable, taking care of fixing up ${prefix} and such.
dnl
dnl Note that the 3 argument form is only supported with autoconf 2.13 and
dnl later (i.e. only where AC_DEFINE supports 3 arguments).
dnl
dnl Examples:
dnl
dnl    AC_DEFINE_DIR(DATADIR, datadir)
dnl    AC_DEFINE_DIR(PROG_PATH, bindir, [Location of installed binaries])
dnl
dnl @version $ac_define_dir.m4,v 1.2 1999/09/27 15:10:44 simons Exp$
dnl @author Alexandre Oliva <oliva@lsd.ic.unicamp.br>

AC_DEFUN(AC_DEFINE_DIR, [
	ac_expanded=`(
	    test "x$prefix" = xNONE && prefix="$ac_default_prefix"
	    test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
	    eval echo \""[$]$2"\"
        )`
	ifelse($3, ,
	  AC_DEFINE_UNQUOTED($1, "$ac_expanded"),
	  AC_DEFINE_UNQUOTED($1, "$ac_expanded", $3))
])

dnl @synopsis AM_GUILE(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
dnl Figure out how to use Guile (but unlike the guile.m4 macro, don't
dnl abort if Guile isn't there at all).
dnl
dnl Looks for Guile 1.6 or newer (since that's what GNU Backgammon needs).
dnl
dnl @author Gary Wong <gtw@gnu.org>

AC_DEFUN(AM_GUILE,[
  AC_ARG_WITH(guile-prefix,[  --with-guile-prefix=PREFIX Prefix where Guile is installed (optional)], guile_config_prefix="$withval", guile_config_prefix="")
  if test "$guile_config_prefix"; then
    GUILE_CONFIG=$guile_config_prefix/bin/guile-config
  fi
  AC_PATH_PROG(GUILE_CONFIG, guile-config, no)
  if test "$GUILE_CONFIG" = "no"; then
    no_guile=yes
    ifelse([$2], , :, [$2])
  else
    AC_MSG_CHECKING(for Guile version 1.6 or newer)
    guile_major_version=`$GUILE_CONFIG --version 2>&1 | \
	sed -n 's/.* \([[0-9]]\+\)[[^ ]]*$/\1/p'`
    guile_minor_version=`$GUILE_CONFIG --version 2>&1 | \
	sed -n 's/.* [[0-9]]\+.\([[0-9]]\+\)[[^ ]]*$/\1/p'`
    if test `expr "${guile_major_version:-0}" \* 1000 + \
	"${guile_minor_version:-0}"` -ge 1006; then
      GUILE_CFLAGS="`$GUILE_CONFIG compile`"
      GUILE_LIBS="`$GUILE_CONFIG link`"
      AC_MSG_RESULT(yes)
      ifelse([$1], , :, [$1])
    else
      no_guile=yes
      AC_MSG_RESULT(no)
      ifelse([$2], , :, [$2])
    fi
  fi
  AC_SUBST(GUILE_CFLAGS)
  AC_SUBST(GUILE_LIBS)
])

dnl @synopsis AM_GTK2
dnl
dnl Figure out how to use GTK+ 2.0 (but unlike the gtk-2.0.m4
dnl macro, cope gracefully if it's not there at all).
dnl
dnl @author Gary Wong <gtw@gnu.org>

AC_DEFUN(AM_GTK2,[
  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  no_gtk=""
  if test "$PKG_CONFIG" = "no"; then
    no_gtk=yes
  else
    AC_MSG_CHECKING(for GTK+ version 2 or newer)
    if $PKG_CONFIG --atleast-version=2.0.0 gtk+-2.0; then
      GTK_CFLAGS=`$PKG_CONFIG --cflags gtk+-2.0`
      GTK_LIBS=`$PKG_CONFIG --libs gtk+-2.0`
      gtk2=yes
    else
      no_gtk=yes
    fi
  fi
  if test "$no_gtk" = ""; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
])

dnl @synopsis AC_PROG_CC_IEEE
dnl
dnl Determine if the C compiler requires the -ieee flag.
dnl
dnl @author Gary Wong <gtw@gnu.org>

dnl FIXME it would be better to have a more robust check than looking for
dnl __DECC, but I don't have a system to test it on.

AC_DEFUN(AC_PROG_CC_IEEE,[
  AC_CACHE_CHECK(whether ${CC-cc} requires -ieee option, ac_cv_c_ieee,
[AC_EGREP_CPP(yes,
[#ifdef __DECC
  yes
#endif
], eval "ac_cv_c_ieee=yes", eval "ac_cv_c_ieee=no")])
  if test "$ac_cv_c_ieee" = "yes"; then
    CFLAGS="$CFLAGS -ieee"
  fi
])


# Configure paths for GTK+
# Owen Taylor     97-11-3

dnl AM_PATH_GTK([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GTK, and define GTK_CFLAGS and GTK_LIBS
dnl
AC_DEFUN(AM_PATH_GTK,
[dnl 
dnl Get the cflags and libraries from the gtk-config script
dnl
AC_ARG_WITH(gtk-prefix,[  --with-gtk-prefix=PFX   Prefix where GTK is installed (optional)],
            gtk_config_prefix="$withval", gtk_config_prefix="")
AC_ARG_WITH(gtk-exec-prefix,[  --with-gtk-exec-prefix=PFX Exec prefix where GTK is installed (optional)],
            gtk_config_exec_prefix="$withval", gtk_config_exec_prefix="")
AC_ARG_ENABLE(gtktest, [  --disable-gtktest       Do not try to compile and run a test GTK program],
		    , enable_gtktest=yes)

  for module in . $4
  do
      case "$module" in
         gthread) 
             gtk_config_args="$gtk_config_args gthread"
         ;;
      esac
  done

  if test x$gtk_config_exec_prefix != x ; then
     gtk_config_args="$gtk_config_args --exec-prefix=$gtk_config_exec_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_exec_prefix/bin/gtk-config
     fi
  fi
  if test x$gtk_config_prefix != x ; then
     gtk_config_args="$gtk_config_args --prefix=$gtk_config_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_prefix/bin/gtk-config
     fi
  fi

  AC_PATH_PROG(GTK_CONFIG, gtk-config, no)
  min_gtk_version=ifelse([$1], ,0.99.7,$1)
  AC_MSG_CHECKING(for GTK - version >= $min_gtk_version)
  no_gtk=""
  if test "$GTK_CONFIG" = "no" ; then
    no_gtk=yes
  else
    GTK_CFLAGS=`$GTK_CONFIG $gtk_config_args --cflags`
    GTK_LIBS=`$GTK_CONFIG $gtk_config_args --libs`
    gtk_config_major_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gtk_config_minor_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gtk_config_micro_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gtktest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GTK_CFLAGS"
      LIBS="$GTK_LIBS $LIBS"
dnl
dnl Now check if the installed GTK is sufficiently new. (Also sanity
dnl checks the results of gtk-config to some extent
dnl
      rm -f conf.gtktest
      AC_TRY_RUN([
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gtktest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gtk_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gtk_version");
     exit(1);
   }

  if ((gtk_major_version != $gtk_config_major_version) ||
      (gtk_minor_version != $gtk_config_minor_version) ||
      (gtk_micro_version != $gtk_config_micro_version))
    {
      printf("\n*** 'gtk-config --version' returned %d.%d.%d, but GTK+ (%d.%d.%d)\n", 
             $gtk_config_major_version, $gtk_config_minor_version, $gtk_config_micro_version,
             gtk_major_version, gtk_minor_version, gtk_micro_version);
      printf ("*** was found! If gtk-config was correct, then it is best\n");
      printf ("*** to remove the old version of GTK+. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gtk-config was wrong, set the environment variable GTK_CONFIG\n");
      printf("*** to point to the correct copy of gtk-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
#if defined (GTK_MAJOR_VERSION) && defined (GTK_MINOR_VERSION) && defined (GTK_MICRO_VERSION)
  else if ((gtk_major_version != GTK_MAJOR_VERSION) ||
	   (gtk_minor_version != GTK_MINOR_VERSION) ||
           (gtk_micro_version != GTK_MICRO_VERSION))
    {
      printf("*** GTK+ header files (version %d.%d.%d) do not match\n",
	     GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     gtk_major_version, gtk_minor_version, gtk_micro_version);
    }
#endif /* defined (GTK_MAJOR_VERSION) ... */
  else
    {
      if ((gtk_major_version > major) ||
        ((gtk_major_version == major) && (gtk_minor_version > minor)) ||
        ((gtk_major_version == major) && (gtk_minor_version == minor) && (gtk_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GTK+ (%d.%d.%d) was found.\n",
               gtk_major_version, gtk_minor_version, gtk_micro_version);
        printf("*** You need a version of GTK+ newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GTK+ is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gtk-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GTK+, but you can also set the GTK_CONFIG environment to point to the\n");
        printf("*** correct copy of gtk-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gtk=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gtk" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GTK_CONFIG" = "no" ; then
       echo "*** The gtk-config script installed by GTK could not be found"
       echo "*** If GTK was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GTK_CONFIG environment variable to the"
       echo "*** full path to gtk-config."
     else
       if test -f conf.gtktest ; then
        :
       else
          echo "*** Could not run GTK test program, checking why..."
          CFLAGS="$CFLAGS $GTK_CFLAGS"
          LIBS="$LIBS $GTK_LIBS"
          AC_TRY_LINK([
#include <gtk/gtk.h>
#include <stdio.h>
],      [ return ((gtk_major_version) || (gtk_minor_version) || (gtk_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GTK or finding the wrong"
          echo "*** version of GTK. If it is not finding GTK, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtk gtk-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GTK was incorrectly installed"
          echo "*** or that you have moved GTK since it was installed. In the latter case, you"
          echo "*** may want to edit the gtk-config script: $GTK_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GTK_CFLAGS=""
     GTK_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
  rm -f conf.gtktest
])


dnl PKG_CHECK_MODULES(GSTUFF, gtk+-2.0 >= 1.3 glib = 1.3.4, action-if, action-not)
dnl defines GSTUFF_LIBS, GSTUFF_CFLAGS, see pkg-config man page
dnl also defines GSTUFF_PKG_ERRORS on error
AC_DEFUN(PKG_CHECK_MODULES, [
  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(for $2)

        if $PKG_CONFIG --exists "$2" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING($1_CFLAGS)
            $1_CFLAGS=`$PKG_CONFIG --cflags "$2"`
            AC_MSG_RESULT($$1_CFLAGS)

            AC_MSG_CHECKING($1_LIBS)
            $1_LIBS=`$PKG_CONFIG --libs "$2"`
            AC_MSG_RESULT($$1_LIBS)
        else
            $1_CFLAGS=""
            $1_LIBS=""
            ## If we have a custom action on failure, don't print errors, but 
            ## do set a variable so people can do so.
            $1_PKG_ERRORS=`$PKG_CONFIG --errors-to-stdout --print-errors "$2"`
            ifelse([$4], ,echo $$1_PKG_ERRORS,)
        fi

        AC_SUBST($1_CFLAGS)
        AC_SUBST($1_LIBS)
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi

  if test $succeeded = yes; then
     ifelse([$3], , :, [$3])
  else
     ifelse([$4], , AC_MSG_ERROR([Library requirements ($2) not met; consider adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a nonstandard prefix so pkg-config can find them.]), [$4])
  fi
])



# Configure paths for GLIB
# Owen Taylor     97-11-3

dnl AM_PATH_GLIB([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GLIB, and define GLIB_CFLAGS and GLIB_LIBS, if "gmodule" or 
dnl gthread is specified in MODULES, pass to glib-config
dnl
AC_DEFUN(AM_PATH_GLIB,
[dnl 
dnl Get the cflags and libraries from the glib-config script
dnl
AC_ARG_WITH(glib-prefix,[  --with-glib-prefix=PFX   Prefix where GLIB is installed (optional)],
            glib_config_prefix="$withval", glib_config_prefix="")
AC_ARG_WITH(glib-exec-prefix,[  --with-glib-exec-prefix=PFX Exec prefix where GLIB is installed (optional)],
            glib_config_exec_prefix="$withval", glib_config_exec_prefix="")
AC_ARG_ENABLE(glibtest, [  --disable-glibtest       Do not try to compile and run a test GLIB program],
		    , enable_glibtest=yes)

  if test x$glib_config_exec_prefix != x ; then
     glib_config_args="$glib_config_args --exec-prefix=$glib_config_exec_prefix"
     if test x${GLIB_CONFIG+set} != xset ; then
        GLIB_CONFIG=$glib_config_exec_prefix/bin/glib-config
     fi
  fi
  if test x$glib_config_prefix != x ; then
     glib_config_args="$glib_config_args --prefix=$glib_config_prefix"
     if test x${GLIB_CONFIG+set} != xset ; then
        GLIB_CONFIG=$glib_config_prefix/bin/glib-config
     fi
  fi

  for module in . $4
  do
      case "$module" in
         gmodule) 
             glib_config_args="$glib_config_args gmodule"
         ;;
         gthread) 
             glib_config_args="$glib_config_args gthread"
         ;;
      esac
  done

  AC_PATH_PROG(GLIB_CONFIG, glib-config, no)
  min_glib_version=ifelse([$1], ,0.99.7,$1)
  AC_MSG_CHECKING(for GLIB - version >= $min_glib_version)
  no_glib=""
  if test "$GLIB_CONFIG" = "no" ; then
    no_glib=yes
  else
    GLIB_CFLAGS=`$GLIB_CONFIG $glib_config_args --cflags`
    GLIB_LIBS=`$GLIB_CONFIG $glib_config_args --libs`
    glib_config_major_version=`$GLIB_CONFIG $glib_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    glib_config_minor_version=`$GLIB_CONFIG $glib_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    glib_config_micro_version=`$GLIB_CONFIG $glib_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_glibtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GLIB_CFLAGS"
      LIBS="$GLIB_LIBS $LIBS"
dnl
dnl Now check if the installed GLIB is sufficiently new. (Also sanity
dnl checks the results of glib-config to some extent
dnl
      rm -f conf.glibtest
      AC_TRY_RUN([
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.glibtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_glib_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_glib_version");
     exit(1);
   }

  if ((glib_major_version != $glib_config_major_version) ||
      (glib_minor_version != $glib_config_minor_version) ||
      (glib_micro_version != $glib_config_micro_version))
    {
      printf("\n*** 'glib-config --version' returned %d.%d.%d, but GLIB (%d.%d.%d)\n", 
             $glib_config_major_version, $glib_config_minor_version, $glib_config_micro_version,
             glib_major_version, glib_minor_version, glib_micro_version);
      printf ("*** was found! If glib-config was correct, then it is best\n");
      printf ("*** to remove the old version of GLIB. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If glib-config was wrong, set the environment variable GLIB_CONFIG\n");
      printf("*** to point to the correct copy of glib-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
  else if ((glib_major_version != GLIB_MAJOR_VERSION) ||
	   (glib_minor_version != GLIB_MINOR_VERSION) ||
           (glib_micro_version != GLIB_MICRO_VERSION))
    {
      printf("*** GLIB header files (version %d.%d.%d) do not match\n",
	     GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     glib_major_version, glib_minor_version, glib_micro_version);
    }
  else
    {
      if ((glib_major_version > major) ||
        ((glib_major_version == major) && (glib_minor_version > minor)) ||
        ((glib_major_version == major) && (glib_minor_version == minor) && (glib_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GLIB (%d.%d.%d) was found.\n",
               glib_major_version, glib_minor_version, glib_micro_version);
        printf("*** You need a version of GLIB newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GLIB is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the glib-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GLIB, but you can also set the GLIB_CONFIG environment to point to the\n");
        printf("*** correct copy of glib-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_glib=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_glib" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GLIB_CONFIG" = "no" ; then
       echo "*** The glib-config script installed by GLIB could not be found"
       echo "*** If GLIB was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GLIB_CONFIG environment variable to the"
       echo "*** full path to glib-config."
     else
       if test -f conf.glibtest ; then
        :
       else
          echo "*** Could not run GLIB test program, checking why..."
          CFLAGS="$CFLAGS $GLIB_CFLAGS"
          LIBS="$LIBS $GLIB_LIBS"
          AC_TRY_LINK([
#include <glib.h>
#include <stdio.h>
],      [ return ((glib_major_version) || (glib_minor_version) || (glib_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GLIB or finding the wrong"
          echo "*** version of GLIB. If it is not finding GLIB, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtk gtk-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GLIB was incorrectly installed"
          echo "*** or that you have moved GLIB since it was installed. In the latter case, you"
          echo "*** may want to edit the glib-config script: $GLIB_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GLIB_CFLAGS=""
     GLIB_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GLIB_CFLAGS)
  AC_SUBST(GLIB_LIBS)
  rm -f conf.glibtest
])


# Configure paths for GTK+EXTRA
# Owen Taylor     97-11-3
# Adrian Feiguin  01-04-03 

dnl AM_PATH_GTK_EXTRA([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GTK_EXTRA, and define GTK_EXTRA_CFLAGS and GTK_EXTRA_LIBS
dnl
AC_DEFUN(AM_PATH_GTK_EXTRA,
[dnl 
dnl Get the cflags and libraries from the gtkextra-config script
dnl
AC_ARG_WITH(gtkextra-prefix,[  --with-gtkextra-prefix=PFX   Prefix where GTK_EXTRA is installed (optional)],
            gtkextra_config_prefix="$withval", gtkextra_config_prefix="")
AC_ARG_WITH(gtkextra-exec-prefix,[  --with-gtkextra-exec-prefix=PFX Exec prefix where GTK_EXTRA is installed (optional)],
            gtkextra_config_exec_prefix="$withval", gtkextra_config_exec_prefix="")
AC_ARG_ENABLE(gtkextratest, [  --disable-gtkextratest       Do not try to compile and run a test GTK_EXTRA program],
		    , enable_gtkextratest=yes)

  for module in . $4
  do
      case "$module" in
         gthread) 
             gtkextra_config_args="$gtkextra_config_args gthread"
         ;;
      esac
  done

  if test x$gtkextra_config_exec_prefix != x ; then
     gtkextra_config_args="$gtkextra_config_args --exec-prefix=$gtkextra_config_exec_prefix"
     if test x${GTK_EXTRA_CONFIG+set} != xset ; then
        GTK_EXTRA_CONFIG=$gtkextra_config_exec_prefix/bin/gtkextra-config
     fi
  fi
  if test x$gtkextra_config_prefix != x ; then
     gtkextra_config_args="$gtkextra_config_args --prefix=$gtkextra_config_prefix"
     if test x${GTK_EXTRA_CONFIG+set} != xset ; then
        GTK_EXTRA_CONFIG=$gtkextra_config_prefix/bin/gtkextra-config
     fi
  fi

  AC_PATH_PROG(GTK_EXTRA_CONFIG, gtkextra-config, no)
  min_gtkextra_version=ifelse([$1], ,0.99.13,$1)
  AC_MSG_CHECKING(for GTK_EXTRA - version >= $min_gtkextra_version)
  no_gtkextra=""
  if test "$GTK_EXTRA_CONFIG" = "no" ; then
    no_gtkextra=yes
  else
    GTK_EXTRA_CFLAGS=`$GTK_EXTRA_CONFIG $gtkextra_config_args --cflags`
    GTK_EXTRA_LIBS=`$GTK_EXTRA_CONFIG $gtkextra_config_args --libs`
    gtkextra_config_major_version=`$GTK_EXTRA_CONFIG $gtkextra_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gtkextra_config_minor_version=`$GTK_EXTRA_CONFIG $gtkextra_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gtkextra_config_micro_version=`$GTK_EXTRA_CONFIG $gtkextra_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gtkextratest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GTK_EXTRA_CFLAGS"
      LIBS="$GTK_EXTRA_LIBS $LIBS"
dnl
dnl Now check if the installed GTK_EXTRA is sufficiently new. (Also sanity
dnl checks the results of gtkextra-config to some extent
dnl
      rm -f conf.gtkextratest
      AC_TRY_RUN([
#include <gtkextra/gtkextra.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gtkextratest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gtkextra_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gtkextra_version");
     exit(1);
   }

  if ((gtkextra_major_version != $gtkextra_config_major_version) ||
      (gtkextra_minor_version != $gtkextra_config_minor_version) ||
      (gtkextra_micro_version != $gtkextra_config_micro_version))
    {
      printf("\n*** 'gtkextra-config --version' returned %d.%d.%d, but GTK_EXTRA+ (%d.%d.%d)\n", 
             $gtkextra_config_major_version, $gtkextra_config_minor_version, $gtkextra_config_micro_version,
             gtkextra_major_version, gtkextra_minor_version, gtkextra_micro_version);
      printf ("*** was found! If gtkextra-config was correct, then it is best\n");
      printf ("*** to remove the old version of GTK_EXTRA+. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gtkextra-config was wrong, set the environment variable GTK_EXTRA_CONFIG\n");
      printf("*** to point to the correct copy of gtkextra-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
#if defined (GTK_EXTRA_MAJOR_VERSION) && defined (GTK_EXTRA_MINOR_VERSION) && defined (GTK_EXTRA_MICRO_VERSION)
  else if ((gtkextra_major_version != GTK_EXTRA_MAJOR_VERSION) ||
	   (gtkextra_minor_version != GTK_EXTRA_MINOR_VERSION) ||
           (gtkextra_micro_version != GTK_EXTRA_MICRO_VERSION))
    {
      printf("*** GTK_EXTRA+ header files (version %d.%d.%d) do not match\n",
	     GTK_EXTRA_MAJOR_VERSION, GTK_EXTRA_MINOR_VERSION, GTK_EXTRA_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     gtkextra_major_version, gtkextra_minor_version, gtkextra_micro_version);
    }
#endif /* defined (GTK_EXTRA_MAJOR_VERSION) ... */
  else
    {
      if ((gtkextra_major_version > major) ||
        ((gtkextra_major_version == major) && (gtkextra_minor_version > minor)) ||
        ((gtkextra_major_version == major) && (gtkextra_minor_version == minor) && (gtkextra_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GTK_EXTRA+ (%d.%d.%d) was found.\n",
               gtkextra_major_version, gtkextra_minor_version, gtkextra_micro_version);
        printf("*** You need a version of GTK_EXTRA+ newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GTK_EXTRA+ is always available from ftp://ftp.gtkextra.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gtkextra-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GTK_EXTRA+, but you can also set the GTK_EXTRA_CONFIG environment to point to the\n");
        printf("*** correct copy of gtkextra-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gtkextra=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gtkextra" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GTK_EXTRA_CONFIG" = "no" ; then
       echo "*** The gtkextra-config script installed by GTK_EXTRA could not be found"
       echo "*** If GTK_EXTRA was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GTK_EXTRA_CONFIG environment variable to the"
       echo "*** full path to gtkextra-config."
     else
       if test -f conf.gtkextratest ; then
        :
       else
          echo "*** Could not run GTK_EXTRA test program, checking why..."
          CFLAGS="$CFLAGS $GTK_EXTRA_CFLAGS"
          LIBS="$LIBS $GTK_EXTRA_LIBS"
          AC_TRY_LINK([
#include <gtkextra/gtkextra.h>
#include <stdio.h>
],      [ return ((gtkextra_major_version) || (gtkextra_minor_version) || (gtkextra_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GTK_EXTRA or finding the wrong"
          echo "*** version of GTK_EXTRA. If it is not finding GTK_EXTRA, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK_EXTRA package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtkextra gtkextra-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GTK_EXTRA was incorrectly installed"
          echo "*** or that you have moved GTK_EXTRA since it was installed. In the latter case, you"
          echo "*** may want to edit the gtkextra-config script: $GTK_EXTRA_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GTK_EXTRA_CFLAGS=""
     GTK_EXTRA_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GTK_EXTRA_CFLAGS)
  AC_SUBST(GTK_EXTRA_LIBS)
  rm -f conf.gtkextratest
])

dnl Test for the ArtsC interface.

AC_DEFUN([AM_PATH_ARTSC],
[
AC_ARG_ENABLE(artsc-test,
[  --disable-artsc-test      Do not compile and run a test ArtsC program.],
[artsc_test="yes"],
[artsc_test="no"])

dnl Search for the arts-config program.
AC_PATH_PROG(ARTSC_CONFIG, artsc-config, no)
min_artsc_version=ifelse([$1],,0.9.5,$1)
AC_MSG_CHECKING(for ArtsC - version >= $min_artsc_version)

no_artsc=""
if test "x$ARTSC_CONFIG" != "xno"; then
  ARTSC_CFLAGS=`$ARTSC_CONFIG --cflags`
  ARTSC_LIBS=`$ARTSC_CONFIG --libs`

  artsc_major_version=`$ARTSC_CONFIG $artsc_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
  artsc_minor_version=`$ARTSC_CONFIG $artsc_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
  artsc_micro_version=`$ARTSC_CONFIG $artsc_config_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

  # Test if a simple ArtsC program can be created.
  if test "x$artsc_test" = "xyes"; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $ARTSC_CFLAGS"
    LIBS="$LIBS $ARTSC_LIBS"

    rm -f conf.artsc
    AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <artsc.h>

char*
my_strdup(char *str)
{
  char *new_str;
 
  if (str) {
    new_str = malloc((strlen(str) + 1) * sizeof(char));
    strcpy(new_str, str);
  } else {
    new_str = NULL;
  }

  return new_str;
}
 
int main()
{
  int major, minor, micro;
  char *tmp_version;
 
  system ("touch conf.artsc");

  /* HP/UX 9 writes to sscanf strings */
  tmp_version = my_strdup("$min_artsc_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_artsc_version");
     exit(1);
  }
 
  if (($artsc_major_version > major) ||
     (($artsc_major_version == major) && ($artsc_minor_version > minor)) ||
     (($artsc_major_version == major) && ($artsc_minor_version == minor)
                                    && ($artsc_micro_version >= micro))) {
    return 0;
  } else {
    printf("\n*** 'artsc-config --version' returned %d.%d.%d, but the minimum version\n", $artsc_major_version, $artsc_minor_version, $artsc_micro_version);
    printf("*** of ARTSC required is %d.%d.%d. If artsc-config is correct, then it is\n", major, minor, micro);
    printf("*** best to upgrade to the required version.\n");
    printf("*** If artsc-config was wrong, set the environment variable ARTSC_CONFIG\n");
    printf("*** to point to the correct copy of artsc-config, and remove the file\n");
    printf("*** config.cache before re-running configure\n");
    return 1;
  }
}
],, no_artsc=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
   CFLAGS="$ac_save_CFLAGS"
   LIBS="$ac_save_LIBS"
  fi
else
  no_artsc=yes
fi

if test "x$no_artsc" != "xyes"; then
  AC_MSG_RESULT(yes)
  ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(no)

  if test "x$ARTSC_CONFIG" = "xno" ; then
    echo "*** The artsc-config script installed by ArtsC could not be found"
    echo "*** If ArtsC was installed in PREFIX, make sure PREFIX/bin is in"
    echo "*** your path, or set the ARTSC_CONFIG environment variable to the"
    echo "*** full path to artsc-config."
  else
    if test -f conf.artsc ; then
      :
    else
      echo "*** Could not run ArtsC test program, checking why..."
      CFLAGS="$CFLAGS $ARTSC_CFLAGS"
      LIBS="$LIBS $ARTSC_LIBS"
      AC_TRY_LINK([
#include <stdio.h>
#include <artsc.h>
],[return 0;],
[ echo "*** The test program compiled, but did not run. This usually means"
  echo "*** that the run-time linker is not finding ArtsC or finding the wrong"
  echo "*** version of ArtsC. If it is not finding ArtsC, you'll need to set your"
  echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
  echo "*** to the installed location  Also, make sure you have run ldconfig if that"
  echo "*** is required on your system"
  echo "***"
  echo "*** If you have an old version installed, it is best to remove it, although"
  echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
  [ echo "*** The test program failed to compile or link. See the file config.log for the"
  echo "*** exact error that occured. This usually means ArtsC was incorrectly installed"
  echo "*** or that you have moved ArtsC since it was installed. In the latter case, you"
  echo "*** may want to edit the artsc-config script: $ARTSC_CONFIG" ])
      CFLAGS="$ac_save_CFLAGS"
      LIBS="$ac_save_LIBS"
    fi
  fi
  ARTSC_CFLAGS=""
  ARTSC_LIBS=""
  ifelse([$3], , :, [$3])
fi

AC_SUBST(ARTSC_CFLAGS)
AC_SUBST(ARTSC_LIBS)
rm -f conf.artsc

])

