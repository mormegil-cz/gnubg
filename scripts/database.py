#
# database.py
#
# by Joern Thyssen <jth@gnubg.org>, 2004
#
# This file contains the functions for adding matches to a relational
# database.
#
# The modules us the DB API V2 python modules. 
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
                                                                                  

#
# Return next free "id" from the given table
# Paramters: conn (the database connection, DB API v2)
#            tablename (the tablename)
# Returns: next free "id"
#

def next_id(conn,tablename):

   # fetch next_id from control table

   q = "SELECT next_id FROM gnubg.control WHERE tablename = '%s'" % (tablename)

   c = conn.cursor()
   c.execute(q)

   row = c.fetchone()

   c.close()

   if row:
     
     next_id = row[0]

     # update control data with new next id
     
     q = "UPDATE gnubg.control SET next_id = %d WHERE tablename = '%s'" \
         % (next_id+1, tablename)

     c = conn.cursor()
     c.execute(q)
     c.close()
     
   else:
     next_id = 0

     # insert next id

     q = "INSERT INTO gnubg.control (tablename,next_id) VALUES ('%s',%d)" \
         % (tablename,next_id+1)

     c = conn.cursor()
     c.execute(q)
     c.close()

   return next_id

#
# Add players to database
#
# Parameters: conn (the database connection, DB API v2)
#             name (the name of the player)
#             env_id (the env. of the match)
#
# Returns: 0 on success, non-zero otherwise
#

def addPlayer(conn,name,env_id):

   # check if player exists

   query = "SELECT per.person_id FROM gnubg.player pla INNER JOIN gnubg.person per ON pla.person_id = per.person_id  WHERE per.name = '%s'" % ( name )

   cursor = conn.cursor()
   cursor.execute(query)

   row = cursor.fetchone()
   cursor.close()

   if row:
      # player already exists
      return row[0]
   else:
      # player does not exist

      # Insert person

      person_id = next_id(conn,"person")

      query = "INSERT INTO gnubg.person (person_id,name,notes) VALUES (%d,'%s','')" % ( person_id, name )
      cursor = conn.cursor()
      cursor.execute(query)
      cursor.close()

      # Insert player

      query = "INSERT INTO gnubg.player (person_id,env_id) VALUES (%d,%d)" % (person_id, env_id)
      cursor = conn.cursor()
      cursor.execute(query)
      cursor.close()

      return person_id

#
# getKey: return key if found in dict, otherwise return empty string
#
#

def getKey(d,key):

   if d.has_key(key):
      return d[key]
   else:
      return ''

#
# Calculate a/b, but return 0 for b=0
#

def calc_rate(a,b):
   if b == 0:
      return 0.0
   else:
      return a/b

#
# Add statistics
# Parameters: conn (the database connection, DB API v2)
#             person_id (the person id for this statistics)
#             gs (the game/match statistics)
#

