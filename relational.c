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

#include <stdio.h>


#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "i18n.h"
#include "relational.h"
#include "backgammon.h"

#if USE_PYTHON

#undef HAVE_FSTAT
#include <Python.h>

#if USE_GTK
#include "gtkgame.h"
#endif

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
#endif /* USE_PYTHON */

extern void
CommandRelationalAddMatch( char *sz ) {

#if USE_PYTHON
  PyObject *v, *r;
  int env_id = 0;
  int force = FALSE;
  char *pch;

  if (sz && *sz)
    env_id = ParseNumber( &sz );
  if ( env_id < 0 )
    env_id = 0;

  pch = NextToken( &sz );
  force = pch && *pch && 
    ( !strcasecmp( "on", pch ) || !strcasecmp( "yes", pch ) ||
      !strcasecmp( "true", pch ) );

  if (!(r = Connect()))
	  return;

  /* add match to database */
  if ( ! ( v = PyObject_CallMethod( r, "addmatch", "ii", env_id, force)) ) {
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

#else /* USE_PYTHON */
  outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */

}

extern void
CommandRelationalTest( char *sz ) {

#if USE_PYTHON
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

#else /* USE_PYTHON */
  outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */

}

extern void
CommandRelationalHelp( char *sz ) {
#if USE_PYTHON
	LoadDatabasePy();
#endif
  CommandNotImplemented( sz );
}

extern void
CommandRelationalShowEnvironments( char *sz )
{
#if USE_PYTHON
	/* Use the Select command */
	CommandRelationalSelect("env_id, place FROM env "
                         "ORDER BY env_id");

#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}

extern void
CommandRelationalShowDetails( char *sz )
{
#if USE_PYTHON
	PyObject *v, *r;

	if (!sz || !*sz)
	{
		outputl( _("You must specify a player name to list the details for "
		"(see `help relational show details').") );
		return;
	}

	r = Connect();

	/* list env */
	if (!(v = PyObject_CallMethod(r, "list_details", "s", sz)))
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
			else
				outputl( _("unknown return value") );
		}
		else
			outputl( _("invalid return type") );
	}

	Py_DECREF( v );

	Disconnect(r);

#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}

extern void
CommandRelationalShowPlayers( char *sz )
{
#if USE_PYTHON
	/* Use the Select command */
	CommandRelationalSelect("person.name AS Player, nick.name AS Nickname, env.place AS env"
		" FROM nick INNER JOIN env INNER JOIN person"
		" ON nick.env_id = env.env_id AND nick.person_id = person.person_id"
		" ORDER BY person.name");

#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}

extern void CommandRelationalErase(char *sz)
{
#if USE_PYTHON
	PyObject *v, *r;

	if (!sz || !*sz)
	{
		outputl( _("You must specify a player name to list the details for "
		"(see `help relational erase').") );
		return;
	}

	r = Connect();

	/* remove player */
	if (!(v = PyObject_CallMethod(r, "erase_player", "s", sz)))
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
		else
			outputl( _("unknown return value") );
	}
	else
		outputl( _("invalid return (non-integer)") );

	Py_DECREF(v);

	Disconnect(r);

#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}

extern void CommandRelationalEraseAll(char *sz)
{
#if USE_PYTHON
	PyObject *v, *r;

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

#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}

#if USE_PYTHON

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

#if USE_PYTHON
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
		outputl(_("Error running query"));
		PyErr_Print();
		return FALSE;
	}

	if (!PySequence_Check(v))
	{
		outputl( _("invalid return (non-tuple)") );
		Py_DECREF(v);
		Disconnect(r);
		return FALSE;
	}
		
	i = PySequence_Size(v);
	j = 0;
	if (PySequence_Size(v) > 0)
	{
		PyObject *cols = PySequence_GetItem(v, 0);
		if (!PySequence_Check(cols))
			j = 0;
		else
			j = PySequence_Size(cols);
	}
	if (i <= 1 || j == 0)
	{
		pRow->rows = pRow->cols = 0;
		Py_DECREF(v);
		Disconnect(r);
		return FALSE;
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

				if (PyString_Check(e2))
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
#endif
#endif

extern void CommandRelationalShowRecords(char *sz)
{
#if USE_PYTHON
#if USE_GTK
	GtkShowRelational();
#else
	CommandNotImplemented( sz );
#endif
#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}

extern void CommandRelationalSelect(char *sz)
{
#if USE_PYTHON
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

#else /* USE_PYTHON */
	outputl( _("This build was not compiled with support for Python.\n") );
#endif /* !USE_PYTHON */
}
#if USE_PYTHON
extern void RelationalUpdatePlayerDetails(int player_id, const char* newName,
										  const char* newNotes)
{
	char query[1024];
	sprintf(query, "person SET name = \"%s\", notes = \"%s\" WHERE person_id = %d",
		newName, newNotes, player_id);
	UpdateQuery(query);
}
#endif
