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

DATABASE=/home/ace/.gnubg/ace.db
PLAYER=$1

NICK_ID_SEARCH="select nick_id from nick where name like '%$1%';"

NICK_NAME_SEARCH="select name from nick where name like '%$1%';"

NICK_ID_RESULT=`sqlite3 $DATABASE <<EOF
$NICK_ID_SEARCH
EOF`

STATS_SEARCH="select a.match_id as No, count(b.game_id) as Games, a.actual_result as Result, a.luck_adjusted_result as Luck_Adjusted, a.snowie_error_rate_per_move*(1000) as Snowie, b.added as Date from matchstat a, game b, nick c where a.match_id = b.match_id and a.nick_id = c.nick_id and c.nick_id = '$NICK_ID_RESULT' group by a.match_id;" 

NICK_NAME_RESULT=`sqlite3 $DATABASE <<EOF
$NICK_NAME_SEARCH
EOF`

POINTS_WON="select sum(a.actual_result) from matchstat a, nick c where a.nick_id = c.nick_id and c.nick_id = '$NICK_ID_RESULT';" 

TOTAL_RESULT=`sqlite3 $DATABASE <<EOF
$POINTS_WON
EOF`

STATS_RESULT=`sqlite3 $DATABASE  <<EOF
.mode column 
.headers ON
.output tmp.file 
$STATS_SEARCH 
EOF`

echo
cat tmp.file
rm tmp.file

echo
echo Result ${NICK_NAME_RESULT}: $TOTAL_RESULT Point\(s\)
echo

exit 0

