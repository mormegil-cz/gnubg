#!/bin/sh
if [ ! -f gnubg.c ]; then
    echo You must run autogen.sh in the top level source directory.
    exit 1
fi

touch Makefile.in aclocal.m4 config.h.in configure
touch board3d/Makefile.in
touch doc/Makefile.in
touch ext/Makefile.in
touch intl/Makefile.in
touch lib/Makefile.in
touch m4/Makefile.in
touch met/Makefile.in
touch po/Makefile.in
touch scripts/Makefile.in
touch sounds/Makefile.in
touch textures/Makefile.in
touch xpm/Makefile.in
