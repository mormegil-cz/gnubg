; GNU Backgammon sound effects Csound orchestra file

sr	= 44100
kr	= 4410
ksmps	= 10
nchnls	= 1

	instr 1 ; Die against cork
a1	tambourine p4, 0.1, 2, 0.3, 0.05, p5, 1400, 2025
a2	tone a1, p6
	out a2
	endin

	instr 2 ; Die against die
k1	timeinsts
a1	tambourine p4, 0.1, 1, 0.3, 0.05, p5, 5600+40000*k1, 8100+40000*k1
	out a1
	endin

	instr 3 ; Chequer against chequer
k1	timeinsts
a1	tambourine p4, 0.1, 1, 0.3, 0.05, p5, 1400+20000*k1, 2025+20000*k1
a2	tone a1, p6
	out a2
	endin
