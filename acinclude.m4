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
dnl @author Gary Wong <gtw@gnu.org>

AC_DEFUN(AM_GUILE,[
  AC_PATH_PROG(GUILE_CONFIG, guile-config, no)
  if test "$GUILE_CONFIG" = "no"; then
    no_guile=yes
    ifelse([$2], , :, [$2])
  else
    GUILE_CFLAGS="`guile-config compile`"
    GUILE_LDFLAGS="`guile-config link`"
    ifelse([$1], , :, [$1])
  fi
  AC_SUBST(GUILE_CFLAGS)
  AC_SUBST(GUILE_LDFLAGS)
])
