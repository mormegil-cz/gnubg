/*
 * latex.c
 *
 * by Gary Wong <gtw@gnu.org>, 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"

static char*aszLuckTypeLaTeXAbbr[] = { "$--$", "$-$", "", "$+$", "$++$" };

static void Points1_12( FILE *pf, int y ) {

    int i;
    
    for( i = 1; i <= 12; i++ )
	fprintf( pf, "\\put(%d,%d){\\makebox(20,10){\\textsf{%d}}}\n",
		 330 - i * 20 - ( i > 6 ) * 20, y, i );
}

static void Points13_24( FILE *pf, int y ) {

    int i;
    
    for( i = 13; i <= 24; i++ )
	fprintf( pf, "\\put(%d,%d){\\makebox(20,10){\\textsf{%d}}}\n",
		 i * 20 - 190 + ( i > 18 ) * 20, y, i );
}

static void LaTeXPrologue( FILE *pf ) {

    fputs( "\\documentclass{article}\n"
	   "\\usepackage{epic,eepic}\n"
	   "\\newcommand{\\board}{\n"
	   "\\shade\\path(70,20)(80,120)(90,20)(110,20)(120,120)(130,20)"
	   "(150,20)(160,120)\n"
	   "(170,20)(70,20)\n"
	   "\\path(70,20)(80,120)(90,20)(100,120)(110,20)(120,120)(130,20)"
	   "(140,120)(150,20)\n"
	   "(160,120)(170,20)(180,120)(190,20)\n"
	   "\\shade\\path(90,240)(100,140)(110,240)(130,240)(140,140)(150,240)"
	   "(170,240)\n"
	   "(180,140)(190,240)(90,240)\n"
	   "\\path(70,240)(80,140)(90,240)(100,140)(110,240)(120,140)(130,240)"
	   "(140,140)\n"
	   "(150,240)(160,140)(170,240)(180,140)(190,240)\n"
	   "\\shade\\path(210,20)(220,120)(230,20)(250,20)(260,120)(270,20)"
	   "(290,20)(300,120)\n"
	   "(310,20)(210,20)\n"
	   "\\path(210,20)(220,120)(230,20)(240,120)(250,20)(260,120)(270,20)"
	   "(280,120)\n"
	   "(290,20)(300,120)(310,20)(320,120)(330,20)\n"
	   "\\shade\\path(230,240)(240,140)(250,240)(270,240)(280,140)"
	   "(290,240)(310,240)\n"
	   "(320,140)(330,240)(230,240)\n"
	   "\\path(210,240)(220,140)(230,240)(240,140)(250,240)(260,140)"
	   "(270,240)(280,140)\n"
	   "(290,240)(300,140)(310,240)(320,140)(330,240)\n"
	   "\\path(60,10)(340,10)(340,250)(60,250)(60,10)\n"
	   "\\path(70,20)(190,20)(190,240)(70,240)(70,20)\n"
	   "\\path(210,20)(330,20)(330,240)(210,240)(210,20)}\n\n", pf );

    fputs( "\\newcommand{\\blackboard}{\n"
	   "\\board\n", pf );
    Points1_12( pf, 10 );
    Points13_24( pf, 240 );
    fputs( "}\n\n", pf );

    fputs( "\\newcommand{\\whiteboard}{\n"
	   "\\board\n", pf );
    Points1_12( pf, 240 );
    Points13_24( pf, 10 );
    fputs( "}\n\n"
	   "\\addtolength\\textwidth{144pt}\n"
	   "\\addtolength\\textheight{144pt}\n"
	   "\\addtolength\\oddsidemargin{-72pt}\n"
	   "\\addtolength\\evensidemargin{-72pt}\n"
	   "\\addtolength\\topmargin{-72pt}\n\n"
	   "\\begin{document}\n", pf );

    /* FIXME define commands for \cubetext, \labeltext, etc. here, which
       are scaled to a size specified by the user.  The other functions
       should then use those definitions. */
}

static void LaTeXEpilogue( FILE *pf ) {

    fputs( "\\end{document}\n", pf );
}

