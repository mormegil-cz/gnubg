/*
* matchequity.c
* by Joern Thyssen, 1999-2002
*
* Calculate matchequity table based on 
* formulas from [insert the Managent Science ref here].
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of version 2 of the GNU General Public License as
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

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"
#include "list.h"

#if HAVE_LIBXML2
#include <libxml/tree.h>
#include <libxml/parser.h>
#endif

#if !HAVE_ERF
extern double erf( double x );
#endif

#define MAXCUBELEVEL  7
#define DELTA         0.08
#define DELTABAR      0.06
#define G1            0.25
#define G2            0.15
#define GAMMONRATE    0.25

#include "matchequity.h"

typedef struct _parameter {

  char *szName, *szValue;

} parameter;


typedef struct _metparameters {

  char *szName;
  list lParameters;

} metparameters;

typedef struct _metdata {

  /* Data saved */

  /* Pre crawford MET */

  float aarMET[ MAXSCORE ][ MAXSCORE ];
  metparameters mpPreCrawford;

  /* post-crawford MET */

  float arMETPostCrawford[ MAXSCORE ];
  metparameters mpPostCrawford;

  /* MET info */

  metinfo mi;

} metdata;

/*
* A1 (A2) is the match equity of player 1 (2)
* Btilde is the post-crawford match equities.
*/

float aafMET [ MAXSCORE ][ MAXSCORE ];
float afMETPostCrawford [ MAXSCORE ];

metinfo miCurrent;


/*
* Match equity table from Kit Woolsey: "How to Play Tournament
* Backgammon", GammonPress, 1993.
*
* Used with permission from Kit Woolsey. 
*/

float aafMETWoolsey[ 15 ][ 15 ] =
{ { 0.50, 0.70, 0.75, 0.83, 0.85, 0.90, 0.91, 0.94, 0.95, 0.97,
    0.97, 0.98, 0.98, 0.99, 0.99 }, 
  { 0.30, 0.50, 0.60, 0.68, 0.75, 0.81, 0.85, 0.88, 0.91, 0.93,
    0.94, 0.95, 0.96, 0.97, 0.98 }, 
  { 0.25, 0.40, 0.50, 0.59, 0.66, 0.71, 0.76, 0.80, 0.84, 0.87,
    0.90, 0.92, 0.94, 0.95, 0.96 }, 
  { 0.17, 0.32, 0.41, 0.50, 0.58, 0.64, 0.70, 0.75, 0.79, 0.83,
    0.86, 0.88, 0.90, 0.92, 0.93 }, 
  { 0.15, 0.25, 0.34, 0.42, 0.50, 0.57, 0.63, 0.68, 0.73, 0.77,
    0.81, 0.84, 0.87, 0.89, 0.90 }, 
  { 0.10, 0.19, 0.29, 0.36, 0.43, 0.50, 0.56, 0.62, 0.67, 0.72,
    0.76, 0.79, 0.82, 0.85, 0.87 }, 
  { 0.09, 0.15, 0.24, 0.30, 0.37, 0.44, 0.50, 0.56, 0.61, 0.66,
    0.70, 0.74, 0.78, 0.81, 0.84 }, 
  { 0.06, 0.12, 0.20, 0.25, 0.32, 0.38, 0.44, 0.50, 0.55, 0.60,
    0.65, 0.69, 0.73, 0.77, 0.80 }, 
  { 0.05, 0.09, 0.16, 0.21, 0.27, 0.33, 0.39, 0.45, 0.50, 0.55,
    0.60, 0.64, 0.68, 0.72, 0.76 }, 
  { 0.03, 0.07, 0.13, 0.17, 0.23, 0.28, 0.34, 0.40, 0.45, 0.50,
    0.55, 0.60, 0.64, 0.68, 0.71 }, 
  { 0.03, 0.06, 0.10, 0.14, 0.19, 0.24, 0.30, 0.35, 0.40, 0.45,
    0.50, 0.55, 0.59, 0.63, 0.67 }, 
  { 0.02, 0.05, 0.08, 0.12, 0.16, 0.21, 0.26, 0.31, 0.36, 0.40,
    0.45, 0.50, 0.54, 0.58, 0.62 }, 
  { 0.02, 0.04, 0.06, 0.10, 0.13, 0.18, 0.22, 0.27, 0.32, 0.36,
    0.41, 0.46, 0.50, 0.54, 0.58 }, 
  { 0.01, 0.03, 0.05, 0.08, 0.11, 0.15, 0.19, 0.23, 0.28, 0.32,
    0.37, 0.42, 0.46, 0.50, 0.54 }, 
  { 0.01, 0.02, 0.04, 0.07, 0.10, 0.13, 0.16, 0.20, 0.24, 0.29,
    0.33, 0.38, 0.42, 0.46, 0.50 } }; 

