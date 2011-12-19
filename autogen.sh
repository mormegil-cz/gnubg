#! /bin/sh

set -x

rm -f aclocal.m4
aclocal -I m4

if [ `uname -s` = Darwin ]
then
    LIBTOOLIZE=glibtoolize
else
    LIBTOOLIZE=libtoolize
fi

# If we use libtool-2, libtoolize below will recreate them, but if we use
# libtool-1 we don't want them, which could happen if we use a shared
# source directory or work from a "make dist" made on a libtool-2 system.
#
rm -f m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4
$LIBTOOLIZE --force --copy

# In case we got a "You should update your `aclocal.m4' by running aclocal."
# from libtoolize.
#
aclocal -I m4

autoheader
automake --add-missing --copy -Wno-portability
autoconf
