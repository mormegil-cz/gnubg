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


def PyMySQLConnect(database, user, password, hostname):
    global connection

    try:
        import MySQLdb
    except:
        # See if pymsql (pure Python replacement) is available. Works on MS
        # Windows
        import pymysql as MySQLdb

    hostport = hostname.strip().split(':')
    try:
        mysql_host = hostport[0]
    except:
        mysql_host = 'localhost'

    try:
        mysql_portstr = hostport[1]
        try:
            mysql_port = int(mysql_portstr)
        except:
            return -1
    except:
        mysql_port = 3306

    try:
        connection = MySQLdb.connect(
            host=mysql_host, port=mysql_port, db=database, user=user, passwd=password)
        return 1
    except:
        # See if mysql is there
        try:
            connection = MySQLdb.connect(
                host=mysql_host, port=mysql_port, user=user, passwd=password)
            # See if database present
            cursor = connection.cursor()
            r = cursor.execute(
                'show databases where `Database` = "' + database + '";')
            if (r != 0):
                return -2  # failed
            cursor.execute('create database ' + database)
            connection = MySQLdb.connect(
                host=mysql_host, port=mysql_port, db=database, user=user, passwd=password)
            return 0
        except:
            return -1  # failed

    return connection


def PyPostgreConnect(database, user, password, hostname):
    global connection
    import pgdb

    postgres_host = hostname.strip()
    try:
        connection = pgdb.connect(
            host=postgres_host, user=user, password=password, database=database)
        return 1
    except:
        # See if postgres is there
        try:
            # See if database present
            connection = pgdb.connect(
                host=postgres_host, user=user, password=password, database='postgres')

            cursor = connection.cursor()
            cursor.execute(
                'select datname from pg_database where datname=\'' + database + '\'')
            r = cursor.fetchone()
            if (r is not None):
                return -2  # database found

            cursor.execute('END')
            cursor.execute('create database ' + database)
            connection = pgdb.connect(
                host=postgres_host, database=database, user=user, password=password)
            return 0
        except:
            return -1  # failed

    return connection


def PySQLiteConnect(dbfile):
    global connection
    from sqlite3 import dbapi2 as sqlite
    connection = sqlite.connect(dbfile)
    return connection


def PyDisconnect():
    global connection
    try:
        connection.close()
    except:
        pass


def PySelect(str):
    global connection
    cursor = connection.cursor()
    try:
        cursor.execute("SELECT " + str)
    except:
        return 0

    all = list(cursor.fetchall())
    if (len(all) > 0):
        titles = [cursor.description[i][0]
                  for i in range(len(cursor.description))]
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
