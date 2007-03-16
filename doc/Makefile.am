gnubg_helpdir = $(pkgdatadir)/doc
gnubg_help_DATA =  gnubg.html gnubg.xml

man_MANS = gnubg.6

gnubg_imagedir =  $(pkgdatadir)/doc/images
gnubg_image_DATA = images/*.png

EXTRA_DIST = gnubgdb.xml gnubgman.xml images/*.png images/*.txt gnubg/gnubg.6 \
	     gnubg/gnubg.html gnubg/gnubg.texi gnubg/gnubg.info gnubg/gnubg.xml

man6_MANS = gnubg.6

info_TEXINFOS = gnubg.texi

all: 

gnubg.6: gnubg/gnubg.6
	-cp gnubg/gnubg.6 .

gnubg.html: gnubg/gnubg.html
	-cp gnubg/gnubg.html .

gnubg.texi: gnubg/gnubg.texi
	-cp gnubg/gnubg.texi .

gnubg.info: gnubg/gnubg.info
	-cp gnubg/gnubg.info .

gnubg.xml: gnubg/gnubg.xml
	-cp gnubg/gnubg.xml .

gnubg:
	mkdir gnubg

gnubg/gnubg.6: 
	-docbook2man gnubgman.xml
	-cp gnubg.6 gnubg/

gnubg/gnubg.html: gnubgdb.xml gnubg
	-xsltproc -o gnubg/gnubg.html --nonet http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl gnubgdb.xml

gnubg/gnubg.texi: gnubgdb.xml  gnubg
	-docbook2texi --encoding=UTF-8 gnubgdb.xml
	-cp gnubg.texi gnubg/

gnubg/gnubg.xml: gnubg.texi gnubg
	-sed '/@detailmenu/,/@end detailmenu/ d' gnubg.texi | makeinfo -o gnubg/gnubg.xml --xml


install-data-local: gnubg.html gnubg.xml gnubg.info images/*.png
	$(mkinstalldirs) $(gnubg_helpdir)/images
	-for file in gnubg.html gnubg.xml gnubgdb.xml gnubg.info; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(gnubg_helpdir)/$$basefile; \
	done

