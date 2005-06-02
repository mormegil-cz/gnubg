Èeština pro GNUBG
*****************

Jak nainstalovat?
-----------------
Pokud ho snad ještì nemáte, budete opravdu potøebovat GNU Backgammon.
Odkaz na staení najdete na stránkách na adrese http://www.gnubg.org/,
nemusíte se bát, jedná se o volnı software šíøenı zdarma pod GNU licencí.

Poté rozbalte archív s èeskım pøekladem do adresáøe, do kterého jste
GNU Backgammon nainstalovali. (Tím v nìm vznikne adresáø LOCALE.)

Spuste GNU Backgammon.

Pokud provozujete èeskı operaèní systém, je dost moné, e program ji
bìí v èeštinì. Pokud ne, zvolte nabídku Settings, v ní poloku Options.
V oknì, které se tím otevøe, zvolte poslední záloku Other. Na této
záloce dole je poloka Language. Z nabídky vyberte Czech. Poté potvrïte
stisknutím OK. Ulote nastavení pomocí poloky Save settings v nabídce
Settings (pokud nastavení neuloíte, nebude to fungovat!). Odejdìte
z GNU Backgammonu a spuste ho znovu. Teï u by mìl bıt urèitì èesky.

Uívejte si hru v èeštinì!


Problémy?
---------
Otázka:  V mém programu v nabídce jazykù èeština chybí! Co mám dìlat?
Odpovìï: Máte dvì monosti: Buï si stáhnìte novou verzi programu (na
stránkách http://www.gnubg.org/ najdìte odkaz na "Daily builds", odkud
si mùete stáhnout kadı den aktuální vıvojovou verzi), nebo mùete
jazyk nastavit ruènì: V nabídce Edit vyberte Enter command a do
nabídnutého øádku zadejte "set lang cs" (bez uvozovek). Poté ulote
nastavení a restartujte gnubg podle pøedchozího návodu. Všechno by mìlo
fungovat.

Otázka: Pøi experimentování jsem si nastavil nìjakı šílenı jazyk a teï
nièemu nerozumím. Jak tam mám vrátit èeštinu?
Odpovìï: V adresáøi programu GNU Backgammon je soubor .gnubgautorc. Ten
otevøete v nìjakém textovém editoru a najdìte v nìm text "set lang" (bez
uvozovek). Buï mùete celı øádek vymazat, nebo nastavit jazykovı kód
na jazyk, kterı chcete pouívat (kód èeštiny je cs).

Otázka: Skoro všechno je èesky, ale obèas se objeví i nìco anglicky. Kde
je problém?
Odpovìï: Systém GNU gettext, pomocí kterého je lokalizace GNU Backgammonu
provedena, funguje tak, e pokud není nalezen pøeklad pro danı termín,
je pouita pùvodní (anglická) verze. Take, pokud se vám obèas objeví
nìco anglicky, znamená to, e v pøekladu nìco chybí. Další informace
viz následující kapitolka.


Chyby v pøekladu
----------------
Pokud najdete nìjakou chybu v pøekladu (jako tøeba pøeklep, pravopisnou
chybu, chybnì pøeloenı termín, atd.), rozhodnì mi o tom dejte vìdìt!
Moje e-mailová adresa je mormegil@centrum.cz a pokud u mailu pouijete
nìjakı smysluplnı pøedmìt, ve kterém se objeví "pøeklad gnubg", bude to
fajn.

Ale budu rád, pokud pøed nahlášením chyby budete pøemıšlet. Pokud jste si
právì stáhli nejnovìjší verzi gnubg, ve které se objevily nové texty, tak
byste se rozhodnì nemìli divit, e dosud nejsou pøeloeny. Obecnì, pokud
se nìkde objeví pùvodní anglickı text, je to buï proto, e se objevil
v nové verzi, pro kterou dosud nemáte pøeklad, nebo proto, e je to chyba
v pùvodním gnubg. Díky mechanismu pøekladu se _nemùe_ stát, e bych
zapomnìl pøeloit jednu vìtu. Proto prosím mìjte strpení, na odstranìní
takovıch problémù se pracuje. Pøesto, pokud se vám zdá, e pøeklad daného
text u chybí dost dlouho (anebo je na nìjakém zøídka pouívaném místì),
dejte mi vìdìt, je moné, e jsem si té chyby dosud ani nevšiml.

Pokud se vám nezdá nìjakı odbornı termín (tøeba proto, e jste zvyklí na
pùvodní anglickı), neznamená to ještì, e je jeho pøeklad chybnı. Pokud mi
navrhnete lepší termín, budu ho zvaovat, ale stínosti typu "mì se víc líbí
anglickı termín" budu ignorovat (jako odpovìï si pøedstavte "tak pouívejte
anglickou verzi").


Nové verze
----------
Jeliko GNU Backgammon se stále bouølivì vyvíjí, prùbìnì v nìm pøibıvají
(a mìní se) texty. Proto i pøeklad se musí vyvíjet. Nové verze pøekladu
by se mìly šíøit spoleènì s GNU Backgammonem, ale budu se snait udrovat
vdy poslední verzi na http://mormegil.wz.cz/bg/
Doporuèeníhodné je vdy updatovat pøeklad souèasnì s GNU Backgammonem tak,
abyste pouívali vdy tu verzi pøekladu, která souhlasí s programem.


Licence
-------
Èeskı pøeklad je šíøen podle stejné licence jako celı balík GNU Backgammon,
tzn. podle GNU licence, co znamená, e ho smíte volnì kopírovat.
(Podrobnosti viz text licence.)


Autor
-----
Autor pøekladu, Petr Kadlec, je k zastiení na e-mailové adrese
mormegil@centrum.cz, na které oèekává i vaše pøipomínky a informace
o chybách v pøekladu. Obèas ho mùete zastihnout i na ICQ (#68196926).
