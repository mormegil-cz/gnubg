info_TEXINFOS = gnubg.texi

EXTRA_DIST = rulfig1.png rulfig2.png rulfig3.png rulfig4.png rulfig5.png \
	rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt
SUFFICES = .png .eps

%.eps: %.png
	pngtopnm -mix -background 1.0,1.0,1.0 < $< | pnmtops -noturn > $@

gnubg.dvi: rulfig1.eps rulfig2.eps rulfig3.eps rulfig4.eps rulfig5.eps

gnubg.info: rulfig1.txt rulfig2.txt rulfig3.txt rulfig4.txt rulfig5.txt
