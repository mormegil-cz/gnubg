#
# batch.py -- batch import and analysis of multiple files using gnubg
#
# by Jon Kinsey <Jon_Kinsey@hotmail.com>, 2004
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

"""
 batch.py -- batch import and analysis of multiple files using gnubg

 by Jon Kinsey <Jon_Kinsey@hotmail.com>, 2004
\n"""

# Achim Mueller <ace@gnubg.org>
#
# Usage: start gnubg -t
# (no game) load python batch.py
# Answer questions regarding source and destination directory
# Enjoy the analysis

import os

# file extensions, names and gnubg import commands
extensions = ['mat', 'pos', 'sgg', 'tmg', 'txt']
extTypes = ['.mat', '.pos', 'Games grid', 'True money game', 'Snowie text']
extCmds = ['mat', 'pos', 'sgg', 'tmg', 'snowietxt']

LAST_VALUES = "batch.dirs"


def CheckFiles(files, dir, destDir):
    "Check files aren't already analyzed"
    fileList = [[], [], [], [], []]
    foundFile = False

    for eType in extTypes:
        numType = extTypes.index(eType)
        for file in files[numType]:
            # Check that file hasn't already been analyzed
            dot = file.rfind('.')
            existingOutputFile = destDir + file[: dot] + '.sgf'
            if (os.access(existingOutputFile, os.F_OK)):
                # See if newer
                if os.stat(existingOutputFile).st_mtime > os.stat(dir + file).st_mtime:
                    continue    # Skip as already processed
            foundFile = True
            fileList[numType].append(file)

    if foundFile:
        return fileList
    else:
        print "  ** All files in directory already processed **"


def GetFiles(dir):
    "Look for gnubg import files in dir"
    try:
        files = os.listdir(dir)
    except:
        print "  ** Directory not found **"
        return 0

    fileList = [[], [], [], [], []]
    foundAnyFile = False
    foundBGFile = False
    # Check each file in dir
    for file in files:
        # Check it's a file (not a directory)
        if os.path.isfile(dir + file):
            foundAnyFile = True
            # Check has supported extension
            dot = file.rfind('.')
            if dot != -1:
                ext = file[dot + 1:].lower()
                if ext in extensions:
                    foundBGFile = True
                    ftype = extensions.index(ext)
                    fileList[ftype].append(file)

    if foundBGFile:
        return fileList
    else:
        if not foundAnyFile:
            print "  ** No files in directory **"
        else:
            print "  ** No valid files found in directory **"
        return 0


def AnalyzeFile(prompt, file, dir, destDir, type):
    "Run commands to analyze file in gnubg"
    gnubg.command('import ' + extCmds[type] + ' "' + dir + file + '"')
    print prompt + " Analysing " + file
    gnubg.command('analyze match')
    file = file[:-len(extensions[type])] + "sgf"
    gnubg.command('save match "' + destDir + file + '"')


def GetYN(prompt):
    confirm = ''
    while len(confirm) == 0 or (confirm[0] != 'y' and confirm[0] != 'n'):
        confirm = raw_input(prompt + " (y/n): ").lower()
    return confirm


def GetDir(prompt):
    dir = raw_input(prompt)
    if dir:
        # Make sure dir ends in a slash
        if (dir[-1] != '\\' and dir[-1] != '/'):
            dir = dir + '/'
    return dir


def BatchImport():
    "Import and analyse all files in a directory"
    dirs = [0, 0]
    # See if last dirs saved
    if (os.access(LAST_VALUES, os.F_OK)):
        file = open(LAST_VALUES, "r")
        dirs = file.readlines()
        file.close()
        # Remove new lines
        dirs[0] = dirs[0][:-1]
        dirs[1] = dirs[1][:-1]
        if os.path.isdir(dirs[0]) and os.path.isdir(dirs[1]):
            print "Last used dirs:\n from:", dirs[0][:-1], "\n   to:", dirs[1][:-1]
            if GetYN("Reuse") == 'n':
                dirs = [0, 0]
        else:
            dirs = [0, 0]

    while True:
        # Get directory with original files in
        if (dirs[0] == 0):
            dirs[0] = GetDir(
                "Directory containing files to import (enter-exit): ")
            if not dirs[0]:
                return

        # Look for some files
        inFiles = GetFiles(dirs[0])
        if not inFiles:
            dirs = [0, 0]
            continue

        if (dirs[1] == 0):
            # Get directory to put analyzed files in
            while True:
                dirs[1] = GetDir(
                    "Directory to put analyzed files in (enter-same dir): ")
                if not dirs[1]:
                    dirs[1] = dirs[0]

                if os.path.isdir(dirs[1]):
                    break
                print "  ** Directory not found **"

        # Make sure files are new
        files = CheckFiles(inFiles, dirs[0], dirs[1])
        if files:
            break
        dirs = [0, 0]

    # Display files that will be analyzed
    numFound = 0
    for eType in extTypes:
        numType = extTypes.index(eType)
        if len(files[numType]):
            print "\n" + eType + " files:"
            for file in files[numType]:
                print "    " + file
                numFound = numFound + 1

    if (numFound > 1):
        print "\n", numFound, "files found\n"

    # Check user wants to continue
    if GetYN("Continue") == 'n':
        return

    # Save dirs for next time
    file = open(LAST_VALUES, "w")
    file.write(dirs[0] + "\n")
    file.write(dirs[1] + "\n")
    file.close()

    # Analyze each file
    num = 0
    for eType in extTypes:
        if len(files[extTypes.index(eType)]):
            print "\n" + eType + " files:"
            numType = extTypes.index(eType)
            for file in files[numType]:
                num = num + 1
                prompt = "\n(%d/%d)" % (num, numFound)
                AnalyzeFile(prompt, file, dirs[0], dirs[1], numType)

    print "\n** Finished **"
    return

# Run batchimport on load
try:
    print __doc__
    BatchImport()
except Exception, (e):
    print "Error:", e
