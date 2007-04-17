gnubg_helpdir = $(DESTDIR)$(pkgdatadir)/doc
gnubg_help_DATA =  gnubg.html

man_MANS = gnubg.6

gnubg_imagedir =  $(pkgdatadir)/doc/images
gnubg_image_DATA = images/*.png

EXTRA_DIST = *.xml gnubg/* images/*.png images/*.txt

man6_MANS = gnubg.6

all: gnubg.6 gnubg.html allabout.html gnubg.texi gnubg.info

gnubg.6: gnubgman.xml
	-docbook2man gnubgman.xml 2>.gnubgman.err || \
	cp gnubg/gnubg.6 .

gnubg.html: gnubgdb.xml images/*.png
	xsltproc -o gnubg.html --nonet http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl gnubgdb.xml || \
	cp gnubg/gnubg.html .


allabout.html: allabout.xml images/*.png
	xsltproc -o allabout.html --nonet http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl allabout.xml || \
	cp gnubg/allabout.html .

gnubg.texi: gnubgdb.xml
	docbook2texi --encoding=UTF-8 gnubgdb.xml 2>.gnubgtexi.err || \
	cp gnubg/gnubg.texi .

gnubg.info: gnubg.texi
	makeinfo gnubg.texi -o gnubg.info 2>.gnubginfo.err || \
	cp gnubg/gnubg.info .

install-data-local: gnubg.html allabout.html gnubg.info images/*.png
	$(mkinstalldirs) $(gnubg_helpdir)/images
	-for file in gnubg.html allabout.html gnubgdb.xml allabout.xml gnubg.info; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(gnubg_helpdir)/$$basefile; \
	done

CLEANFILES=*.html
DISTCLEANFILES=gnubg.texi gnubg.info gnubg.6