float aafMETJacobs[ 25 ][ 25 ] = 
{ { 0.500, 0.690, 0.748, 0.822, 0.844, 0.894, 0.909, 0.938, 0.946,
    0.963, 0.968, 0.978, 0.981, 0.987, 0.989, 0.992, 0.994, 0.996,
    0.996, 0.997, 0.998, 0.998, 0.999, 0.999, 0.999, } ,

  { 0.310, 0.500, 0.603, 0.686, 0.753, 0.810, 0.851, 0.886, 0.910,
    0.931, 0.946, 0.959, 0.967, 0.975, 0.980, 0.985, 0.988, 0.991,
    0.993, 0.995, 0.996, 0.997, 0.998, 0.998, 0.999, } , 

  { 0.252, 0.397, 0.500, 0.579, 0.654, 0.719, 0.769, 0.814, 0.848,
    0.879, 0.902, 0.922, 0.937, 0.950, 0.960, 0.969, 0.975, 0.981,
    0.984, 0.988, 0.990, 0.993, 0.994, 0.995, 0.996, } ,

  { 0.178, 0.314, 0.421, 0.500, 0.579, 0.647, 0.704, 0.753, 0.795,
    0.831, 0.861, 0.886, 0.907, 0.925, 0.939, 0.951, 0.960, 0.968,
    0.974, 0.979, 0.983, 0.987, 0.989, 0.992, 0.993, } ,

  { 0.156, 0.247, 0.346, 0.421, 0.500, 0.568, 0.629, 0.683, 0.730,
    0.773, 0.808, 0.840, 0.866, 0.889, 0.908, 0.924, 0.938, 0.949,
    0.958, 0.966, 0.972, 0.978, 0.982, 0.985, 0.988, } ,

  { 0.106, 0.190, 0.281, 0.353, 0.432, 0.500, 0.563, 0.620, 0.672,
    0.718, 0.759, 0.795, 0.826, 0.853, 0.876, 0.896, 0.913, 0.928,
    0.940, 0.950, 0.959, 0.966, 0.972, 0.977, 0.981, } ,

  { 0.091, 0.149, 0.231, 0.296, 0.371, 0.437, 0.500, 0.557, 0.611,
    0.660, 0.704, 0.744, 0.779, 0.811, 0.838, 0.862, 0.883, 0.901,
    0.917, 0.930, 0.941, 0.951, 0.959, 0.966, 0.972, } , 

  { 0.062, 0.114, 0.186, 0.247, 0.317, 0.380, 0.443, 0.500, 0.555,
    0.606, 0.653, 0.695, 0.734, 0.768, 0.799, 0.827, 0.851, 0.873,
    0.891, 0.907, 0.921, 0.934, 0.944, 0.953, 0.961, } ,

  { 0.054, 0.090, 0.152, 0.205, 0.270, 0.328, 0.389, 0.445, 0.500,
    0.551, 0.600, 0.644, 0.685, 0.723, 0.757, 0.788, 0.816, 0.840,
    0.862, 0.881, 0.898, 0.913, 0.926, 0.937, 0.946, } ,

  { 0.037, 0.069, 0.121, 0.169, 0.227, 0.282, 0.340, 0.394, 0.449,
    0.500, 0.549, 0.595, 0.638, 0.678, 0.715, 0.748, 0.779, 0.806,
    0.831, 0.853, 0.872, 0.890, 0.905, 0.918, 0.930, } ,

  { 0.032, 0.054, 0.098, 0.139, 0.192, 0.241, 0.296, 0.347, 0.400,
    0.451, 0.500, 0.546, 0.591, 0.632, 0.671, 0.706, 0.739, 0.769,
    0.797, 0.821, 0.844, 0.864, 0.881, 0.897, 0.911, } ,

  { 0.022, 0.041, 0.078, 0.114, 0.160, 0.205, 0.256, 0.305, 0.356,
    0.405, 0.454, 0.500, 0.545, 0.587, 0.627, 0.665, 0.700, 0.732,
    0.761, 0.789, 0.813, 0.835, 0.856, 0.874, 0.890, } ,

  { 0.019, 0.033, 0.063, 0.093, 0.134, 0.174, 0.221, 0.266, 0.315,
    0.362, 0.409, 0.455, 0.500, 0.543, 0.584, 0.622, 0.659, 0.693,
    0.725, 0.754, 0.781, 0.805, 0.828, 0.848, 0.866, } ,

  { 0.013, 0.025, 0.050, 0.075, 0.111, 0.147, 0.189, 0.232, 0.277,
    0.322, 0.368, 0.413, 0.457, 0.500, 0.541, 0.581, 0.618, 0.654,
    0.687, 0.718, 0.747, 0.774, 0.798, 0.820, 0.841, } ,

  { 0.011, 0.020, 0.040, 0.061, 0.092, 0.124, 0.162, 0.201, 0.243,
    0.285, 0.329, 0.373, 0.416, 0.459, 0.500, 0.540, 0.578, 0.614,
    0.649, 0.682, 0.712, 0.741, 0.767, 0.791, 0.814, } ,

  { 0.008, 0.015, 0.031, 0.049, 0.076, 0.104, 0.138, 0.173, 0.212,
    0.252, 0.294, 0.335, 0.378, 0.419, 0.460, 0.500, 0.539, 0.576,
    0.611, 0.645, 0.677, 0.707, 0.735, 0.761, 0.785, } ,

  { 0.006, 0.012, 0.025, 0.040, 0.062, 0.087, 0.117, 0.149, 0.184,
    0.221, 0.261, 0.300, 0.341, 0.382, 0.422, 0.461, 0.500, 0.537,
    0.573, 0.608, 0.641, 0.672, 0.702, 0.729, 0.755, } ,

  { 0.004, 0.009, 0.019, 0.032, 0.051, 0.072, 0.099, 0.127, 0.160,
    0.194, 0.231, 0.268, 0.307, 0.346, 0.386, 0.424, 0.463, 0.500,
    0.536, 0.571, 0.605, 0.637, 0.668, 0.697, 0.724, } ,

  { 0.004, 0.007, 0.016, 0.026, 0.042, 0.060, 0.083, 0.109, 0.138,
    0.169, 0.203, 0.239, 0.275, 0.313, 0.351, 0.389, 0.427, 0.464,
    0.500, 0.535, 0.570, 0.602, 0.634, 0.664, 0.692, } ,

  { 0.003, 0.005, 0.012, 0.021, 0.034, 0.050, 0.070, 0.093, 0.119,
    0.147, 0.179, 0.211, 0.246, 0.282, 0.318, 0.355, 0.392, 0.429,
    0.465, 0.500, 0.534, 0.568, 0.600, 0.631, 0.660, } ,

  { 0.002, 0.004, 0.010, 0.017, 0.028, 0.041, 0.059, 0.079, 0.102,
    0.128, 0.156, 0.187, 0.219, 0.253, 0.288, 0.323, 0.359, 0.395,
    0.430, 0.466, 0.500, 0.534, 0.566, 0.598, 0.628, } ,

  { 0.002, 0.003, 0.007, 0.013, 0.022, 0.034, 0.049, 0.066, 0.087,
    0.110, 0.136, 0.165, 0.195, 0.226, 0.259, 0.293, 0.328, 0.363,
    0.398, 0.432, 0.466, 0.500, 0.533, 0.565, 0.596, } ,

  { 0.001, 0.002, 0.006, 0.011, 0.018, 0.028, 0.041, 0.056, 0.074,
    0.095, 0.119, 0.144, 0.172, 0.202, 0.233, 0.265, 0.298, 0.332,
    0.366, 0.400, 0.434, 0.467, 0.500, 0.532, 0.563, } ,

  { 0.001, 0.002, 0.005, 0.008, 0.015, 0.023, 0.034, 0.047, 0.063,
    0.082, 0.103, 0.126, 0.152, 0.180, 0.209, 0.239, 0.271, 0.303,
    0.336, 0.369, 0.402, 0.435, 0.468, 0.500, 0.531, } ,

  { 0.001, 0.001, 0.004, 0.007, 0.012, 0.019, 0.028, 0.039, 0.054,
    0.070, 0.089, 0.110, 0.134, 0.159, 0.186, 0.215, 0.245, 0.276,
    0.308, 0.340, 0.372, 0.404, 0.437, 0.469, 0.500, }
};


