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
   def __write_db_file__(self) :
        fname = self.homedir+'database'
        f = open(fname, 'w')
        if (not f):
            return
        if (self.dsn):
            f.write("gnubg*dsn %s\n" % self.dsn)
        if (self.host):
            f.write("gnubg*host %s\n" % self.host)
        if (self.user):
            f.write("gnubg*user %s\n" % self.user)
        if (self.password):
            f.write("gnubg*password %s\n" % self.password)
        if (self.database):
            f.write("gnubg*database %s\n" % self.database)
        if (self.db_type):
            if (self.db_type == DB_SQLITE):
                f.write("gnubg*type sqlite\n")
            if (self.db_type == DB_POSTGRESQL):
                f.write("gnubg*type postgres\n")
            if (self.db_type == DB_MYSQL):
                f.write("gnubg*type mysql\n")
        if (self.games):
            f.write("gnubg*games yes")
        f.close()
        
   # parse out the gnubg* lines from database config file
   def __configure__( self ) :
      global DBFILE
      fname = self.homedir+'database'
      file = os.access(fname, os.F_OK)
      if (file == 0):
         self.__write_db_file__()
         file = os.access(fname, os.F_OK)
      if (file == 0):
         print "Database prefence file not found"
         return
      try :
        f = open(fname, "r" )
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
         if self.database != "" :
            DBFILE = self.database
         else :
            DBFILE = "data.db"
      elif self.db_type == DB_MYSQL :
         if not self.database :
            self.database = "gnubg"
         self.match_table_name = "match_tbl"
      
   def __init__(self, datadir="", homedir=""):

      self.datadir = datadir
      self.homedir = homedir
      self.conn = None
      self.dsn = ""
      self.database = self.homedir+"gnubg.db"
      self.user = ""
      self.password = ""
      self.host = ""
      self.games = True
      self.match_table_name = "match"
      self.db_type = DB_SQLITE
      
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
   def __playerExists(self,name):

      return self.__runqueryvalue("SELECT player_id FROM player WHERE name = '%s'" % ( name ))

   #
   # Add players to database
   #
   # Parameters: conn (the database connection, DB API v2)
   #             name (the name of the player)
   #
   # Returns: player id or None on error
   #
   
   def __addPlayer(self, name):
      
      # check if player exists
      player_id = self.__playerExists(name)
      if (player_id == None):
         # player does not exist - Insert new player
         player_id = self.__next_id("player")
         query = ("INSERT INTO player(player_id,name,notes)" \
            "VALUES (%d,'%s','')") % (player_id, name)
         cursor = self.conn.cursor()
         cursor.execute(query)
         cursor.close()
         
      return player_id

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
   #             player_id (the player id for this statistics)
   #             gs (the game/match statistics)
   #             gs_other (opponent's game/match statistics)
   #             table_name (matchstat or gamestat)
   
   def __addStat( self, gm_id, player_id, gs, gs_other, table_name ):

      if table_name == "gamestat" :
         gms_id = self.__next_id("gamestat")
      else :
         gms_id = self.__next_id("matchstat")

      # identification
      s1 = "%d,%d,%d," % (gms_id, gm_id, player_id)
      
      # chequer play statistics
      chs = gs[ 'moves' ]
      m = chs[ 'marked' ]
      s2 = "%d, %d,%d,%d,%d,%d,%d,%f,%f,%f,%f,%d," % \
           ( chs[ 'total-moves' ], \
             chs[ 'unforced-moves' ], \
             m[ 'unmarked' ], \
             0, \
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
   #             player_id0 (player_id of player 0)
   #             player_id1 (player_id of player 1)
   #             m (the gnubg match)             
   
   def __addgames( self, player_id0, player_id1, m, match_id):

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
         query = ("INSERT INTO game(game_id, match_id, player_id0, player_id1, " \
                  "score_0, score_1, result, added, game_number, crawford) " \
                  "VALUES (%d, %d, %d, %d, %d, %d, %d, " + CURRENT_TIME + ", %d, %d )") % \
                  (game_id, match_id, player_id0, player_id1, \
                   g[ 'info' ][ 'score-X' ], g[ 'info'][ 'score-O' ], res,
                   g[ 'info'][ 'game_number' ], g[ 'info' ].get( 'crawford', False ) )

         cursor = self.conn.cursor()
         cursor.execute(query)
         cursor.close()

         # add game statistics for both players

         # print "addstat: ", game_id, player_id0, g[ 'stats' ][ 'X' ][ 'moves' ]
         if self.__addStat( game_id, player_id0, g[ 'stats' ][ 'X' ],
                            g [ 'stats' ][ 'O' ], "gamestat" ) == None:
            print "Error adding player 0's stat to database."
            return None

         # print "addstat: ", game_id, player_id1, g[ 'stats' ][ 'O' ][ 'moves' ]
         if self.__addStat( game_id, player_id1, g[ 'stats' ][ 'O' ],
                            g[ 'stats' ][ 'X' ], "gamestat" ) == None:
            print "Error adding player 1's stat to database."
            return None
         
   #
   # Add match
   # Parameters: conn (the database connection, DB API v2)
   #             player_id0 (player_id of player 0)
   #             player_id1 (player_id of player 1)
   #             m (the gnubg match)             
   
   def __addMatch(self, player_id0, player_id1, m, key):

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

      query = ("INSERT INTO %s(match_id, checksum, player_id0, player_id1, " \
              "result, length, added, rating0, rating1, event, round, place, annotator, comment, date) " \
              "VALUES (%d, '%s', %d, %d, %d, %d, " + CURRENT_TIME + ", '%s', '%s', '%s', '%s', '%s', '%s', '%s', %s) ") % \
              (self.match_table_name,
               match_id, key, player_id0, player_id1, \
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

      if self.__addStat( match_id, player_id0, m[ 'stats' ][ 'X' ],
                   m [ 'stats' ][ 'O' ], "matchstat" ) == None:
         print "Error adding player 0's stat to database."
         return None

      if self.__addStat( match_id, player_id1, m[ 'stats' ][ 'O' ],
                   m [ 'stats' ][ 'X' ], "matchstat" ) == None:
         print "Error adding player 1's stat to database."
         return None

      # add games for both players
      if self.games :
         self.__addgames( player_id0, player_id1, m, match_id)
      return match_id
         
   #
   # Add match to database
   # Parameters: conn (the database connection, DB API v2)
   #             m (the match as obtained from gnubg)
   # Returns: n/a
   #
   
   def __add_match(self, m, key):

      # add players

      mi = m[ 'match-info' ]
      
      player_id0 = self.__addPlayer( mi[ 'X' ][ 'name' ])
      if player_id0 == None:
         return None
      
      player_id1 = self.__addPlayer( mi[ 'O' ][ 'name' ])
      if player_id1 == None:
         return None
      
      # add match

      match_id = self.__addMatch(player_id0, player_id1, m, key)
      if match_id == None:
         return None

      return match_id

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
          sqlfile = open(self.datadir+"gnubg.game.sql", "r")
      else:
          sqlfile = open(self.datadir+"gnubg.sql", "r")
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
         from sqlite3 import dbapi2 as sqlite
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

   def addmatch(self,force):

      if self.conn == None:
         return -1

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

         match_id = self.__add_match(m, key)
         if match_id <> None:
            self.conn.commit()
            # Match successfully added to database
            return 0
         else:
            # Error adding match to database
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

   def list_details(self, player):

      if self.conn == None:
         return None

      player_id = self.__playerExists(player);
      
      if (player_id == None):
        return -1

      stats = []

      row = self.__runqueryvalue("SELECT count(*) FROM %s WHERE player_id = %d" % \
         (self.match_table_name, player_id));
      if row == None:
         played = 0
      else:
         played = int(row)
      stats.append(("Matches played", played))

      row = self.__runqueryvalue("SELECT COUNT(*) FROM %s WHERE (player_id0 = %d and result = 1)" \
        " OR (player_id1 = %d and result = -1)" % (self.match_table_name, player_id, player_id));
      if row == None:
         wins = 0
      else:
         wins = int(row)
      stats.append(("Matches won", wins))

      row = self.__runqueryvalue("SELECT AVG(overall_error_total) FROM matchstat" \
         " WHERE player_id = %d" % (player_id));
      if row == None:
         rate = 0
      else:
         rate = row
      stats.append(("Average error rate", rate))

      return stats

   # NB. This removes a player
   def erase_player(self, player):

      if self.conn == None:
         return None

      player_id = self.__playerExists(player);
      
      if (player_id == None):
        return -1

      # from query to select all matches involving player
      mq = "FROM %s WHERE player_id0 = %d OR player_id1 = %d" % \
         (self.match_table_name, player_id, player_id)

      # first remove any matchstats
      self.__runquery("DELETE FROM matchstat WHERE match_id in " \
         "(select match_id %s)" % (mq));

      # then remove any matches
      self.__runquery("DELETE " + mq);
      
      # then the player
      self.__runquery("DELETE FROM player WHERE player_id = %d" % \
         (player_id));

      self.conn.commit()
      
      return 1

   def erase_all(self):

      if self.conn == None:
         return None

      # first remove all matchstats
      self.__runquery("DELETE FROM matchstat");

      # then remove all matches
      self.__runquery("DELETE FROM %s" % (self.match_table_name ) );
      
      # finally remove the players
      self.__runquery("DELETE FROM player");

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
