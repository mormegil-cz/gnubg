info_TEXINFOS = gnubg.texi

EXTRA_DIST = rulfig1.png rulfig2.png rulfig3.png rulfig4.png rulfig5.png \
	rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt \
	annealing.gp annealing.txt fdl.texi
SUFFIXES = .png .eps .texi .html .txt
EPSFILES = rulfig1.eps rulfig2.eps rulfig3.eps rulfig4.eps rulfig5.eps \
	annealing.eps
TXTFILES = rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt \
	annealing.txt
CLEANFILES = $(EPSFILES) anneal.txt

txt: gnubg.txt

ps: gnubg.ps

html: gnubg.html

gnubg.dvi: $(EPSFILES) fdl.texi

gnubg.html: $(TXTFILES) annealing.png fdl.texi

gnubg.info: $(TXTFILES) fdl.texi

gnubg.ps: $(EPSFILES) fdl.texi

gnubg.txt: $(TXTFILES) fdl.texi

annealing.txt annealing.eps: annealing.gp
	$(GNUPLOT) $< && \
	$(TR) -d '\f' < anneal.txt > $(srcdir)/annealing.txt

annealing.png: annealing.eps
	pstopnm -portrait -xsize 1024 $<
	pnmscale 0.5 < annealing.eps001.ppm | pnmtopng > \
		$(srcdir)/annealing.png

.png.eps:
	$(PNGTOPNM) -mix -background 1.0,1.0,1.0 < $< | $(PNMTOPS) -noturn > $@

.texi.html:
	cd $(srcdir) && $(MAKEINFO) --html `echo $< | sed 's,.*/,,'`

.texi.txt:
	cd $(srcdir) && \
	$(MAKEINFO) --no-split --no-headers `echo $< | sed 's,.*/,,'` > $@