float aafMETSnowie[ 15 ][ 15 ] = 
{
  {0.500, 0.685, 0.748, 0.819, 0.843, 0.891, 0.907, 0.935, 0.944,
   0.961, 0.967, 0.977, 0.980, 0.986, 0.988},

  {0.315, 0.500, 0.594, 0.664, 0.735, 0.791, 0.832, 0.866, 0.893,
   0.916, 0.932, 0.947, 0.957, 0.967, 0.973},

  {0.252, 0.406, 0.500, 0.572, 0.645, 0.707, 0.755, 0.797, 0.832,
   0.863, 0.887, 0.908, 0.924, 0.939, 0.950},

  {0.181, 0.336, 0.428, 0.500, 0.573, 0.637, 0.690, 0.736, 0.777,
   0.813, 0.843, 0.868, 0.890, 0.909, 0.924},

  {0.157, 0.265, 0.355, 0.427, 0.500, 0.565, 0.621, 0.671, 0.716,
   0.757, 0.792, 0.823, 0.849, 0.872, 0.892},

  {0.109, 0.209, 0.293, 0.363, 0.435, 0.500, 0.558, 0.610, 0.659,
   0.703, 0.742, 0.777, 0.807, 0.834, 0.857},

  {0.093, 0.168, 0.245, 0.310, 0.379, 0.442, 0.500, 0.552, 0.603,
   0.649, 0.691, 0.729, 0.762, 0.793, 0.820},

  {0.065, 0.134, 0.203, 0.264, 0.329, 0.390, 0.448, 0.500, 0.551,
   0.598, 0.642, 0.682, 0.718, 0.752, 0.781},

  {0.056, 0.107, 0.168, 0.223, 0.284, 0.341, 0.397, 0.449, 0.500,
   0.548, 0.593, 0.634, 0.673, 0.709, 0.741},

  {0.039, 0.084, 0.137, 0.187, 0.243, 0.297, 0.351, 0.402, 0.452,
   0.500, 0.546, 0.588, 0.628, 0.666, 0.700},

  {0.033, 0.068, 0.113, 0.157, 0.208, 0.258, 0.309, 0.358, 0.407,
   0.454, 0.500, 0.543, 0.584, 0.623, 0.659},

  {0.023, 0.053, 0.092, 0.132, 0.177, 0.223, 0.271, 0.318, 0.366,
   0.412, 0.457, 0.500, 0.542, 0.581, 0.618},

  {0.020, 0.043, 0.076, 0.110, 0.151, 0.193, 0.238, 0.282, 0.327,
   0.372, 0.416, 0.458, 0.500, 0.540, 0.578},

  {0.014, 0.033, 0.061, 0.091, 0.128, 0.166, 0.207, 0.248, 0.291,
   0.334, 0.377, 0.419, 0.460, 0.500, 0.538},

  {0.012, 0.027, 0.050, 0.076, 0.108, 0.143, 0.180, 0.219, 0.259,
   0.300, 0.341, 0.382, 0.422, 0.462, 0.500},
};



/*
 * Calculate area under normal distribution curve (with mean rMu and 
 * std.dev rSigma) from rMax to rMin.
 * 
 * Input:
 *   rMin, rMax: upper and lower integral limits
 *   rMu, rSigma: normal distribution parameters.
 *
 * Returns:
 *  area
 *
 */

static float
NormalDistArea ( float rMin, float rMax, float rMu, float rSigma ) {

  float rtMin, rtMax;
  float rInt1, rInt2;

  rtMin = ( rMin - rMu ) / rSigma;
  rtMax = ( rMax - rMu ) / rSigma;

  rInt1 = ( erf( rtMin / sqrt(2) ) + 1.0f ) / 2.0f;
  rInt2 = ( erf( rtMax / sqrt(2) ) + 1.0f ) / 2.0f;

  return rInt2 - rInt1;

}



/*
 * Calculate post-Crawford match equity table
 *
 * Formula taken from Zadeh, Management Science xx
 *
 * Input:
 *    rG: gammon rate for trailer
 *    rFD2: value of free drop at 1-away, 2-away
 *    rFD4: value of free drop at 1-away, 4-away
 *    iStart: score where met starts, e.g.
 *       iStart = 0 : 1-away, 1-away
 *       iStart = 15: 1-away, 16-away
 *    
 *   
 * Output:
 *    afMETPostCrawford: post-Crawford match equity table
 *
 */

void 
initPostCrawfordMET ( float afMETPostCrawford[ MAXSCORE ],
                      const int iStart,
                      const float rG,
                      const float rFD2, const float rFD4 ) {

  int i;

  /*
   * Calculate post-crawford match equities
   */

  for ( i = iStart; i < MAXSCORE; i++ ) {

    afMETPostCrawford[ i ] = 
      rG * 0.5 * 
      ( (i-4 >=0) ? afMETPostCrawford[ i-4 ] : 1.0 )
      + (1.0 - rG) * 0.5 * 
      ( (i-2 >=0) ? afMETPostCrawford[ i-2 ] : 1.0 );

    /*
     * add 1.5% at 1-away, 2-away for the free drop
     * add 0.4% at 1-away, 4-away for the free drop
     */

    if ( i == 1 )
      afMETPostCrawford[ i ] -= rFD2;

    if ( i == 3 )
      afMETPostCrawford[ i ] -= rFD4;

  }


}


/*
* Calculate match equity table from Zadeh's formula published
* in Management Science xx.xx,xx.
*
* Input:
*    rG1: gammon rate for leader of match
*    rG2: gammon rate for trailer of match
*    rDelta, rDeltaBar: parameters in the Zadeh model
*      (something to do with cube efficiencies)
*    rG3: post-Crawford gammon rate for trailer of match
*    afMETPostCrawford: post-Crawford match equity table
*
* Output:
*    aafMET: match equity table
*
*/

