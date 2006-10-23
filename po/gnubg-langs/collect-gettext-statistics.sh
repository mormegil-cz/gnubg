#!/bin/sh

MSGFMT=msgfmt
CATALOGS_DIR=`pwd`/..

do_collect()
{
    echo '<?php'
    echo '$catstatus = array('

    for i in ${CATALOGS_DIR}/*.po ; do
        catname=`basename $i .po`
        lastdate=`sed -ne 's/"PO-Revision-Date: \(....-..-..\) .*/\1/p' $i`
        x=`$MSGFMT --verbose -o /dev/null $i 2>&1 | \
               grep 'translated messages' | \
               sed -e 's/[,\.]//g' \
                   -e 's/\([0-9]\+\) translated messages\?/TR=\1/' \
                   -e 's/\([0-9]\+\) fuzzy translations\?/FZ=\1/' \
                   -e 's/\([0-9]\+\) untranslated messages\?/UT=\1/'`
        TR=0 FZ=0 UT=0
        eval $x
        echo "\"$catname\" => array($TR, $FZ, $UT, \"$lastdate\"),"
    done

    echo ');'
    echo '?>'
}

do_collect >__catstatus.php 
