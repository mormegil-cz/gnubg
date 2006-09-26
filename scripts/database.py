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
# it under the terms of version 2 of the GNU General Public License as
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
                                                                                  

import gnubg
import sys
import os
import traceback
import string
import re

# different database types
DB_SQLITE = 1
DB_POSTGRESQL = 2
DB_MYSQL = 3

DBFILE = ""

class relational:

   # parse out the gnubg* lines from database config file
   def __configure__( self ) :
      global DBFILE
      fname = "database";
      file = os.access(fname, os.F_OK)
      if (file == 0):
        home = os.environ.get("HOME", 0)
        if home :
           fname = "%s/%s" % ( home, ".gnubg/database" )
           file = os.access(fname, os.F_OK)
        if (file == 0):
           print "Database prefence file not found"
           return
      try :
        f = open( fname, "r" )
      except IOError, e :
        if e.errno == 2 :
           return
        raise e

      for line in f.readlines() :
         re.sub( r'#.*', '', line )
         line.strip()
         mo = re.match( r'^gnubg\*(\S+)\s+(\S.*)', line)
         if mo :
            (item, value) = mo.groups()
            mo = re.match(r'([\'"])(.*)\1', value )
            if mo :
               value = mo.group( 2 )
            if item == "dsn" : self.dsn = value
            if item == "host" : self.host = value
            if item == "user" : self.user = value
            if item == "password" : self.password = value
            if item == "database" : self.database = value
            if item == "type" :
               if re.match( r'^postgres', value ) :
                  self.db_type = DB_POSTGRESQL
               elif re.match( r'sqlite', value ) :
                  self.db_type = DB_SQLITE
               elif re.match( r'mysql', value ) :
                  self.db_type = DB_MYSQL
                  
            if item == "games" :
               if value in ["yes", "on", "true", "1" ] :
                  self.games = True
               else :
                  self.games = False

      f.close()

      # rebuild the dsn to include user/password and maybe host
      parts = self.dsn.split( ':' )
      if self.db_type == DB_POSTGRESQL and self.host and not re.match( r'.*:', self.host) :
         parts[ 0 ] = self.host
         self.host = None
      if self.database :
         while len( parts ) < 2 :  parts.append( "" )
         parts[ 1 ] = self.database
      if self.user :
         while len( parts ) < 3 : parts.append( "" )
         parts[ 2 ] = self.user
      if self.password :
         while len( parts ) < 4 : parts.append( "" )
         parts[ 3 ] = self.password
      self.dsn = ":".join( parts )
      if self.db_type == DB_SQLITE :
         if self.database :
            DBFILE = self.database
         else :
            DBFILE = "data.db"
      elif self.db_type == DB_MYSQL :
         if not self.database :
            self.database = "gnubg"
         self.match_table_name = "match_tbl"
      
   def __init__(self):

      self.conn = None
      
      self.dsn = ":gnubg"
      self.database = None
      self.user = None
      self.password = None
      self.host = None
      self.games = False
      self.match_table_name = "match"
      
      self.db_type = DB_POSTGRESQL
      
      self.__configure__()

   #
   # Return next free "id" from the given table
   # Paramters: conn (the database connection, DB API v2)
   #            tablename (the tablename)
   # Returns: next free "id"
   #

   def __next_id(self,tablename):

      # fetch next_id from control table
      next_id = self.__runqueryvalue("SELECT next_id FROM control WHERE tablename = '%s'" % (tablename))

      if next_id:
     
         # update control data with new next id
         self.__runquery("UPDATE control SET next_id = %d WHERE tablename = '%s'" \
             % (next_id+1, tablename))
     
      else:
         next_id = 0

         # insert next id
         self.__runquery("INSERT INTO control (tablename,next_id) VALUES ('%s',%d)" \
             % (tablename,next_id+1))
      
      return next_id

   # check if player exists
   def __playerExists(self,name,env_id):

      return self.__runqueryvalue("SELECT nick_id FROM nick WHERE name = '%s' AND env_id = %d" % \
         ( name, env_id ))

   #
   # Add players to database
   #
   # Parameters: conn (the database connection, DB API v2)
   #             name (the name of the player)
   #             env_id (the env. of the match)
   #
   # Returns: player id or None on error
   #
   
   def __addPlayer(self,name,env_id):
      
      # check if player exists
      nick_id = self.__playerExists(name, env_id)
      if (nick_id == None):
         # player does not exist - Insert new player
         # pick a unique name (add a number to end if required)
         present = self.__runqueryvalue("SELECT person_id FROM person WHERE name = '%s'" % (name))
         if present:
            postfix = 1
            while present:
               postfix = postfix + 1
               playername = "%s%d" % (name, postfix)
               present = self.__runqueryvalue("SELECT person_id FROM person WHERE name = '%s'" % (playername))
         else:
            playername = name
         
         person_id = self.__next_id("person")
         nick_id = self.__next_id("nick")

         # first add a new person
         query = ("INSERT INTO person(person_id,name,notes)" \
            "VALUES (%d,'%s','')") % (person_id, playername)
         cursor = self.conn.cursor()
         cursor.execute(query)
         cursor.close()
         # now add the nickname
         query = ("INSERT INTO nick(nick_id,env_id,person_id,name)" \
            "VALUES (%d,%d,'%d','%s')") % (nick_id, env_id, person_id, name)
         cursor = self.conn.cursor()
         cursor.execute(query)
         cursor.close()
         
      return nick_id

   #
   # getKey: return key if found in dict, otherwise return empty string
   #
   #

   def __getKey(self,d,key):
      
      if d.has_key(key):
         return d[key]
      else:
         return ''

   #
   # Calculate a/b, but return 0 for b=0
   #

   def __calc_rate(self,a,b):
      if b == 0:
         return 0.0
      else:
         return a/b

   #
   # Add game/match statistics
   # Parameters: conn (the database connection, DB API v2)
   #             gm_id (the match or game id)
   #             nick_id (the nick id for this statistics)
   #             gs (the game/match statistics)
   #             gs_other (opponent's game/match statistics)
   #             table_name (matchstat or gamestat)
   
   def __addStat( self, gm_id, nick_id, gs, gs_other, table_name ):

      if table_name == "gamestat" :
         gms_id = self.__next_id("gamestat")
      else :
         gms_id = self.__next_id("matchstat")

      # identification
      s1 = "%d,%d,%d," % (gms_id, gm_id, nick_id)
      
      # chequer play statistics
      chs = gs[ 'moves' ]
      m = chs[ 'marked' ]
      s2 = "%d,%d,%d,%d,%d,%d,%d,%f,%f,%f,%f,%d," % \
           ( chs[ 'total-moves' ], \
             chs[ 'unforced-moves' ], \
             m[ 'unmarked' ], \
             m[ 'good' ], \
             m[ 'doubtful' ], \
             m[ 'bad' ], \
             m[ 'very bad' ], \
             chs[ 'error-skill' ], \
             chs[ 'error-cost' ], \
             self.__calc_rate( chs[ 'error-skill' ], chs[ 'unforced-moves' ] ), \
             self.__calc_rate( chs[ 'error-cost' ], chs[ 'unforced-moves' ] ), \
             gnubg.errorrating( self.__calc_rate( chs[ 'error-skill' ],
                                             chs[ 'unforced-moves' ] ) ) ) 


      # luck statistics

      lus = gs[ 'dice' ]
      m = lus[ 'marked-rolls' ]
      s3 = "%d,%d,%d,%d,%d,%f,%f,%f,%f,%d," % \
           ( m[ 'verygood' ], \
             m[ 'good' ], \
             m[ 'unmarked' ], \
             m[ 'bad' ], \
             m[ 'verybad' ], \
             lus[ 'luck' ], \
             lus[ 'luck-cost' ], \
             self.__calc_rate( lus[ 'luck' ], chs[ 'total-moves' ] ), \
             self.__calc_rate( lus[ 'luck-cost' ], chs[ 'total-moves' ] ), \
             gnubg.luckrating( self.__calc_rate( lus[ 'luck' ],
                                            chs[ 'total-moves' ] ) ) ) 

      # cube statistics
   
      cus = gs[ 'cube' ]
      s4 = "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%d," % \
           ( cus[ 'total-cube' ], \
             cus[ 'close-cube' ], \
             cus[ 'n-doubles' ], \
             cus[ 'n-takes' ], \
             cus[ 'n-drops' ], \
             cus[ 'missed-double-below-cp' ], \
             cus[ 'missed-double-above-cp' ], \
             cus[ 'wrong-double-below-dp' ], \
             cus[ 'wrong-double-above-tg' ], \
             cus[ 'wrong-take' ], \
             cus[ 'wrong-drop' ], \
             cus[ 'err-missed-double-below-cp-skill' ], \
             cus[ 'err-missed-double-above-cp-skill' ], \
             cus[ 'err-wrong-double-below-dp-skill' ], \
             cus[ 'err-wrong-double-above-tg-skill' ], \
             cus[ 'err-wrong-take-skill' ], \
             cus[ 'err-wrong-drop-skill' ], \
             cus[ 'err-missed-double-below-cp-cost' ], \
             cus[ 'err-missed-double-above-cp-cost' ], \
             cus[ 'err-wrong-double-below-dp-cost' ], \
             cus[ 'err-wrong-double-above-tg-cost' ], \
             cus[ 'err-wrong-take-cost' ], \
             cus[ 'err-wrong-drop-cost' ], \
             # gnubg is giving per-move, not totals here
             cus[ 'error-skill' ] * cus[ 'close-cube' ], \
             cus[ 'error-cost' ] * cus[ 'close-cube' ], \
             cus[ 'error-skill' ], \
             cus[ 'error-cost' ], \
             gnubg.errorrating( cus[ 'error-skill' ] ) )
      
      # overall
      
      s5 = "%f,%f,%f,%f,%d,%f,%f,%f," % \
           ( cus[ 'error-skill' ] * cus[ 'close-cube' ] +
             chs[ 'error-skill' ], \
             cus[ 'error-cost' ]  * cus[ 'close-cube' ] +
             chs[ 'error-cost' ], \
             self.__calc_rate( cus[ 'error-skill' ]  * cus[ 'close-cube' ] \
                               + chs[ 'error-skill' ], \
                          cus[ 'close-cube' ] + chs[ 'unforced-moves' ] ),
             self.__calc_rate( cus[ 'error-cost' ]  * cus[ 'close-cube' ] + \
                                chs[ 'error-cost' ], \
                          cus[ 'close-cube' ] + chs[ 'unforced-moves' ] ),
             
             gnubg.errorrating( self.__calc_rate( cus[ 'error-skill' ] * \
                                                  cus[ 'close-cube' ] + \
                                             chs[ 'error-skill' ], \
                                             cus[ 'close-cube' ] +
                                             chs[ 'unforced-moves' ] ) ),
             lus[ 'actual-result' ],
             lus[ 'luck-adjusted-result' ],
             self.__calc_rate( cus[ 'error-skill' ]  * cus[ 'close-cube' ] + \
                               chs[ 'error-skill' ], \
                          chs[ 'total-moves' ] +
                          gs_other[ 'moves' ][ 'total-moves' ] ) )


      # for matches only
      if lus.has_key( 'fibs-rating-difference' ):
         s6 = '%f,' % ( lus[ 'fibs-rating-difference' ] )
      else:
         s6 = 'NULL,'

      if gs.has_key( 'error-based-fibs-rating' ):
         s = gs[ 'error-based-fibs-rating' ]
         if s.has_key( 'total' ):
            s6 = s6 + "%f," % ( s[ 'total' ] )
         else:
            s6 = s6 + "NULL,"
         if s.has_key( 'chequer' ):
            s6 = s6 + "%f," % ( s[ 'chequer' ] )
         else:
            s6 = s6 + "NULL,"
         if s.has_key( 'cube' ):
            s6 = s6 + "%f," % ( s[ 'cube' ] )
         else:
            s6 = s6 + "NULL,"
      else:
         s6 = s6 + "NULL,NULL,NULL,"

      # for money sessions only
      if gs.has_key( 'ppg-advantage' ):
         s = gs[ 'ppg-advantage' ]
         s7 = "%f,%f,%f,%f," % ( \
         s[ 'actual' ], \
         s[ 'actual-ci' ], \
         s[ 'luck-adjusted' ], \
         s[ 'luck-adjusted-ci' ] )
      else:
         s7 = "NULL,NULL,NULL,NULL,"

      # time penalties
      if gs.has_key( 'time' ):
         s8 = "%d,%f,%f" % \
           ( gs[ 'time' ][ 'time-penalty' ], \
             gs[ 'time' ][ 'time-penalty-skill' ], \
             gs[ 'time' ][ 'time-penalty-cost' ] )
      else:
         s8 = "0, 0, 0"
      
      query = "INSERT INTO %s VALUES (" % ( table_name )+ \
              s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8 + " );"
      cursor = self.conn.cursor()
      cursor.execute(query)
      cursor.close()
      
      return 0

   #
   # Add games
   # Parameters: conn (the database connection, DB API v2)
   #             person_id0 (nick_id of player 0)
   #             person_id1 (nick_id of player 1)
   #             m (the gnubg match)             
   
   def __addgames( self, nick_id0, nick_id1, m, match_id):

      # add games - first get an array of games dictionaris
      ga = m[ 'games' ]
      #assign game numbers by total score at start of each game
      ts = {}
      i = -1;
      for g in ga :
         i += 1
         ts[ g[ 'info'][ 'score-X' ] + g['info'][ 'score-O' ] ] = i

      tsa = ts.keys()
      tsa.sort()
      ml = m[ 'match-info' ][ 'match-length' ]
      for i in range( 0, len( tsa ) ) :
         ga[ ts[ tsa[ i ] ] ][ 'info' ][ 'game_number' ] = i 

      for g in ga :
         game_id = self.__next_id("game")
         if ml > 0 :
            g[ 'info'][ 'score-X' ] = ml - g[ 'info'][ 'score-X' ]
            g[ 'info'][ 'score-O' ] = ml - g[ 'info'][ 'score-O' ]
            
      
         # CURRENT_TIMESTAMP - SQL99, may have different names in various databases
         if self.db_type == DB_SQLITE:
            CURRENT_TIME = "datetime('now', 'localtime')"
         else :
            CURRENT_TIME = 'CURRENT_TIMESTAMP'

         res = g[ 'info' ].get( 'points-won', 0 )
         if g[ 'info' ].get( 'winner', 'X' ) == 'O' : res = -res
         query = ("INSERT INTO game(game_id, match_id, nick_id0, nick_id1, " \
                  "score_0, score_1, result, added, game_number, crawford) " \
                  "VALUES (%d, %d, %d, %d, %d, %d, %d, " + CURRENT_TIME + ", %d, %d )") % \
                  (game_id, match_id, nick_id0, nick_id1, \
                   g[ 'info' ][ 'score-X' ], g[ 'info'][ 'score-O' ], res,
                   g[ 'info'][ 'game_number' ], g[ 'info' ].get( 'crawford', False ) )

         cursor = self.conn.cursor()
         cursor.execute(query)
         cursor.close()

         # add game statistics for both players

         # print "addstat: ", game_id, nick_id0, g[ 'stats' ][ 'X' ][ 'moves' ]
         if self.__addStat( game_id, nick_id0, g[ 'stats' ][ 'X' ],
                            g [ 'stats' ][ 'O' ], "gamestat" ) == None:
            print "Error adding player 0's stat to database."
            return None

         # print "addstat: ", game_id, nick_id1, g[ 'stats' ][ 'O' ][ 'moves' ]
         if self.__addStat( game_id, nick_id1, g[ 'stats' ][ 'O' ],
                            g[ 'stats' ][ 'X' ], "gamestat" ) == None:
            print "Error adding player 1's stat to database."
            return None
         
   #
   # Add match
   # Parameters: conn (the database connection, DB API v2)
   #             person_id0 (nick_id of player 0)
   #             person_id1 (nick_id of player 1)
   #             m (the gnubg match)             
   
   def __addMatch(self,nick_id0,nick_id1,m,key):

      # add match

      match_id = self.__next_id("match")
      
      mi = m[ 'match-info' ]
      
      if mi.has_key( 'date' ):
         date = "'%04d-%02d-%02d'" % ( mi[ 'date' ][ 2 ],
                                       mi[ 'date' ][ 1 ],
                                       mi[ 'date' ][ 0 ] )
      else:
         date = "NULL"

      # CURRENT_TIMESTAMP - SQL99, may have different names in various databases
      if self.db_type == DB_SQLITE:
         CURRENT_TIME = "datetime('now', 'localtime')"
      else :
         CURRENT_TIME = 'CURRENT_TIMESTAMP'

      query = ("INSERT INTO %s(match_id, checksum, nick_id0, nick_id1, " \
              "result, length, added, rating0, rating1, event, round, place, annotator, comment, date) " \
              "VALUES (%d, '%s', %d, %d, %d, %d, " + CURRENT_TIME + ", '%s', '%s', '%s', '%s', '%s', '%s', '%s', %s) ") % \
              (self.match_table_name,
               match_id, key, nick_id0, nick_id1, \
               mi[ 'result' ], mi[ 'match-length' ],
               self.__getKey( mi[ 'X' ], 'rating' )[0:80],
               self.__getKey( mi[ 'O' ], 'rating' )[0:80],
               self.__getKey( mi, 'event' )[0:80],
               self.__getKey( mi, 'round' )[0:80],
               self.__getKey( mi, 'place' )[0:80],
               self.__getKey( mi, 'annotator' )[0:80],
               self.__getKey( mi, 'comment' )[0:80], date )
      cursor = self.conn.cursor()
      cursor.execute(query)
      cursor.close()

      # add match statistics for both players

      if self.__addStat( match_id, nick_id0, m[ 'stats' ][ 'X' ],
                   m [ 'stats' ][ 'O' ], "matchstat" ) == None:
         print "Error adding player 0's stat to database."
         return None

      if self.__addStat( match_id, nick_id1, m[ 'stats' ][ 'O' ],
                   m [ 'stats' ][ 'X' ], "matchstat" ) == None:
         print "Error adding player 1's stat to database."
         return None

      # add games for both players
      if self.games :
         self.__addgames( nick_id0, nick_id1, m, match_id)
      return match_id
         
   #
   # Add match to database
   # Parameters: conn (the database connection, DB API v2)
   #             m (the match as obtained from gnubg)
   #             env_id (the environment of the match)
   # Returns: n/a
   #
   
   def __add_match(self,m,env_id,key):

      # add players

      mi = m[ 'match-info' ]
      
      nick_id0 = self.__addPlayer( mi[ 'X' ][ 'name' ], env_id)
      if nick_id0 == None:
         return None
      
      nick_id1 = self.__addPlayer( mi[ 'O' ][ 'name' ], env_id)
      if nick_id1 == None:
         return None
      
      # add match

      match_id = self.__addMatch(nick_id0, nick_id1, m, key)
      if match_id == None:
         return None

      return match_id

   def __add_env(self, env_name):

      env_id = self.__next_id("env")
      self.__runquery("INSERT INTO env(env_id, place)" \
            "VALUES (%d,'%s')" % (env_id, env_name))

      return True

   # Simply function to run a query
   def __runquery(self, q):
      cursor = self.conn.cursor()
      cursor.execute(q);
      cursor.close()

   # Simply function to run a query and return single value
   def __runqueryvalue(self, q):
   
      cursor = self.conn.cursor()
      cursor.execute(q)
      row = cursor.fetchone()
      cursor.close()

      if row:
         return row[0]
      else:
         return None

   #
   # Check for existing match
   #
   #

   def is_existing_match(self, key):

      return self.__runqueryvalue("SELECT match_id FROM %s WHERE checksum = '%s'" % (self.match_table_name, key))

   #
   # Main logic
   #

   def createdatabase(self):
      
      cursor = self.conn.cursor()
      # Open file which has db create sql statments
      if self.games: 
          sqlfile = open("gnubg.game.sql", "r")
      else:
          sqlfile = open("gnubg.sql", "r")
      done = False
      stmt = ""
      # Loop through file and run sql commands
      while not done:
        line = sqlfile.readline()
        if len(line) > 0:
           line = string.strip(line)
           if (line[0:2] != '--'):
              stmt = stmt + line
              if (len(line) > 0 and line[-1] == ';'):
                 cursor.execute(stmt)
                 stmt = ""
        else:
           done = True

      self.conn.commit()
      sqlfile.close()

   def connect(self):

      if self.db_type == DB_SQLITE:
         from pysqlite2 import dbapi2 as sqlite
      elif self.db_type == DB_POSTGRESQL:
         # Import the DB API v2 postgresql module
         import pgdb
         import _pg
      elif self.db_type == DB_MYSQL :
         import MySQLdb
         
      try:
         if self.db_type == DB_SQLITE:
            dbfile = os.access(DBFILE, os.F_OK)
            self.conn = sqlite.connect(DBFILE)
            if (dbfile == 0):
               self.createdatabase()

         elif self.db_type == DB_POSTGRESQL:
            if self.host :
               self.conn = pgdb.connect( dsn = self.dsn, host = self.host )
            else :
               self.conn = pgdb.connect( dsn = self.dsn )

         elif self.db_type == DB_MYSQL :
            if self.host :
               if self.user :
                  if self.password :
                     self.conn = MySQLdb.connect( db = self.database, user=self.user, passwd=self.password, host = self.host )
               else :
                  self.conn = MySQLdb.connect( db = self.database, host = self.host )
            elif self.user :
               if self.password :
                  self.conn = MySQLdb.connect( db = self.database, user=self.user, passwd=self.password )
               else :
                     self.conn = MySQLdb.connect( db = self.database, user=self.user )
            else :
               self.conn = MySQLdb.connect( db = self.databasem )
               
         return self.conn
