/*
 * gnubgmodule.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003.
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

#if USE_PYTHON

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include <signal.h>
#include <assert.h>
#include <string.h>

#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include <stdio.h>
#include "i18n.h"
#include "backgammon.h"
#include "eval.h"
#include "matchequity.h"
#include "path.h"
#include "positionid.h"


static PyObject *
BoardToPy( int anBoard[ 2 ][ 25 ] ) {

  return 
    Py_BuildValue( "[[i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i],"
                   " [i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i]]",
                   anBoard[ 0 ][ 0 ],
                   anBoard[ 0 ][ 1 ],
                   anBoard[ 0 ][ 2 ],
                   anBoard[ 0 ][ 3 ],
                   anBoard[ 0 ][ 4 ],
                   anBoard[ 0 ][ 5 ],
                   anBoard[ 0 ][ 6 ],
                   anBoard[ 0 ][ 7 ],
                   anBoard[ 0 ][ 8 ],
                   anBoard[ 0 ][ 9 ],
                   anBoard[ 0 ][ 10 ],
                   anBoard[ 0 ][ 11 ],
                   anBoard[ 0 ][ 12 ],
                   anBoard[ 0 ][ 13 ],
                   anBoard[ 0 ][ 14 ],
                   anBoard[ 0 ][ 15 ],
                   anBoard[ 0 ][ 16 ],
                   anBoard[ 0 ][ 17 ],
                   anBoard[ 0 ][ 18 ],
                   anBoard[ 0 ][ 19 ],
                   anBoard[ 0 ][ 20 ],
                   anBoard[ 0 ][ 21 ],
                   anBoard[ 0 ][ 22 ],
                   anBoard[ 0 ][ 23 ],
                   anBoard[ 0 ][ 24 ],
                   anBoard[ 1 ][ 0 ],
                   anBoard[ 1 ][ 1 ],
                   anBoard[ 1 ][ 2 ],
                   anBoard[ 1 ][ 3 ],
                   anBoard[ 1 ][ 4 ],
                   anBoard[ 1 ][ 5 ],
                   anBoard[ 1 ][ 6 ],
                   anBoard[ 1 ][ 7 ],
                   anBoard[ 1 ][ 8 ],
                   anBoard[ 1 ][ 9 ],
                   anBoard[ 1 ][ 10 ],
                   anBoard[ 1 ][ 11 ],
                   anBoard[ 1 ][ 12 ],
                   anBoard[ 1 ][ 13 ],
                   anBoard[ 1 ][ 14 ],
                   anBoard[ 1 ][ 15 ],
                   anBoard[ 1 ][ 16 ],
                   anBoard[ 1 ][ 17 ],
                   anBoard[ 1 ][ 18 ],
                   anBoard[ 1 ][ 19 ],
                   anBoard[ 1 ][ 20 ],
                   anBoard[ 1 ][ 21 ],
                   anBoard[ 1 ][ 22 ],
                   anBoard[ 1 ][ 23 ],
                   anBoard[ 1 ][ 24 ] );

}

static PyObject *
Board1ToPy( int anBoard [ 25 ] ) {

  return 
    Py_BuildValue( "[i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i]",
                   anBoard[ 0 ],
                   anBoard[ 1 ],
                   anBoard[ 2 ],
                   anBoard[ 3 ],
                   anBoard[ 4 ],
                   anBoard[ 5 ],
                   anBoard[ 6 ],
                   anBoard[ 7 ],
                   anBoard[ 8 ],
                   anBoard[ 9 ],
                   anBoard[ 10 ],
                   anBoard[ 11 ],
                   anBoard[ 12 ],
                   anBoard[ 13 ],
                   anBoard[ 14 ],
                   anBoard[ 15 ],
                   anBoard[ 16 ],
                   anBoard[ 17 ],
                   anBoard[ 18 ],
                   anBoard[ 19 ],
                   anBoard[ 20 ],
                   anBoard[ 21 ],
                   anBoard[ 22 ],
                   anBoard[ 23 ],
                   anBoard[ 24 ] );

}


static int
PyToBoard1( PyObject *p, int anBoard[ 25 ] ) {

  int j;
  PyObject *pi;

  for ( j = 0; j < 25; ++j ) {
    if ( ! ( pi = PyList_GetItem( p, j ) ) )
      return -1;
    
    anBoard[ j ] = (int) PyInt_AsLong( pi );
      
  }

  return 0;

}

static int
PyToBoard( PyObject *p, int anBoard[ 2 ][ 25 ] ) {

  PyObject *py;
  int i;

  for ( i = 0; i < 2; ++i ) {
    if ( ! ( py = PyList_GetItem( p, i ) ) )
      return -1;

    if ( PyToBoard1( py, anBoard[ i ] ) )
      return -1;

  }

  return 0;

}


static PyObject *
CubeInfoToPy( const cubeinfo *pci ) {

  return Py_BuildValue( "{s:i,s:i,s:i,s:i,s:(i,i),"
                        "s:i,s:i,s:i,s:((f,f),(f,f)),s:i}",
                        "cube", pci->nCube,
                        "cubeowner", pci->fCubeOwner,
                        "move", pci->fMove,
                        "matchto", pci->nMatchTo,
                        "score", pci->anScore[ 0 ], pci->anScore[ 1 ],
                        "crawford", pci->fCrawford,
                        "jacoby", pci->fJacoby,
                        "beavers", pci->fBeavers,
                        "gammonprice", 
                        pci->arGammonPrice[ 0 ],
                        pci->arGammonPrice[ 2 ],
                        pci->arGammonPrice[ 1 ],
                        pci->arGammonPrice[ 3 ],
                        "bgv", pci->bgv );

}


static int
PyToCubeInfo( PyObject *p, cubeinfo *pci ) {

  PyObject *pyKey, *pyValue;
  int iPos = 0;
  char *pchKey;
  static char *aszKeys[] = {
    "jacoby", "crawford", "move", "beavers", "cube", "matchto",
    "bgv", "cubeowner", "score", "gammonprice", NULL };
  int iKey;
  int i;
  void *ap[] = { &pci->fJacoby, &pci->fCrawford, &pci->fMove,
                 &pci->fBeavers, &pci->nCube, &pci->nMatchTo,
                 &pci->bgv, &pci->fCubeOwner, pci->anScore,
                 pci->arGammonPrice };
  int *pi;
  float *pf;
  
  while( PyDict_Next( p, &iPos, &pyKey, &pyValue ) ) {

    if ( ! ( pchKey = PyString_AsString( pyKey ) ) )
      return -1;

    iKey = -1;

    for ( i = 0; aszKeys[ i ] && iKey < 0; ++i )
      if ( ! strcmp( aszKeys[ i ], pchKey ) )
        iKey = i;

    if ( iKey < 0 ) {
      /* unknown dict value */
      PyErr_SetString( PyExc_ValueError, 
                       _("invalid dict value in cubeinfo "
                         "(see gnubg.setcubeinfo() for an example)") );
      return -1;
    }

    switch( iKey ) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      /* simple integer */
      if ( ! PyInt_Check( pyValue ) ) {
        /* unknown dict value */
        PyErr_SetString( PyExc_ValueError, 
                         _("invalid value cubeinfo "
                           "(see gnubg.setcubeinfo() for an example)") );
        return -1;
      }
      
      *((int *) ap[ iKey ]) = (int) PyInt_AsLong( pyValue );

      break;

    case 8:
      /* score */
      pi = (int *) ap[ iKey ];
      if ( ! PyArg_ParseTuple( pyValue, "ii", pi, pi + 1 ) )
        return -1;
      break;

    case 9:
      /* gammon price */
      pf = (float *) ap[ iKey ];
      if ( ! PyArg_ParseTuple( pyValue, "(ff)(ff)", 
                               pf, pf + 2, pf + 1, pf + 3 ) )
        return -1;
      break;
      
    default:

      assert( FALSE );

    }

  }

  return 0;

}


