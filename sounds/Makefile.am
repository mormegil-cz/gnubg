pkgdata_DATA = double.wav drop.wav move.wav roll.wav take.wav

EXTRA_DIST = gnubg.orc

%.wav: %.sco gnubg.orc
	$(CSOUND) gnubg.orc $< -W -o $@
