// -*- C++ -*-

/*
 * bearoffgammon.h
 *
 * by Joseph Heled, 2000
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
 */

#if !defined( BEAROFFGAMMON_H )
#define BEAROFFGAMMON_H

#if defined( __GNUG__ )
#pragma interface
#endif

#if defined(__cplusplus)
#define CONST const
#else
#define CONST
#endif

// pack for space
struct GammonProbs {
  unsigned long p1 : 16;  // 0 - 36^2
  unsigned long p2 : 16;  // 0 - 36^3
  unsigned long p3 : 24;  // 0 - 36^4
  unsigned long p0 : 8;   // 0 - 36
};

#if defined( __cplusplus )
extern "C" {
#endif

extern CONST struct GammonProbs*
getBearoffGammonProbs(int b[6]);

extern CONST long*
getRaceBGprobs(int board[6]);

#if defined( __cplusplus )
static unsigned int const RBG_NPROBS = 5;
#else
#define RBG_NPROBS 5
#endif

#if defined( __cplusplus )
}
#endif

#endif
