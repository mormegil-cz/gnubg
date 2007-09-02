/*
 * pub_eval.c
 *
 * by Gerry Tesauro
 *
 * modified by Gary Wong for GNU Backgammon, 1999
 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#include "config.h"
#include "eval.h"
static float wr[122] = {
    .00000f, -.17160f, .27010f, .29906f, -.08471f, .00000f, -1.40375f,
    -1.05121f, .07217f, -.01351f, .00000f, -1.29506f, -2.16183f, .13246f,
    -1.03508f, .00000f, -2.29847f, -2.34631f, .17253f, .08302f, .00000f,
    -1.27266f, -2.87401f, -.07456f, -.34240f, .00000f, -1.34640f, -2.46556f,
    -.13022f, -.01591f, .00000f, .27448f, .60015f, .48302f, .25236f, .00000f,
    .39521f, .68178f, .05281f, .09266f, .00000f, .24855f, -.06844f, -.37646f,
    .05685f, .00000f, .17405f, .00430f, .74427f, .00576f, .00000f, .12392f,
    .31202f, -.91035f, -.16270f, .00000f, .01418f, -.10839f, -.02781f, -.88035f,
    .00000f, 1.07274f, 2.00366f, 1.16242f, .22520f, .00000f, .85631f, 1.06349f,
    1.49549f, .18966f, .00000f, .37183f, -.50352f, -.14818f, .12039f, .00000f,
    .13681f, .13978f, 1.11245f, -.12707f, .00000f, -.22082f, .20178f, -.06285f,
    -.52728f, .00000f, -.13597f, -.19412f, -.09308f, -1.26062f, .00000f, 3.05454f,
    5.16874f, 1.50680f, 5.35000f, .00000f, 2.19605f, 3.85390f, .88296f, 2.30052f,
    .00000f, .92321f, 1.08744f, -.11696f, -.78560f, .00000f, -.09795f, -.83050f,
    -1.09167f, -4.94251f, .00000f, -1.00316f, -3.66465f, -2.56906f, -9.67677f,
    .00000f, -2.77982f, -7.26713f, -3.40177f, -12.32252f, .00000f, 3.42040f
}, wc[122] = {
    .25696f, -.66937f, -1.66135f, -2.02487f, -2.53398f, -.16092f, -1.11725f,
    -1.06654f, -.92830f, -1.99558f, -1.10388f, -.80802f, .09856f, -.62086f,
    -1.27999f, -.59220f, -.73667f, .89032f, -.38933f, -1.59847f, -1.50197f,
    -.60966f, 1.56166f, -.47389f, -1.80390f, -.83425f, -.97741f, -1.41371f,
    .24500f, .10970f, -1.36476f, -1.05572f, 1.15420f, .11069f, -.38319f, -.74816f,
    -.59244f, .81116f, -.39511f, .11424f, -.73169f, -.56074f, 1.09792f, .15977f,
    .13786f, -1.18435f, -.43363f, 1.06169f, -.21329f, .04798f, -.94373f, -.22982f,
    1.22737f, -.13099f, -.06295f, -.75882f, -.13658f, 1.78389f, .30416f, .36797f,
    -.69851f, .13003f, 1.23070f, .40868f, -.21081f, -.64073f, .31061f, 1.59554f,
    .65718f, .25429f, -.80789f, .08240f, 1.78964f, .54304f, .41174f, -1.06161f,
    .07851f, 2.01451f, .49786f, .91936f, -.90750f, .05941f, 1.83120f, .58722f,
    1.28777f, -.83711f, -.33248f, 2.64983f, .52698f, .82132f, -.58897f, -1.18223f,
    3.35809f, .62017f, .57353f, -.07276f, -.36214f, 4.37655f, .45481f, .21746f,
    .10504f, -.61977f, 3.54001f, .04612f, -.18108f, .63211f, -.87046f, 2.47673f,
    -.48016f, -1.27157f, .86505f, -1.11342f, 1.24612f, -.82385f, -2.77082f,
    1.23606f, -1.59529f, .10438f, -1.30206f, -4.11520f, 5.62596f, -2.75800f
}, x[122];

static void setx( int pos[] ) {
    /* sets input vector x[] given board position pos[] */
    int j, jm1, n;
    
    /* initialize */
    for(j=0;j<122;++j) x[j] = 0.0;
    
    /* first encode board locations 24-1 */
    for(j=1;j<=24;++j) {
	jm1 = j - 1;
	n = pos[25-j];
	if(n!=0) {
	    if(n==-1) x[5*jm1+0] = 1.0;
	    if(n==1) x[5*jm1+1] = 1.0;
	    if(n>=2) x[5*jm1+2] = 1.0;
	    if(n==3) x[5*jm1+3] = 1.0;
	    if(n>=4) x[5*jm1+4] = (n-3)/2.0f;
	}
    }
    /* encode opponent barmen */
    x[120] = -(pos[0])/2.0f;
    /* encode computer's menoff */
    x[121] = (pos[26])/15.0f;
}

extern float pubeval( int race, int pos[] ) {
    /* Backgammon move-selection evaluation function
       for benchmark comparisons.  Computes a linear
       evaluation function:  Score = W * X, where X is
       an input vector encoding the board state (using
       a raw encoding of the number of men at each location),
       and W is a weight vector.  Separate weight vectors
       are used for racing positions and contact positions.
       Makes lots of obvious mistakes, but provides a
       decent level of play for benchmarking purposes. */
    
    /* Provided as a public service to the backgammon
       programming community by Gerry Tesauro, IBM Research.
       (e-mail: tesauro@watson.ibm.com)                     */
    
    /* The following inputs are needed for this routine:
       
       race   is an integer variable which should be set
       based on the INITIAL position BEFORE the move.
       Set race=1 if the position is a race (i.e. no contact)
       and 0 if the position is a contact position.
       
       pos[]  is an integer array of dimension 28 which
       should represent a legal final board state after
       the move. Elements 1-24 correspond to board locations
       1-24 from computer's point of view, i.e. computer's
       men move in the negative direction from 24 to 1, and
       opponent's men move in the positive direction from
       1 to 24. Computer's men are represented by positive
       integers, and opponent's men are represented by negative
       integers. Element 25 represents computer's men on the
       bar (positive integer), and element 0 represents opponent's
       men on the bar (negative integer). Element 26 represents
       computer's men off the board (positive integer), and
       element 27 represents opponent's men off the board
       (negative integer).                                  */
    
    int i;
    float score;
    
    if(pos[26]==15) return(99999999.f);
    /* all men off, best possible move */
    
    setx(pos); /* sets input array x[] */
    score = 0.0;
    if(race) /* use race weights */
	for(i=0;i<122;++i) score += wr[i]*x[i];
    else /* use contact weights */
	for(i=0;i<122;++i) score += wc[i]*x[i];

    return(score);
}
