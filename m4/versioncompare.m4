# AS_VERSION_COMPARE(VERSION-1, VERSION-2,
#                    [ACTION-IF-LESS], [ACTION-IF-EQUAL], [ACTION-IF-GREATER])
# -----------------------------------------------------------------------------
# Compare two strings possibly containing shell variables as version strings.
AC_DEFUN([AS_VERSION_COMPARE],
[if test "x[$1]" = "x[$2]"; then
  # the strings are equal.  run ACTION-IF-EQUAL and bail
  m4_default([$4], :)
m4_ifvaln([$3$5], [else
  # first unletter the versions
  # this only works for a single trailing letter
  dnl echo has a double-quoted arg to allow for shell expansion.
  as_version_1=`echo "[$1]" |
                 sed 's/\([abcedfghi]\)/.\1/;
                      s/\([jklmnopqrs]\)/.1\1/;
                      s/\([tuvwxyz]\)/.2\1/;
 y/abcdefghijklmnopqrstuvwxyz/12345678901234567890123456/;'`
  as_version_2=`echo "[$2]" |
                 sed 's/\([abcedfghi]\)/.\1/;
                      s/\([jklmnopqrs]\)/.1\1/;
                      s/\([tuvwxyz]\)/.2\1/;
 y/abcdefghijklmnopqrstuvwxyz/12345678901234567890123456/;'`
  as_count=1
  as_save_IFS=$IFS
  IFS=.
  as_retval=-1
  for vsub1 in $as_version_1; do
    vsub2=`echo "$as_version_2" |awk -F. "{print \\\$$as_count}"`
    if test -z "$vsub2" || test $vsub1 -gt $vsub2; then
      as_retval=1
      break
    elif test $vsub1 -lt $vsub2; then
      break
    fi
    as_count=`expr $as_count + 1`
  done
  IFS=$as_save_IFS
  if test $as_retval -eq -1; then
    m4_default([$3], :)
  m4_ifval([$5], [else
$5])
  fi])
fi
]) # AS_VERSION_COMPARE