static void DrawLaTeXPoint( FILE *pf, int i, int fPlayer, int c ) {

    int j, x, y;

    if( i < 6 )
	x = 320 - 20 * i;
    else if( i < 12 )
	x = 300 - 20 * i;
    else if( i < 18 )
	x = 20 * i - 160;
    else if( i < 24 )
	x = 20 * i - 140;
    else if( i == 24 ) /* bar */
	x = 200;
    else /* off */
	x = 365;
    
    for( j = 0; j < c; j++ ) {
	if( j == 5 || ( i == 24 && j == 3 ) ) {
	    fprintf( pf, "\\whiten\\path(%d,%d)(%d,%d)(%d,%d)(%d,%d)(%d,%d)\n"
		     "\\path(%d,%d)(%d,%d)(%d,%d)(%d,%d)(%d,%d)\n"
		     "\\put(%d,%d){\\makebox(10,10){\\textsf{%d}}}\n",
		     x - 5, y - 5, x + 5, y - 5, x + 5, y + 5, x - 5, y + 5,
		     x - 5, y - 5,
		     x - 5, y - 5, x + 5, y - 5, x + 5, y + 5, x - 5, y + 5,
		     x - 5, y - 5,
		     x - 5, y - 5, c );
	    break;
	}

	if( i == 24 )
	    /* bar */
	    y = fPlayer ? 60 + 20 * j : 200 - 20 * j;
	else if( fPlayer )
	    y = i < 12 || i == 25 ? 30 + 20 * j : 230 - 20 * j;
	else
	    y = i >= 12 && i != 25 ? 30 + 20 * j : 230 - 20 * j;
	
	fprintf( pf, fPlayer ? "\\put(%d,%d){\\circle{10}}"
		 "\\put(%d,%d){\\blacken\\circle{20}}\n" :
		 "\\put(%d,%d){\\whiten\\circle{20}}"
		 "\\put(%d,%d){\\circle{20}}\n", x, y, x, y );
    }
}

static void PrintLaTeXBoard( FILE *pf, int anBoard[ 2 ][ 25 ], int fPlayer,
			     int fCubeOwner, int nCube ) {

    int anOff[ 2 ] = { 15, 15 }, i, y;

    /* FIXME print position ID and pip count, and the player on roll.
       Print player names too? */
    
    fprintf( pf, "\\bigskip\\pagebreak[1]\\begin{center}\\begin{picture}"
	     "(356,240)(22,10)\n"
	     "\\%sboard\n", fPlayer ? "black" : "white" );

    for( i = 0; i < 25; i++ ) {
	anOff[ 0 ] -= anBoard[ 0 ][ i ];
	anOff[ 1 ] -= anBoard[ 1 ][ i ];

	DrawLaTeXPoint( pf, i, 0, anBoard[ !fPlayer ][ i ] );
	DrawLaTeXPoint( pf, i, 1, anBoard[ fPlayer ][ i ] );
    }

    DrawLaTeXPoint( pf, 25, 0, anOff[ !fPlayer ] );
    DrawLaTeXPoint( pf, 25, 1, anOff[ fPlayer ] );

    if( fCubeOwner < 0 )
	y = 130;
    else if( fCubeOwner )
	y = 30;
    else
	y = 230;

#if 0
    /* EEPIC ovals don't work very well */
    fprintf( pf, "\\put(35,%d){\\setlength\\maxovaldiam {7pt} \\oval(24,24)}"
	     "\\put(23,%d){\\makebox(24,24){%d}}\n", y, y - 12,
	     nCube == 1 ? 64 : nCube );
#else
    fprintf( pf, "\\path(23,%d)(47,%d)(47,%d)(23,%d)(23,%d)"
	     "\\put(23,%d){\\makebox(24,24){\\textsf{\\LARGE %d}}}\n",
	     y - 12, y - 12, y + 12, y + 12, y - 12, y - 12,
	     nCube == 1 ? 64 : nCube );
#endif
    
    fputs( "\\end{picture}\\end{center}\\vspace{-4mm}\n\n\\nopagebreak[4]\n",
	   pf );
}

