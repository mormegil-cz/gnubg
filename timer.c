/*
* timer.c
* by Jon Kinsey, 2003
*
* Accurate win32 timer or default timer
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

#include "config.h"
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef WIN32
#include "windows.h"

double perFreq = 0;

int setup_timer()
{
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq))
	{	/* Timer not supported */
		return 0;
	}
	else
	{
		perFreq = ((double)freq.QuadPart) / 1000;
		return 1;
	}
}

double get_time()
{	/* Return elapsed time in milliseconds */
	LARGE_INTEGER timer;

	if (!perFreq)
	{
		if (!setup_timer())
			return clock() / 1000.0;
	}

	QueryPerformanceCounter(&timer);
	return timer.QuadPart / perFreq;
}

#else

#if 1

double get_time()
{	/* Return elapsed time in milliseconds */
	struct timeval tv;
	gettimeofday(&tv, 0);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

#else

double perFreq = 0;

int setup_timer()
{
    perFreq = __get_clockfreq() / 1000.0;
     return 1;
}

double get_time()
{    /* Return elapsed time in milliseconds */
    if (!perFreq)
    {
        if (!setup_timer())
            return clock() / 1000.0;
    }
{
    unsigned long long int val;
    __asm__ __volatile__("rdtsc" : "=A" (val) : );

    return val / perFreq;
}
}

#endif

#endif
