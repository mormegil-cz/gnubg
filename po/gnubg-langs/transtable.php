<?php



$langnames = array(

    "cs" => "Czech",

    "da" => "Danish",

    "de" => "German",

    "en" => "English",

    "en_US" => "English US",

    "es" => "Spanish",

    "fr" => "French",

    "is" => "Icelandic",

    "it" => "Italian",

    "ja" => "Japanese",

    "ro" => "Romanian",

    "ru" => "Russian",

    "tr" => "Turkish",

);



$catstatus = array();

$authors = array();

$show_author = 0;



include '__catstatus.php';

include '__config.php';



?>



<p>



<table class="translations">

    <tr>

        <th class="lang">language</th>

        <th class="trans">translated</th>

        <th class="fuzzy">fuzzy</th>

        <th class="untrans">untrans.</th>

        <th class="graph">graph</th>

        <th class="lastupdate">last updated</th>

        <th class="download">download</th>

    </tr>

    <?php

        function get_langname($langcode)

        {

            global $langnames;

            if (strlen($langcode) > 2 && $langcode[2] == '_')

                $key=$langcode[0].$langcode[1];

            else

                $key=$langcode;

            return $langnames[$key];

        }



        function cmp_catstatus($a, $b)

        {

            $la = get_langname($a);

            $lb = get_langname($b);



            if ($la == $lb) {

                return 0;

            }

            return ($la < $lb) ? -1 : 1;

        }



        uksort($catstatus, "cmp_catstatus");

        

        while (list($lang, $value) = each ($catstatus))

        {

            $remark = "";

            $cnt_total = $value[0] + $value[1] + $value[2];

            if ($cnt_total == 0) {

                $cnt_total = 1;

                $remark = "<b>(BROKEN!)</b>";

            }

            $cnt_trans = 100 * $value[0] / $cnt_total;

            $cnt_fuzzy = 100 * $value[1] / $cnt_total;

            $cnt_untrans = 100 * $value[2] / $cnt_total;

            $w_trans = (int)($graph_width * $value[0] / $cnt_total);

            $w_fuzzy = (int)($graph_width * ($value[0] + $value[1]) / $cnt_total) - $w_trans;

            $w_untrans = (int)($graph_width * ($value[0] + $value[1] + $value[2]) / $cnt_total) - $w_trans - $w_fuzzy;

            if ($remark != "")

                echo "<tr class=\"error\">";

            else if ($value[1] + $value[2] > 0)

                echo "<tr class=\"incomplete\">";

            else

                echo "<tr class=\"complete\">";



            echo "<td class=\"lang\">", get_langname($lang), " ($lang) $remark</td>";

            printf("<td class=\"trans\">%0.1f %%</td>", $cnt_trans);

            printf("<td class=\"fuzzy\">%0.1f %%</td>", $cnt_fuzzy);

            printf("<td class=\"untrans\">%0.1f %%</td>", $cnt_untrans);

            echo "<td class=\"graph\">";

            if ($cnt_trans != 0) {

                echo "<img src=\"$img_translated\" width=\"$w_trans\" height=\"16\"/>";

            }

            if ($cnt_fuzzy != 0) {

                echo "<img src=\"$img_fuzzy\" width=\"$w_fuzzy\" height=\"16\"/>";

            }

            if ($cnt_untrans != 0) {

                echo "<img src=\"$img_untranslated\" width=\"$w_untrans\" height=\"16\"/>";

            }

            echo "</td>";

            echo "<td class=\"lastupdate\">$value[3]</td>";

            echo "<td class=\"download\"><a href=\"$cvs_url$lang.po?rev=$cvs_branch&content-type=text/plain\">$lang.po</a></td>";

            echo "</tr>\n";

        }



        echo "<tr></tr>";

        echo "<tr class=\"template\">";

        echo "<td>Template file</td><td></td><td></td><td></td><td></td><td></td>";

        echo "<td class=\"download\"><a href=\"$cvs_url$potfile?rev=$cvs_branch&content-type=text/plain\">$potfile</a></td>";

        echo "</tr>\n";

     ?>

</table>



</p>



</p>

