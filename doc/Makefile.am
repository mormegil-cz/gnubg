gnubg_helpdir = $(DESTDIR)$(pkgdatadir)/doc
gnubg_help_DATA =  gnubg.html

man_MANS = gnubg.6

gnubg_imagedir =  $(pkgdatadir)/doc/images
gnubg_image_DATA = images/*.png

EXTRA_DIST = *.xml gnubg/* images/*.png images/*.txt

man6_MANS = gnubg.6

all: gnubg.6 gnubg.html allabout.html gnubg.texi gnubg.info gnubg.pdf allabout.pdf

gnubg.6: gnubgman.xml
	$(DOCBOOK2MAN) gnubgman.xml || \
		cp gnubg/gnubg.6 .

gnubg.pdf: gnubgdb.xml images/*.png
	$(DBLATEX) gnubgdb.xml -o gnubg.pdf || cp gnubg/gnubg.pdf .

allabout.pdf: allabout.xml images/*.png
	$(DBLATEX) allabout.xml -o allabout.pdf || cp gnubg/allabout.pdf .

gnubg.html: gnubgdb.xml images/*.png
	$(XSLTPROC) \
		--stringparam chunker.output.encoding ISO8859-1 \
		--stringparam  use.id.as.filename 1 \
		--stringparam  root.filename  '' \
		--stringparam  html.longdesc 0 \
		--nonet http://docbook.sourceforge.net/release/xsl/current/xhtml/onechunk.xsl \
		gnubgdb.xml || \
		cp gnubg/gnubg.html .


allabout.html: allabout.xml images/*.png
	$(XSLTPROC) \
		--stringparam chunker.output.encoding ISO8859-1 \
		--stringparam  use.id.as.filename 1 \
		--stringparam  root.filename  '' \
		--nonet http://docbook.sourceforge.net/release/xsl/current/xhtml/onechunk.xsl \
		allabout.xml || \
		cp gnubg/allabout.html .

gnubg.texi: gnubgdb.xml
	$(DOCBOOK2TEXI) --encoding=ISO-8859-1 --string-param prefer-textobjects=1 gnubgdb.xml || \
	cp gnubg/gnubg.texi .

gnubg.info: gnubg.texi
	makeinfo gnubg.texi -o gnubg.info 2>.gnubginfo.err || \
	cp gnubg/gnubg.info .

html_node: gnubg.texi
	makeinfo --html gnubg.texi -o html_node

install-data-local: gnubg.html allabout.html gnubg.info images/*.png
	$(mkinstalldirs) $(gnubg_helpdir)/images
	-for file in gnubg.html allabout.html gnubgdb.xml allabout.xml gnubg.info; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(gnubg_helpdir)/$$basefile; \
	done

MOSTLYCLEANFILES=*.html gnubg.6 gnubg.info gnubg.texi manpage.links manpage.refs
DISTCLEANFILES=gnubg.texi gnubg.info gnubg.6
