#!/bin/bash
# 
# query_player.sh - a small shell script for getting some player's data
#
# by Achim Mueller <ace@gnubg.org>, 2008
#
# Usage: query_player.sh [search pattern]
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

DATABASE=/home/ace/.gnubg/gnubg.db
PLAYER=$1

NAME_ID_SEARCH="select player_id from player where name like '%$1%';"

NAME_SEARCH="select name from player where name like '%$1%';"

NAME_ID_RESULT=`sqlite3 $DATABASE <<EOF
$NAME_ID_SEARCH
EOF`

# only for getting your number of games correctly, usually you have the id 1.
if [ ${NAME_ID_RESULT} =  16 -o ${NAME_ID_RESULT} = 39 ] 
    then PLAYER_ID="player_id1"
    else PLAYER_ID="player_id0"
fi




STATS_SEARCH="select a.session_id as No, count(b.game_id) as Games, a.actual_result as Result, a.luck_adjusted_result as Luck_Adjusted, a.snowie_error_rate_per_move*(1000) as Snowie, c.added as Date from matchstat a, game b, session c where a.session_id = b.session_id and a.player_id = '${NAME_ID_RESULT}' and a.player_id = b.${PLAYER_ID} and a.session_id = c.session_id group by a.session_id ; "

NICK_NAME_RESULT=`sqlite3 $DATABASE <<EOF
$NAME_SEARCH
EOF`


POINTS_WON="select sum(actual_result) from matchstat where player_id = '$NAME_ID_RESULT';"

NUMBER_GAMES="select count(game_id) from game where ${PLAYER_ID} = '${NAME_ID_RESULT}';" 

## Still wrong
#ERROR_AVR="select sum((a.total_moves*a.snowie_error_rate_per_move)*a.total_moves/sum(a.total_moves)) from matchstat a, nick c where a.nick_id = c.nick_id and c.nick_id = '$NICK_ID_RESULT';"

TOTAL_RESULT=`sqlite3 $DATABASE <<EOF
$POINTS_WON
EOF`

STATS_RESULT=`sqlite3 $DATABASE <<EOF
.mode column 
.headers ON
.output tmp.file 
$STATS_SEARCH 
EOF`

NUMBER_GAMES_RESULT=`sqlite3 $DATABASE <<EOF
$NUMBER_GAMES
EOF`

ERROR_AVR_RESULT=`sqlite3 $DATABASE <<EOF
$ERROR_AVR
EOF`

echo
cat tmp.file
# rm tmp.file

echo
echo "Result ${NICK_NAME_RESULT} (${NAME_ID_RESULT}): $TOTAL_RESULT Point(s) in $NUMBER_GAMES_RESULT games(s). Snowie error rate: $ERROR_AVR_RESULT"
echo

exit 0

