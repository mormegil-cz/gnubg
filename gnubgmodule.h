/*
 * pythonmodule.h
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
 *
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

#ifndef _PYTHONMODULE_H_
#define _PYTHONMODULE_H_

#if USE_PYTHON
#ifdef WIN32
/* needed for mingw inclusion of Python.h */
#ifndef _MSC_VER
#include <stdint.h>
#endif
#endif
#include <Python.h>
#endif

extern void PythonInitialise(char *argv0);
extern void PythonShutdown(void);
extern void PythonRun(const char *sz);
extern int LoadPythonFile(const char *sz);

#endif /* _PYTHONMODULE_H_ */
