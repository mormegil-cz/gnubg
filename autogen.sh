#! /bin/sh

set -x
aclocal -I m4
if [ `uname -s` = Darwin ]
then
    LIBTOOLIZE=glibtoolize
else
    LIBTOOLIZE=libtoolize
fi
$LIBTOOLIZE --force --copy
autoheader
automake --add-missing --copy -Wno-portability
autoconf

