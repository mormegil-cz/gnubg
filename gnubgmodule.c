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

#if HAVE_CONFIG_H
#include "config.h"
#if USE_PYTHON
#undef HAVE_FSTAT
#endif
#endif

#if USE_PYTHON
#include <Python.h>
#if HAVE_CONFIG_H
#undef HAVE_FSTAT
#include "config.h"
#endif

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

#define IGNORE __attribute__ ((unused))

static PyObject *
BoardToPy( int anBoard[ 2 ][ 25 ] )
{
  PyObject* b = PyTuple_New(2);
  PyObject* b0 = PyTuple_New(25);
  PyObject* b1 = PyTuple_New(25);
  unsigned int k;
  
  for(k = 0; k < 25; ++k) {
    PyTuple_SET_ITEM(b0, k, PyInt_FromLong(anBoard[ 0 ][k]));
    PyTuple_SET_ITEM(b1, k, PyInt_FromLong(anBoard[ 1 ][k]));
  }

  PyTuple_SET_ITEM(b, 0, b0);
  PyTuple_SET_ITEM(b, 1, b1);

  return b;
}

static PyObject *
Board1ToPy( int anBoard [ 25 ] ) {
  unsigned int k;
  PyObject* b = PyTuple_New(25);
  
  for(k = 0; k < 25; ++k) {
    PyTuple_SET_ITEM(b, k, PyInt_FromLong(anBoard[k]));
  }
  
  return b;
}


static int
PyToBoard1( PyObject* p, int anBoard[ 25 ] )
{
  if( PySequence_Check(p) && PySequence_Size(p) == 25 ) {
    int j;

    for ( j = 0; j < 25; ++j ) {
      PyObject* pi = PySequence_Fast_GET_ITEM(p, j);
    
      anBoard[ j ] = (int) PyInt_AsLong( pi );
    }
    return 1;
  }

  return 0;
}

static int
PyToBoard(PyObject* p, int anBoard[ 2 ][ 25 ])
{
  if( PySequence_Check(p) && PySequence_Size(p) == 2 ) {
    int i;
    for(i = 0; i < 2; ++i) {
      PyObject* py = PySequence_Fast_GET_ITEM(p, i);

      if ( ! PyToBoard1( py, anBoard[ i ] ) ) {
	return 0;
      }
    }
    return 1;
  }

  return 0;
}