static PyObject *
EvalContextToPy( const evalcontext *pec ) {

  return Py_BuildValue( "{s:i,s:i,s:i,s:i,s:f}",
                        "cubeful", pec->fCubeful,
                        "plies", pec->nPlies,
                        "reduced", pec->nReduced,
                        "deterministic", pec->fDeterministic,
                        "noise", pec->rNoise );

}


static int
PyToEvalContext( PyObject *p, evalcontext *pec ) {

  PyObject *pyKey, *pyValue;
  int iPos = 0;
  char *pchKey;
  static char *aszKeys[] = {
    "cubeful", "plies", "reduced", "deterministic", "noise", NULL };
  int iKey;
  int i;
  
  while( PyDict_Next( p, &iPos, &pyKey, &pyValue ) ) {

    if ( ! ( pchKey = PyString_AsString( pyKey ) ) )
      return -1;

    iKey = -1;

    for ( i = 0; aszKeys[ i ] && iKey < 0; ++i )
      if ( ! strcmp( aszKeys[ i ], pchKey ) )
        iKey = i;

    if ( iKey < 0 ) {
      /* unknown dict value */
      PyErr_SetString( PyExc_ValueError, 
                       _("invalid dict value in evalcontext "
                         "(see gnubg.evalcontext() for an example)") );
      return -1;
    }

    switch( iKey ) {
    case 0:
    case 1:
    case 2:
    case 3:
      /* simple integer */
      if ( ! PyInt_Check( pyValue ) ) {
        /* not an integer */
        PyErr_SetString( PyExc_ValueError, 
                         _("invalid value evalcontext "
                           "(see gnubg.evalcontext() for an example)") );
        return -1;
      }

      i = (int) PyInt_AsLong( pyValue );

      if ( !iKey )
        pec->fCubeful = i ? 1 : 0;
      else if ( iKey == 1 )
        pec->nPlies = ( i < 8 ) ? i : 7;
      else if ( iKey == 2 )
        pec->nReduced = ( i < 8 ) ? i : 7;
      else 
        pec->fDeterministic = i ? 1 : 0;

      break;

    case 4:
      /* float */
      if( ! PyFloat_Check( pyValue ) ) {
        /* not a float */
        PyErr_SetString( PyExc_ValueError,
                         _("invalid value in evalcontext "
                           "(see gnubg.evalcontext() for an example)" ) );
        return -1;
      }

      pec->rNoise = (float) PyFloat_AsDouble( pyValue );

      break;

    default:

      assert( FALSE );

    }

  }

  return 0;

}



