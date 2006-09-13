/*
 * relational.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2004.
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
#endif

#if USE_PYTHON

#include "gnubgmodule.h"

#if USE_GTK
#include "gtkgame.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <glib.h>

#include "relational.h"
#include "backgammon.h"
#include "positionid.h"
#include "rollout.h"
#include <glib/gi18n.h>


static void
LoadDatabasePy( void ) {

  static int fPyLoaded = FALSE;
  char *pch;

  if ( fPyLoaded )
    return;

  fPyLoaded = TRUE;

  pch = g_strdup( "database.py" );
  CommandLoadPython( pch );
  g_free( pch );

}

static PyObject *Connect()
{
	PyObject *m, *d, *v, *r;

	/* load database.py */
	LoadDatabasePy();

	/* connect to database */
	if (!(m = PyImport_AddModule("__main__")))
	{
		outputl( _("Error importing __main__") );
		return NULL;
	}

	d = PyModule_GetDict(m);

	/* create new object */
	if (!(r = PyRun_String("relational()", Py_eval_input, d, d)))
	{
		PyErr_Print();
		return NULL;
	}
	else if (r == Py_None)
	{
		outputl( _("Error calling relational()") );
		return NULL;
	}

	/* connect to database */
	if (!(v = PyObject_CallMethod(r, "connect", "" )))
	{
		PyErr_Print();
		Py_DECREF(r);
		return NULL;
	}
	else if (v == Py_None)
	{
		outputl( _("Error connecting to database") );
		Py_DECREF(r);
		return NULL;
	}
	else 
	{
		Py_DECREF(v);
		return r;
	}
}

static void Disconnect(PyObject *r)
{
	PyObject *v;
	if (!(v = PyObject_CallMethod(r, "disconnect", "")))
	{
		PyErr_Print();
	}
	else
	{
		Py_DECREF(v);
	}

	Py_DECREF(r);
}

extern int RelationalMatchExists()
{
  int ret = -1;
  PyObject *v, *r;

  if (!(r = Connect()))
	  return -1;

  /* Check if match is in database */
  if (!(v = PyObject_CallMethod(r, "is_existing_match", "O", PythonMatchChecksum(0, 0))))
  {
    PyErr_Print();
    Py_DECREF(r);
    return -1;
  }
  else
  {
    if (v == Py_None)
		ret = 0;
	else if (PyInt_Check(v))
		ret = 1;

    Py_DECREF( v );
  }

  Disconnect(r);

  return ret;
}

int GameOver()
{
  	int anFinalScore[2];
	int nMatch;
	const list* firstGame = lMatch.plNext->p;
	if (firstGame)
	{
		const moverecord* pmr = firstGame->plNext->p;
		if (pmr)
		{
			assert(pmr->mt == MOVE_GAMEINFO);
			nMatch = pmr->g.nMatch;
			if (ms.nMatchTo)
			{	/* Match - check someone has won */
				return (getFinalScore(anFinalScore) && ((anFinalScore[0] >= nMatch)
					|| (anFinalScore[1] >= nMatch)));
			}
			else
			{	/* Session - check game over */
				return (ms.gs == GAME_OVER);
			}
		}
	}
	return FALSE;
}

