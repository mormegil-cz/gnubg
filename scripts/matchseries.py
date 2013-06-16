#
# matchseries.py
#
# Martin Janke <lists@janke.net>, 2004
# Achim Mueller <ace@gnubg.org>, 2004
# Joern Thyssen <jth@gnubg.org>, 2004
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
# Play an arbitray number of matches.
#
# For example, the script could be used to play gnubg 0-ply using
# the Snowie MET against gnubg 0-ply using the Woolsey-Heinrich MET.
# This is achieved by using the external player interface.
#
# gnubg -t << EOF
# set matchequitytable "met/snowie.xml"
# set evaluation chequerplay eval plies 0
# set evaluation cubed eval plies 0
# external localhost:10000
# EOF
#
# gnubg -t << EOF
# set matchequitytable "met/woolsey.xml"
# set player 1 gnu
# set player 1 chequerplay evaluation plies 0
# set player 1 cubedecision evaluation plies 0
# set player 0 external localhost:10000
# >
# from matchseries import *
# playMatchSeries (matchLength = 3, noOfMatches = 1000,
#                  statsFile = "statistics.txt", sgfBasePath = None,
#                  matBasePath = None)
# EOF
#
# $Id$
#

import gnubg


def playMatchSeries(statsFile=None,  # log file
                    matchLength=7,
                    noOfMatches=100,
                    sgfBasePath=None,  # optional
                    matBasePath=None):  # optional
    """Starts noOfMatches matchLength pointers. For every match
    the running score, gammoms (g) and backgammons (b) and the match winner
    is written to 'statsFile':
    g       gammon
    b       backgammon
    (g)     gammon, but not relevsnt (end of match)
    (b)     backgammon, but not relevant
    g(b)    backgammon, but gammon would have been enough to win the match

    If the optional parameters 'sgfBasePath' and 'matBasePath' are set to a
    path, the matches are saved as sgf or mat file."""

    for i in range(0, noOfMatches):

        if not statsFile:
            raise ValueError('Parameter "statsFile" is mandatory')

        gnubg.command('new match ' + str(matchLength))

        matchInfo = formatMatchInfo(gnubg.match(analysis=0, boards=0))

        f = open(statsFile, 'a')
        f.write(matchInfo)
        f.close

        if sgfBasePath:
            gnubg.command('save match ' + sgfBasePath +
                          str(i) + '.sgf')
        if matBasePath:
            gnubg.command('export match mat ' + matBasePath +
                          str(i) + '.mat')


def formatMatchInfo(matchInfo):
    tempS = ''
    outString = ''
    score = [0, 0]
    matchLength = matchInfo['match-info']['match-length']

    for game in matchInfo['games']:
        pw = game['info']['points-won']
        if game['info']['winner'] == 'O':
            winner = 1
        else:
            winner = 0
        oldScore = score[winner]
        score[winner] += pw

        cube = getCube(game)
        print 'cube: ', cube

        if pw == cube:
            gammon = ''
        elif pw == 2 * cube:
            gammon = 'g'
            if (cube + oldScore) >= matchLength:  # gammon not relevant
                gammon = '(g)'

        elif pw == 3 * cube:
            gammon = 'b'

            if (cube + oldScore) >= matchLength:  # backgammon not relevant
                gammon = '(b)'
            elif (2 * cube + oldScore) >= matchLength:
                # backgammon not relevant, but gammon
                gammon = 'g(b)'

        outString += ',%d:%d%s' % (score[0], score[1], gammon)

    outString = str(winner) + outString + '\n'

    return outString


def getCube(game):
    """returns the cube value of the game"""
    doubled = 0
    cube = 1
    for turn in game['game']:
        if turn['action'] == 'double':
            doubled = 1
            continue

        if doubled:
            if turn['action'] == 'take':
                cube *= 2
                doubled = 0
            else:  # dropped
                break
    return cube