static PyObject *
PythonCubeInfo( PyObject *self, PyObject *args ) {

  cubeinfo ci;
  int nCube = ms.nCube;
  int fCubeOwner = ms.fCubeOwner;
  int fMove = ms.fMove;
  int nMatchTo = ms.nMatchTo;
  int anScore[ 2 ] = { ms.anScore[ 0 ], ms.anScore[ 1 ] };
  int fCrawford = ms.fCrawford;
  int fJacoby = ms.fJacoby;
  int fBeavers = fBeavers;
  bgvariation bgv = ms.bgv;

  if ( ! PyArg_ParseTuple( args, "|iiii(ii)iiii:cubeinfo", 
                           &nCube, &fCubeOwner, &fMove, &nMatchTo, 
                           &anScore[ 0 ], &anScore[ 1 ], &fCrawford, &bgv ) )
    return NULL;

  if ( SetCubeInfo( &ci, nCube, fCubeOwner, fMove, nMatchTo, anScore,
                    fCrawford, fJacoby, fBeavers, bgv ) ) {
    printf( "error in SetCubeInfo\n" );
    return NULL;
  }

  return CubeInfoToPy( &ci );

}


static PyObject *
PythonEvalContext( PyObject *self, PyObject *args ) {

  evalcontext ec;
  int fCubeful = 0, nPlies = 0, nReduced = 0, fDeterministic = 1;
  float rNoise = 0.0f;

  if ( ! PyArg_ParseTuple( args, "|iiiif",
                           &fCubeful, &nPlies, &nReduced, &fDeterministic,
                           &rNoise ) )
    return NULL;

  ec.fCubeful = fCubeful ? 1 : 0;
  ec.nPlies = ( nPlies < 8 ) ? nPlies : 7;
  ec.nReduced = ( nReduced < 8 ) ? nReduced : 7;
  ec.fDeterministic = fDeterministic ? 1 : 0;
  ec.rNoise = rNoise;
                           
  return EvalContextToPy( &ec );

}

