/*
 * dbprovider.c
 *
 * by Jon Kinsey, 2008
 *
 * Database providers
 *
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

#include "config.h"

#include <glib.h>
#include "dbprovider.h"
#include "gnubgmodule.h"
#include "backgammon.h"
#include "util.h"
#if USE_PYTHON
#include <Python.h>
#endif
#include <glib/gi18n.h>

DBProviderType dbProviderType = SQLITE;
#if USE_PYTHON
PyObject *pdict;
RowSet* ConvertPythonToRowset(PyObject *v);

int PySQLiteConnect(const char *dbfilename, const char *user, const char *password);
int PyMySQLConnect(const char *dbfilename, const char *user, const char *password);
int PyPostgreConnect(const char *dbfilename, const char *user, const char *password);
void PyDisconnect();
RowSet *PySelect(const char* str);
int PyUpdateCommand(const char* str);
void PyCommit();
GList *PySQLiteGetDatabaseList(const char *user, const char *password);
GList *PyMySQLGetDatabaseList(const char *user, const char *password);
GList *PyPostgreGetDatabaseList(const char *user, const char *password);
int PySQLiteDeleteDatabase(const char *dbfilename, const char *user, const char *password);
int PyMySQLDeleteDatabase(const char *dbfilename, const char *user, const char *password);
int PyPostgreDeleteDatabase(const char *dbfilename, const char *user, const char *password);
#endif
int SQLiteConnect(const char *dbfilename, const char *user, const char *password);
void SQLiteDisconnect();
RowSet *SQLiteSelect(const char* str);
int SQLiteUpdateCommand(const char* str);
void SQLiteCommit();
GList *SQLiteGetDatabaseList(const char *user, const char *password);
int SQLiteDeleteDatabase(const char *dbfilename, const char *user, const char *password);

DBProvider providers[NUM_PROVIDERS] =
{
#if USE_PYTHON
	{PySQLiteConnect, PyDisconnect, PySelect, PyUpdateCommand, PyCommit, PySQLiteGetDatabaseList,
		PySQLiteDeleteDatabase,
		"SQLite (Python)", "SQLite3 connection included in latest Python version", FALSE, "gnubg", "", ""},
	{PyMySQLConnect, PyDisconnect, PySelect, PyUpdateCommand, PyCommit, PyMySQLGetDatabaseList,
		PyMySQLDeleteDatabase,
		"MySQL (Python)", "MySQL connection via MySQLdb Python module", TRUE, "gnubg", "", ""},
	{PyPostgreConnect, PyDisconnect, PySelect, PyUpdateCommand, PyCommit, PyPostgreGetDatabaseList,
		PyPostgreDeleteDatabase,
		"Postgres (Python)", "PostgreSQL connection via PyGreSQL Python module", TRUE, "gnubg", "", ""},
#endif
	{SQLiteConnect, SQLiteDisconnect, SQLiteSelect, SQLiteUpdateCommand, SQLiteCommit, SQLiteGetDatabaseList,
		SQLiteDeleteDatabase,
		"SQLite", "Direct SQLite3 connection", FALSE, "gnubg", "", ""},
};

const char *dbTypes[NUM_PROVIDERS] = {
#if USE_PYTHON
	"PythonSQLite", "PythonMySQL", "PythonPostgre"
#endif
	"SQLite"
};

DBProviderType GetTypeFromName(const char *name)
{
	int i;
	for (i = 0; i < NUM_PROVIDERS; i++)
	{
		if (!StrCaseCmp(name, dbTypes[i]))
			break;
	}
	if (i == NUM_PROVIDERS)
		return INVALID_PROVIDER;

	return (DBProviderType)i;
}

void SetDBParam(const char *db, const char *key, const char *value)
{
	DBProviderType type = GetTypeFromName(db);
	if (type == INVALID_PROVIDER)
		return;

	if (!StrCaseCmp(key, "database"))
		providers[type].database = g_strdup(value);
	else if (!StrCaseCmp(key, "username"))
		providers[type].username = g_strdup(value);
	else if (!StrCaseCmp(key, "password"))
		providers[type].password = g_strdup(value);
}

void SetDBType(const char *type)
{
	dbProviderType = GetTypeFromName(type);
}

const char *GetDBType()
{
	return dbTypes[dbProviderType];
}

void SetDBSettings(DBProviderType dbType, const char *database, const char *user, const char *password)
{
	dbProviderType = dbType;
	providers[dbProviderType].database = g_strdup(database);
	providers[dbProviderType].username = g_strdup(user);
	providers[dbProviderType].password = g_strdup(password);
}

void RelationalSaveSettings(FILE *pf)
{
	int i;
	fprintf(pf, "relational setup dbtype=%s\n", GetDBType());
	for (i = 0; i < NUM_PROVIDERS; i++)
	{
		DBProvider* pdb = GetDBProvider(i);
		fprintf(pf, "relational setup %s-database=%s\n", dbTypes[i], pdb->database);
		fprintf(pf, "relational setup %s-username=%s\n", dbTypes[i], pdb->username);
		fprintf(pf, "relational setup %s-password=%s\n", dbTypes[i], pdb->password);
	}
}

extern DBProvider* GetDBProvider(DBProviderType dbType)
{
#ifdef USE_PYTHON
	static int setup = FALSE;
	if (!setup)
	{
		if (LoadPythonFile("database.py") == 0)
		{
			PyObject *m;
			/* Get main python dictionary */
			if ((m = PyImport_AddModule("__main__")) == NULL)
			{
				outputl( _("Error importing __main__") );
			}
			else
			{
				pdict = PyModule_GetDict(m);
				setup = TRUE;
			}
		}
		if (!setup)
			return NULL;
	}