void
initMETZadeh ( float aafMET[ MAXSCORE ][ MAXSCORE ], 
               const float afMETPostCrawford[ MAXSCORE ],
               const float rG1, const float rG2, 
               const float rDelta, const float rDeltaBar ) {

  int i,j,k;
  int nCube;
  int nCubeValue, nCubePrimeValue;

/*
* D1bar (D2bar) is the equity of player 1 (2) at the drop point
* of player 2 (1) assuming dead cubes. 
* D1 (D2) is the equity of player 1 (2) at the drop point
* of player 2 (1) assuming semiefficient recubes.
*/

  float aaafD1 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  float aaafD2 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  float aaafD1bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  float aaafD2bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];


  /*
* Calculate 1-away,n-away match equities
*/

  for ( i = 0; i < MAXSCORE; i++ ) {

    aafMET[ i ][ 0 ] = 
      rG2 * 0.5 *
      ( (i-2 >=0) ? afMETPostCrawford[ i-2 ] : 1.0 )
      + (1.0 - rG2) * 0.5 * 
      ( (i-1 >=0) ? afMETPostCrawford[ i-1 ] : 1.0 );
    aafMET[ 0 ][ i ] = 1.0 - aafMET[ i ][ 0 ];

  }

  for ( i = 0; i < MAXSCORE ; i++ ) {

    for ( j = 0; j <= i ; j++ ) {

      for ( nCube = MAXCUBELEVEL-1; nCube >=0 ; nCube-- ) {

        nCubeValue = 1;
        for ( k = 0; k < nCube ; k++ )
          nCubeValue *= 2;

        /* Calculate D1bar */

        nCubePrimeValue = GetCubePrimeValue ( i, j, nCubeValue );

        aaafD1bar[ i ][ j ][ nCube ] = 
          ( GET_MET ( i - nCubeValue, j, aafMET )
            - rG2 * GET_MET ( i, j - 4 * nCubePrimeValue, aafMET )
            - (1.0-rG2) * GET_MET ( i, j - 2 * nCubePrimeValue, aafMET ) )
          /
          ( rG1 * GET_MET( i - 4 * nCubePrimeValue, j, aafMET )
            + (1.0-rG1) * GET_MET ( i - 2 * nCubePrimeValue, j, aafMET )
            - rG2 * GET_MET ( i, j - 4 * nCubePrimeValue, aafMET )
            - (1.0-rG2) * GET_MET ( i, j - 2 * nCubePrimeValue, aafMET ) );



        if ( i != j ) {

          nCubePrimeValue = GetCubePrimeValue ( j, i, nCubeValue );

          aaafD1bar[ j ][ i ][ nCube ] = 
            ( GET_MET ( j - nCubeValue, i, aafMET )
              - rG2 * GET_MET ( j, i - 4 * nCubePrimeValue, aafMET )
              - (1.0-rG2) * GET_MET ( j, i - 2 * nCubePrimeValue, aafMET ) )
            /
            ( rG1 * GET_MET( j - 4 * nCubePrimeValue, i, aafMET )
              + (1.0-rG1) * GET_MET ( j - 2 * nCubePrimeValue, i, aafMET )
              - rG2 * GET_MET ( j, i - 4 * nCubePrimeValue, aafMET )
              - (1.0-rG2) * GET_MET ( j, i - 2 * nCubePrimeValue, aafMET ) );


  
        }

        /* Calculate D2bar */

        nCubePrimeValue = GetCubePrimeValue ( j, i, nCubeValue );

        aaafD2bar[ i ][ j ][ nCube ] = 
          ( GET_MET ( j - nCubeValue, i, aafMET )
            - rG2 * GET_MET ( j, i - 4 * nCubePrimeValue, aafMET )
            - (1.0-rG2) * GET_MET ( j, i - 2 * nCubePrimeValue, aafMET ) )
          /
          ( rG1 * GET_MET( j - 4 * nCubePrimeValue, i, aafMET )
            + (1.0-rG1) * GET_MET ( j - 2 * nCubePrimeValue, i, aafMET )
            - rG2 * GET_MET ( j, i - 4 * nCubePrimeValue, aafMET )
            - (1.0-rG2) * GET_MET ( j, i - 2 * nCubePrimeValue, aafMET ) );


        if ( i != j ) {

          nCubePrimeValue = GetCubePrimeValue ( i, j, nCubeValue );
  
          aaafD2bar[ j ][ i ][ nCube ] = 
            ( GET_MET ( i - nCubeValue, j, aafMET )
              - rG2 * GET_MET ( i, j - 4 * nCubePrimeValue, aafMET )
              - (1.0-rG2) * GET_MET ( i, j - 2 * nCubePrimeValue, aafMET ) )
            /
            ( rG1 * GET_MET( i - 4 * nCubePrimeValue, j, aafMET )
              + (1.0-rG1) * GET_MET ( i - 2 * nCubePrimeValue, j, aafMET )
              - rG2 * GET_MET ( i, j - 4 * nCubePrimeValue, aafMET )
              - (1.0-rG2) * GET_MET ( i, j - 2 * nCubePrimeValue, aafMET ) );
  


        }

        /* Calculate D1 */

        if ( ( i < 2 * nCubeValue ) || ( j < 2 * nCubeValue ) ) {

          aaafD1[ i ][ j ][ nCube ] = aaafD1bar[ i ][ j ][ nCube ];
          if ( i != j )
            aaafD1[ j ][ i ][ nCube ] = aaafD1bar[ j ][ i ][ nCube ];

        } 
        else {
  
          aaafD1[ i ][ j ][ nCube ] = 
            1.0 + 
            ( aaafD2[ i ][ j ][ nCube+1 ] + rDelta )
            * ( aaafD1bar[ i ][ j ][ nCube ] - 1.0 );
          if ( i != j )
            aaafD1[ j ][ i ][ nCube ] = 
              1.0 + 
              ( aaafD2[ j ][ i ][ nCube+1 ] + rDelta )
              * ( aaafD1bar[ j ][ i ][ nCube ] - 1.0 );
  
        }


        /* Calculate D2 */

        if ( ( i < 2 * nCubeValue ) || ( j < 2 * nCubeValue ) ) {

          aaafD2[ i ][ j ][ nCube ] = aaafD2bar[ i ][ j ][ nCube ];
          if ( i != j )
            aaafD2[ j ][ i ][ nCube ] = aaafD2bar[ j ][ i ][ nCube ];

        } 
        else {
  
          aaafD2[ i ][ j ][ nCube ] = 
            1.0 + 
            ( aaafD1[ i ][ j ][ nCube+1 ] + rDelta )
            * ( aaafD2bar[ i ][ j ][ nCube ] - 1.0 );

          if ( i != j ) 
            aaafD2[ j ][ i ][ nCube ] = 
              1.0 + 
              ( aaafD1[ j ][ i ][ nCube+1 ] + rDelta )
              * ( aaafD2bar[ j ][ i ][ nCube ] - 1.0 );
        }


        /* Calculate A1 & A2 */

        if ( (nCube == 0) && ( i > 0 ) && ( j > 0 ) ) {

          aafMET[ i ][ j ] = 
            ( 
             ( aaafD2[ i ][ j ][ 0 ] + rDeltaBar - 0.5 )
             * GET_MET( i - 1, j, aafMET ) 
             + ( aaafD1[ i ][ j ][ 0 ] + rDeltaBar - 0.5 )
             * GET_MET( i, j - 1, aafMET ) )
            /
            (
             aaafD1[ i ][ j ][ 0 ] + rDeltaBar +
             aaafD2[ i ][ j ][ 0 ] + rDeltaBar - 1.0 );


          if ( i != j )
            aafMET[ j ][ i ] = 1.0 - aafMET[ i ][ j ];

        }

      }

    }

  }

}


int 
GetCubePrimeValue ( int i, int j, int nCubeValue ) {

  if ( (i < 2 * nCubeValue) && (j >= 2 * nCubeValue ) )

    /* automatic double */

    return 2*nCubeValue;
  else
    return nCubeValue;

}