int MoveAnalysed(moverecord *pmr, matchstate *pms, list *plGame,
              evalsetup *pesChequer, evalsetup *pesCube,
              movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ])
{
    static int anBoardMove[ 2 ][ 25 ];
    static unsigned char auch[ 10 ];
    static cubeinfo ci;
    static float rSkill, rChequerSkill;
    static evalsetup esDouble; /* shared between the
				  double and subsequent take/drop */
    doubletype dt;
    taketype tt;
    const xmovegameinfo* pmgi = &((moverecord *) plGame->plNext->p)->g;
    int    is_initial_position = 1;

    /* analyze this move */

    FixMatchState ( pms, pmr );

    /* check if it's the initial position: no cube analysis and special
       luck analysis */

    if (pmr->mt != MOVE_GAMEINFO)
	{
      InitBoard( anBoardMove, pms->bgv );
      is_initial_position = !memcmp( anBoardMove, pms->anBoard, 2 * 25 * sizeof ( int ) );
    }

    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	break;

    case MOVE_NORMAL:
		if (pmr->fPlayer != pms->fMove)
		{
			SwapSides(pms->anBoard);
		    pms->fMove = pmr->fPlayer;
		}

		rSkill = rChequerSkill = 0.0f;
		GetMatchStateCubeInfo( &ci, pms );
      
		/* cube action? */
      
		if (!is_initial_position && pmgi->fCubeUse && GetDPEq ( NULL, NULL, &ci ) )
		{
			if (pmr->CubeDecPtr->esDouble.et == EVAL_NONE)
				return FALSE;
		}

		/* luck analysis */
		if (pmr->rLuck == ERR_VAL)
			return FALSE;

		/* evaluate move */
      
	    memcpy( anBoardMove, pms->anBoard,
		    sizeof( anBoardMove ) );
	    ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
	    PositionKey ( anBoardMove, auch );
	  
		if (pmr->esChequer.et == EVAL_NONE && pmr->n.iMove != -1)
			return FALSE;
      
	break;
      
    case MOVE_DOUBLE:

      /* always analyse MOVE_DOUBLEs as they are shared with the subsequent
         MOVE_TAKEs or MOVE_DROPs. */

        dt = DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );

        if ( dt != DT_NORMAL )
          break;

	/* cube action */	    
	if (pmgi->fCubeUse)
	{
	    GetMatchStateCubeInfo( &ci, pms );
	  
	    if ( GetDPEq ( NULL, NULL, &ci ) ||
                 ci.fCubeOwner < 0 || ci.fCubeOwner == ci.fMove )
		{
			if (pmr->CubeDecPtr->esDouble.et == EVAL_NONE)
				return FALSE;
	    }
	}
      
	break;
      
    case MOVE_TAKE:

        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;
      
		if( fAnalyseCube && pmgi->fCubeUse && esDouble.et != EVAL_NONE )
		{
			GetMatchStateCubeInfo( &ci, pms );
			if (pmr->CubeDecPtr->esDouble.et == EVAL_NONE)
				return FALSE;
		}

	break;
      
    case MOVE_DROP:
      
        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;

		if (pmr->CubeDecPtr->esDouble.et == EVAL_NONE)
			return FALSE;

		break;
      
    case MOVE_RESIGN:

      /* swap board if player not on roll resigned */
      if( pmr->fPlayer != pms->fMove ) {
        SwapSides( pms->anBoard );
        pms->fMove = pmr->fPlayer;
      }
      
      if ( pesCube->et != EVAL_NONE ) {
        
        int nResign;
        float rBefore, rAfter;

        GetMatchStateCubeInfo ( &ci, pms );

        if ( cmp_evalsetup ( pesCube, &pmr->r.esResign ) > 0 ) {
          nResign =
            getResignation ( pmr->r.arResign, pms->anBoard, 
                             &ci, pesCube );
          
        }

        getResignEquities ( pmr->r.arResign, &ci, pmr->r.nResigned,
                            &rBefore, &rAfter );

        pmr->r.esResign = *pesCube;

        pmr->r.stResign = pmr->r.stAccept = SKILL_NONE;

        if ( rAfter < rBefore ) {
          /* wrong resign */
          pmr->r.stResign = Skill ( rAfter - rBefore );
          pmr->r.stAccept = SKILL_GOOD; /* VERYGOOD; */
        }

        if ( rBefore < rAfter ) {
          /* wrong accept */
          pmr->r.stAccept = Skill ( rBefore - rAfter );
          pmr->r.stResign = SKILL_GOOD; /* VERYGOOD; */
        }


      }

      break;
      
    case MOVE_SETDICE:
	if( pmr->fPlayer != pms->fMove ) {
	    SwapSides( pms->anBoard );
	    pms->fMove = pmr->fPlayer;
	}
      
        if ( afAnalysePlayers && ! afAnalysePlayers[ pmr->fPlayer ] )
          /* we do not analyse this player */
          break;
      
	GetMatchStateCubeInfo( &ci, pms );
	break;

    case MOVE_TIME: 
    case MOVE_SETBOARD:	  
    case MOVE_SETCUBEVAL:
    case MOVE_SETCUBEPOS:
      break;
	default:
		assert(0);
    }

	ApplyMoveRecord(pms, plGame, pmr);

    return TRUE;
}

int GameAnalysed(list *plGame)
{
	list *pl;
	moverecord *pmrx = (moverecord *) plGame->plNext->p; 
	matchstate msAnalyse;

	assert(pmrx->mt == MOVE_GAMEINFO);

	for (pl = plGame->plNext; pl != plGame; pl = pl->plNext)
	{
		if (!MoveAnalysed(pl->p, &msAnalyse, plGame,
								&esAnalysisChequer, &esAnalysisCube, aamfAnalysis))
			return FALSE;
	}

	return TRUE;
}

