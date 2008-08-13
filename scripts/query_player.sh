#!/bin/bash
# 
# query_player.sh - a small shell script for getting some player's data
#
# by Achim Mueller <ace@gnubg.org>, 2008
#
# Usage: query_player.sh [search pattern] [output]
#
# search pattern: a player's name or parts of it
# output: [screen|csv] the way the result is presented, either on your screen in
#                       readable columns or into a csv file for usage in a 
#                       spreadsheet.
#
#
################################################################################# 
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of version 3 or later of the GNU General Public License as #
# published by the Free Software Foundation.                                    #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#################################################################################

# Put the path to your database here:
DATABASE=/Users/ace/.gnubg/gnubg.db
OUTPUT="$2"


NAME_ID_SEARCH="select player_id from player where name like '%$1%';"

NAME_SEARCH="select name from player where name like '%$1%';"

NAME_ID_RESULT=`sqlite3 $DATABASE <<EOF
.header off
$NAME_ID_SEARCH
EOF`

# echo $NAME_ID_RESULT

STATS_SEARCH=" select s.session_id as No
	, s.player_id0 as Player
	, round(m1.snowie_error_rate_per_move*1000,2) as Snowie
        , round(m1.error_based_fibs_rating,1) as Fibs
	, s.player_id1 as Opp 
	, p1.name as Name
	, round(m2.snowie_error_rate_per_move*1000,2) as Snowie
	, round(m2.error_based_fibs_rating,1) as Fibs_Opp
	, round(50+m1.luck_adjusted_result*100,2) as LAR
	, round(50+(m2.overall_error_total-m1.overall_error_total)*100,2) as MWC
	, s.length as Length
	, round(m1.actual_result+0.5) as Result
 from session as s
 join matchstat as m1
	on m1.session_id = s.session_id
	and m1.player_id = s.player_id0
 join matchstat as m2
	on m2.session_id = s.session_id
	and m2.player_id = s.player_id1 
 join player as p2
	on p2.player_id = s.player_id0
	and p1.player_id = s.player_id1
 join player as p1
	on p1.player_id = s.player_id1
	and p2.player_id = s.player_id0
 where player_id0 = '$NAME_ID_RESULT' 
union
 select s.session_id
	, s.player_id1 as Player
	, round(m1.snowie_error_rate_per_move*1000,2) as Snowie
	, round(m1.error_based_fibs_rating,1) as Fibs
	, s.player_id0 as Opp
	, p2.name as name
	, round(m2.snowie_error_rate_per_move*1000,2) as Snowie
	, round(m2.error_based_fibs_rating,1) Fibs_Opp
	, round(50+m1.luck_adjusted_result*100,2) as LAR
	, round(50+(m2.overall_error_total-m1.overall_error_total)*100,2) as MWC
	, s.length as Length
	, round(m1.actual_result+0.5) as Result
 from session  as s
 join matchstat as m2
	on m2.session_id = s.session_id
	and m2.player_id = s.player_id0
 join matchstat as m1
	on m1.session_id = s.session_id
	and m1.player_id = s.player_id1 
 join player as p2
	on p2.player_id = s.player_id0
	and p1.player_id = s.player_id1
 join player as p1
	on p1.player_id = s.player_id1
	and p2.player_id = s.player_id0
 where player_id1 = '$NAME_ID_RESULT';" 


NICK_NAME_RESULT=`sqlite3 $DATABASE <<EOF
.headers OFF
$NAME_SEARCH
EOF`

POINTS_WON="select sum(round(actual_result+0.5,0)) from matchstat where player_id = '$NAME_ID_RESULT';"

NUMBER_MATCHES="select count(session_id) from matchstat where player_id = '$NAME_ID_RESULT';" 

TOTAL_ERRORS="select round(sum(snowie_error_rate_per_move*snowie_moves)*1000/sum(snowie_moves),2) from matchstat where player_id = $NAME_ID_RESULT;"

TOTAL_RESULT=`sqlite3 $DATABASE <<EOF
.headers OFF
$POINTS_WON
EOF`

STATS_RESULT_SCREEN=`sqlite3 $DATABASE <<EOF
.mode column 
.headers ON
.output tmp.file 
$STATS_SEARCH 
EOF`

STATS_RESULT_CSV=`sqlite3 $DATABASE <<EOF
.mode csv 
.headers ON
.output gnubg_db.txt 
$STATS_SEARCH 
EOF`

NUMBER_MATCHES_RESULT=`sqlite3 $DATABASE <<EOF
.headers OFF
$NUMBER_MATCHES
EOF`

ERROR_AVR_RESULT=`sqlite3 $DATABASE <<EOF
.headers OFF
$TOTAL_ERRORS
EOF`

case "$OUTPUT" in
	
    screen|"") $STATS_RESULT_SCREEN 
 	       echo
	       cat tmp.file
	       rm tmp.file
	       echo
	       echo "Result ${NICK_NAME_RESULT} (${NAME_ID_RESULT}): $TOTAL_RESULT win(s) in $NUMBER_MATCHES_RESULT matches. Snowie error rate: $ERROR_AVR_RESULT"
	       echo
	       ;;

	 csv) $STATS_RESULT_CSV
	      ;;

            *) echo "Usage: query_player.sh [search pattern] [screen|csv]" ;;
esac
exit 0