extern int
GetPoints ( float arOutput [ 5 ], cubeinfo *pci, float arCP[ 2 ] ) {

  /*
   * Input:
   * - arOutput: we need the gammon and backgammon ratios
   *   (we assume arOutput is evaluate for pci -> fMove)
   * - anScore: the current score.
   * - nMatchTo: matchlength
   * - pci: value of cube, who's turn is it
   * 
   *
   * Output:
   * - arCP : cash points with live cube
   * These points are necesary for the linear
   * interpolation used in cubeless -> cubeful equity 
   * transformation.
   */

  /* Match play */

  /* normalize score */

  int i = pci->nMatchTo - pci->anScore[ 0 ] - 1;
  int j = pci->nMatchTo - pci->anScore[ 1 ] - 1;



  int nCube = pci -> nCube;

  float arCPLive [ 2 ][ MAXCUBELEVEL ];
  float arCPDead [ 2 ][ MAXCUBELEVEL ];
  float rG0, rG1, rBG0, rBG1;

  int nDead, n, nMax, nCubeValue, nCubePrimeValue;

  /* Gammon and backgammon ratio's. 
     Avoid division by zero in extreme cases. */

  if ( ! pci -> fMove ) {

    /* arOutput evaluated for player 0 */

    if ( arOutput[ OUTPUT_WIN ] > 0.0 ) {
      rG0 = ( arOutput[ OUTPUT_WINGAMMON ] -
              arOutput[ OUTPUT_WINBACKGAMMON ] ) /
        arOutput[ OUTPUT_WIN ];
      rBG0 = arOutput[ OUTPUT_WINBACKGAMMON ] /
        arOutput[ OUTPUT_WIN ];
    }
    else {
      rG0 = 0.0;
      rBG0 = 0.0;
    }

    if ( arOutput[ OUTPUT_WIN ] < 1.0 ) {
      rG1 = ( arOutput[ OUTPUT_LOSEGAMMON ] -
              arOutput[ OUTPUT_LOSEBACKGAMMON ] ) /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
      rBG1 = arOutput[ OUTPUT_LOSEBACKGAMMON ] /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
    }
    else {
      rG1 = 0.0;
      rBG1 = 0.0;
    }

  }
  else {

    /* arOutput evaluated for player 1 */

    if ( arOutput[ OUTPUT_WIN ] > 0.0 ) {
      rG1 = ( arOutput[ OUTPUT_WINGAMMON ] -
              arOutput[ OUTPUT_WINBACKGAMMON ] ) /
        arOutput[ OUTPUT_WIN ];
      rBG1 = arOutput[ OUTPUT_WINBACKGAMMON ] /
        arOutput[ OUTPUT_WIN ];
    }
    else {
      rG1 = 0.0;
      rBG1 = 0.0;
    }

    if ( arOutput[ OUTPUT_WIN ] < 1.0 ) {
      rG0 = ( arOutput[ OUTPUT_LOSEGAMMON ] -
              arOutput[ OUTPUT_LOSEBACKGAMMON ] ) /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
      rBG0 = arOutput[ OUTPUT_LOSEBACKGAMMON ] /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
    }
    else {
      rG0 = 0.0;
      rBG0 = 0.0;
    }
  }

  /* Find out what value the cube has when you or your
     opponent give a dead cube. */

  nDead = nCube;
  nMax = 0;

  while ( ( i >= 2 * nDead ) && ( j >= 2 * nDead ) ) {
    nMax ++;
    nDead *= 2;
  }

  for ( nCubeValue = nDead, n = nMax; 
        nCubeValue >= nCube; 
        nCubeValue >>= 1, n-- ) {

    /* Calculate dead and live cube cash points.
       See notes by me (Joern Thyssen) available from the
       'doc' directory.  (FIXME: write notes :-) ) */

    /* Even though it's a dead cube we take account of the opponents
       automatic redouble. */

    nCubePrimeValue = GetCubePrimeValue ( i, j, nCubeValue );

    /* Dead cube cash point for player 0 */

    arCPDead[ 0 ][ n ] = 
      ( ( 1.0 - rG1 - rBG1 ) * 
        GET_MET ( i, j - 2 * nCubePrimeValue, aafMET ) 
        + rG1 * GET_MET ( i, j - 4 * nCubePrimeValue, aafMET ) 
        + rBG1 * GET_MET ( i, j - 6 * nCubePrimeValue, aafMET ) 
        - GET_MET ( i - nCubeValue, j, aafMET ) )
      /
      ( ( 1.0 - rG1 - rBG1 ) * 
        GET_MET ( i, j - 2 * nCubePrimeValue, aafMET ) 
        + rG1 * GET_MET ( i, j - 4 * nCubePrimeValue, aafMET ) 
        + rBG1 * GET_MET ( i, j - 6 * nCubePrimeValue, aafMET ) 
        - ( 1.0 - rG0 - rBG0 ) *
        GET_MET( i - 2 * nCubePrimeValue, j, aafMET )
        - rG0 * GET_MET ( i - 4 * nCubePrimeValue, j, aafMET )
        - rBG0 * GET_MET ( i - 6 * nCubePrimeValue, j, aafMET ) );

    /* Dead cube cash point for player 1 */

    nCubePrimeValue = GetCubePrimeValue ( j, i, nCubeValue );

    arCPDead[ 1 ][ n ] = 
      ( ( 1.0 - rG0 - rBG0 ) * 
        GET_MET ( j, i - 2 * nCubePrimeValue, aafMET ) 
        + rG0 * GET_MET ( j, i - 4 * nCubePrimeValue, aafMET ) 
        + rBG0 * GET_MET ( j, i - 6 * nCubePrimeValue, aafMET ) 
        - GET_MET ( j - nCubeValue, i, aafMET ) )
      /
      ( ( 1.0 - rG0 - rBG0 ) * 
        GET_MET ( j, i - 2 * nCubePrimeValue, aafMET ) 
        + rG0 * GET_MET ( j, i - 4 * nCubePrimeValue, aafMET ) 
        + rBG0 * GET_MET ( j, i - 6 * nCubePrimeValue, aafMET ) 
        - ( 1.0 - rG1 - rBG1 ) *
        GET_MET( j - 2 * nCubePrimeValue, i, aafMET )
        - rG1 * GET_MET ( j - 4 * nCubePrimeValue, i, aafMET )
        - rBG1 * GET_MET ( j - 6 * nCubePrimeValue, i, aafMET ) );

    /* Live cube cash point for player 0 and 1 */

    if ( ( i < 2 * nCubeValue ) || ( j < 2 * nCubeValue ) ) {

      /* The doubled cube is going to be dead */

      arCPLive[ 0 ][ n ] = arCPDead[ 0 ][ n ];
      arCPLive[ 1 ][ n ] = arCPDead[ 1 ][ n ];

    } 
    else {

      /* Doubled cube is alive */

      arCPLive[ 0 ][ n ] = 1.0 - arCPLive[ 1 ][ n + 1] *
        ( GET_MET ( i - nCubeValue, j, aafMET )   
          - ( 1.0 - rG0 - rBG0 ) *
          GET_MET( i - 2 * nCubeValue, j, aafMET )
          - rG0 * GET_MET ( i - 4 * nCubeValue, j, aafMET )
          - rBG0 * GET_MET ( i - 6 * nCubeValue, j, aafMET ) )
        /
        ( GET_MET ( i, j - 2 * nCubeValue, aafMET )
          - ( 1.0 - rG0 - rBG0 ) *
          GET_MET( i - 2 * nCubeValue, j, aafMET )
          - rG0 * GET_MET ( i - 4 * nCubeValue, j, aafMET )
          - rBG0 * GET_MET ( i - 6 * nCubeValue, j, aafMET ) );

      arCPLive[ 1 ][ n ] = 1.0 - arCPLive[ 0 ][ n + 1 ] *
        ( GET_MET ( j - nCubeValue, i, aafMET )   
          - ( 1.0 - rG1 - rBG1 ) *
          GET_MET( j - 2 * nCubeValue, i, aafMET )
          - rG1 * GET_MET ( j - 4 * nCubeValue, i, aafMET )
          - rBG1 * GET_MET ( j - 6 * nCubeValue, i, aafMET ) )
        /
        ( GET_MET ( j, i - 2 * nCubeValue, aafMET )
          - ( 1.0 - rG1 - rBG1 ) *
          GET_MET( j - 2 * nCubeValue, i, aafMET )
          - rG1 * GET_MET ( j - 4 * nCubeValue, i, aafMET )
          - rBG1 * GET_MET ( j - 6 * nCubeValue, i, aafMET ) );

    }

  }

  /* return cash point for current cube level */

  arCP[ 0 ] = arCPLive[ 0 ][ 0 ];
  arCP[ 1 ] = arCPLive[ 1 ][ 0 ];

  /* debug output 
     for ( n = nMax; n >= 0; n-- ) {

     printf ("Cube %i\n"
     "Dead cube:    cash point 0 %6.3f\n"
     "              cash point 1 %6.3f\n"
     "Live cube:    cash point 0 %6.3f\n"
     "              cash point 1 %6.3f\n\n", 
     n,
     arCPDead[ 0 ][ n ], arCPDead[ 1 ][ n ],
     arCPLive[ 0 ][ n ], arCPLive[ 1 ][ n ] );

     }
  */

  return 0;

}