#if Postgresql
#      except _pg.error, e:
#         print e
      except:
         print "Unhandled exception..."
         traceback.print_exc()

      return None

   def disconnect(self):
         
      if self.conn == None:
         return

      self.conn.close()

   def addenv(self,env_name):

      if self.conn == None:
         return -3
         
      # Check env not already present
      env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % env_name)
      if (env_id):
         return -2

      if (self.__add_env(env_name)):
         self.conn.commit()
         # Environment successfully added to database
         return 1
      else:
         # Error adding env to database
         return -1

   def addmatch(self,env="",force=0):

      if self.conn == None:
         return -3

      # check env_id
      if env == "":
         env_id = self.__runqueryvalue("SELECT env_id FROM env ORDER BY env_id")
      else:
         env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % env)
      if env_id == None:
         return -4

      m = gnubg.match(0,0,1,0)
      if m:
         key = gnubg.matchchecksum()
         existing_id = self.is_existing_match(key)
         if existing_id <> None:
            if (force):
               # Replace match in database - so first delete existing match
               self.__runquery("DELETE FROM matchstat WHERE match_id = %d" % existing_id);
               self.__runquery("DELETE FROM %s WHERE match_id = %d" % (self.match_table_name, existing_id ) );
            else:
               # Match is already in the database
               return -3

         match_id = self.__add_match(m,env_id,key)
         if match_id <> None:
            self.conn.commit()
            # Match successfully added to database
            return 0
         else:
            # Error add match to database
            return -1
      else:
         # No match
         return -2

   def test(self):

      if self.conn == None:
         return -1

      # verify that the main table is present
      rc = 0

      cursor = self.conn.cursor()
      try:
         cursor.execute("SELECT COUNT(*) from match;")
      except:
         rc = -2

      cursor.close()

      return rc

   def list_details(self, player, env=""):

      if self.conn == None:
         return None

      # check env_id
      if env == "":
         env_id = self.__runqueryvalue("SELECT env_id FROM env ORDER BY env_id")
      else:
         env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % env)
      if env_id == None:
         return -4

      nick_id = self.__playerExists(player, env_id);
      
      if (nick_id == None):
        return -1

      stats = []

      row = self.__runqueryvalue("SELECT count(*) FROM %s WHERE nick_id0 = %d OR nick_id1 = %d" % \
         (self.match_table_name, nick_id, nick_id));
      if row == None:
         played = 0
      else:
         played = int(row)
      stats.append(("Matches played", played))

      row = self.__runqueryvalue("SELECT COUNT(*) FROM %s WHERE (nick_id0 = %d and result = 1)" \
        " OR (nick_id1 = %d and result = -1)" % (self.match_table_name, nick_id, nick_id));
      if row == None:
         wins = 0
      else:
         wins = int(row)
      stats.append(("Matches won", wins))

      row = self.__runqueryvalue("SELECT AVG(overall_error_total) FROM matchstat" \
         " WHERE nick_id = %d" % (nick_id));
      if row == None:
         rate = 0
      else:
         rate = row
      stats.append(("Average error rate", rate))

      return stats

   def erase_env(self, env_name):
   
      if self.conn == None:
         return None

      # Check that more than 1 env exists
      if self.__runqueryvalue("SELECT COUNT(env_id) FROM env") == 1:
         return -2

      # first look for env
      env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % ( env_name ))

      if env_id == None:
         return -1

      # from query to select all matches involving env
      mq1 = "(SELECT match_id FROM %s INNER JOIN nick ON match.nick_id0 = nick.nick_id" \
         " WHERE env_id = %d)" % (self.match_table_name, env_id )
      mq2 = "(SELECT match_id FROM %s INNER JOIN nick ON match.nick_id1 = nick.nick_id" \
         " WHERE env_id = %d)" % (self.match_table_name, env_id )

      # first remove any matchstats
      self.__runquery("DELETE FROM matchstat WHERE match_id in " + mq1);
      self.__runquery("DELETE FROM matchstat WHERE match_id in " + mq2);

      # then remove any matches
      self.__runquery("DELETE FROM %s WHERE match_id in " % (self.match_table_name ) + mq1 );
      self.__runquery("DELETE FROM %s WHERE match_id in " % (self.match_table_name ) + mq2);
      
      # then any nicks
      self.__runquery("DELETE FROM nick WHERE env_id = %d" % \
         (env_id));
      
      # finally remove the environment
      self.__runquery("DELETE FROM env WHERE env_id = %d" % \
         (env_id));
         
      # remove any orphan persons (no nicks left)
      self.__runquery("DELETE FROM person WHERE person_id in " \
         "(SELECT person.person_id FROM person LEFT OUTER JOIN nick " \
         "ON nick.person_id = person.person_id WHERE nick.env_id IS NULL)");

      self.conn.commit()
      
      return 1

   def link_players(self, nick_name, env_name, player_name):

      if self.conn == None:
         return None

      env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % env_name)
      if (env_id == None):
         return -2

      person_id = self.__runqueryvalue("SELECT person_id FROM person WHERE name = '%s'" % player_name)
      if (env_id == None):
         return -2

      # Just need to change the person_id for the nick
      self.__runquery("UPDATE nick SET person_id = %d WHERE env_id = %d AND name = '%s' " \
         % (person_id, env_id, nick_name))

      # remove any orphan persons (no nicks left)
      self.__runquery("DELETE FROM person WHERE person_id in " \
         "(SELECT person.person_id FROM person LEFT OUTER JOIN nick " \
         "ON nick.person_id = person.person_id WHERE nick.env_id IS NULL)");

      self.conn.commit()

      return 1

   def rename_env(self, env_name, new_name):
   
      if self.conn == None:
         return None

      # first look for env
      env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % ( env_name ))
      if env_id == None:
         return -1

      # change the name
      self.__runquery("UPDATE env SET place = '%s' WHERE env_id = %d " \
         % (new_name, env_id))

      self.conn.commit()

      return 1

   # NB. This removes a nick - not a player and all their nicks
   def erase_player(self, player, env=""):

      if self.conn == None:
         return None

      # check env_id
      if env == "":
         env_id = self.__runqueryvalue("SELECT env_id FROM env ORDER BY env_id")
      else:
         env_id = self.__runqueryvalue("SELECT env_id FROM env WHERE place = '%s'" % env)
      if env_id == None:
         return -4

      nick_id = self.__playerExists(player, env_id);
      
      if (nick_id == None):
        return -1

      # from query to select all matches involving player
      mq = "FROM %s WHERE nick_id0 = %d OR nick_id1 = %d" % \
         (self.match_table_name, nick_id, nick_id)

      # first remove any matchstats
      self.__runquery("DELETE FROM matchstat WHERE match_id in " \
         "(select match_id %s)" % (mq));

      # then remove any matches
      self.__runquery("DELETE " + mq);
      
      # then the nick
      self.__runquery("DELETE FROM nick WHERE nick_id = %d" % \
         (nick_id));
      
      # remove any orphan persons (no nicks left)
      self.__runquery("DELETE FROM person WHERE person_id in " \
         "(SELECT person.person_id FROM person LEFT OUTER JOIN nick " \
         "ON nick.person_id = person.person_id WHERE nick.env_id IS NULL)");

      self.conn.commit()
      
      return 1

   def erase_all(self):

      if self.conn == None:
         return None

      # first remove all matchstats
      self.__runquery("DELETE FROM matchstat");

      # then remove all matches
      self.__runquery("DELETE FROM %s" % (self.match_table_name ) );
      
      # then all nicks
      self.__runquery("DELETE FROM nick");
      
      # finally remove the players
      self.__runquery("DELETE FROM person");

      self.conn.commit()
      
      return 1

   def select(self, str):

      if self.conn == None:
         return None

      cursor = self.conn.cursor()
      try:
         cursor.execute("SELECT " + str);
      except:
         cursor.close()
         return None

      all = cursor.fetchall()
      if (all):
        titles = [cursor.description[i][0] for i in range(len(cursor.description))]
        all.insert(0, titles)

      cursor.close()

      return all

   def update(self, str):

      if self.conn == None:
         return None

      cursor = self.conn.cursor()
      try:
         cursor.execute("UPDATE " + str);
      except:
         cursor.close()
         return None

      self.conn.commit()

      return 1
