; GNU Backgammon music Csound orchestra file

sr	= 22050
kr	= 2205
ksmps	= 10
nchnls	= 2

garvbl	init 0
garvbr	init 0
gamrvbl	init 0
gamrvbr	init 0

	instr 1 ; Trumpet
ifreq	= cpspch(p4)
iamp	= ampdbfs(p5+25)
ipan	= p6
itens	= 0.52
iatt	= 0.03
ivibf	= 6
aenv	xadsr p7, p8, 0.3, 0.5
asound	wgbrass iamp, ifreq, itens, iatt, ivibf, aenv/2, 1, 50
acomb	= asound * aenv
afilt	tone acomb, 10*log(ifreq)*log(ifreq)
al, ar	locsig afilt, ipan, 1, 0.8
ar1, ar2 locsend
	 outs al, ar
gamrvbl = gamrvbl + ar1
gamrvbr = gamrvbr + ar2
	endin

	instr 2 ; Massive reverb
arvbl	nreverb gamrvbl, 3, 0.5
arvbr	nreverb gamrvbr, 3, 0.5
	outs arvbl, arvbr
gamrvbl	= 0
gamrvbr	= 0
	endin

	instr 3 ; Guitar
ifreq	= cpspch(p4-1+0.08) ; A-flat major
iamp	= ampdbfs(p5+25)
ipan	= p6
ipickup = 0.85
irand	gauss 0.15
ipluck  = 0.25 + irand
idamp	= 3
ifilt	= 100
aexcite	lfo 1, 3, 1
abase	wgpluck ifreq, iamp, ipickup, ipluck, idamp, ifilt, aexcite
alo	wgpluck ifreq*0.99, iamp*0.21, ipickup, ipluck*1.1, idamp,ifilt,aexcite
ahi	wgpluck ifreq*1.01, iamp*0.19, ipickup, ipluck*0.9, idamp,ifilt,aexcite
aenv	adsr 0.007, 0.005, 0.7, 0.01
acomb	= (abase+alo+ahi) * aenv
afilt	tone acomb, 10*log(ifreq)*log(ifreq)
al, ar	locsig afilt, ipan, 1, 0.08
ar1, ar2 locsend
	 outs al, ar
garvbl = garvbl + ar1
garvbr = garvbr + ar2
	endin

	instr 4 ; Violin
ifreq	= cpspch(p4-1+0.08) ; A-flat major
iamp	= ampdbfs(p5+25)
ipan	= p6
ipres	= 3
ivibf	= 6
irand	gauss 0.03
irat    = 0.127 + irand
aenv	adsr 0.05, 0.08, 0.9, 0.1
astring	wgbow iamp, ifreq, ipres, irat, ivibf, aenv/100, 1, 50
acomb	= astring * aenv
afilt	tone acomb, 10*log(ifreq)*log(ifreq)
al, ar	locsig afilt, ipan, 1, 0.1
ar1, ar2 locsend
	 outs al, ar
garvbl = garvbl + ar1
garvbr = garvbr + ar2
	endin

	instr 5 ; Global reverb
arvbl	nreverb garvbl, 2, 0.8
arvbr	nreverb garvbr, 2, 0.8
	outs arvbl, arvbr
garvbl	= 0
garvbr	= 0
	endin