extern float
GetDoublePointDeadCube ( float arOutput [ 5 ], cubeinfo *pci ) {
  /*
   * Calculate double point for dead cubes
   */

  if ( ! pci->nMatchTo ) {

    /* Use Rick Janowski's formulas
       [insert proper reference here] */

    float rW, rL;

    if ( arOutput[ 0 ] > 0.0 )
      rW = 1.0 + ( arOutput[ 1 ] + arOutput[ 2 ] ) / arOutput[ 0 ];
    else
      rW = 1.0;

    if ( arOutput[ 0 ] < 1.0 )
      rL = 1.0 + ( arOutput[ 3 ] + arOutput[ 4 ] ) /
        ( 1.0 - arOutput [ 0 ] );
    else
      rL = 1.0;

    if ( pci->fCubeOwner == -1 && pci->fJacoby) {

      /* centered cube */

      if ( pci->fBeavers ) {
        return ( rL - 0.25 ) / ( rW + rL - 0.5 );
      }
      else {
        return ( rL - 0.5 ) / ( rW + rL - 1.0 );
      }
    }
    else {

      /* redoubles or Jacoby rule not in effect */

      return rL / ( rL + rW );
    }

  }
  else {

    /* Match play */

    /* normalize score */

    int i = pci->nMatchTo - pci->anScore[ pci->fMove ] - 1;
    int j = pci->nMatchTo - pci->anScore[ ! pci->fMove ] - 1;
    int nCube = pci->nCube;

    float rG1, rBG1, rG2, rBG2, rDTW, rNDW, rDTL, rNDL;
    float rRisk, rGain;

    /* FIXME: avoid division by zero */
    if ( arOutput[ OUTPUT_WIN ] > 0.0 ) {
      rG1 = ( arOutput[ OUTPUT_WINGAMMON ] -
              arOutput[ OUTPUT_WINBACKGAMMON ] ) /
        arOutput[ OUTPUT_WIN ];
      rBG1 = arOutput[ OUTPUT_WINBACKGAMMON ] /
        arOutput[ OUTPUT_WIN ];
    }
    else {
      rG1 = 0.0;
      rBG1 = 0.0;
    }

    if ( arOutput[ OUTPUT_WIN ] < 1.0 ) {
      rG2 = ( arOutput[ OUTPUT_LOSEGAMMON ] -
              arOutput[ OUTPUT_LOSEBACKGAMMON ] ) /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
      rBG2 = arOutput[ OUTPUT_LOSEBACKGAMMON ] /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
    }
    else {
      rG2 = 0.0;
      rBG2 = 0.0;
    }

    /* double point */

    /* FIXME: use the BG ratio */
    /* match equity for double, take; win */
    rDTW = (1.0 - rG1 - rBG1) * GET_MET ( i - 2 * nCube, j, aafMET ) +
      rG1 * GET_MET ( i - 4 * nCube, j, aafMET );
    rBG1 * GET_MET ( i - 6 * nCube, j, aafMET );

    /* match equity for no double; win */
    rNDW = (1.0 - rG1 - rBG1) * GET_MET ( i - nCube, j, aafMET ) +
      rG1 * GET_MET ( i - 2 * nCube, j, aafMET );
    rBG1 * GET_MET ( i - 3 * nCube, j, aafMET );

    /* match equity for double, take; loose */
    rDTL = (1.0 - rG2 - rBG2) * GET_MET ( i, j - 2 * nCube, aafMET ) +
      rG2 * GET_MET ( i, j - 4 * nCube, aafMET );
    rBG2 * GET_MET ( i, j - 6 * nCube, aafMET );

    /* match equity for double, take; loose */
    rNDL = (1.0 - rG2 - rBG2) * GET_MET ( i, j - nCube, aafMET ) +
      rG2 * GET_MET ( i, j - 2 * nCube, aafMET );
    rBG2 * GET_MET ( i, j - 3 * nCube, aafMET );

    /* risk & gain */

    rRisk = rNDL - rDTL;
    rGain = rDTW - rNDW;

    return rRisk / ( rRisk +  rGain );

  }

}



/* 
 * Extend match equity table to MAXSCORE using
 * David Montgomery's extrapolation algorithm.
 *
 * Input:
 *    nMaxScore: the length of the native met
 *    aarMET: the match equity table for scores below nMaxScore
 *
 * Output:
 *    aarMET: the match equity table for scores up to MAXSCORE.
 *
 */

static void
ExtendMET ( float aarMET[ MAXSCORE ][ MAXSCORE ],
            const int nMaxScore ) {

  static const float arStddevTable[] =
  { 0, 1.24, 1.27, 1.47, 1.50, 1.60, 1.61, 1.66, 1.68, 1.70, 1.72, 1.77 };

  float rStddev0, rStddev1, rGames, rSigma;
  int i,j;
  int nScore0, nScore1;

/* Extend match equity table */

  for ( i = nMaxScore; i < MAXSCORE; i++ ) {

    for ( j = 0; j <= i ; j++ ) {

      nScore0 = i + 1;
      nScore1 = j + 1;

      rGames = ( nScore0 + nScore1 ) / 2.00f;

      if ( nScore0 > 10 ) 
        rStddev0 = 1.77;
      else
        rStddev0 = arStddevTable[ nScore0 ];

      if ( nScore1 > 10 ) 
        rStddev1 = 1.77;
      else
        rStddev1 = arStddevTable[ nScore1 ];

      rSigma =
        sqrt ( rStddev0 * rStddev0 + rStddev1 * rStddev1 )
        * sqrt ( rGames );

      assert ( 6.0f * rSigma > nScore1 - nScore0 );

      aafMET[ i ][ j ] =
        1.0 - NormalDistArea ( nScore1 - nScore0, 6.0f * rSigma,
                               0.0f, rSigma );

    }
  }

  /* Generate j > i part of MET */

  for ( i = 0; i < MAXSCORE; i++ )
    for ( j = ( (i < nMaxScore) ? nMaxScore : i + 1);
          j < MAXSCORE; j++ )
      aafMET[ i ][ j ] = 1.0 - aafMET[ j ][ i ];

}


static void
initMP ( metparameters *pmp ) {

  pmp->szName = NULL;
  ListCreate ( &pmp->lParameters );

}


static void 
initMD ( metdata *pmd ) {

  pmd->mi.szName = NULL;
  pmd->mi.szFileName = NULL;
  pmd->mi.szDescription = NULL;

  initMP ( &pmd->mpPreCrawford );
  initMP ( &pmd->mpPostCrawford );

}


extern void
freeP ( parameter *pp ) {

  if ( pp->szName )
    free ( pp->szName );
  if ( pp->szValue )
    free ( pp->szValue );
  free ( pp );

}


extern void
freeMP ( metparameters *pmp ) {

  list *pl;

  if ( pmp->szName )
    free ( pmp->szName );

  pl = &pmp->lParameters;

  while( pl->plNext != pl ) {

    freeP ( pl->plNext->p );
    ListDelete( pl->plNext );

  }

}