int MatchAnalysed()
{
	list *pl;

	for (pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext)
	{
		if (!GameAnalysed(pl->p))
			return FALSE;
	}
	return TRUE;
}

extern void
CommandRelationalAddMatch( char *sz ) {

  PyObject *v, *r;
  char* env;
  int force = FALSE;
  char *pch;
  char warnings[1024];
  *warnings = '\0';

  if (ListEmpty(&lMatch))
  {
      outputl( _("No match is being played.") );
      return;
  }

  /* Warn if match is not finished or fully analyzed */
  if (!GameOver())
	  strcat(warnings, _("The match is not finished\n"));
  if (!MatchAnalysed())
	  strcat(warnings, _("All of the match is not analyzed\n"));

  if (*warnings)
  {
	strcat(warnings, _("\nAdd match anyway?"));
	if (!GetInputYN(warnings))
	    return;
  }

  env = NextToken( &sz );
  if (!env)
	  env = "";

  pch = NextToken( &sz );
  force = pch && *pch && 
    ( !strcasecmp( "on", pch ) || !strcasecmp( "yes", pch ) ||
      !strcasecmp( "true", pch ) );

  if (!(r = Connect()))
	  return;

  /* add match to database */
  if ( ! ( v = PyObject_CallMethod( r, "addmatch", "si", env, force)) ) {
    PyErr_Print();
    Py_DECREF( r );
    return;
  }
  else {
    if ( PyInt_Check( v ) ) {
      int l = PyInt_AsLong( v );
      switch( l ) {
      case 0:
        outputl( _("Match succesfully added to database") );
        break;
      case -1:
        outputl( _("Error adding match to database") );
        break;
      case -2:
        outputl( _("No match is in progress") );
        break;
      case -3:
        outputl( _("Match is already in database") );
        break;
      case -4:
        outputl( _("Unknown environment") );
        break;
      default:
        outputf( _("Unknown return code %d from addmatch"), l );
        break;
      }
    }
    else {
      outputl( _("Hmm, addmatch return non-integer...") );
    }
    Py_DECREF( v );
  }

  Disconnect(r);

}

int env_added;	/* Horrid flag to see if next function worked... */

