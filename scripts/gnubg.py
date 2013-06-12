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

# Add the scrpts directory to the module path to allow 
# for modules from this directory to be improted
import sys
sys.path.append('./scripts')

def gnubg_InteractivePyShell_tui(argv=[''], banner=None):
    import sys, traceback, code

    try:
        sys.argv=argv

        from IPython.frontend.terminal.embed import InteractiveShellEmbed
        from IPython import __version__ as ipyversion
        from IPython.config.loader import Config
    except:
        try:
            import readline
        except:
            try:
                import pyreadline as readline
            except:
                pass
        try:
            import rlcompleter
            readline.parse_and_bind('tab: complete')
        except:
            pass

        if (banner == None):
            banner = 'Python ' + sys.version 

        code.interact(banner=banner,local=globals())
        return True

    try:
        cfg = Config()
        prompt_config = cfg.PromptManager
        prompt_config.in_template = 'In <\\#> > '
        prompt_config.in2_template = '   .\\D. > '
        prompt_config.out_template = 'Out<\\#> > '
        cfg.InteractiveShell.confirm_exit = False

        if banner == None:
            banner = 'IPython ' + ipyversion + ', Python ' + sys.version 

        # We want to execute in the name space of the CALLER of this function,
        # not within the namespace of THIS function.
        # This allows us to have changes made in the IPython environment 
        # visible to the CALLER of this function 
 
        # Go back one frame and get the locals. 
        call_frame = sys._getframe(0).f_back
        calling_ns = call_frame.f_locals

        ipshell = InteractiveShellEmbed(config=cfg,user_ns=calling_ns,banner1=banner)
        ipshell()

        # Cleanup the sys environment (including exception handlers)
        ipshell.restore_sys_module_state()

        return True

    except:
        traceback.print_exc()

    return False


def gnubg_InteractivePyShell_gui(argv=['','-n']):
    import sys
    sys.argv=argv

    try:
        import idlelib.PyShell
        try:
            idlelib.PyShell.main()
            return True;
        except:
            traceback.print_exc()
    except:
        pass

    return False


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

    

# Following code is intended as an example on the usage of the match command.
# It illustrates how to iterate over matches and do something useful with the
# navigate command.

import os.path

def skillBad(s) :
  return s and (s == "very bad" or s == "bad" or s == "doubtful")

def exportBad(baseName) :
  """ For current analyzed match, export all moves/cube decisions marked
  doubtful or bad"""

  # Get current match
  m = gnubg.match()

  # Go to match start
  gnubg.navigate()

  # Skill of previous action, to avoid exporting double actions twice 
  prevSkill = None

  # Exported position number, used in file name
  poscount = 0
  
  for game in m["games"] :
    for action in game["game"] :
      
      analysis = action.get("analysis", None)
      if analysis :
        type = action["action"]
        skill = analysis.get("skill", None)
        bad = skillBad(skill)
        
        if type == "move" :
          if skillBad(analysis.get("cube-skill", None)) :
            bad = True
        elif type == "take" or type == "drop" :
          if skillBad(prevSkill) :
            # Already exported
            bad = False

        if bad :
          exportfile = "%s__%d.html" % (os.path.splitext(baseName)[0],poscount)
          gnubg.command("export position html " + "\"" + exportfile + "\"")
          poscount += 1

      # Advance to next record
      gnubg.navigate(1)
      
    # Advance to next game
    gnubg.navigate(game=1)

