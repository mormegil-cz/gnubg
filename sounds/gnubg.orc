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
a2	tone a1, 8000
	out a2
	endin

	instr 3 ; Chequer against chequer
k1	timeinsts
a1	tambourine p4, 0.1, 1, 0.3, 0.05, p5, 1400+20000*k1, 2025+20000*k1
a2	tone a1, p6
	out a2
	endin

	instr 4 ; Pencil scratches
a1	sandpaper p4/1000, 0.01, 3200, 0.5, 0.00002
	out a1*0.3
	endin

	instr 5 ; Arbitrary tone
k1	expseg 0.01, p3, 0.5
a1	lfo p4, (1+k1)*p5, 1
a2	tone a1, 500
	out a2
	endin