extern void
getDefaultMET ( metdata *pmd ) {

  int i;
  parameter *pp;

  static parameter apPreCrawford[] = 
  { { "gammon-rate-leader", "0.25" },
    { "gammon-rate-trailer", "0.15" },
    { "delta", "0.08" },
    { "deltebar", "0.06" } };

  static parameter apPostCrawford[] = 
  { { "gammon-rate-trailer", "0.25" },
    { "free-drop-2-away", "0.015" },
    { "free-drop-4-away", "0.004" } };

  /* Setup default met */

  initMD ( pmd );

  for ( i = 0; i < 4; i++ ) {
    pp = (parameter *) malloc ( sizeof ( parameter ) );
    pp->szName = strdup ( apPreCrawford[ i ].szName );
    pp->szValue = strdup ( apPreCrawford[ i ].szValue );
    ListInsert ( &pmd->mpPreCrawford.lParameters, pp );
  }

  for ( i = 0; i < 3; i++ ) {
    pp = (parameter *) malloc ( sizeof ( parameter ) );
    pp->szName = strdup ( apPostCrawford[ i ].szName );
    pp->szValue = strdup ( apPostCrawford[ i ].szValue );
    ListInsert ( &pmd->mpPostCrawford.lParameters, pp );
  }

  pmd->mpPreCrawford.szName = strdup ( "zadeh" );
  pmd->mpPostCrawford.szName = strdup ( "zadeh" );

  pmd->mi.szName = strdup ( "N. Zadeh, Management Science 23, 986 (1977)" );
  pmd->mi.szFileName = strdup ( "met/zadeh.xml" );
  pmd->mi.szDescription = strdup ( "" );
  pmd->mi.nLength = MAXSCORE;

}


static int
initMETFromParameters ( float aafMET [ MAXSCORE ][ MAXSCORE ],
                        float afMETPostCrawford[ MAXSCORE ],
                        int nLength,
                        metparameters *pmp ) {

  if ( ! strcmp ( pmp->szName, "zadeh" ) ) {
    
    float rG1 = 0.25;
    float rG2 = 0.15;
    float rDelta = 0.08;
    float rDeltaBar = 0.06;
    list *pl; 
    parameter *pp;

    /* 
     * Use Zadeh's formulae 
     */

    /* obtain parameters */

    for ( pl = pmp->lParameters.plNext; pl != &pmp->lParameters; 
          pl = pl->plNext ) {

      pp = pl->p;

      if ( ! strcmp ( pp->szName, "gammon-rate-leader" ) )
        rG1 = atof ( pp->szValue );
      else if ( ! strcmp ( pp->szName, "gammon-rate-trailer" ) )
        rG2 = atof ( pp->szValue );
      else if ( ! strcmp ( pp->szName, "delta" ) )
        rDelta = atof ( pp->szValue );
      else if ( ! strcmp ( pp->szName, "delta-bar" ) )
        rDeltaBar = atof ( pp->szValue );

    }
    

    /* calculate table */

    initMETZadeh ( aafMET, afMETPostCrawford, rG1, rG2, rDelta, rDeltaBar );
    return 0;

  }
  else {

    /* unknown type */

    return -1;

  }    
  

}


static int
initPostCrawfordMETFromParameters ( float afMETPostCrawford[ MAXSCORE ],
                                    int nLength,
                                    metparameters *pmp ) {

  if ( ! strcmp ( pmp->szName, "zadeh" ) ) {
    
    float rG = 0.25;
    float rFD2 = 0.015;
    float rFD4 = 0.004;
    list *pl;
    parameter *pp;

    /* 
     * Use Zadeh's formulae 
     */

    /* obtain parameters */

    for ( pl = pmp->lParameters.plNext; pl != &pmp->lParameters; 
          pl = pl->plNext ) {

      pp = pl->p;

      if ( ! strcmp ( pp->szName, "gammon-rate-trailer" ) )
        rG = atof ( pp->szValue );
      else if ( ! strcmp ( pp->szName, "free-drop-2-away" ) )
        rFD2 = atof ( pp->szValue );
      else if ( ! strcmp ( pp->szName, "free-drop-4-away" ) )
        rFD4 = atof ( pp->szValue );

    }

    /* calculate table */

    initPostCrawfordMET ( afMETPostCrawford, 0, rG, rFD2, rFD4 );
    return 0;

  }
  else {

    /* unknown type */

    return -1;

  }    
  

}


extern void
InitMatchEquity ( const char *szFileName ) {

  int i,j;

  metdata md;
  int rc;

  static int fTableLoaded = FALSE;

#ifdef HAVE_LIBXML2

  /*
   * Read match equity table from XML file
   */

  if ( readMET ( &md, szFileName ) ) {

    if ( ! fTableLoaded ) {

      /* must read met */

      getDefaultMET ( &md );

    }
    else {

      fprintf ( stderr, "Error reading MET\n" );
      return;

    }
    
  }

  fTableLoaded = TRUE;

#else

  printf ( "Your version of GNU Backgammon does not support\n"
           "reading match equity files.\n"
           "Fallback to default Zadeh table.\n" );

  getDefaultMET ( &md );

#endif

  /*
   * Copy met to current met
   * Extend met (if needed)
   */

  /* post-Crawford table */

  if ( ! strcmp ( md.mpPostCrawford.szName, "explicit" ) ) {

    /* copy and extend table */

    /* FIXME: implement better extension of post-Crawford table */

    for ( i = 0; i < md.mi.nLength; i++ )
      afMETPostCrawford[ i ] = md.arMETPostCrawford[ i ];

    initPostCrawfordMET ( afMETPostCrawford, md.mi.nLength, 
                          GAMMONRATE, 0.015, 0.004 );

  }
  else {

    /* generate match equity table using Zadeh's formula */

    if ( initPostCrawfordMETFromParameters ( afMETPostCrawford, 
                                             md.mi.nLength,
                                             &md.mpPostCrawford ) < 0 ) {

      fprintf ( stderr, "Error generating post-Crawford MET\n" );
      return;

    }

  }

  /* pre-Crawford table */

  if ( ! strcmp ( md.mpPreCrawford.szName, "explicit" ) ) {

    /* copy table */

    for ( i = 0; i < md.mi.nLength; i++ )
      for ( j = 0; j < md.mi.nLength; j++ )
        aafMET[ i ][ j ] = md.aarMET[ i ][ j ];

  }
  else {

    /* generate matc equity table using Zadeh's formula */

    if ( initMETFromParameters ( aafMET, afMETPostCrawford,
                                 md.mi.nLength,
                                 &md.mpPreCrawford ) < 0 ) {

      fprintf ( stderr, "Error generating pre-Crawford MET\n" );
      return;

    }

  }

  /* Extend match equity table */

  ExtendMET ( aafMET, md.mi.nLength );


  /* garbage collect */

  freeMP ( &md.mpPreCrawford );
  freeMP ( &md.mpPostCrawford );

  if ( miCurrent.szName ) free ( miCurrent.szName );
  if ( miCurrent.szFileName ) free ( miCurrent.szFileName );
  if ( miCurrent.szDescription ) free ( miCurrent.szDescription );

  /* save match equity table information */

  memcpy ( &miCurrent, &md.mi, sizeof ( metinfo ) );

}




#if HAVE_LIBXML2

static void
parseRow ( float arRow[], xmlDocPtr doc, xmlNodePtr root ) {

  int iCol;
  xmlNodePtr cur;

  iCol = 0;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "me" ) ) {
      arRow[ iCol ]  = 
        atof ( xmlNodeListGetString ( doc, cur->xmlChildrenNode, 1 ) );

      iCol++;

    }

  }

}


