soundsdir = $(pkgdatadir)/sounds
sounds_DATA = chequer.wav dance.wav double.wav drop.wav fanfare.wav \
	gameover.wav haere-ra.wav matchover.wav move.wav resign.wav roll.wav \
	take.wav

EXTRA_DIST = gnubg.orc music.orc chequer.sco dance.sco double.sco drop.sco \
	fanfare.sco gameover.sco haere-ra.sco matchover.sco move.sco \
	resign.sco roll.sco take.sco $(sounds_DATA)

%.wav: %.sco music.orc gnubg.orc
	if [ $< = haere-ra.sco -o $< = fanfare.sco ]; then orc=music.orc; \
	else orc=gnubg.orc; fi; $(CSOUND) $$orc $< -W -o $@.tmp && \
	$(SRCONV) -W -r 22050 -o $@ $@.tmp; $(RM) $@.tmp