def addStat(conn,person_id,gs,gs_other):

   stat_id = next_id(conn,"stat")

   # identification
   s1 = "%d,%d," % (stat_id, person_id )

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
     calc_rate( chs[ 'error-skill' ], chs[ 'unforced-moves' ] ), \
     calc_rate( chs[ 'error-cost' ], chs[ 'unforced-moves' ] ), \
     gnubg.errorrating( calc_rate( chs[ 'error-skill' ],
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
          calc_rate( lus[ 'luck' ], chs[ 'total-moves' ] ), \
          calc_rate( lus[ 'luck-cost' ], chs[ 'total-moves' ] ), \
          gnubg.luckrating( calc_rate( lus[ 'luck' ],
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
          cus[ 'missed-double-below-cp' ], \
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
          cus[ 'error-skill' ], \
          cus[ 'error-cost' ], \
          calc_rate( cus[ 'error-skill' ], cus[ 'close-cube' ] ), \
          calc_rate( cus[ 'error-cost' ], cus[ 'close-cube' ] ), \
          gnubg.errorrating( calc_rate( cus[ 'error-skill' ],
                                        cus[ 'close-cube' ] ) ) ) 

   # overall

   s5 = "%f,%f,%f,%f,%d,%f,%f,%f," % \
        ( cus[ 'error-skill' ] + chs[ 'error-skill' ], \
          cus[ 'error-cost' ] + chs[ 'error-cost' ], \
          calc_rate( cus[ 'error-skill' ] + chs[ 'error-skill' ], \
                     cus[ 'close-cube' ] + chs[ 'unforced-moves' ] ),
          calc_rate( cus[ 'error-cost' ] + chs[ 'error-cost' ], \
                     cus[ 'close-cube' ] + chs[ 'unforced-moves' ] ),

          gnubg.errorrating( calc_rate( cus[ 'error-skill' ] +
                                        chs[ 'error-skill' ], \
                                        cus[ 'close-cube' ] +
                                        chs[ 'unforced-moves' ] ) ),
          lus[ 'actual-result' ],
          lus[ 'luck-adjusted-result' ],
          calc_rate( cus[ 'error-skill' ] + chs[ 'error-skill' ], \
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
   s8 = "%d,%f,%f" % \
        ( gs[ 'time' ][ 'time-penalty' ], \
          gs[ 'time' ][ 'time-penalty-skill' ], \
          gs[ 'time' ][ 'time-penalty-cost' ] )

   query = "INSERT INTO gnubg.stat VALUES (" + \
           s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8 + ");"
   cursor = conn.cursor()
   cursor.execute(query)
   cursor.close()

   return stat_id

#
# Add match
# Parameters: conn (the database connection, DB API v2)
#             env_id (the env)
#             person_id0 (person_id of player 0)
#             person_id1 (person_id of player 1)
#             m (the gnubg match)             

def addMatch(conn,env_id,person_id0,person_id1,m):

   stat_id = addStat(conn,person_id0,m[ 'stats' ][ 'X' ],m [ 'stats' ][ 'O' ])
   if stat_id == None:
      return None

   stat_id = addStat(conn,person_id1,m[ 'stats' ][ 'O' ],m [ 'stats' ][ 'X' ])
   if stat_id == None:
      return None

   match_id = next_id(conn,"match")

   mi = m[ 'match-info' ]

   if mi.has_key( 'date' ):
      date = "'%04d-%02d-%02d'" % ( mi[ 'date' ][ 2 ],
                                    mi[ 'date' ][ 1 ],
                                    mi[ 'date' ][ 0 ] )
   else:
      date = "'NULL'"

   query = "INSERT INTO gnubg.match (match_id,env_id0,person_id0,env_id1,person_id1,result,length,added,rating0,rating1,event,round,place,annotator,comment,date,stat_id) VALUES (%d,%d,%d,%d,%d,%d,%d,CURRENT_TIMESTAMP,'%s','%s','%s','%s','%s','%s','%s',%s,%d) " % \
           (match_id,env_id,person_id0,env_id,person_id1, \
            -1,mi[ 'match-length' ],
            getKey( mi[ 'X' ], 'rating' ),
            getKey( mi[ 'O' ], 'rating' ),
            getKey( mi, 'event' ), getKey( mi, 'round' ),
            getKey( mi, 'place' ), getKey( mi, 'annotator' ),
            getKey( mi, 'comment' ), date, stat_id )

   cursor = conn.cursor()
   cursor.execute(query)
   cursor.close()

   return match_id
         

#
# Add match to database
# Parameters: conn (the database connection, DB API v2)
#             m (the match as obtained from gnubg)
#             env_id (the environment of the match)
# Returns: n/a
#

def add_match(conn,m,env_id):

   # add players

   mi = m[ 'match-info' ]

   person_id0 = addPlayer( conn, mi[ 'X' ][ 'name' ], env_id)
   if person_id0 == None:
      return None
   
   person_id1 = addPlayer( conn, mi[ 'O' ][ 'name' ], env_id)
   if person_id1 == None:
      return None

   # add match

   match_id = addMatch( conn, env_id, person_id0, person_id1, m)
   if match_id == None:
      return None

   return match_id

#
# Main logic
#

# Import the DB API v2 postgresql module
import pgdb

conn = pgdb.connect(dsn=':gnubg')

m = gnubg.match(0,0,1,0)
if m:
   match_id = add_match(conn,m,0)
   print "Match successfully added to database"
else:
   print "No match"

conn.commit()
conn.close()
   
  