static PyObject *
CubeInfoToPy( const cubeinfo *pci )
{
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


static PyObject*
EvalContextToPy( const evalcontext* pec)
{
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
PythonCubeInfo(PyObject* self IGNORE, PyObject* args) {

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
PythonEvalContext( PyObject* self IGNORE, PyObject *args ) {

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
PythonCommand( PyObject* self IGNORE, PyObject *args ) {

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
  
  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *
PythonBoard( PyObject* self IGNORE, PyObject *args ) {

  if ( ! PyArg_ParseTuple( args, ":board" ) )
    return NULL;

  if ( ms.gs == GAME_NONE ) {
    /* no board available */
    Py_INCREF(Py_None);
    return Py_None;
  }

  return BoardToPy( ms.anBoard );
}


static PyObject *
PythonEvaluate( PyObject* self IGNORE, PyObject *args ) {

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

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
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

  {
    PyObject* p = PyTuple_New(6);
    int k;
    for(k = 0; k < 6; ++k) {
      PyTuple_SET_ITEM(p, k, PyFloat_FromDouble(arOutput[k]));
    }
    return p;
  }
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
PythonMET( PyObject* self IGNORE, PyObject *args ) {

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
PythonEq2mwc( PyObject* self IGNORE, PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  float r = 0.0f;
  cubeinfo ci;

  if ( ! PyArg_ParseTuple( args, "|fO:eq2mwc", &r, &pyCubeInfo ) )
    return NULL;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  return PyFloat_FromDouble( eq2mwc( r, &ci ) );

}

static PyObject *
PythonMwc2eq( PyObject* self IGNORE, PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  float r = 0.0f;
  cubeinfo ci;

  if ( ! PyArg_ParseTuple( args, "|fO:mwc2eq", &r, &pyCubeInfo ) )
    return NULL;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  return PyFloat_FromDouble( mwc2eq( r, &ci ) );
}


static PyObject *
PythonPositionID( PyObject* self IGNORE, PyObject *args ) {

  PyObject *pyBoard = NULL;
  int anBoard[ 2 ][ 25 ];

  memcpy( anBoard, ms.anBoard, sizeof anBoard );

  if ( ! PyArg_ParseTuple( args, "|O:positionid", &pyBoard ) )
    return NULL;

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  return PyString_FromString( PositionID( anBoard ) );

}

static PyObject *
PythonPositionFromID( PyObject* self IGNORE, PyObject *args )
{
  char* sz = NULL;
  int anBoard[ 2 ][ 25 ];

  if( ! PyArg_ParseTuple( args, "|s:positionfromid", &sz ) ) {
    return NULL;
  }

  if( sz ) {
    if( ! PositionFromID(anBoard, sz) ) {
      PyErr_SetString( PyExc_ValueError, 
		       _("invalid positionid") );
      return NULL;
    }
  } else {
    memcpy( anBoard, ms.anBoard, sizeof anBoard );
  }

  return BoardToPy( anBoard );
}


static PyObject *
PythonPositionKey( PyObject* self IGNORE, PyObject *args ) {

  PyObject *pyBoard = NULL;
  int anBoard[ 2 ][ 25 ];
  unsigned char auch[ 10 ];

  memcpy( anBoard, ms.anBoard, sizeof anBoard );

  if ( ! PyArg_ParseTuple( args, "|O!:positionkey", &PyList_Type, &pyBoard ) )
    return NULL;

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  PositionKey( anBoard, auch );

  {
    PyObject* a = PyTuple_New(10);
    int i;
    for(i = 0; i < 10; ++i) {
      PyTuple_SET_ITEM(a, i, PyInt_FromLong(auch[i]));
    }
    return a;
  }
}

static PyObject *
PythonPositionFromKey( PyObject* self IGNORE, PyObject *args ) {

  int anBoard[ 2 ][ 25 ];
  int i;
  PyObject *pyKey = NULL;
  PyObject *py;
  unsigned char auch[ 10 ];

  if( ! PyArg_ParseTuple( args, "|O!:positionfromkey", &PyList_Type, &pyKey ) )
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
PythonPositionBearoff( PyObject* self IGNORE, PyObject *args )
{
  PyObject *pyBoard = NULL;
  int nChequers = 15;
  int nPoints = 6;
  int anBoard[ 25 ];

  memcpy( anBoard, 0, sizeof anBoard );

  if ( ! PyArg_ParseTuple( args, "|Oii:positionbearoff", 
                           &pyBoard, &nPoints, &nChequers ) )
    return NULL;

  if ( pyBoard && !PyToBoard1( pyBoard, anBoard ) )
    return NULL;

  return PyInt_FromLong( PositionBearoff( anBoard, nPoints, nChequers ) );
}

static PyObject *
PythonPositionFromBearoff( PyObject* self IGNORE, PyObject *args ) {

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

#if PY_MAJOR_VERSION < 2 || (PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 3)

/* Bool introduced in 2.3 */
#define PyBool_FromLong PyInt_FromLong

/* Fix incorrect prototype in early python */
#define CHARP_HACK (char*)
#else
#define CHARP_HACK
#endif

static inline void
DictSetItemSteal(PyObject* dict, const char* key, PyObject* val)
{
  int const s = PyDict_SetItemString(dict, CHARP_HACK key, val);  
  {                                                         assert( s == 0 ); }
  Py_DECREF(val);
}

typedef struct {
  const evalcontext*    ec;
  const rolloutcontext* rc;
} PyMatchState;

static PyObject*
diffContext(const evalcontext* c, PyMatchState* ms)
{
  const evalcontext* s = ms->ec;
  
  if( !s ) {
    ms->ec = c;
    return 0;
  }

  if( cmp_evalcontext(s, c) == 0 ) {
    return 0;
  }

  {
    PyObject* context = PyDict_New();

    if( c->fCubeful != s->fCubeful ) {
      DictSetItemSteal(context, "cubeful", PyInt_FromLong(c->fCubeful));
    }

    if( c->nPlies != s->nPlies ) {
      DictSetItemSteal(context, "plies", PyInt_FromLong(c->nPlies));
    }

    if( c->nReduced != s->nReduced ) {
      DictSetItemSteal(context, "reduced", PyInt_FromLong(c->nReduced));
    }

    if( c->fDeterministic != s->fDeterministic ) {
      DictSetItemSteal(context, "deterministic",
		       PyInt_FromLong(c->fDeterministic));
    }
    
    if( c->rNoise != s->rNoise ) {
      DictSetItemSteal(context, "noise", PyFloat_FromDouble(c->rNoise));
    }
    
    return context;
  }
}

static PyObject*
diffRolloutContext(const rolloutcontext* c, PyMatchState* ms)
{
  const rolloutcontext* s = ms->rc;
  
  if( !s ) {
    ms->rc = c;
    return 0;
  }

  {
    PyObject* context = PyDict_New();

    if( c->fCubeful != s->fCubeful ) {
      DictSetItemSteal(context, "cubeful", PyInt_FromLong(c->fCubeful));
    }

    if( c->fVarRedn != s->fVarRedn ) {
      DictSetItemSteal(context, "variance-reduction",
		       PyInt_FromLong(c->fVarRedn));
    }

    if( c->fInitial != s->fInitial ) {
      DictSetItemSteal(context, "initial-position",
		       PyInt_FromLong(c->fInitial));
    }

    if( c->fRotate != s->fRotate ) {
      DictSetItemSteal(context, "quasi-random-dice",
		       PyInt_FromLong(c->fRotate));
    }

    if( c->fLateEvals != s->fLateEvals ) {
      DictSetItemSteal(context, "late-eval",
		       PyInt_FromLong(c->fLateEvals));
    }
    
    if( c->fDoTruncate != s->fDoTruncate ) {
      DictSetItemSteal(context, "truncated-rollouts",
		       PyInt_FromLong(c->fDoTruncate));
    }
    
    if( c->nTruncate != s->nTruncate ) {
      DictSetItemSteal(context, "n-truncation", PyInt_FromLong(c->nTruncate));
    }

    if( c->fTruncBearoff2 != s->fTruncBearoff2 ) {
      DictSetItemSteal(context, "truncate-bearoff2",
		       PyInt_FromLong(c->fTruncBearoff2));
    }
    
    if( c->fTruncBearoffOS != s->fTruncBearoffOS ) {
      DictSetItemSteal(context, "truncate-bearoffOS",
		       PyInt_FromLong(c->fTruncBearoffOS));
    }
    
    if( c->fStopOnSTD != s->fStopOnSTD ) {
      DictSetItemSteal(context, "stop-on-std",
		       PyInt_FromLong(c->fStopOnSTD));
    }
    
    if( c->nTrials != s->nTrials ) {
      DictSetItemSteal(context, "trials", PyInt_FromLong(c->nTrials));
    }

    if( c->nSeed != s->nSeed ) {
      DictSetItemSteal(context, "seed", PyInt_FromLong(c->nSeed));
    }
    
    if( c->nMinimumGames != s->nMinimumGames ) {
      DictSetItemSteal(context, "minimum-games",
		       PyInt_FromLong(c->nMinimumGames));
    }
    
    if( PyDict_Size(context) == 0 ) {
      Py_DECREF(context);
      context = 0;
    }
    
    return context;
  }
}

static PyObject*
RolloutContextToPy(const rolloutcontext* rc)
{
  PyObject* dict =
    Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,"
		  "s:i,s:i,s:i}",
		  "cubeful", rc->fCubeful,
		  "variance-reduction", rc->fVarRedn,
		  "initial-position", rc->fInitial,
		  "quasi-random-dice", rc->fRotate,
		  "late-eval", rc->fLateEvals,
		  "truncated-rollouts", rc->fDoTruncate,
		  "n-truncation", rc->nTruncate,
		  "truncate-bearoff2", rc->fTruncBearoff2,
		  "truncate-bearoffOS", rc->fTruncBearoffOS,
		  "stop-on-std", rc->fStopOnSTD,
		  "trials", rc->nTrials,
		  "seed", rc->nSeed,
		  "minimum-games", rc->nMinimumGames);
  return dict;
}

  
static PyObject*
PyMove(const int move[8])
{
  /* Lazy. Allocate full move, resize later */
	    
  PyObject* moveTuple = PyTuple_New(4);
  int i;
	    
  for(i = 0; i < 4; ++i) {
    if( move[2*i] < 0 ) {
      break;
    }
    {
      PyObject* c = Py_BuildValue("(ii)", move[2*i], move[2*i+1]);
      
      PyTuple_SET_ITEM(moveTuple, i, c);
    }
  }

  if( i < 4 ) {
    int s = _PyTuple_Resize(&moveTuple, i);                  assert(s != -1);
  }
  
  return moveTuple;
}

static const char*
luckString(lucktype const lt, int const ignoreNone)
{
  switch( lt ) {
    case LUCK_VERYBAD: return "verybad";
    case LUCK_BAD: return "bad";
    case LUCK_NONE: return ignoreNone ? 0 : "unmarked";
    case LUCK_GOOD: return "good";
    case LUCK_VERYGOOD: return "verygood";
  }
  assert(0);
  return 0;
}
    
static const char*
skillString(skilltype const st, int const ignoreNone)
{
  switch( st ) {
    case SKILL_VERYBAD:     return "very bad";
    case SKILL_BAD:         return "bad";
    case SKILL_DOUBTFUL:    return "doubtful";
    case SKILL_NONE:        return ignoreNone ? 0 : "unmarked";
    case SKILL_GOOD:        return "good";
  }
  assert(0);
  return 0;
}

static void
addSkill(PyObject* dict, skilltype const st, const char* name)
{
  if( dict ) {
    const char* s = skillString(st, 1);
    if( s ) {
      if( ! name ) name = "skill";
      
      DictSetItemSteal(dict, name, PyString_FromString(s));
    }
  }
}

static void
addLuck(PyObject* dict, float const rLuck, lucktype const lt)
{
  if( dict ) {
    const char* l = luckString(lt, 1);

    if( rLuck != ERR_VAL ) {
      DictSetItemSteal(dict, "luck-value", PyFloat_FromDouble(rLuck));
    }

    if( l ) {
      DictSetItemSteal(dict, "luck", PyString_FromString(l));
    }
  }
}

static PyObject*
PyMoveAnalysis(const movelist* pml, PyMatchState* ms)
{
  int i;

  unsigned int n = 0;
  
  for(i = 0; i < pml->cMoves; i++) {
    const move* mi = &pml->amMoves[i] ;
	
    switch( mi->esMove.et ) {
      case EVAL_EVAL:
      case EVAL_ROLLOUT:
      {
	++n;
	break;
      }
      
      default: break;
    }
  }

  if( !n ) {
    return 0;
  }

  {
    PyObject* l = PyTuple_New(n);
    n = 0;
    
    for(i = 0; i < pml->cMoves; i++) {
      const move* mi = &pml->amMoves[i] ;
      PyObject* v = 0;;
      
      switch( mi->esMove.et ) {
        case EVAL_EVAL:
	{
	  PyObject* m = PyMove(mi->anMove);
	  v = Py_BuildValue("{s:s,s:O,s:(fffff),s:f}",
			    "type", "eval",
			    "move", m,
			    "probs", mi->arEvalMove[0], mi->arEvalMove[1],
			    mi->arEvalMove[2], mi->arEvalMove[3],
			    mi->arEvalMove[4],
			    "score", mi->rScore);
	  Py_DECREF(m);

	  {
	    PyObject* c = diffContext(&mi->esMove.ec, ms);
	    if( c ) {
	      DictSetItemSteal(v, "evalcontext", c);
	    }
	  }

	  break;
	}    
        case EVAL_ROLLOUT:
	{
	  PyObject* m = PyMove(mi->anMove);
	  const evalsetup* pes = &mi->esMove;
	  const float* p =  mi->arEvalMove;
	  const float* s =  mi->arEvalStdDev;
	
	  v = Py_BuildValue("{s:s,s:O,s:i,s:(fffff),s:f,s:f"
			    ",s:(fffff),s:f,s:f}",
			    "type", "rollout",
			    "move", m,
			    "trials", pes->rc.nGamesDone,

			    "probs", p[0], p[1], p[2], p[3], p[4],
			    "match-eq",   p[OUTPUT_EQUITY],
			    "cubeful-eq", p[OUTPUT_CUBEFUL_EQUITY],
			    "probs-std", s[0], s[1], s[2], s[3], s[4],
			    "match-eq-std",   s[OUTPUT_EQUITY],
			    "cubeful-eq-std", s[OUTPUT_CUBEFUL_EQUITY]);
	  Py_DECREF(m);

	  {
	    PyObject* c = diffRolloutContext(&pes->rc, ms);
	    if( c ) {
	      DictSetItemSteal(v, "rollout-context", c);
	    }
	  }
	  
	  break;
	}

        case EVAL_NONE: break;

        default: assert( 0 );
      }

      if( v ) {
	PyTuple_SET_ITEM(l, n++, v);
      }
    }

    return l;
  }
}

static PyObject*
PyDoubleAnalysis(const evalsetup* pes,
		 const float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
		 const float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
		 PyMatchState* ms,
		 int const verbose)
{
  PyObject* dict = 0;
  
  switch( pes->et ) {
    case EVAL_EVAL:
    {
      const float* p = aarOutput[0];
      
      dict =
	Py_BuildValue("{s:s,s:(fffff),s:f,s:f}",
		      "type", "eval",
		      "probs", p[0], p[1], p[2], p[3], p[4],
		      "nd-cubeful-eq", p[OUTPUT_CUBEFUL_EQUITY],
		      "dt-cubeful-eq", aarOutput[1][OUTPUT_CUBEFUL_EQUITY]);
      
      if( verbose ) {
	DictSetItemSteal(dict, "nd-match-eq",
			 PyFloat_FromDouble(p[OUTPUT_EQUITY]));
	DictSetItemSteal(dict,"dt-match-eq",
			 PyFloat_FromDouble(aarOutput[1][OUTPUT_EQUITY]));
      }

      {
	PyObject* c = diffContext(&pes->ec, ms);
	if( c ) {
	  DictSetItemSteal(dict, "eval-context", c);
	}
      }
      break;
    }
    case EVAL_ROLLOUT:
    {
      const float* nd  = aarOutput[0];
      const float* nds = aarStdDev[0];
      const float* dt  = aarOutput[1];
      const float* dts = aarStdDev[1];
      
      dict =
	Py_BuildValue("{s:s,s:i,"
		      "s:(fffff),s:f,s:f,"
		      "s:(fffff),s:f,s:f,"
		      "s:(fffff),s:f,s:f,"
		      "s:(fffff),s:f,s:f}",

		      "type", "rollout",
		      "trials", pes->rc.nGamesDone,

		      "nd-probs", nd[0], nd[1], nd[2], nd[3], nd[4],
		      "nd-match-eq",   nd[OUTPUT_EQUITY],
		      "nd-cubeful-eq", nd[OUTPUT_CUBEFUL_EQUITY],
				  
		      "nd-probs-std", nds[0], nds[1], nds[2], nds[3], nds[4],
		      "nd-match-eq-std",   nds[OUTPUT_EQUITY],
		      "nd-cubeful-eq-std", nds[OUTPUT_CUBEFUL_EQUITY],

		      "dt-probs", dt[0], dt[1], dt[2], dt[3], dt[4],
		      "dt-match-eq",   dt[OUTPUT_EQUITY],
		      "dt-cubeful-eq", dt[OUTPUT_CUBEFUL_EQUITY],
		      
		      "dt-probs-std", dts[0], dts[1], dts[2], dts[3], dts[4],
		      "dt-match-eq-std",   dts[OUTPUT_EQUITY],
		      "dt-cubeful-eq-std", dts[OUTPUT_CUBEFUL_EQUITY]);
      
      {
	PyObject* c = diffRolloutContext(&pes->rc, ms);
	if( c ) {
	  DictSetItemSteal(dict, "rollout-context", c);
	}
      }
      break;
    }
    default:
      assert( 0 );
  }

  return dict;
}

static PyObject*
PyGameStats(const statcontext* sc)
{
  PyObject* p[2];
  if( ! (sc->fMoves || sc->fCube || sc->fDice) ) {
    return 0;
  }

  p[0] = PyDict_New();
  p[1] = PyDict_New();
  
  if( sc->fMoves ) {
    int side;
    for(side = 0; side < 2; ++side) {
      
      PyObject* d =
	Py_BuildValue("{s:i,s:i,s:f,s:f}",
		      "unforced-moves", sc->anUnforcedMoves[side],
		      "total-moves", sc->anTotalMoves[side],
		      "error-skill", sc->arErrorCheckerplay[side][0],
		      "error-cost", sc->arErrorCheckerplay[side][1]);
      PyObject* m = PyDict_New();

      {
	skilltype st;
	for( st = SKILL_VERYBAD; st < N_SKILLS; st++ ) {
	  DictSetItemSteal(m, skillString(st, 0),
			   PyInt_FromLong(sc->anMoves[side][st]));
	}
      }

      DictSetItemSteal(d, "marked", m);

      DictSetItemSteal(p[side], "moves", d);
    }
  }
  
  if( sc->fCube ) {
    int side;
    for(side = 0; side < 2; ++side) {
      PyObject* d =
	Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i"
		      "s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f}",
		      "total-cube",sc->anTotalCube[side],
		      "n-doubles",sc->anDouble[side],
		      "n-takes", sc->anTake[side],
		      "n-drops", sc->anPass[side],

		      "missed-double-dp",sc->anCubeMissedDoubleDP[side],
		      "missed-double-tg", sc->anCubeMissedDoubleTG[side],
		      "wrong-double-dp", sc->anCubeWrongDoubleDP[side],
		      "wrong-double-tg",sc->anCubeWrongDoubleTG[side],
		      "wrong-take", sc->anCubeWrongTake[side],
		      "wrong-drop", sc->anCubeWrongPass[side],

		      "err-missed-double-dp-skill",
		      sc->arErrorMissedDoubleDP[side][0],
		      "err-missed-double-dp-cost",
		      sc->arErrorMissedDoubleDP[side][1],

		      "err-missed-double-tg-skill",
		      sc->arErrorMissedDoubleTG[side][ 0 ],
		      "err-missed-double-tg-cost",
		      sc->arErrorMissedDoubleTG[side][ 1 ],

		      "err-wrong-double-dp-skill",
		      sc->arErrorWrongDoubleDP[side][ 0 ],
		      "err-wrong-double-dp-cost",
		      sc->arErrorWrongDoubleDP[side][ 1 ],

		      "err-wrong-double-tg-skill",
		      sc->arErrorWrongDoubleTG[side][ 0 ],
		      "err-wrong-double-tg-cost",
		      sc->arErrorWrongDoubleTG[side][ 1 ],

		      "err-wrong-take-skill",
		      sc->arErrorWrongTake[side][ 0 ],
		      "err-wrong-take-cost",
		      sc->arErrorWrongTake[side][ 1 ],
			
		      "err-wrong-drop-skill",
		      sc->arErrorWrongPass[side][ 0 ],
		      "err-wrong-drop-cost",
		      sc->arErrorWrongPass[side][ 1 ] );

      DictSetItemSteal(p[side], "cube", d);
    }
  }
    
  if( sc->fDice ) {
    int side;
    for(side = 0; side < 2; ++side) {
      PyObject* d =
	Py_BuildValue("{s:f,s:f}",
		      "luck", sc->arLuck[side][0],
		      "luck-cost", sc->arLuck[side][1]);
    
      PyObject* m = PyDict_New();

      {
	lucktype lt;
	for( lt = LUCK_VERYBAD; lt <= LUCK_VERYGOOD; lt++ ) {
	  DictSetItemSteal(m, luckString(lt, 0),
			   PyInt_FromLong(sc->anLuck[0][lt]));
	}
      }
      
      DictSetItemSteal(d, "marked-rolls", m);

      DictSetItemSteal(p[side], "dice", d);
    }
  }

  {
    PyObject* d = PyDict_New();

    DictSetItemSteal(d, "X", p[0]);
    DictSetItemSteal(d, "O", p[1]);

    return d;
  }
}

/* plGame: Game as a list of records.
   doAnalysis: if true, add analysis info.
   verbose: if true, add derived analysis data.
   scMatch: if non-0, add game & match statistics.
 */

static PyObject*
PythonGame(const list*    plGame,
	   int const      doAnalysis,
	   int const      verbose,
	   statcontext*   scMatch,
	   int const      includeBoards,
	   PyMatchState*  ms)
{
  const list* pl = plGame->plNext;
  const moverecord* pmr = pl->p;            
  const movegameinfo* g = &pmr->g;

  PyObject* gameDict = PyDict_New();
  PyObject* gameInfoDict = PyDict_New();

  {                                       assert( pmr->mt == MOVE_GAMEINFO ); }

  if( ! (gameDict && gameInfoDict) ) {
    PyErr_SetString(PyExc_MemoryError, "");
    return 0;
  }
  
  DictSetItemSteal(gameInfoDict, "score-X", PyInt_FromLong(g->anScore[0]));
  DictSetItemSteal(gameInfoDict, "score-O", PyInt_FromLong(g->anScore[1]));

  /* Set Crawford info only if it is relevant, i.e. if any side is 1 point away
     from winning the match */
  
  if( g->anScore[0] + 1 == g->nMatch || g->anScore[1] + 1 == g->nMatch ) {
    DictSetItemSteal(gameInfoDict, "crawford",
			 PyBool_FromLong(g->fCrawfordGame));
  }
    
  if( g->fWinner >= 0 ) {
    DictSetItemSteal(gameInfoDict, "winner",
			 PyString_FromString(g->fWinner ? "O" : "X"));
    
    DictSetItemSteal(gameInfoDict, "points-won",
			 PyInt_FromLong(g->nPoints));
    
    DictSetItemSteal(gameInfoDict, "resigned",
			 PyBool_FromLong(g->fResigned));
  } else {
    Py_INCREF(Py_None);
    DictSetItemSteal(gameInfoDict, "winner", Py_None);
  }

  if( g->nAutoDoubles ) {
    DictSetItemSteal(gameInfoDict, "initial-cube",
		     PyInt_FromLong(1 << g->nAutoDoubles) );
  }

  DictSetItemSteal(gameDict, "info", gameInfoDict);
    
  if( scMatch ) {
    updateStatisticsGame( plGame );
    
    AddStatcontext(&g->sc, scMatch);
    
    {
      PyObject* s = PyGameStats(&g->sc);

      if( s ) {
	DictSetItemSteal(gameDict, "stats", s);
      }
    }
  }
  
  {
    int nRecords = 0;
    int anBoard[2][25];
    PyObject* gameTuple;

    {
      list* t;
      for( t = pl->plNext; t != plGame; t = t->plNext ) {
	++nRecords;
      }
    }

    gameTuple = PyTuple_New(nRecords);

    nRecords = 0;

    if( includeBoards ) {
      InitBoard(anBoard, g->bgv);
    }

    for( pl = pl->plNext; pl != plGame; pl = pl->plNext ) {
      const char* action = 0;
      int player = -1;
      PyObject* recordDict = PyDict_New();
      PyObject* analysis = doAnalysis ? PyDict_New() : 0;
      
      pmr = pl->p;
      
      switch( pmr->mt ) {
	case MOVE_NORMAL:
	{
	  const movenormal* n = &pmr->n;

	  action = "move";
	  player = n->fPlayer;

	  {
	    PyObject* dice = Py_BuildValue("(ii)", n->anRoll[0], n->anRoll[1]);
	    
	    DictSetItemSteal(recordDict, "dice", dice);
	  }

	  DictSetItemSteal(recordDict, "move", PyMove(n->anMove));

	  if( includeBoards ) {
	    DictSetItemSteal(recordDict, "board",
			     PyString_FromString(PositionID(anBoard)));
	    
	    ApplyMove(anBoard, n->anMove, 0);
	    SwapSides(anBoard);
	  }
	  
	  if( analysis ) {
	    if( n->esDouble.et != EVAL_NONE ) {
	      PyObject* d =
		PyDoubleAnalysis(&n->esDouble,   n->aarOutput, n->aarStdDev,
				 ms, verbose);
	      {
		int s = PyDict_Merge(analysis, d, 1);     assert( s != -1 );
	      }
	      Py_DECREF(d);
	    }
	  
	  
	    if( n->ml.cMoves ) {
	      PyObject* a = PyMoveAnalysis(&n->ml, ms);

	      if( a ) {
		DictSetItemSteal(analysis, "moves", a);

		DictSetItemSteal(analysis, "imove", PyInt_FromLong(n->iMove));
	      }
	    }

	    addLuck(analysis, n->rLuck, n->lt);
	    addSkill(analysis, n->stMove, 0);
	    addSkill(analysis, n->stCube, "cube-skill");
	  }
	    
	  break;
	}
	case MOVE_DOUBLE:
	{
	  const movedouble* d = &pmr->d;
	  action = "double";
	  player = d->fPlayer;

	  if( includeBoards ) {
	    DictSetItemSteal(recordDict, "board",
			     PyString_FromString(PositionID(anBoard)));
	  }
	    
	  if( analysis ) {
	    const cubedecisiondata* c = d->CubeDecPtr;
	    if( c->esDouble.et != EVAL_NONE ) {
	      PyObject* d = PyDoubleAnalysis(&c->esDouble, c->aarOutput,
					     c->aarStdDev, ms, verbose);
	      {
		int s = PyDict_Merge(analysis, d, 1);     assert( s != -1 );
	      }
	      Py_DECREF(d);
	    }
	  
	    addSkill(analysis, d->st, 0);
	  }
	    
	  break;
	}
	case MOVE_TAKE:
	{
	  const movedouble* d = &pmr->d;
	  action = "take";
	  player = d->fPlayer;

	  /* use nAnimals to point to double analysis ? */
	  
	  addSkill(analysis, d->st, 0);
	    
	  break;
	}
	case MOVE_DROP:
	{
	  const movedouble* d = &pmr->d;
	  action = "drop";
	  player = d->fPlayer;
	    
	  addSkill(analysis, d->st, 0);
	    
	  break;
	}
	case MOVE_RESIGN:
	{
	  const moveresign* r = &pmr->r;
	  action = "resign";
	  player = r->fPlayer;
	  break;
	}
	
	case MOVE_SETBOARD:
	{
	  const movesetboard* sb = &pmr->sb;
	  PyObject* id = PyString_FromString(PositionIDFromKey(sb->auchKey));

	  action = "set";

	  DictSetItemSteal(recordDict, "board", id);

	  if( includeBoards ) {
	    /* (FIXME) what about side? */
	    PositionFromKey(anBoard, sb->auchKey);
	  }
	  
	  break;
	}
	    
	case MOVE_SETDICE:
	{
	  const movesetdice* sd = &pmr->sd;
          PyObject *dice;

	  player = sd->fPlayer;
	  action = "set";

	  dice = Py_BuildValue("(ii)", sd->anDice[0], sd->anDice[1]);

	  DictSetItemSteal(recordDict, "dice", dice);
	    
	  addLuck(analysis, sd->rLuck, sd->lt);
	    
	  break;
	}
	    
	case MOVE_SETCUBEVAL:
	{
	  const movesetcubeval* scv = &pmr->scv;
	  action = "set";
	  DictSetItemSteal(recordDict, "cube", PyInt_FromLong(scv->nCube));
	  break;
	}
	    
	case MOVE_SETCUBEPOS:
	{
	  const movesetcubepos* scp = &pmr->scp;
	  const char* s[] = {"centered", "X", "O"};
	  const char* o = s[scp->fCubeOwner + 1];
	  
	  action = "set";
	  DictSetItemSteal(recordDict, "cube-owner",
			       PyString_FromString(o));
	  break;
	}

	default:
	{
	  assert(0);
	}
      }

      if( action ) {
	DictSetItemSteal(recordDict, "action",
			     PyString_FromString(action));
      }
      
      if( player != -1 ) {
	DictSetItemSteal(recordDict, "player",
			     PyString_FromString(player ? "O" : "X"));
      }

      if( analysis ) {
	if( PyDict_Size(analysis) > 0 ) {
	  DictSetItemSteal(recordDict, "analysis", analysis);
	} else {
	  Py_DECREF(analysis); analysis = 0;
	}
      }
      
      if( pmr->a.sz ) {
	DictSetItemSteal(recordDict, "comment",
			     PyString_FromString(pmr->a.sz));
      }

      PyTuple_SET_ITEM(gameTuple, nRecords, recordDict);
      ++nRecords;
    }

    DictSetItemSteal(gameDict, "game", gameTuple);
  }

  return gameDict;
}

static void
addProperty(PyObject* dict, const char* name, const char* val)
{
  if( ! val ) {
    return;
  }

  DictSetItemSteal(dict, name, PyString_FromString(val));
}
  
static PyObject*
PythonMatch(PyObject* self IGNORE, PyObject* args, PyObject* keywds)
{
  /* take match info from first game */
  const list* firstGame = lMatch.plNext->p;
  const moverecord* pmr;
  const movegameinfo* g;
  int includeAnalysis = 1;
  int verboseAnalysis = 0;
  int statistics = 0;
  int boards = 1;
  PyObject* matchDict;
  PyObject* matchInfoDict;
  PyMatchState s;
  
  static char* kwlist[] = {"analysis", "boards", "statistics", "verbose", 0};
  
  if( ! firstGame ) {
    Py_INCREF(Py_None);
    return Py_None;
  }
  
  pmr = firstGame->plNext->p;
  {                                       assert( pmr->mt == MOVE_GAMEINFO ); }
  g = &pmr->g;

  if( !PyArg_ParseTupleAndKeywords(args, keywds, "|iiii", kwlist,
				   &includeAnalysis, &boards, &statistics,
				   &verboseAnalysis) )
    return 0;

  
  matchDict = PyDict_New();
  matchInfoDict = PyDict_New();

  if( !matchDict && !matchInfoDict ) {
    PyErr_SetString(PyExc_MemoryError, "");
    return 0;
  }

  PushLocale("C");

  assert( g->i == 0 );

  /* W,X,0 
     B,O,1 */
  {
    int side;
    for(side = 0; side < 2; ++side) {
      PyObject* d = PyDict_New();
      addProperty(d, "rating", mi.pchRating[side]);
      addProperty(d, "name", ap[side].szName);

      DictSetItemSteal(matchInfoDict, side == 0 ? "X" : "O", d);
    }
  }

  DictSetItemSteal(matchInfoDict, "match-length", PyInt_FromLong(g->nMatch));
  
  if( mi.nYear ) {
    PyObject* date =  Py_BuildValue("(iii)", mi.nDay, mi.nMonth, mi.nYear);

    DictSetItemSteal(matchInfoDict, "date", date);
  }

  addProperty(matchInfoDict, "event", mi.pchEvent);
  addProperty(matchInfoDict, "round", mi.pchRound);
  addProperty(matchInfoDict, "place", mi.pchPlace);
  addProperty(matchInfoDict, "annotator", mi.pchAnnotator);
  addProperty(matchInfoDict, "comment", mi.pchComment);

  {
    char* v[] = { "Standard", "Nackgammon", "Hypergammon1", "Hypergammon2",
		  "Hypergammon3" };

    addProperty(matchInfoDict, "variation", v[g->bgv]);
  }
  
  {
    unsigned int n = !g->fCubeUse + !!g->fCrawford + !!g->fJacoby;
    if( n ) {
      PyObject* rules = PyTuple_New(n);

      n = 0;
      if( !g->fCubeUse ) {
	PyTuple_SET_ITEM(rules, n++, PyString_FromString("NoCube"));
      }
      if( g->fCrawford ) {
	PyTuple_SET_ITEM(rules, n++, PyString_FromString("Crawford"));
      }
      if( g->fJacoby ) {
	PyTuple_SET_ITEM(rules, n++, PyString_FromString("Jacoby"));
      }

      DictSetItemSteal(matchInfoDict, "rules", rules);
    }
  }

  DictSetItemSteal(matchDict, "match-info", matchInfoDict);

  s.ec = 0;
  s.rc = 0;
  
  {
    int nGames = 0;
    const list* pl;
    PyObject *matchTuple;
    statcontext scMatch;

    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext ) {
      ++nGames;
    }

    if( statistics ) {
      IniStatcontext(&scMatch);
    }
    
    matchTuple = PyTuple_New(nGames);

    nGames = 0;
    for(pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext) {
      PyObject* g = PythonGame(pl->p, includeAnalysis, verboseAnalysis,
			       statistics ? &scMatch : 0 , boards, &s);

      if( ! g ) {
	/* Memory leaked. out of memory anyway */
	return 0;
      }
      
      PyTuple_SET_ITEM(matchTuple, nGames, g);
      ++nGames;
    }

    DictSetItemSteal(matchDict, "games", matchTuple);

    if( statistics ) {
      PyObject* s = PyGameStats(&scMatch);

      if( s ) {
	DictSetItemSteal(matchDict, "stats", s);
      }
    }
  }

  if( s.ec ) {
    PyObject* e = EvalContextToPy(s.ec);
    DictSetItemSteal(matchInfoDict, "default-eval-context", e);
  }
  
  if( s.rc ) {
    PyObject* e = RolloutContextToPy(s.rc);
    DictSetItemSteal(matchInfoDict, "default-rollout-context", e);

    /* No need for that, I think */
/*     DictSetItemSteal(matchInfoDict, "sgf-rollout-version", */
/* 		     PyInt_FromLong(SGF_ROLLOUT_VER)); */
  }

  PopLocale();

  return matchDict;
}


static PyObject*
PythonNavigate(PyObject* self IGNORE, PyObject* args, PyObject* keywds)
{
  int nextRecord = INT_MIN;
  int nextGame = INT_MIN;
  int gamesDif = 0;
  int recordsDiff = 0;
  PyObject *r;
  
  static char* kwlist[] = {"next", "game", 0};

  if( ! lMatch.plNext ) {
    PyErr_SetString(PyExc_RuntimeError, "no active match");
    return 0;
  }
    
  if( !PyArg_ParseTupleAndKeywords(args, keywds, "|ii", kwlist,
				   &nextRecord, &nextGame) )
    return 0;


  r = 0;
  
  if( nextRecord == INT_MIN && nextGame == INT_MIN ) {
    /* no args, go to start */
    ChangeGame( lMatch.plNext->p );
  } else {

    if( nextRecord != INT_MIN && nextRecord < 0 ) {
      PyErr_SetString(PyExc_ValueError, "negative next record");
      return 0;
    }
  
    if( nextGame != INT_MIN && nextGame != 0 ) {
      list* pl= lMatch.plNext;

      for( ; pl->p != plGame && pl != &lMatch; pl = pl->plNext)
	;	

      {                                            assert( pl->p == plGame ); }
      {
	int n = nextGame;
	if( n > 0 ) {
	  while( n > 0 && pl->plNext->p ) {
	    pl = pl->plNext;
	    --n;
	  }
	} else if( n < 0 ) {
	  while( n < 0 && pl->plPrev->p ) {
	    pl = pl->plPrev;
	    ++n;
	  }
	}
	ChangeGame(pl->p);

	gamesDif = abs(nextGame) - n;
      }
    }
  
    if( nextRecord != INT_MIN ) {
      recordsDiff = nextRecord - InternalCommandNext(0, nextRecord);
    }

    /* (HACK)
       If normal move, set global dice for export and such.
     */
    if ( plLastMove->plNext && plLastMove->plNext->p ) {
      const moverecord* r = (const moverecord*)(plLastMove->plNext->p);
      
      if( r->mt == MOVE_NORMAL ) {
	memcpy(ms.anDice, r->n.anRoll, sizeof(ms.anDice));
      }
    }
    
    if( recordsDiff || gamesDif ) {
      r = Py_BuildValue("(ii)", recordsDiff, gamesDif);
    }
  }
  
  /* (HACK) */
  if( ms.gs ==  GAME_NONE) {
    ms.gs = GAME_PLAYING;
  }

  if( ! r ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }

  return r;
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
  { "match", (PyCFunction)PythonMatch, METH_VARARGS|METH_KEYWORDS,
    "Get the current match" },
  { "navigate", (PyCFunction)PythonNavigate, METH_VARARGS|METH_KEYWORDS,
    "" },
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

  PySys_SetArgv( 1, (char **) &argv0 );

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
