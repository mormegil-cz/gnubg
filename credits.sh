#!/bin/sh

column < /dev/null || exit 0

contributors=/tmp/cont.$$
credit=/tmp/cred.$$

cat > AUTHORS <<'EOF'
                         GNU Backgammon was written by:

               Joseph Heled     �ystein Johansen     David Montgomery   
                  Jim Segrave     J�rn Thyssen     Gary Wong



        Contributors (of bug reports, suggestions, and patches) include:

EOF

cat > $contributors <<'EOF'
Kazuaki Asai
Eric Augustine
Erik Barfoed
Ron Barry
Steve Baedke
Holger Bochnig
Chuck Bower
Nick Bratby
Renzo Campagna
Craig Campbell
John Chuang
Tristan Colgate
Olivier Croisille
Ned Cross
Bill Davidson
Michael Depreli
Ian Dunstan
Max Durbano
Peter Eberhard
Robert Eberlein
Fotis
Ric Gerace
Michel Grimminck
Eric Groleau
Jeff Haferman
Morten Juul Henriksen
Alain Henry
Jens Hoefkens
Martin Janke
Nis Jorgensen
Matija Kejzar
James F. Kibler
Jonathan Kinsey
Johnny Kirk
Gerhard Knop
Robert Konigsberg
Corrin Lakeland
Tim Laursen
Eli Liang
Kevin Maguire
Giulio De Marco
William Maslen
Thomas Meyer
Achim Mueller
Magnar Naustdalslid
Dave Neary
Rolf Nielsen
Mirori Orange
Peter Orum
Roy Passfield
Thomas Patrick
Billie Patterson
Mike Petch
Zvezdan Petkovic
Nardy Pillards
Petri Pitk�nen
Sam Pottle
Henrik Ravn
James Rech
Jared Riley
Klaus Rindholt
Oliver Riordan
Rod Roark
Hans-J�rgen Sch�fer
Steve Schreiber
Hugh Sconyers
Martin Schrode
Paul Selick
Sho Sengoku
Ian Shaw
Albert Silver
Alberta di Silvio
Peter Sochovsky
Mark Spencer
Scott Steiner
Maik Stiebler
W. Stroop (Rob)
Kaoru Takahasi
Yoshito Takeuchi
Jacques Thiriat
Malene Thyssen
Sander van Rijnswou
Robert-Jan Veldhuizen
Morten Wang
JP White
Chris Wilson
Kit Woolsey
Frank Worrell
Christopher D. Yep
Anders Zachrison
EOF

column -c 72 < $contributors | expand | sed 's/^/    /' >> AUTHORS

cat >> AUTHORS <<'EOF'


  Credit is also due to those who have published information about backgammon
   playing programs (references will appear here later).  GNU Backgammon has
                              borrowed ideas from:

EOF

cat > $credit <<'EOF'
Hans Berliner
Chuck Bower
Rick Janowski
Brian Sheppard
Gerry Tesauro
Morten Wang
Michael Zehr
EOF

column -c 72 < $credit | expand | sed 's/^/             /' >> AUTHORS

cat >> AUTHORS <<'EOF'                            

       
    The manual for GNU Backgammon includes a chapter describing the rules of
      backgammon, written by Tom Keith for his Backgammon Galore web site
                             <http://www.bkgm.com/>.


  Library code from the following authors has been included in GNU Backgammon:

     Ulrich Drepper (an implementation of the public domain MD5 algorithm)
             Bob Jenkins (the ISAAC pseudo random number generator)
       Takuji Nishimura and Makoto Matsumoto (the Mersenne Twister PRNG)
                Gerry Tesauro (the "pubeval" position evaluator)


       Please send bug reports for GNU Backgammon to: <bug-gnubg@gnu.org>
EOF

cat > credits.c <<'EOF'
/* Do not modify this file!  It is created automatically by credits.sh.
   Modify credits.sh instead. */

char *aszCredits[] = {
EOF

cat $contributors $credit - <<'EOF' | sort -f -k 2 | uniq | \
    sed 's/.*/    "&",/' >> credits.c
Tom Keith
EOF

cat >> credits.c <<'EOF'
    0
};
EOF
