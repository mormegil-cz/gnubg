/*
 * pub_eval.c
 *
 * by Gerry Tesauro
 *
 * modified by Gary Wong for GNU Backgammon, 1999
 *
 * $Id$
 */

static float wr[122] = {
    .00000, -.17160, .27010, .29906, -.08471, .00000, -1.40375,
    -1.05121, .07217, -.01351, .00000, -1.29506, -2.16183, .13246,
    -1.03508, .00000, -2.29847, -2.34631, .17253, .08302, .00000,
    -1.27266, -2.87401, -.07456, -.34240, .00000, -1.34640, -2.46556,
    -.13022, -.01591, .00000, .27448, .60015, .48302, .25236, .00000,
    .39521, .68178, .05281, .09266, .00000, .24855, -.06844, -.37646,
    .05685, .00000, .17405, .00430, .74427, .00576, .00000, .12392,
    .31202, -.91035, -.16270, .00000, .01418, -.10839, -.02781, -.88035,
    .00000, 1.07274, 2.00366, 1.16242, .22520, .00000, .85631, 1.06349,
    1.49549, .18966, .00000, .37183, -.50352, -.14818, .12039, .00000,
    .13681, .13978, 1.11245, -.12707, .00000, -.22082, .20178, -.06285,
    -.52728, .00000, -.13597, -.19412, -.09308, -1.26062, .00000, 3.05454,
    5.16874, 1.50680, 5.35000, .00000, 2.19605, 3.85390, .88296, 2.30052,
    .00000, .92321, 1.08744, -.11696, -.78560, .00000, -.09795, -.83050,
    -1.09167, -4.94251, .00000, -1.00316, -3.66465, -2.56906, -9.67677,
    .00000, -2.77982, -7.26713, -3.40177, -12.32252, .00000, 3.42040
}, wc[122] = {
    .25696, -.66937, -1.66135, -2.02487, -2.53398, -.16092, -1.11725,
    -1.06654, -.92830, -1.99558, -1.10388, -.80802, .09856, -.62086,
    -1.27999, -.59220, -.73667, .89032, -.38933, -1.59847, -1.50197,
    -.60966, 1.56166, -.47389, -1.80390, -.83425, -.97741, -1.41371,
    .24500, .10970, -1.36476, -1.05572, 1.15420, .11069, -.38319, -.74816,
    -.59244, .81116, -.39511, .11424, -.73169, -.56074, 1.09792, .15977,
    .13786, -1.18435, -.43363, 1.06169, -.21329, .04798, -.94373, -.22982,
    1.22737, -.13099, -.06295, -.75882, -.13658, 1.78389, .30416, .36797,
    -.69851, .13003, 1.23070, .40868, -.21081, -.64073, .31061, 1.59554,
    .65718, .25429, -.80789, .08240, 1.78964, .54304, .41174, -1.06161,
    .07851, 2.01451, .49786, .91936, -.90750, .05941, 1.83120, .58722,
    1.28777, -.83711, -.33248, 2.64983, .52698, .82132, -.58897, -1.18223,
    3.35809, .62017, .57353, -.07276, -.36214, 4.37655, .45481, .21746,
    .10504, -.61977, 3.54001, .04612, -.18108, .63211, -.87046, 2.47673,
    -.48016, -1.27157, .86505, -1.11342, 1.24612, -.82385, -2.77082,
    1.23606, -1.59529, .10438, -1.30206, -4.11520, 5.62596, -2.75800
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
	    if(n>=4) x[5*jm1+4] = (float)(n-3)/2.0;
	}
    }
    /* encode opponent barmen */
    x[120] = -(float)(pos[0])/2.0;
    /* encode computer's menoff */
    x[121] = (float)(pos[26])/15.0;
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
    
    if(pos[26]==15) return(99999999.);
    /* all men off, best possible move */
    
    setx(pos); /* sets input array x[] */
    score = 0.0;
    if(race) /* use race weights */
	for(i=0;i<122;++i) score += wr[i]*x[i];
    else /* use contact weights */
	for(i=0;i<122;++i) score += wc[i]*x[i];

    return(score);
}