static void PrintLaTeXComment( FILE *pf, unsigned char *pch ) {

    /* Translation table from GNU recode, by François Pinard. */
    static struct translation {
	unsigned char uch;
	char *sz;
    } at[] = {
	{ 10, "\n\n" },		    /* \n */
	{ 35, "\\#"},		    /* # */
	{ 36, "\\$"},		    /* $ */
	{ 37, "\\%"},		    /* % */
	{ 38, "\\&"},		    /* & */
	{ 60, "\\mbox{$<$}"},	    /* < */
	{ 62, "\\mbox{$>$}"},	    /* > */
	{ 92, "\\mbox{$\\backslash{}$}"}, /* \ */
	{ 94, "\\^{}" },	    /* ^ */
	{ 95, "\\_"},		    /* _ */
	{123, "\\{"},		    /* { */
	{124, "\\mbox{$|$}"},	    /* | */
	{125, "\\}"},		    /* } */
	{126, "\\~{}" },	    /* ~ */
	{160, "~"},                 /* no-break space */
	{161, "!`"},                /* inverted exclamation mark */
	{163, "\\pound{}"},         /* pound sign */
	{167, "\\S{}"},             /* paragraph sign, section sign */
	{168, "\\\"{}"},            /* diaeresis */
	{169, "\\copyright{}"},     /* copyright sign */
	{171, "``"},                /* left angle quotation mark */
	{172, "\\neg{}"},           /* not sign */
	{173, "\\-"},               /* soft hyphen */
	{176, "\\mbox{$^\\circ$}"}, /* degree sign */
	{177, "\\mbox{$\\pm$}"},    /* plus-minus sign */
	{178, "\\mbox{$^2$}"},      /* superscript two */
	{179, "\\mbox{$^3$}"},      /* superscript three */
	{180, "\\'{}"},             /* acute accent */
	{181, "\\mbox{$\\mu$}"},    /* small greek mu, micro sign */
	{183, "\\cdotp"},           /* middle dot */
	{184, "\\,{}"},             /* cedilla */
	{185, "\\mbox{$^1$}"},      /* superscript one */
	{187, "''"},                /* right angle quotation mark */
	{188, "\\frac1/4{}"},       /* vulgar fraction one quarter */
	{189, "\\frac1/2{}"},       /* vulgar fraction one half */
	{190, "\\frac3/4{}"},       /* vulgar fraction three quarters */
	{191, "?`"},                /* inverted question mark */	
	{192, "\\`A"},              /* capital A with grave accent */
	{193, "\\'A"},              /* capital A with acute accent */
	{194, "\\^A"},              /* capital A with circumflex accent */
	{195, "\\~A"},              /* capital A with tilde */
	{196, "\\\"A"},             /* capital A diaeresis */
	{197, "\\AA{}"},            /* capital A with ring above */
	{198, "\\AE{}"},            /* capital diphthong A with E */
	{199, "\\c{C}"},            /* capital C with cedilla */
	{200, "\\`E"},              /* capital E with grave accent */
	{201, "\\'E"},              /* capital E with acute accent */
	{202, "\\^E"},              /* capital E with circumflex accent */
	{203, "\\\"E"},             /* capital E with diaeresis */
	{204, "\\`I"},              /* capital I with grave accent */
	{205, "\\'I"},              /* capital I with acute accent */
	{206, "\\^I"},              /* capital I with circumflex accent */
	{207, "\\\"I"},             /* capital I with diaeresis */
	{209, "\\~N"},              /* capital N with tilde */
	{210, "\\`O"},              /* capital O with grave accent */
	{211, "\\'O"},              /* capital O with acute accent */
	{212, "\\^O"},              /* capital O with circumflex accent */
	{213, "\\~O"},              /* capital O with tilde */
	{214, "\\\"O"},             /* capital O with diaeresis */
	{216, "\\O{}"},             /* capital O with oblique stroke */
	{217, "\\`U"},              /* capital U with grave accent */
	{218, "\\'U"},              /* capital U with acute accent */
	{219, "\\^U"},              /* capital U with circumflex accent */
	{220, "\\\"U"},             /* capital U with diaeresis */
	{221, "\\'Y"},              /* capital Y with acute accent */
	{223, "\\ss{}"},            /* small german sharp s */
	{224, "\\`a"},              /* small a with grave accent */
	{225, "\\'a"},              /* small a with acute accent */
	{226, "\\^a"},              /* small a with circumflex accent */
	{227, "\\~a"},              /* small a with tilde */
	{228, "\\\"a"},             /* small a with diaeresis */
	{229, "\\aa{}"},            /* small a with ring above */
	{230, "\\ae{}"},            /* small diphthong a with e */
	{231, "\\c{c}"},            /* small c with cedilla */
	{232, "\\`e"},              /* small e with grave accent */
	{233, "\\'e"},              /* small e with acute accent */
	{234, "\\^e"},              /* small e with circumflex accent */
	{235, "\\\"e"},             /* small e with diaeresis */
	{236, "\\`{\\i}"},          /* small i with grave accent */
	{237, "\\'{\\i}"},          /* small i with acute accent */
	{238, "\\^{\\i}"},          /* small i with circumflex accent */
	{239, "\\\"{\\i}"},         /* small i with diaeresis */
	{241, "\\~n"},              /* small n with tilde */
	{242, "\\`o"},              /* small o with grave accent */
	{243, "\\'o"},              /* small o with acute accent */
	{244, "\\^o"},              /* small o with circumflex accent */
	{245, "\\~o"},              /* small o with tilde */
	{246, "\\\"o"},             /* small o with diaeresis */
	{248, "\\o{}"},             /* small o with oblique stroke */
	{249, "\\`u"},              /* small u with grave accent */
	{250, "\\'u"},              /* small u with acute accent */
	{251, "\\^u"},              /* small u with circumflex accent */
	{252, "\\\"u"},             /* small u with diaeresis */
	{253, "\\'y"},              /* small y with acute accent */
	{255, "\\\"y"},             /* small y with diaeresis */
	{0, NULL}	
    };
    int i;

    if( !pch )
	return;

    while( *pch ) {
	for( i = 0; at[ i ].uch; i++ )
	    if( at[ i ].uch > *pch ) {
		/* no translation required */
		putc( *pch, pf );
		break;
	    } else if( at[ i ].uch == *pch ) {
		/* translation found */
		fputs( at[ i ].sz, pf );
		break;
	    }
	pch++;
    }

    fputs( "\n\n", pf );
}