static PyObject *
PythonCommand( PyObject *self, PyObject *args ) {

  char *pch;
  char *sz;
  psighandler sh;

  if ( ! PyArg_ParseTuple( args, "s:command", &pch ) )
    return NULL;

  sz = strdup( pch );

  PortableSignal( SIGINT, HandleInterrupt, &sh, FALSE );
  HandleCommand( sz, acTop );
  NextTurn( FALSE );
  outputx();
  free( sz );
  PortableSignalRestore( SIGINT, &sh );
  if( fInterrupt ) {
    raise( SIGINT );
    fInterrupt = FALSE;
  }
  
  return Py_BuildValue( "" ); /* None */

}


static PyObject *
PythonBoard( PyObject *self, PyObject *args ) {

  if ( ! PyArg_ParseTuple( args, ":board" ) )
    return NULL;

  if ( ms.gs == GAME_NONE )
    /* no board available */
    return Py_BuildValue( "" );

  return BoardToPy( ms.anBoard );
}


static PyObject *
PythonEvaluate( PyObject *self, PyObject *args ) {

  PyObject *pyBoard = NULL;
  PyObject *pyCubeInfo = NULL;
  PyObject *pyEvalContext = NULL;

  int anBoard[ 2 ][ 25 ];
  cubeinfo ci;
  evalcontext ec = { 0, 0, 0, 1, 0.0f };
  float arOutput[ 7 ];

  memcpy( anBoard, ms.anBoard, sizeof anBoard );
  GetMatchStateCubeInfo( &ci, &ms );

  if ( ! PyArg_ParseTuple( args, "|OOO", 
                           &pyBoard, &pyCubeInfo, &pyEvalContext ) )
    return NULL;

  if ( pyBoard && PyToBoard( pyBoard, anBoard ) )
    return NULL;

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  if ( pyEvalContext && PyToEvalContext( pyEvalContext, &ec ) )
    return NULL;

  if ( GeneralEvaluationE( arOutput, anBoard, &ci, &ec ) ) {
    PyErr_SetString( PyExc_StandardError, 
                     _("interupted/errro in GeneralEvaluateE") );
    return NULL;
  }

  return Py_BuildValue( "[fffffff]", 
                        arOutput[ 0 ],
                        arOutput[ 1 ],
                        arOutput[ 2 ],
                        arOutput[ 3 ],
                        arOutput[ 4 ],
                        arOutput[ 5 ],
                        arOutput[ 6 ] );

}


static PyObject *
METRow( float ar[ MAXSCORE ], const int n ) {

  int i;
  PyObject *pyList;
  PyObject *pyf;

  if ( ! ( pyList = PyList_New( n ) ) )
    return NULL;

  for ( i = 0; i < n; ++i ) {

    if ( ! ( pyf = PyFloat_FromDouble( ar[ i ] ) ) )
      return NULL;

    if ( PyList_SetItem( pyList, i, pyf ) < 0 )
      return NULL;

  }

  return pyList;

}

static PyObject *
METPre( float aar[ MAXSCORE ][ MAXSCORE ], const int n ) {

  int i;
  PyObject *pyList;
  PyObject *pyRow;

  if ( ! ( pyList = PyList_New( n ) ) )
    return NULL;

  for ( i = 0; i < n; ++i ) {
    if ( ! ( pyRow = METRow( aar[ i ], n ) ) )
      return NULL;

    if ( PyList_SetItem( pyList, i, pyRow ) < 0 )
      return NULL;

  }

  return pyList;

}




static PyObject *
PythonMET( PyObject *self, PyObject *args ) {

  int n = ms.nMatchTo ? ms.nMatchTo : MAXSCORE;
  int i;
  PyObject *pyMET;
  PyObject *pyList;

  if ( ! PyArg_ParseTuple( args, "|i:matchequiytable", &n ) )
    return NULL;

  if ( n < 0 || n > MAXSCORE ) {
    PyErr_SetString( PyExc_ValueError, _("invalid matchlength" ) );
    return NULL;
  }

  if ( ! ( pyMET = PyList_New( 3 ) ) )
    return NULL;

  /* pre-Crawford scores */

  if ( ! ( pyList = METPre( aafMET, n ) ) )
    return NULL;

  if ( PyList_SetItem( pyMET, 0, pyList ) < 0 )
    return NULL;

  /* post-Crawford scores */

  for ( i = 0; i < 2; ++i ) {

    if ( ! ( pyList = METRow( aafMETPostCrawford[ i ], n ) ) )
      return NULL;

    if ( PyList_SetItem( pyMET, i +1, pyList ) < 0 )
      return NULL;

  }

  return pyMET;

}


