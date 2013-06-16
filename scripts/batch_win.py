#
# batch_win.py -- batch import and analysis of multiple files using gnubg
#
# by Oystein Johansen <oystein@gnubg.org>, 2004
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

# This file is inspired of the batch.py file of Jon Kinsey. The code
# contains MS-Windows specific code and will therefore only work on MS-Windows
# together with GNU Backgammon and Python.

import win32ui
import win32con

# file extensions, names and gnubg import commands

extensions = ['mat', 'pos', 'sgg', 'tmg', 'txt']
extTypes = ['.mat', '.pos', 'Games grid', 'True money game', 'Snowie text']
extCmds = ['mat', 'pos', 'sgg', 'tmg', 'snowietxt']


def BatchAnalyze(filelist):
    for file in filelist:
        dot = file.rfind('.')
        if dot != -1:
            ext = file[dot + 1:].lower()
            if ext in extensions:
                AnalyzeFile(file, extensions.index(ext))


def GetFiles():
    import win32ui
    import win32con
    filedialog = win32ui.CreateFileDialog(
        1, "", "", win32con.OFN_ALLOWMULTISELECT | win32con.OFN_HIDEREADONLY)
    filedialog.SetOFNTitle("Select files to analyse")
    filedialog.DoModal()

    return filedialog.GetPathNames()


def AnalyzeFile(file, type):
    "Run commands to analyze file in gnubg"
    gnubg.command('import ' + extCmds[type] + ' "' + file + '"')
    gnubg.command('analyze match')
    file = file[:-len(extensions[type])] + "sgf"
    gnubg.command('save match "' + file + '"')

files = GetFiles()
BatchAnalyze(files)