static void PrintLaTeXCubeAnalysis( FILE *pf, int nCube, int fCubeOwner,
				    int fPlayer, int nMatchTo,
				    int anScore[ 2 ], int fCrawford,
				    int fJacoby, int fBeavers,
				    float arDouble[ 4 ], evaltype et,
				    evalsetup *pes ) {
    cubeinfo ci;
    char sz[ 1024 ];

    if( et == EVAL_NONE )
	return;
    
    SetCubeInfo( &ci, nCube, fCubeOwner, fPlayer, nMatchTo, anScore,
		 fCrawford, fJacoby, fBeavers );
    
    if( !GetDPEq( NULL, NULL, &ci ) )
	/* No cube action possible */
	return;
    
    GetCubeActionSz( arDouble, sz, &ci, fOutputMWC, FALSE );

    /* FIXME use center and tabular environment instead of verbatim */
    fputs( "{\\begin{quote}\\footnotesize\\begin{verbatim}\n", pf );
    fputs( sz, pf );
    fputs( "\\end{verbatim}\\end{quote}}\n", pf );    
}

static void ExportGameLaTeX( FILE *pf, list *plGame ) {
    
    list *pl;
    moverecord *pmr;
    int nCube = 1, anBoard[ 2 ][ 25 ], anScore[ 2 ];
    int fCrawfordLocal = fCrawford, fJacobyLocal = fJacoby, i, fTook = FALSE,
	fBeaversLocal = fBeavers, nMatchToLocal = nMatchTo, fCubeOwner = -1;
    char sz[ 1024 ];

    /* FIXME game introduction? */
    
    InitBoard( anBoard );

    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
	switch( pmr->mt ) {
	case MOVE_GAMEINFO:
	    anScore[ 0 ] = pmr->g.anScore[ 0 ];
	    anScore[ 1 ] = pmr->g.anScore[ 1 ];
	    fCrawfordLocal = pmr->g.fCrawfordGame;
	    nMatchToLocal = pmr->g.nMatch;
	    nCube = 1 << pmr->g.nAutoDoubles;
	    
	    break;
	    
	case MOVE_NORMAL:
	    if( fTook )
		/* no need to print board following a double/take */
		fTook = FALSE;
	    else
		PrintLaTeXBoard( pf, anBoard, pmr->n.fPlayer, fCubeOwner,
				 nCube );
	    
	    PrintLaTeXCubeAnalysis( pf, nCube, fCubeOwner, pmr->n.fPlayer,
				    nMatchToLocal, anScore, fCrawfordLocal,
				    fJacobyLocal, fBeaversLocal,
				    pmr->n.arDouble, pmr->n.etDouble,
				    &pmr->n.esDouble );

	    sprintf( sz, "%d%d%s: ", pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ],
		     aszLuckTypeLaTeXAbbr[ pmr->n.lt ] );
	    FormatMove( strchr( sz, 0 ), anBoard, pmr->n.anMove );
	    fprintf( pf, "\\begin{center}%s%s\\end{center}\n\n", sz,
		     aszSkillTypeAbbr[ pmr->n.st ] );

	    /* FIXME use center and tabular environment instead of verbatim */
	    fputs( "{\\footnotesize\\begin{verbatim}\n", pf );
	    for( i = 0; i < pmr->n.ml.cMoves && i <= pmr->n.iMove; i++ ) {
		if( i >= 5 /* FIXME allow user to choose limit */ &&
		    i != pmr->n.iMove )
		    continue;

		putc( i == pmr->n.iMove ? '*' : ' ', pf );
		FormatMoveHint( sz, anBoard, &pmr->n.ml, i,
				i != pmr->n.iMove ||
				i != pmr->n.ml.cMoves - 1 );
		fputs( sz, pf );
	    }
	    fputs( "\\end{verbatim}}", pf );    
		
	    PrintLaTeXComment( pf, pmr->a.sz );
	    
	    ApplyMove( anBoard, pmr->n.anMove, FALSE );
	    SwapSides( anBoard );
	    
	    break;
	    
	case MOVE_DOUBLE:
	    PrintLaTeXBoard( pf, anBoard, pmr->d.fPlayer, fCubeOwner, nCube );

	    PrintLaTeXCubeAnalysis( pf, nCube, fCubeOwner, pmr->d.fPlayer,
				    nMatchToLocal, anScore, fCrawfordLocal,
				    fJacobyLocal, fBeaversLocal,
				    pmr->d.arDouble, pmr->d.etDouble,
				    &pmr->d.esDouble );

	    fprintf( pf, "\\begin{center}Double%s\\end{center}\n\n",
		     aszSkillTypeAbbr[ pmr->d.st ] );
	    
	    PrintLaTeXComment( pf, pmr->a.sz );
	    
	    nCube <<= 1;
	    fCubeOwner = !pmr->d.fPlayer;
	    
	    break;
	    
	case MOVE_TAKE:
	    fTook = TRUE;
	    fprintf( pf, "\\begin{center}Take%s\\end{center}\n\n",
		     aszSkillTypeAbbr[ pmr->d.st ] );

	    PrintLaTeXComment( pf, pmr->a.sz );
	    
	    break;
	    
	case MOVE_DROP:
	    fprintf( pf, "\\begin{center}Drop%s\\end{center}\n\n",
		     aszSkillTypeAbbr[ pmr->d.st ] );

	    PrintLaTeXComment( pf, pmr->a.sz );
	    
	    break;
	    
	case MOVE_RESIGN:
	    /* FIXME print resign */
	    /* FIXME print resignation analysis, if available */
	    PrintLaTeXComment( pf, pmr->a.sz );
	    /* FIXME adjust score */
	    break;
	case MOVE_SETDICE:
	    /* ignore */
	    break;
	case MOVE_SETBOARD:
	case MOVE_SETCUBEVAL:
	case MOVE_SETCUBEPOS:
	    /* FIXME apply */
	    break;
	}
    }
    
    if( ( GameStatus( anBoard ) ) )
	/* FIXME print game result */
	;
}

extern void CommandExportGameLaTeX( char *sz ) {

    FILE *pf;
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export"
		 "game latex')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    LaTeXPrologue( pf );
    
    ExportGameLaTeX( pf, plGame );

    LaTeXEpilogue( pf );
    
    if( pf != stdout )
	fclose( pf );
}

extern void CommandExportMatchLaTeX( char *sz ) {
    
    FILE *pf;
    list *pl;

    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export "
		 "match latex')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    LaTeXPrologue( pf );

    /* FIXME write match introduction? */
    
    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
	ExportGameLaTeX( pf, pl->p );
    
    LaTeXPrologue( pf );
    
    if( pf != stdout )
	fclose( pf );
}