#endif
	if (dbType == INVALID_PROVIDER)
		return NULL;

	return &providers[dbType];
}

#if USE_PYTHON
int PyMySQLConnect(const char *dbfilename, const char *user, const char *password)
{
	int iret;
	PyObject *ret;
	char *buf = g_strdup_printf("PyMySQLConnect(r'%s', '%s', '%s')", dbfilename, user, password);
	/* Connect to database*/
	ret = PyRun_String(buf, Py_eval_input, pdict, pdict);
	g_free(buf);
	if (ret == NULL || !PyInt_Check(ret) || (iret = PyInt_AsLong(ret)) < 0)
	{
		PyErr_Print();
		return -1;
	}
    else if (iret == 0) 
	{	/* New database - populate */
		return 0;
	}
	return 1;
}

int PyPostgreConnect(const char *dbfilename, const char *user, const char *password)
{
	int iret;
	PyObject *ret;
	char *buf = g_strdup_printf("PyPostgreConnect(r'%s', '%s', '%s')", dbfilename, user, password);
	/* Connect to database*/
	ret = PyRun_String(buf, Py_eval_input, pdict, pdict);
	g_free(buf);
	if (ret == NULL || !PyInt_Check(ret) || (iret = PyInt_AsLong(ret)) < 0)
	{
		PyErr_Print();
		return -1;
	}
    else if (iret == 0) 
	{	/* New database - populate */
		return 0;
	}
	return 1;
}

int PySQLiteConnect(const char *dbfilename, const char *user, const char *password)
{
	PyObject *con;
	char *name, *filename, *buf;
	int exists;

	name = g_strdup_printf("%s.db", dbfilename);
	filename = g_build_filename (szHomeDirectory, name, NULL);
	exists = g_file_test(filename, G_FILE_TEST_EXISTS);
	buf = g_strdup_printf("PySQLiteConnect(r'%s')", filename);

	/* Connect to database*/
	con = PyRun_String(buf, Py_eval_input, pdict, pdict);
	g_free(name);
	g_free(filename);
	g_free(buf);
	if (con == NULL)
	{
		PyErr_Print();
		return -1;
	}
	else if (con == Py_None)
	{
		outputl( _("Error calling connect()") );
		return -1;
	}
	if (!exists)
	{	/* Empty database file created - create tables */
		return 0;
	}
	else
		return 1;
}

void PyDisconnect()
{
	if (!PyRun_String("PyDisconnect()", Py_eval_input, pdict, pdict))
		PyErr_Print();
}

RowSet *PySelect(const char* str)
{
	PyObject *rs;
	char *buf = g_strdup_printf("PySelect(\"%s\")", str);

	/* Run select */
	rs = PyRun_String(buf, Py_eval_input, pdict, pdict);
	g_free(buf);

	if (rs)
	{
		return ConvertPythonToRowset(rs);
	}
	else
	{
		PyErr_Print();
		return NULL;
	}
}

int PyUpdateCommand(const char* str)
{
	char *buf = g_strdup_printf("PyUpdateCommand(\"%s\")", str);
	/* Run update */
	PyObject *ret = PyRun_String(buf, Py_eval_input, pdict, pdict);
	g_free(buf);
	if (!ret)
	{
		PyErr_Print();
		return FALSE;
	}
	else
		return TRUE;
}

int PyUpdateCommandReturn(const char* str)
{
	char *buf = g_strdup_printf("PyUpdateCommandReturn(\"%s\")", str);
	/* Run update */
	PyObject *ret = PyRun_String(buf, Py_eval_input, pdict, pdict);
	g_free(buf);
	if (!ret)
	{
		PyErr_Print();
		return FALSE;
	}
	else
		return TRUE;
}

void PyCommit()
{
	if (!PyRun_String("PyCommit()", Py_eval_input, pdict, pdict))
		PyErr_Print();
}

