/*
 * kleinman.c
 *
 * by Øystein Johansen <oeysteij@online.no> 2000
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
 *
 *
 */

/*
 * This function/program gives the Cubeless Winning Probability in race
 * position. It is based on the Kleinman Count.  */

/* Compile like this if you want a standalone program:
   
   gcc -O2 -DSTANDALONE -o kleinman kleinman.c -lm

   or if you just like the function as a object:

   gcc -O2 -c kleinman.c 

*/

#ifdef STANDALONE
#include <stdio.h>
#endif

#include <math.h>

extern float
KleinmanCount (int nPipOnRoll, int nPipNotOnRoll)
{
  int nDiff, nSum;
  float rK, rW;

  nDiff = nPipNotOnRoll - nPipOnRoll;
  nSum = nPipNotOnRoll + nPipOnRoll;

  /* Don't use this routine if player on roll is not the favorite.
     And don't use it if the race is too short.  */

  if ((nDiff < -4) || (nSum < 100))
    return -1;

  rK = (float) (nDiff + 4) * (nDiff + 4) / (nSum - 4);

  if (rK < 1.0)
    rW = 0.5 + 0.267 * sqrt (rK);
  else
    rW = 0.76 + 0.114 * log (rK);

  if (rW > 1.0)
    return 1.0;
  else
    return rW;
}

#ifdef STANDALONE
int
main (int argc, char *argv[])
{
  int a, b;
  float rKC;

  if (argc == 3)
    {
      sscanf (argv[1], "%d", &a);
      sscanf (argv[2], "%d", &b);
      rKC = KleinmanCount (a, b);
      if (rKC == -1)
	printf ("Pipcount unsuitable for Kleinman Count.\n");
      else
	printf ("Cubeless Winning Chance: %f\n", rKC);
    }
  else
    {
      printf ("Usage: %s <pip for player on roll> "
              "<pip for player not on roll>\n", argv[0]);
    }
  return 0;
}
#endif
