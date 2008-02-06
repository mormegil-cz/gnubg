#
# database.py
#
# by Joern Thyssen <jth@gnubg.org>, 2004
#
# This file contains the functions for adding matches to a relational
# database.
#
# The modules use the DB API V2 python modules. 
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 3 or later of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# $Id$
#

connection = 0

def PyMySQLConnect(database, user, password):
  global connection
  import MySQLdb
  try:
    connection = MySQLdb.connect( db = database, user = user, passwd = password )
    return 1
  except:
    # See if mysql is there
    try:
      connection = MySQLdb.connect()
      # See if database present
      cursor = connection.cursor()
      r = cursor.execute('show databases where `Database` = "' + database + '";')
      if (r != 0):
        return -2 # failed
      cursor.execute('create database ' + database)
      connection = MySQLdb.connect( db = database )
      return 0
    except:
      return -1 # failed
    
  return connection

def PyPostgreConnect(database, user, password):
  global connection
  import pgdb
  try:
    connection = pgdb.connect(database=database,user=user,password=password)
    return 1
  except:
    # See if postgres is there
    try:
      connection = pgdb.connect(user=user,password=password)
      # See if database present
      cursor = connection.cursor()
      cursor.execute('select datname from pg_database where datname=\'' + database + '\'')
      r = cursor.fetchone();
      if (r != None):
        return -2 # failed
      cursor.execute('end')
      cursor.execute('create database ' + database)
      connection = pgdb.connect(database=database,user="postgres",password="root")
      return 0
    except:
      return -1 # failed
    
  return connection

def PySQLiteConnect(dbfile):
  global connection
  from sqlite3 import dbapi2 as sqlite
  connection = sqlite.connect(dbfile)
  return connection

def PyDisconnect():
  global connection
  connection.close()

def PySelect(str):
  global connection
  cursor = connection.cursor()
  cursor.execute("SELECT " + str)
  all = list(cursor.fetchall())
  if (len(all) > 0):
    titles = [cursor.description[i][0] for i in range(len(cursor.description))]
    all.insert(0, titles)

  return all

def PyUpdateCommand(stmt):
  global connection
  cursor = connection.cursor()
  cursor.execute(stmt)

def PyUpdateCommandReturn(stmt):
  global connection
  cursor = connection.cursor()
  cursor.execute(stmt)
  return list(cursor.fetchall())

def PyCommit():
  global connection
  connection.commit()
