#!/bin/sh
#
# credits.sh
#
# by Gary Wong <gtw@gnu.org>, 1998, 1999, 2000, 2001, 2002, 2003, 2004
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# $Id$
# 

column < /dev/null || exit 0

authors=/tmp/auth.$$
contributors=/tmp/cont.$$
support=/tmp/supp.$$
translations=/tmp/trans.$$
credit=/tmp/cred.$$
extra=/tmp/extra.$$

cat > $authors <<EOF
Joseph Heled
Oeystein Johansen
Johanthan Kinsey
David Montgomery
Jim Segrave
Joern Thyssen
Gary Wong
EOF

cat > $contributors <<EOF
Olivier Baur
Holger Bochnig
Nis Joergensen
TAKAHASHI Kaoru
Stein Kulseth
Mike Petch
Rod Roark
EOF

cat > $support <<EOF
Oeystein Johansen,Web Pages
Achim Mueller,Manual
Nardy Pillards,Web Pages
Albert Silver,Tutorial
EOF

cat > $translations <<EOF
Petr Kadlec,Czech
Joern Thyssen,Danish
Olivier Baur,French
Achim Mueller,German
Hlynur Sigurgislason,Icelandic
Renzo Campagna,Italian
Kaoru TAKAHASHI,Japanese
Akif Dinc,Turkish
EOF

cat > $credit <<EOF
Elias Kritikos
Misja Alma
Christian Anthon
Kazuaki Asai
Eric Augustine
Erik Barfoed
Ron Barry
Steve Baedke
Olivier Baur
Stefan van den Berg
Holger Bochnig
Chuck Bower
Adrian Bunk
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
Kennedy Fraser
Ric Gerace
Michel Grimminck
Eric Groleau
Jeff Haferman
Morten Juul Henriksen
Alain Henry
Jens Hoefkens
Martin Janke
Nis Jorgensen
Petr Kadlec
Neil Kazaross
Mathias Kegelmann
Matija Kejzar
Bert Van Kerckhove
James F. Kibler
Jonathan Kinsey
Johnny Kirk
Gerhard Knop
Robert Konigsberg
Martin Krainer
Corrin Lakeland
Tim Laursen
Eli Liang
Ege Lundgren
Kevin Maguire
Massimiliano Maini
Giulio De Marco
John Marttila
Alix Martin
William Maslen
Thomas Meyer
Achim Mueller
Daniel Murphy
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
Petri Pitkanen
Sam Pottle
Henrik Ravn
James Rech
Jared Riley
Klaus Rindholt
Oliver Riordan
Rod Roark
Hans-Juergen Schaefer
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
Kaoru TAKAHASHI
Yoshito Takeuchi
Jacques Thiriat
Malene Thyssen
Claes Tornberg
Sander van Rijnswou
Robert-Jan Veldhuizen
Morten Wang
Jeff White
JP White
Mike Whitton
Chris Wilson
Kit Woolsey
Frank Worrell
Christopher D. Yep
Anders Zachrison
Douglas Zare
EOF

cat > $extra <<EOF
Hans Berliner
Chuck Bower
Rick Janowski
Brian Sheppard
Gerry Tesauro
Morten Wang
Douglas Zare
Michael Zehr
EOF

# generate credits.c

cat > credits.h <<EOF
/* Do not modify this file!  It is created automatically by credits.sh.
   Modify credits.sh instead. */

#include "i18n.h"

typedef struct _credEntry {
	char* Name;
	char* Type;
} credEntry;

typedef struct _credits {
	char* Title;
	credEntry *Entry;
} credits;

extern credEntry ceAuthors[];
extern credEntry ceContrib[];
extern credEntry ceTranslations[];
extern credEntry ceSupport[];
extern credEntry ceCredits[];

extern credits creditList[];

extern char aszAUTHORS[];

EOF

cat > credits.c <<EOF
/* Do not modify this file!  It is created automatically by credits.sh.
   Modify credits.sh instead. */

#include "i18n.h"
#include "credits.h"

