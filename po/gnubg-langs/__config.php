<?php
// Configuration of translations status script

// Setup ViewCVS web access here, must point to the directory with .po files:
$cvs_url="http://cvs.savannah.gnu.org/viewcvs/gnubg/gnubg/po/";

// CVS branch used to generate the page:
$cvs_branch="HEAD";

// Name of POT file (in cvs_url directory):
$potfile="gnubg.pot";

// Path to images (1x16 bars):
$img_translated="translated.png";
$img_fuzzy="fuzzy.png";
$img_untranslated="untranslated.png";

// Width of status bar:
$graph_width = 200;

// Uncomment this and define $author array in included file (key: ISO code,
// value: string with author(s) name(s)) if you want to show translator name
// alongside with the translation:

// $show_author = 1;
// include '__authors.php';
?>
