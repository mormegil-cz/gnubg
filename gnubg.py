#
# gnubg.py
#
# This file is read by GNU Backgammon during startup.
# You can add your own user specified functions, if you wish.
# Below are a few examples for inspiration.
#
# Exercise: write a shorter function for calculating pip count!
#
# by Joern Thyssen <jth@gnubg.org>, 2003
#
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


def swapboard(board):
    """Swap the board"""

    return [board[1],board[0]];


def pipcount(board):
    """Calculate pip count"""

    sum = [0, 0];
    for i in range(2):
        for j in range(25):
            sum[i] += ( j + 1 ) * board[i][j]

    return sum;

    