credEntry ceAuthors[] = {
EOF

# Authors

cat $authors | sed -e 's/.*/  {"&", 0},/g' >> credits.c

cat >> credits.c <<EOF
  {0, 0}
};
EOF

# Contributors

cat >> credits.c <<EOF

credEntry ceContrib[] = {
EOF

cat $contributors | sed -e 's/.*/  {"&", 0},/g' >> credits.c

cat >> credits.c <<EOF
  {0, 0}
};
EOF

# Support

cat >> credits.c <<EOF

credEntry ceSupport[] = {
EOF

cat $support | sed -e 's/^\(.*\),\(.*\)$/  {"\1", N_\("\2\"\) },/g' >> credits.c

cat >> credits.c <<EOF
  {0, 0}
};
EOF

# Translations

cat >> credits.c <<EOF

credEntry ceTranslations[] = {
EOF

cat $translations | sed -e 's/^\(.*\),\(.*\)$/  {"\1", N_\("\2\"\) },/g' >> credits.c

cat >> credits.c <<EOF
  {0, 0}
};
EOF

# Contributors

cat >> credits.c <<EOF

credEntry ceCredits[] = {
EOF

cat $credit $extra | sort -f -k 2 | uniq | sed -e 's/.*/  {"&", 0},/g' >> credits.c

cat >> credits.c <<EOF
  {0, 0}
};
EOF

# some C stuff

cat >> credits.c <<EOF

credits creditList[] =
{
	{N_("Developers"), ceAuthors},
	{N_("Code Contributors"), ceContrib},
	{N_("Translations"), ceTranslations},
	{N_("Support"), ceSupport},
	{0, 0}
};

EOF

#
# Generate AUTHORS
#

cat > AUTHORS <<EOF
                         GNU Backgammon was written by:

EOF

column -c 72 < $authors | expand | sed 's/^/    /' >> AUTHORS

cat >> AUTHORS <<EOF
 
                                   Support by:

EOF
cat $support | sed -e 's/^\(.*\),\(.*\)$/ \1 (\2)/g' | column -c 72 | expand | sed 's/^/    /' >> AUTHORS

cat >> AUTHORS <<EOF

                         Contributors of code include:

EOF

column -c 72 < $contributors | expand | sed 's/^/    /' >> AUTHORS

cat >> AUTHORS <<EOF

                            Translations by:

EOF

cat $translations | sed -e 's/^\(.*\),\(.*\)$/ \1 (\2)/g' | column -c 72 | expand | sed 's/^/    /' >> AUTHORS


cat >> AUTHORS <<EOF

             Contributors (of bug reports and suggestions) include:

EOF

column -c 72 < $credit | expand | sed 's/^/    /' >> AUTHORS

cat >> AUTHORS <<'EOF'


  Credit is also due to those who have published information about backgammon
   playing programs (references will appear here later).  GNU Backgammon has
                              borrowed ideas from:

EOF

column -c 72 < $extra | expand | sed 's/^/    /' >> AUTHORS

cat >> AUTHORS <<'EOF'                            

       
    The manual for GNU Backgammon includes a chapter describing the rules of
      backgammon, written by Tom Keith for his Backgammon Galore web site
                             <http://www.bkgm.com/>.


  Library code from the following authors has been included in GNU Backgammon:

     Ulrich Drepper (an implementation of the public domain MD5 algorithm)
             Bob Jenkins (the ISAAC pseudo random number generator)
       Takuji Nishimura and Makoto Matsumoto (the Mersenne Twister PRNG)
                Gerry Tesauro (the "pubeval" position evaluator)


      If you feel that you're not given credits for your contributions to
         GNU Backgammon please write to one of the developers.


       Please send bug reports for GNU Backgammon to: <bug-gnubg@gnu.org>
EOF

#
# Add AUTHORS to credits.c
# 

cat >> credits.c <<EOF
char aszAUTHORS[] =
EOF

sed -e 's/"/\\"/g' AUTHORS | sed -e 's/.*/"&\\n"/g' >> credits.c

cat >> credits.c <<EOF
;
EOF
