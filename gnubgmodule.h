/*
 * pythonmodule.h
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
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

#ifndef _PYTHONMODULE_H_
#define _PYTHONMODULE_H_

#if USE_PYTHON

/* Python.h definse HAVE_FSTAT so save and redefine here */
#if HAVE_FSTAT
#define _HAVE_FSTAT 1
#else
#define _HAVE_FSTAT 0
#endif
#undef HAVE_FSTAT
#include <Python.h>
#undef HAVE_FSTAT
#include "config.h"

extern void
PythonInitialise( const char *szDir );

extern void
PythonShutdown();

extern PyObject *PythonMatchChecksum( PyObject* self, PyObject *args );

#endif /* USE_PYTHON */

#endif /* _PYTHONMODULE_H_ */
