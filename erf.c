/*
 * erf.c
 *
 * by Gary Wong <gtw@gnu.org>, 2001
 *
 * A portable implementation of the Gaussian error function, erf().
 * Based on Algorithm 123, M. Crawford and R. Techo, Comm. ACM, Sep. 1962,
 * which computes the Taylor series expansion of
 * 2/sqrt(pi) * integral from 0 to x of exp( -u^2 ) du.
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
 
#include <math.h>

extern double erf( double x ) {

    double a, u, v, w, y, z, t;
    int n;

    z = 0.0;

    while( x != 0.0 ) {
	if( x > 0.25 )
	    a = -0.25;
	else if( x < -0.25 )
	    a = 0.25;
	else
	    a = -x;

	u = v = M_2_SQRTPI * exp( -x * x );
	y = t = -v * a;
	n = 1;

	while( y - t != y ) {
	    n++;
	    w = -2 * x * v - 2 * u * ( n - 2 );
	    t *= w * a / ( v * n );
	    u = v;
	    v = w;
	    y += t;
	}

	z += y;
	x += a;
    }
	    
    return z;
}