static PyObject *
PythonEq2mwc( PyObject *self, PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  float r = 0.0f;
  cubeinfo ci;

  if ( ! PyArg_ParseTuple( args, "|fO:eq2mwc", &r, &pyCubeInfo ) )
    return NULL;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  return Py_BuildValue( "f", eq2mwc( r, &ci ) );

}

static PyObject *
PythonMwc2eq( PyObject *self, PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  float r = 0.0f;
  cubeinfo ci;

  if ( ! PyArg_ParseTuple( args, "|fO:mwc2eq", &r, &pyCubeInfo ) )
    return NULL;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  return Py_BuildValue( "f", mwc2eq( r, &ci ) );

}


static PyObject *
PythonPositionID( PyObject *self, PyObject *args ) {

  PyObject *pyBoard = NULL;
  int anBoard[ 2 ][ 25 ];

  memcpy( anBoard, ms.anBoard, sizeof anBoard );

  if ( ! PyArg_ParseTuple( args, "|O:positionid", &pyBoard ) )
    return NULL;

  if ( pyBoard && PyToBoard( pyBoard, anBoard ) )
    return NULL;

  return Py_BuildValue( "s", PositionID( anBoard ) );

}

static PyObject *
PythonPositionFromID( PyObject *self, PyObject *args ) {

  char *sz = NULL;
  int anBoard[ 2 ][ 25 ];

  if ( ! PyArg_ParseTuple( args, "|s:positionfromid", &sz ) )
    return NULL;

  if ( sz && PositionFromID( anBoard, sz ) ) {
    PyErr_SetString( PyExc_ValueError, 
                     _("invalid positionid") );
    return NULL;
  }
  else
    memcpy( anBoard, ms.anBoard, sizeof anBoard );

  return BoardToPy( anBoard );

}


static PyObject *
PythonPositionKey( PyObject *self, PyObject *args ) {

  PyObject *pyBoard = NULL;
  int anBoard[ 2 ][ 25 ];
  unsigned char auch[ 10 ];

  memcpy( anBoard, ms.anBoard, sizeof anBoard );

  if ( ! PyArg_ParseTuple( args, "|O!:positionkey", &PyList_Type, &pyBoard ) )
    return NULL;

  if ( pyBoard && PyToBoard( pyBoard, anBoard ) )
    return NULL;

  PositionKey( anBoard, auch );

  return Py_BuildValue( "[iiiiiiiiii]", 
                        auch[ 0 ], auch[ 1 ], auch[ 2 ], auch[ 3 ],
                        auch[ 4 ], auch[ 5 ], auch[ 6 ], auch[ 7 ],
                        auch[ 8 ], auch[ 9 ] );

}

static PyObject *
PythonPositionFromKey( PyObject *self, PyObject *args ) {

  int anBoard[ 2 ][ 25 ];
  int i;
  PyObject *pyKey = NULL;
  PyObject *py;
  unsigned char auch[ 10 ];

  if ( ! PyArg_ParseTuple( args, "|O!:positionfromkey", &PyList_Type, &pyKey ) )
    return NULL;

  if ( pyKey ) {

    for ( i = 0; i < 10; ++i ) {
      if ( ! ( py = PyList_GetItem( pyKey, i ) ) )
        return NULL;
      
      auch[ i ] = (unsigned char) PyInt_AsLong( py );
    }

  }
  else {

    for ( i = 0; i < 10; ++i )
      auch[ i ] = 0;

  }

  PositionFromKey( anBoard, auch );

  return BoardToPy( anBoard );

}


