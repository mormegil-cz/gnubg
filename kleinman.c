/* kleinman.c    Øystein Johansen 2000
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
  float fK, fW;

  nDiff = nPipNotOnRoll - nPipOnRoll;
  nSum = nPipNotOnRoll + nPipOnRoll;

  /* Don't use this routine if player on roll is not the favorite.
     And don't use it if the race is too short.  */

  if ((nDiff < -4) || (nSum < 100))
    return -1;

  fK = (float) (nDiff + 4) * (nDiff + 4) / (nSum - 4);

  if (fK < 1.0)
    fW = 0.5 + 0.267 * sqrt (fK);
  else
    fW = 0.76 + 0.114 * log (fK);

  if (fW > 1.0)
    return 1.0;
  else
    return fW;
}

#ifdef STANDALONE
int
main (int argc, char *argv[])
{
  int a, b;
  float KC;

  if (argc == 3)
    {
      sscanf (argv[1], "%d", &a);
      sscanf (argv[2], "%d", &b);
      KC = KleinmanCount (a, b);
      if (KC == -1)
	printf ("Pipcount unsuitable for Kleinman Count.\n");
      else
	printf ("Cubeless Winning Chance: %f\n", KC);
    }
  else
    {
      printf ("Usage: %s <pip for player on roll> "
              "<pip for player not on roll>\n", argv[0]);
    }
  return 0;
}
#endif