static void
parsePreCrawfordExplicit ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  xmlNodePtr cur;
  int iRow;

  iRow = 0;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "row" ) )
      parseRow ( pmd->aarMET[ iRow++ ], doc, cur );

  }

}

static void
parseParameters ( list *plList, xmlDocPtr doc, xmlNodePtr root ) {

  xmlNodePtr cur;
  char *pc;
  parameter *pp;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "parameter" ) ) {

      pp = (parameter *) malloc ( sizeof ( parameter ) );

      pc = xmlGetProp ( cur, "name" );
      pp->szName = strdup ( pc ? pc : "" );
      
      pc = xmlNodeListGetString ( doc, cur->xmlChildrenNode, 1 );
      pp->szValue = strdup ( pc ? pc : "" );

      ListInsert ( plList, pp );

    }

  }


}


static void
parsePreCrawfordFormula ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  xmlNodePtr cur;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "parameters" ) )
      parseParameters ( &pmd->mpPreCrawford.lParameters, 
                        doc, cur );

  }

}


static void
parsePostCrawfordFormula ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  xmlNodePtr cur;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "parameters" ) )
      parseParameters ( &pmd->mpPostCrawford.lParameters, 
                        doc, cur );

  }

}



static void
parsePostCrawfordExplicit ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  xmlNodePtr cur;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "row" ) )
      parseRow ( pmd->arMETPostCrawford, doc, cur );

  }

}




static void parsePreCrawford ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  if ( ! strcmp ( pmd->mpPreCrawford.szName, "explicit" ) )
    parsePreCrawfordExplicit ( pmd, doc, root );
  else
    parsePreCrawfordFormula ( pmd, doc, root );

}


static void parsePostCrawford ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  if ( ! strcmp ( pmd->mpPostCrawford.szName, "explicit" ) )
    parsePostCrawfordExplicit ( pmd, doc, root );
  else
    parsePostCrawfordFormula ( pmd, doc, root );

}


static void parseInfo ( metdata *pmd, xmlDocPtr doc, xmlNodePtr root ) {

  xmlNodePtr cur;
  char *pc;

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "name" ) ) {
      pc = xmlNodeListGetString ( doc, cur->xmlChildrenNode, 1 );
      pmd->mi.szName = ( pc ) ? strdup ( pc ) : NULL;
    }
    else if ( ! strcmp ( cur->name, "description" ) ) {
      pc = xmlNodeListGetString ( doc, cur->xmlChildrenNode, 1 );
      pmd->mi.szDescription = ( pc ) ? strdup ( pc ) : NULL;
    }
    else if ( ! strcmp ( cur->name, "length" ) )
      pmd->mi.nLength =
        atoi ( xmlNodeListGetString ( doc, cur->xmlChildrenNode, 1 ) );

  }

}

void validateWarning ( void *ctx,
                        const char *msg, 
                        ... ) {

  va_list ap;
  
  va_start ( ap, msg );
  vprintf ( msg, ap );
  va_end ( ap );

}

void validateError ( void *ctx,
                     const char *msg, 
                     ... ) {

  va_list ap;

  va_start ( ap, msg );
  vprintf ( msg, ap );
  va_end ( ap );

}


extern int readMET ( metdata *pmd, const char *szFileName ) {

  xmlDocPtr doc;
  xmlNodePtr root, cur;

  xmlValidCtxt ctxt;

  char *pc;

  int i, j;
  int fError;

  fError = 0;

  /* parse document */

  doc = xmlParseFile( szFileName );

  /* check root */

  root = xmlDocGetRootElement ( doc );

  if ( ! root ) {

    printf ( "Error reading XML file (%s): no document root!\n", szFileName );
    fError = 1;
    goto finish;

  }

  /* check if the dtd has the correct name */

  if ( xmlStrcmp ( root->name, (const xmlChar *) "met" ) ) {

    printf ( "Error reading XML file (%s): wrong DTD (%s)\n", 
             szFileName, root->name );
    fError = 1;
    goto finish;

  }

  /* 
   * validate document 
   */

  /* load catalogs */

  /* FIXME: any tweaks needed for win32? */

  xmlLoadCatalog ( PKGDATADIR "/met/catalog " );
  if ( pc = getenv ( "SGML_CATALOG_FILES" ) )
    xmlLoadCatalog ( pc );
  xmlLoadCatalog ( "./met/catalog" );
  xmlLoadCatalog ( "./catalog" );
       
  if ( ! xmlCatalogGetSystem ( "met.dtd" ) ) {

    printf ( "met DTD not found\n" );
    fError = 1;
    goto finish;

  }

  ctxt.error = validateError;
  ctxt.warning = validateWarning;

  if ( ! xmlValidateDocument ( &ctxt, doc ) ) {

    printf ( "XML does not validate!\n" );
    fError = 1;
    goto finish;

  }

  /* initialise data */

  initMD ( pmd );

  /* fetch information from xml doc */

  for ( cur = root->xmlChildrenNode; cur; cur = cur->next ) {

    if ( ! strcmp ( cur->name, "info" ) )
      parseInfo ( pmd, doc, cur );
    else if ( ! strcmp ( cur->name, "pre-crawford-table" ) ) {

      pc = xmlGetProp ( cur, "type" );
      if ( pc )
        pmd->mpPreCrawford.szName = pc;
      else
        pmd->mpPreCrawford.szName = strdup ( "explicit" );

      parsePreCrawford ( pmd, doc, cur );

    }
    else if ( ! strcmp ( cur->name, "post-crawford-table" ) ) {

      pc = xmlGetProp ( cur, "type" );
      if ( pc )
        pmd->mpPostCrawford.szName = pc;
      else
        pmd->mpPostCrawford.szName = strdup ( "explicit" );

      parsePostCrawford ( pmd, doc, cur );

    }

  }

  if ( pmd->mi.nLength < 0 ) pmd->mi.nLength = MAXSCORE;

#ifdef UNDEF

  /* debug dump */

  printf ( "Name          : '%s'\n", pmd->mi.szName );
  printf ( "Description   : '%s'\n", pmd->mi.szDescription );
  printf ( "Length        : %d\n", pmd->mi.nLength );
  printf ( "pre-Crawford:\n" );

  if ( ! strcmp ( pmd->mpPreCrawford.szName, "explicit" ) ) {
    for ( j = 0; j < pmd->mi.nLength; j++ ) {
      printf ( "row %2d: ", j );
      for ( i = 0; i < pmd->mi.nLength; i++ ) {
        printf ( "%5.3f ", pmd->aarMET[ j ][ i ] );
      }
      printf ( "\n" );
    }
  }

  printf ( "post-Crawford:\n" );

  if ( ! strcmp ( pmd->mpPostCrawford.szName, "explicit" ) ) {
    printf ( "row %2d: ", 0 );
    for ( i = 0; i < pmd->mi.nLength; i++ ) {
      printf ( "%5.3f ", pmd->arMETPostCrawford[ i ] );
    }
    printf ( "\n" );
  }

#endif

 finish:

  if ( doc )
    xmlFreeDoc ( doc );

  if ( ! fError )
    pmd->mi.szFileName = strdup ( szFileName );

  return fError;

  
  
}

#endif

