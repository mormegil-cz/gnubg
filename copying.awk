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
# Stolen from the copying.awk script included in gdb.

BEGIN {
  FS="\"";
  print "/* Do not modify this file!  It is created automatically by";
  print "   copying.awk.  Modify copying.awk instead. */";
  print "";
  print "#include \"config.h\""
  print "char *aszCopying[] = {";
}

/^[ \t]*END OF TERMS AND CONDITIONS[ \t]*$/ {
  print "  0\n};";
  exit;
}

/^[ \t]*15. Disclaimer of Warranty.*$/ {
  print "  0\n}, *aszWarranty[] = {";
}

{
  if ($0 ~ /\f/) {
    print "  \"\",";
  } else {
    printf "  \"";
    for (i = 1; i < NF; i++)
      printf "%s\\\"", $i;
    printf "%s\",\n", $NF;
  }
}
