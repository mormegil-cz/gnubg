%{

/*
 * command_l.l -- lexer for the command
 *
* [insert GPL here]
 */
#include <string.h>

#include "external_y.h"

int prc_lineno = 1;
void escapes(const char *cp, char *tp);

%}

%option noyywrap
%option prefix="ext"
%option outfile="lex.yy.c"

%%

\"[^\"]*\"	{
			char buf[100];

			exttext[strlen(exttext)-1] = '\0';
			escapes(exttext+1, buf);
			extlval.sval = (char *) strdup(buf);
                        BEGIN(INITIAL);
			return STRING;
		}
\'[^\']*\'	{
			char buf[100];

			exttext[strlen(exttext)-1] = '\0';
			escapes(exttext+1, buf);
			extlval.sval = (char *) strdup(buf);
                        BEGIN(0);
			return STRING;
		}


evaluation      { return EVALUATION; }
fibsboard       { return FIBSBOARD; }
cubeful         { return CUBEFUL; }
cubeless        { return CUBELESS; }
cube            { return CUBE; }
plies           { return PLIES; }
noise           { return NOISE; }
reduced         { return REDUCED; }
yes             { return ON; }
on              { return ON; }
no              { return OFF; }
off             { return OFF; }

board:[A-Za-z0-9]+:[A-Za-z0-9]+(:-?[0-9]+)+  { 
			char buf[100];

			escapes(exttext, buf);
			extlval.sval = (char *) strdup(buf);
                        return AFIBSBOARD; 
                }


(#.*)?\\?\n	{ prc_lineno++;	}   /* newline is ignored */

-?[0-9]+	{ extlval.number = atoi(exttext); return NUMBER; }

[^=;:, \t\r\n]+	{
			char buf[100];

			escapes(exttext, buf);
			extlval.sval = (char *) strdup(buf);
			return STRING;
		}

[ \t\r]+	;	/* whitespace */

%%

void escapes(const char *cp, char *tp)
{
    while (*cp)
    {
	int	cval = 0;

	if (*cp == '\\' && strchr("0123456789xX", cp[1]))
	{
	    char *dp;
	    const char *hex = "00112233445566778899aAbBcCdDeEfF";
	    int dcount = 0;

	    if (*++cp == 'x' || *cp == 'X')
		for (++cp; (dp = strchr(hex, *cp)) && (dcount++ < 2); cp++)
		    cval = (cval * 16) + (dp - hex) / 2;
	    else if (*cp == '0')
		while (strchr("01234567",*cp) != (char*)NULL && (dcount++ < 3))
		    cval = (cval * 8) + (*cp++ - '0');
	    else
		while ((strchr("0123456789",*cp)!=(char*)NULL)&&(dcount++ < 3))
		    cval = (cval * 10) + (*cp++ - '0');
	}
	else if (*cp == '\\')		/* C-style character escapes */
	{
	    switch (*++cp)
	    {
	    case '\\': cval = '\\'; break;
	    case 'n': cval = '\n'; break;
	    case 't': cval = '\t'; break;
	    case 'b': cval = '\b'; break;
	    case 'r': cval = '\r'; break;
	    default: cval = *cp;
	    }
	    cp++;
	}
	else
	    cval = *cp++;
	*tp++ = cval;
    }
    *tp = '\0';
}

