soundsdir = $(pkgdatadir)/sounds
sounds_DATA = dance.wav double.wav drop.wav fanfare.wav gameover.wav \
	haere-ra.wav matchover.wav move.wav resign.wav roll.wav take.wav

EXTRA_DIST = gnubg.orc music.orc

%.wav: %.sco music.orc gnubg.orc
	if [ $< = haere-ra.sco -o $< = fanfare.sco ]; then orc=music.orc; \
	else orc=gnubg.orc; fi; $(CSOUND) $$orc $< -W -o $@
