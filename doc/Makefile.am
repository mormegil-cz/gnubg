info_TEXINFOS = gnubg.texi

EXTRA_DIST = rulfig1.png rulfig2.png rulfig3.png rulfig4.png rulfig5.png \
	rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt
SUFFICES = .png .eps

txt: gnubg.txt

ps: gnubg.ps

html: gnubg.html

gnubg.dvi: rulfig1.eps rulfig2.eps rulfig3.eps rulfig4.eps rulfig5.eps

gnubg.info: rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt

.png.eps:
	$(PNGTOPNM) -mix -background 1.0,1.0,1.0 < $< | pnmtops -noturn > $@

.texi.html:
	$(MAKEINFO) --html -I $(srcdir) $<

.texi.txt:
	$(MAKEINFO) --no-split --no-headers -I $(srcdir) $< > $@