RowSet* ConvertPythonToRowset(PyObject *v)
{
	RowSet *pRow;
	size_t i, j;
	if (!PySequence_Check(v))
	{
		outputerrf( _("invalid return (non-tuple)") );
		return NULL;
	}

	i = PySequence_Size(v);
	j = 0;
	if (i > 0)
	{
		PyObject *cols = PySequence_GetItem(v, 0);
		if (!PySequence_Check(cols))
		{
			outputerrf( _("invalid return (non-tuple)") );
			return NULL;
		}
		else
			j = (int)PySequence_Size(cols);
	}

	pRow = MallocRowset(i, j);
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
			size_t j, size;
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
					strcpy(buf, PyString_AsString(PyUnicode_AsUTF8String(e2)));
					size = strlen(buf);
				}
				else if (PyString_Check(e2))
				{
					strcpy(buf, PyString_AsString(e2));
					size = strlen(buf);
				}
				else if (PyInt_Check(e2) || PyLong_Check(e2)
					|| !StrCaseCmp(e2->ob_type->tp_name, "Decimal"))	/* Not sure how to check for decimal type directly */
				{
					sprintf(buf, "%d", (int)PyInt_AsLong(e2));
					size = ((int)log((int)PyInt_AsLong(e2))) + 1;
				}
				else if (PyFloat_Check(e2))
				{
					sprintf(buf, "%.4f", PyFloat_AsDouble(e2));
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
	return pRow;
}

GList *PySQLiteGetDatabaseList(const char *user, const char *password)
{
	GList *glist = NULL;
	GDir *dir = g_dir_open(szHomeDirectory, 0, NULL);
	if (dir)
	{
		const char *filename;
		while ((filename = g_dir_read_name(dir)) != NULL)
		{
			int len = strlen(filename);
			if (len > 3 && !StrCaseCmp(filename + len - 3, ".db"))
			{
				char *db = g_strdup(filename);
				db[len - 3] = '\0';
				glist = g_list_append(glist, db);
			}
		}
		g_dir_close(dir);
	}
	return glist;
}

GList *PyMySQLGetDatabaseList(const char *user, const char *password)
{
	PyObject *rs;

	if (PyMySQLConnect("", user, password) < 0)
		return NULL;

	rs = PyRun_String("PyUpdateCommandReturn(\"Show databases\")", Py_eval_input, pdict, pdict);
	if (rs)
	{
		int i;
		GList *glist = NULL;
		RowSet* list = ConvertPythonToRowset(rs);
		for (i = 0; i < list->rows; i++)
			glist = g_list_append(glist, g_strdup(list->data[i][0]));
		FreeRowset(list);
		return glist;
	}
	else
	{
		PyErr_Print();
		return NULL;
	}
}

GList *PyPostgreGetDatabaseList(const char *user, const char *password)
{
	RowSet *rs;

	if (PyPostgreConnect("", user, password) < 0)
		return NULL;

	rs = PySelect("datname from pg_database");
	if (rs)
	{
		int i;
		GList *glist = NULL;
		for (i = 0; i < rs->rows; i++)
			glist = g_list_append(glist, g_strdup(rs->data[i][0]));
		FreeRowset(rs);
		return glist;
	}
	else
	{
		PyErr_Print();
		return NULL;
	}
}

int PySQLiteDeleteDatabase(const char *dbfilename, const char *user, const char *password)
{
	char *name, *filename;
	int ret;
	name = g_strdup_printf("%s.db", dbfilename);
	filename = g_build_filename (szHomeDirectory, name, NULL);
	/* Delete database file */
	ret = g_unlink(filename);

	g_free(name), g_free(filename);
	return (ret == 0);
}

int PyMySQLDeleteDatabase(const char *dbfilename, const char *user, const char *password)
{
	char *buf;
	int ret;
	if (PyMySQLConnect(dbfilename, user, password) < 0)
		return FALSE;

	buf = g_strdup_printf("DROP DATABASE %s", dbfilename);
	ret = PyUpdateCommand(buf);
	g_free(buf);
	return ret;
}

int PyPostgreDeleteDatabase(const char *dbfilename, const char *user, const char *password)
{
	char *buf;
	int ret;
	if (PyPostgreConnect(dbfilename, user, password) < 0)
		return FALSE;

	buf = g_strdup_printf("DROP DATABASE %s", dbfilename);
	ret = PyUpdateCommand(buf);
	g_free(buf);
	return ret;
}
#endif

int SQLiteConnect(const char *dbfilename, const char *user, const char *password)
{
	return 0;
}

void SQLiteDisconnect()
{
}

RowSet *SQLiteSelect(const char* str)
{
	return 0;
}

int SQLiteUpdateCommand(const char* str)
{
	return 0;
}

void SQLiteCommit()
{
}

GList *SQLiteGetDatabaseList(const char *user, const char *password)
{
	return 0;
}

int SQLiteDeleteDatabase(const char *dbfilename, const char *user, const char *password)
{
	return 0;
}