static PyObject *
PythonPositionBearoff( PyObject *self, PyObject *args ) {


  PyObject *pyBoard = NULL;
  int nChequers = 15;
  int nPoints = 6;
  int anBoard[ 25 ];

  memcpy( anBoard, 0, sizeof anBoard );

  if ( ! PyArg_ParseTuple( args, "|Oii:positionbearoff", 
                           &pyBoard, &nPoints, &nChequers ) )
    return NULL;

  if ( pyBoard && PyToBoard1( pyBoard, anBoard ) )
    return NULL;

  return Py_BuildValue( "i", PositionBearoff( anBoard, nPoints, nChequers ) );

}


static PyObject *
PythonPositionFromBearoff( PyObject *self, PyObject *args ) {

  int anBoard[ 25 ];
  int iPos = 0;
  int nChequers = 15;
  int nPoints = 6;
  int n;

  if ( ! PyArg_ParseTuple( args, "|iii:positionfrombearoff", 
                           &iPos, nChequers, &nPoints  ) )
    return NULL;

  if ( nChequers < 1 || nChequers > 15 || nPoints < 1 || nPoints > 25 ) {
    PyErr_SetString( PyExc_ValueError,
                     _("invalid number of chequers or points") );
    return NULL;
  }

  n = Combination( nChequers + nPoints, nPoints );

  if ( iPos < 0 || iPos >= n ) {
    PyErr_SetString( PyExc_ValueError,
                     _("invalid position number") );
    return NULL;
  }

  memset( anBoard, 0, sizeof anBoard );
  PositionFromBearoff( anBoard, iPos, nPoints, nChequers );

  return Board1ToPy( anBoard );


}


PyMethodDef gnubgMethods[] = {

  { "board", PythonBoard, METH_VARARGS,
    "Get the current board" },
  { "command", PythonCommand, METH_VARARGS,
    "Execute a command" },
  { "evaluate", PythonEvaluate, METH_VARARGS,
    "Cubeless evaluation" },
  { "evalcontext", PythonEvalContext, METH_VARARGS,
    "make a evalcontext" },
  { "eq2mwc", PythonEq2mwc, METH_VARARGS,
    "convert equity to MWC" },
  { "mwc2eq", PythonMwc2eq, METH_VARARGS,
    "convert MWC to equity" },
  { "cubeinfo", PythonCubeInfo, METH_VARARGS,
    "Make a cubeinfo" },
  { "met", PythonMET, METH_VARARGS,
    "return the current match equity table" },
  { "positionid", PythonPositionID, METH_VARARGS,
    "return position ID from board" },
  { "positionfromid", PythonPositionFromID, METH_VARARGS,
    "return board from position ID" },
  { "positionbearoff", PythonPositionBearoff, METH_VARARGS,
    "return the bearoff id for the given position" },
  { "positionfrombearoff", PythonPositionFromBearoff, METH_VARARGS,
    "return the board from the given bearoff id" },
  { "positionkey", PythonPositionKey, METH_VARARGS,
    "return key for position" },
  { "positionfromkey", PythonPositionFromKey, METH_VARARGS,
    "return position from key" },
  { NULL, NULL, 0, NULL }

};

#if HAVE_LIBREADLINE

#if 0
static char *
PythonReadline( char *p ) {

  char *pch;
  int l;

  pch = (char *) readline( p );

  l = pch ? strlen( pch ) : 0;

  pch = (char *) realloc( pch, l + 1 );

  if( l )
    strcat( pch, "\n" );
  else
    strcpy( pch, "" );

  return pch;

}
#endif

#endif

extern void
PythonInitialise( const char *argv0, const char *szDir ) {

  FILE *pf;
  char *pch;

  Py_SetProgramName( (char *) argv0 );
  Py_Initialize();

  /* ensure that python know about our gnubg module */
  Py_InitModule( "gnubg", gnubgMethods );
  PyRun_SimpleString( "import gnubg\n" );

#if HAVE_LIBREADLINE
  /* FIXME: implement switching of readline contexts 
     PyOS_ReadlineFunctionPointer = PythonReadline; */
#endif

  /* run gnubg.py start up script */

  if ( ( pch = PathSearch( "gnubg.py", szDir ) ) &&
       ( pf = fopen( pch, "r" ) ) ) {
    PyRun_AnyFile( pf, pch );
  }
  if ( pch )
    free( pch );

}

extern void
PythonShutdown( void ) {

  Py_Finalize();

}

#endif /* USE_PYTHON */
