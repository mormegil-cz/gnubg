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

#ifdef WIN32
#include "windows.h"

double perFreq = 0;

void setup_timer()
{
	LARGE_INTEGER freq;
	if (QueryPerformanceFrequency(&freq) == 0)
	{
		MessageBox(0, "Timer not supported", "Error", MB_OK);
		perFreq = 1;
	}
	else
		perFreq = ((double)freq.QuadPart) / 1000;
}

double get_time()
{
	LARGE_INTEGER time;

	if (perFreq == 0)
		setup_timer();

	QueryPerformanceCounter(&time);
	return time.QuadPart / perFreq;
}

#else
#include <time.h>

double get_time()
{
	return clock() / 1000.0;
}

#endif
