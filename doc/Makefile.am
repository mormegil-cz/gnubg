info_TEXINFOS = gnubg.texi
gnubg_TEXINFOS = fdl.texi faq.texi bearoff.texi movefilters.texi cubeful.texi scripting.texi absrating.texi
if XML_MANUAL
pkgdata_DATA = gnubg.xml texinfo.dtd rulfig1.png rulfig2.png rulfig3.png \
	rulfig4.png rulfig5.png annealing.png
endif

EXTRA_DIST = rulfig1.png rulfig2.png rulfig3.png rulfig4.png rulfig5.png \
	rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt \
	annealing.gp annealing.txt fdl.texi gnubg.xml texinfo.dtd \
	annealing.png annealing.eps \
	mgtp.gp mgtp.png mgtp.eps mgtp.txt \
	mgtp.gp mgtp.png mgtp.eps mptp.txt
	d11.png d23.png d3.png d5.png d7.png fita2.png \
	fitb.png lsqcoeff.png r11.png r1.png r23.png r3.png r5.png \
	r7.png rallfitted.png rall.png

SUFFIXES = .png .eps .texi .html .txt .xml
EPSFILES = rulfig1.eps rulfig2.eps rulfig3.eps rulfig4.eps rulfig5.eps \
	annealing.eps
TXTFILES = rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt \
	annealing.txt
CLEANFILES = $(EPSFILES) anneal.txt

txt: gnubg.txt

ps: gnubg.ps

html: gnubg.html

xml: gnubg.xml

gnubg.dvi: $(EPSFILES)

gnubg.html: $(TXTFILES) annealing.png mgtp.png

gnubg.info: $(TXTFILES)

gnubg.ps: $(EPSFILES)

gnubg.txt: $(TXTFILES)

gnubg.xml: $(TXTFILES) annealing.png mgtp.png

annealing.txt annealing.eps: annealing.gp
	if [ -n "$(GNUPLOT)" ]; then \
	$(GNUPLOT) $< && \
	$(TR) -d '\f' < anneal.txt > $(srcdir)/annealing.txt; fi

annealing.png: annealing.eps
	if [ -n "$(PSTOPNM)" -a -n "$(PNMSCALE)" -a -n "$(PNMTOPNG)" ]; then \
	$(PSTOPNM) -portrait -xsize 1024 $<; \
	$(PNMSCALE) 0.5 < annealing.eps001.ppm | $(PNMTOPNG) > \
		$(srcdir)/annealing.png; fi
	-rm annealing.eps001.ppm

.png.eps:
	if [ -n "$(PNGTOPNM)" -a -n "$(PNMTOPS)" ]; then \
	$(PNGTOPNM) -mix -background 1.0,1.0,1.0 < $< | \
	$(PNMTOPS) -noturn > $@; fi

.texi.html:
	cd $(srcdir) && $(MAKEINFO) --html `echo $< | sed 's,.*/,,'` -o $@

.texi.txt:
	cd $(srcdir) && \
	$(MAKEINFO) --no-split --no-headers `echo $< | sed 's,.*/,,'` -o $@

## Old versions of makeinfo don't support --xml, and there's not much point
## worrying about them, so we ignore failures.
.texi.xml:
	-cd $(srcdir) && $(MAKEINFO) --xml `echo $< | sed 's,.*/,,'` -o $@ 
	-cp $(datadir)/texinfo/texinfo.dtd $(srcdir)/