extern void CommandRelationalAddEnvironment(char *sz)
{
	PyObject *v, *r;

	env_added = FALSE;

	if (!sz || !*sz)
	{
		outputl( _("You must specify an environment name to add "
		"(see `help relational add environment').") );
		return;
	}

	if (!(r = Connect()))
		return;

	/* add environment to database */
	if (!(v = PyObject_CallMethod(r, "addenv", "s", sz)))
	{
		PyErr_Print();
		Py_DECREF(r);
		return;
	}
	if (PyInt_Check(v))
	{
		int l = PyInt_AsLong( v );
		if (l == -1)
			outputl( _("Error adding environment to database") );
		else if (l == 1)
		{
			outputl( _("Environment succesfully added to database") );
			env_added = TRUE;
		}
		else if (l == -2)
			outputl( _("That environment already exists") );
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

}

extern void
CommandRelationalTest( char *sz ) {

  PyObject *v, *r;

  if (!(r = Connect()))
	  return;

  if ( ! ( v = PyObject_CallMethod( r, "test", "" ) ) ) {
    PyErr_Print();
  }
  else {
    if ( PyInt_Check( v ) ) {
      int l = PyInt_AsLong( v );
      switch( l ) {
      case 0:
        outputl( _("Database test is successful!") );
        break;
      case -1:
        outputl( _("Database connection test failed!\n"
                   "Check that you've created the gnubg database "
                   "and that your database manager is running." ) );
        break;
      case -2:
        outputl( _("Database table check failed!\n"
                   "The table gnubg.match is missing.") );
        break;
      default:
        outputl( _("Database test failed with unknown error!") );
        break;
      }
    }
    else
      outputl( _("Hmm, test returned non-integer") );
    Py_DECREF( v );
  }

  Disconnect(r);

}

extern void
CommandRelationalHelp( char *sz ) {
	LoadDatabasePy();
  CommandNotImplemented( sz );
}

extern void
CommandRelationalShowEnvironments( char *sz )
{
	/* Use the Select command */
	CommandRelationalSelect("place FROM env "
                         "ORDER BY env_id");

}

extern void
CommandRelationalShowDetails( char *sz )
{
	PyObject *v, *r;

	char *player_name, *env;

	if (!sz || !*sz || !(player_name = NextToken(&sz)))
	{
		outputl( _("You must specify a player name to list the details for "
		"(see `help relational show details').") );
		return;
	}
	env = NextToken(&sz);
	if (!env)
		env = "";

	r = Connect();

	/* list env */
	if (!(v = PyObject_CallMethod(r, "list_details", "ss", player_name, env)))
	{
		PyErr_Print();
		return;
	}

	if (PySequence_Check(v))
	{
		int i;

		for ( i = 0; i < PySequence_Size( v ); ++i )
		{
			PyObject *e = PySequence_GetItem(v, i);

			if ( !e )
			{
				outputf( _("Error getting item no %d\n"), i );
				continue;
			}

			if (PySequence_Check(e))
			{
				PyObject *detail = PySequence_GetItem(e, 0);
				PyObject *value = PySequence_GetItem(e, 1);

				if (PyInt_Check(value))
					outputf( _("%s: %d\n"),
						PyString_AsString(detail),
						(int)PyInt_AsLong(value));
				else if (PyFloat_Check(value))
					outputf( _("%s: %.2f\n"),
						PyString_AsString(detail),
						PyFloat_AsDouble(value));
				else
					outputf( _("unknown type"));

				Py_DECREF(detail);
				Py_DECREF(value);
			}
			else
			{
				outputf( _("Item no. %d is not a sequence\n"), i );
				continue;
			}
			Py_DECREF( e );
		}
		outputl( "" );
	}
	else
	{
		if (PyInt_Check(v))
		{
			int l = PyInt_AsLong(v);
			if (l == -1)
				outputl( _("Player not in database") );
			else if (l == -4)
				outputl( _("Unknown environment") );
			else
				outputl( _("unknown return value") );
		}
		else
			outputl( _("invalid return type") );
	}

	Py_DECREF( v );

	Disconnect(r);

}

extern void
CommandRelationalShowPlayers( char *sz )
{
	/* Use the Select command */
	CommandRelationalSelect("person.name AS Player, nick.name AS Nickname, env.place AS env"
		" FROM nick INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person ON nick.person_id = person.person_id"
		" ORDER BY person.name");

}

extern void CommandRelationalErase(char *sz)
{
	PyObject *v, *r;
	char *player_name, *env;

	if (!sz || !*sz || !(player_name = NextToken(&sz)))
	{
		outputl( _("You must specify a player name to remove "
		"(see `help relational erase player').") );
		return;
	}
	env = NextToken(&sz);
	if (!env)
		env = "";

	r = Connect();

	/* remove player */
	if (!(v = PyObject_CallMethod(r, "erase_player", "ss", player_name, env)))
	{
		PyErr_Print();
		return;
	}

	if (PyInt_Check(v))
	{
		int l = PyInt_AsLong( v );
		if (l == -1)
			outputl( _("Player not in database") );
		else if (l == 1)
			outputl( _("player removed from database") );
		else if (l == -4)
			outputl( _("Unknown environment") );
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

}

extern void RelationalLinkNick(char* nick, char* env, char* player)
{	/* Link nick on env to player */
	PyObject *v, *r;

	r = Connect();

	if (!(v = PyObject_CallMethod(r, "link_players", "sss", nick, env, player)))
	{
		PyErr_Print();
		return;
	}

	if (PyInt_Check(v))
	{
		int l = PyInt_AsLong( v );
		if (l == -1)
			outputl( _("Failed to link players") );
		else if (l == 1)
			outputl( _("Players linked successfully") );
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

}

extern void CommandRelationalRenameEnv(char *sz)
{
	PyObject *v, *r;
	char *env_name, *new_name;

	if (!sz || !*sz || !(env_name = NextToken(&sz))
		|| !(new_name = NextToken(&sz)))
	{
		outputl( _("You must specify an environment to rename and "
			"a new name (see `help relational rename environment').") );
		return;
	}

	r = Connect();

	/* rename env */
	if (!(v = PyObject_CallMethod(r, "rename_env", "ss", env_name, new_name)))
	{
		PyErr_Print();
		return;
	}

	if (PyInt_Check(v))
	{
		int l = PyInt_AsLong( v );
		if (l == -1)
			outputl( _("Environment not in database") );
		else if (l == 1)
			outputl( _("Environment renamed") );
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

}

int env_deleted;	/* Horrid flag to see if next function worked... */

extern void CommandRelationalEraseEnv(char *sz)
{
	PyObject *v, *r;

	env_deleted = FALSE;
	if (!sz || !*sz)
	{
		outputl( _("You must specify an environment to remove "
		"(see `help relational erase environment').") );
		return;
	}

	if (fConfirmSave && !GetInputYN( _("Are you sure you want to erase the "
		"environment and all related data?") ))
		return;

	r = Connect();

	/* erase env */
	if (!(v = PyObject_CallMethod(r, "erase_env", "s", sz)))
	{
		PyErr_Print();
		return;
	}

	if (PyInt_Check(v))
	{
		int l = PyInt_AsLong( v );
		if (l == -1)
			outputl( _("Environment not in database") );
		else if (l == -2)
			outputl( _("You must keep at least one environment in the database") );
		else if (l == 1)
		{
			outputl( _("Environment removed from database") );
			env_deleted = TRUE;
		}
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

}

extern void CommandRelationalEraseAll(char *sz)
{
	PyObject *v, *r;

	if( fConfirmSave && !GetInputYN( _("Are you sure you want to erase all "
				       "player records?") ) )
		return;

	r = Connect();

	/* clear database */
	if (!(v = PyObject_CallMethod(r, "erase_all", "")))
	{
		PyErr_Print();
		return;
	}

	if (PyInt_Check(v))
	{
		int l = PyInt_AsLong( v );
		if (l == 1)
			outputl( _("all players removed from database") );
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

}


extern void FreeRowset(RowSet* pRow)
{
	int i, j;
	free(pRow->widths);

	for (i = 0; i < pRow->rows; i++)
	{
		for (j = 0; j < pRow->cols; j++)
		{
			free (pRow->data[i][j]);
		}
		free(pRow->data[i]);
	}
	free(pRow->data);

	pRow->cols = pRow->rows = 0;
	pRow->data = NULL;
	pRow->widths = NULL;
}

void MallocRowset(RowSet* pRow, int rows, int cols)
{
	int i;
	pRow->widths = malloc(cols * sizeof(int));
	memset(pRow->widths, 0, cols * sizeof(int));

	pRow->data = malloc(rows * sizeof(char*));
	for (i = 0; i < rows; i++)
	{
		pRow->data[i] = malloc(cols * sizeof(char*));
		memset(pRow->data[i], 0, cols * sizeof(char*));
	}

	pRow->cols = cols;
	pRow->rows = rows;
}

extern int UpdateQuery(char *sz)
{
	PyObject *v, *r;

	r = Connect();

	/* run query */
	if (!(v = PyObject_CallMethod(r, "update", "s", sz))
		|| v == Py_None)
	{
		outputl(_("Error running query"));
		PyErr_Print();
		return FALSE;
	}
	Py_DECREF(v);
	Disconnect(r);
	return TRUE;
}

extern int RunQuery(RowSet* pRow, char *sz)
{
	PyObject *v, *r;
	int i, j;

	r = Connect();

	/* enter query */
	if (!(v = PyObject_CallMethod(r, "select", "s", sz))
		|| v == Py_None)
	{
		outputerrf(_("Error running query"));
		PyErr_Print();
		return FALSE;
	}

	if (!PySequence_Check(v))
	{
		outputerrf( _("invalid return (non-tuple)") );
		Py_DECREF(v);
		Disconnect(r);
		return FALSE;
	}
		
	i = PySequence_Size(v);
	j = 0;
	if (i > 0)
	{
		PyObject *cols = PySequence_GetItem(v, 0);
		if (!PySequence_Check(cols))
		{
			outputerrf( _("invalid return (non-tuple)") );
			Py_DECREF(v);
			Disconnect(r);
			return FALSE;
		}
		else
			j = PySequence_Size(cols);
	}

	MallocRowset(pRow, i, j);
	for (i = 0; i < pRow->rows; i++)
	{
		PyObject *e = PySequence_GetItem(v, i);

		if (!e)
		{
			outputf(_("Error getting item no %d\n"), i);
			continue;
		}

		if (PySequence_Check(e))
		{
			int j, size;
			for (j = 0; j < pRow->cols; j++)
			{
				char buf[1024];
				PyObject *e2 = PySequence_GetItem(e, j);

				if (!e2)
				{
					outputf(_("Error getting sub item no (%d, %d)\n"), i, j);
					continue;
				}
        if (PyUnicode_Check(e2))
        {
					strcpy(buf, PyString_AsString(PyObject_Str(e2)));
					size = strlen(buf);
        }
				else if (PyString_Check(e2))
				{
					strcpy(buf, PyString_AsString(e2));
					size = strlen(buf);
				}
				else if (PyInt_Check(e2))
				{
					sprintf(buf, "%d", (int)PyInt_AsLong(e2));
					size = ((int)log((int)PyInt_AsLong(e2))) + 1;
				}
				else if (PyFloat_Check(e2))
				{
					sprintf(buf, "%.2f", PyFloat_AsDouble(e2));
					size = ((int)log(PyFloat_AsDouble(e2))) + 1 + 3;
				}
				else if (e2 == Py_None)
				{
					strcpy(buf, _("[null]"));
					size = strlen(buf);
				}
				else
				{
					strcpy(buf, _("[unknown type]"));
					size = strlen(buf);
				}

				if (i == 0 || size > pRow->widths[j])
					pRow->widths[j] = size;

				pRow->data[i][j] = malloc(strlen(buf) + 1);
				strcpy(pRow->data[i][j], buf);

				Py_DECREF(e2);
			}
		}
		else
		{
			outputf(_("Item no. %d is not a sequence\n"), i);
		}

		Py_DECREF(e);
	}

	Py_DECREF(v);
	Disconnect(r);

	return TRUE;
}

extern void CommandRelationalSelect(char *sz)
{
#if !USE_GTK
	int i, j;
#endif
	RowSet r;

	if (!sz || !*sz)
	{
		outputl( _("You must specify a sql query to run').") );
		return;
	}

	if (!RunQuery(&r, sz))
		return;

	if (r.rows == 0)
	{
		outputl(_("No rows found.\n"));
		return;
	}

#if USE_GTK
	GtkShowQuery(&r);
#else
	for (i = 0; i < r.rows; i++)
	{
		if (i == 1)
		{	/* Underline headings */
			char* line, *p;
			int k;
			int totalwidth = 0;
			for (k = 0; k < r.cols; k++)
			{
				totalwidth += r.widths[k] + 1;
				if (k != 0)
					totalwidth += 2;
			}
			line = malloc(totalwidth + 1);
			memset(line, '-', totalwidth);
			p = line;
			for (k = 0; k < r.cols - 1; k++)
			{
				p += r.widths[k];
				p[1] = '|';
				p += 3;
			}
			line[totalwidth] = '\0';
			outputl(line);
			free(line);
		}

		for (j = 0; j < r.cols; j++)
		{
			if (j > 0)
				output(" | ");

			outputf("%*s", r.widths[j], r.data[i][j]);
		}
		outputl("");
	}
#endif
	FreeRowset(&r);

}
extern void RelationalUpdatePlayerDetails(int player_id, const char* newName,
										  const char* newNotes)
{
	char query[1024];
	RowSet r;
	int curPlayerId = player_id;

	/* Can't change the name to an existing one */
	sprintf(query, "person_id FROM person WHERE name = '%s'", newName);
	if (!RunQuery(&r, query))
	{
		outputerrf( _("Error running database command") );
		return;
	}
	if (r.rows > 1)
    curPlayerId = atoi(r.data[1][0]);
	
	if (curPlayerId != player_id)
		outputerrf( _("Player name already exists.  Use the link button to combine different"
			" nicknames for the same player") );
	else
	{
		sprintf(query, "person SET name = \"%s\", notes = \"%s\" WHERE person_id = %d",
			newName, newNotes, player_id);
		if (!UpdateQuery(query))
			outputerrf( _("Error running database command") );
	}
	FreeRowset(&r);
}
#else
#include "backgammon.h"
#include <glib/gi18n.h>
extern void
CommandRelationalAddEnvironment (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalErase (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalRenameEnv (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalEraseEnv (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalEraseAll (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalSelect (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalAddMatch (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalTest (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalHelp (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalShowEnvironments (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalShowDetails (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
extern void
CommandRelationalShowPlayers (char *sz)
{
  outputl (_("This build was not compiled with support for Python.\n"));
}
#endif
