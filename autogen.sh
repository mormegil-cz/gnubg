#! /bin/sh

set -x
aclocal -I m4
libtoolize --force --copy
autoheader
automake --add-missing --copy -Wno-portability
autoconf

