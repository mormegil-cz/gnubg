/*
 * timecontrol.c
 *
 * by Stein Kulseth <steink@opera.com>, 2003
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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "backgammon.h"
#include "eval.h"

#include "i18n.h"
#include "sound.h"
#include "timecontrol.h"

#if USE_GTK
#include "gtkgame.h"
#endif


#if WIN32
#include "ctype.h"
#endif


#if USE_TIMECONTROL

timecontrol tc=
	{ 0, 0, 0, 0, 0, 0,  0, 1.0, 0, 0, 0};

/* Key values */
static char szTCPLAIN[] = N_("plain")
	, szTCFISCHER[] = N_("fischer")
	, szTCBRONSTEIN[] = N_("bronstein")
	, szTCHOURGLASS[] = N_("hourglass")
	, szTCNONE[] = N_("none")
	, szTCTIME[] = N_("tctime")
	, szTCPOINT[] = N_("tcpoint") /* time per point */
	, szTCMOVE[] = N_("tcmove") /* time per move */
	, szTCPENALTY[] = N_("tcpenalty")
	, szTCMULT[] = N_("tcmult") /* time multiplier */
	, szTCOFF[] = "Time control off"
	;

int dmpscoreDecision( int fPlayer)
{
    return (ms.anScore[0] == ms.anScore[1] &&
    	ms.anScore[0] == ms.nMatchTo-1);
}

int dmpDecision( int fPlayer)
{
    return (ms.anScore[0] + ms.nCube >= ms.nMatchTo &&
    	ms.anScore[1] + ms.nCube >= ms.nMatchTo);
}

int crawfordDecision( int fPlayer)
{
    return (ms.fCrawford);
}

int postcrawfordDecision( int fPlayer)
{
    return (ms.fPostCrawford);
}

int botDecision( int fPlayer)
{
    return (ap[fPlayer].pt == PLAYER_GNU ||
	ap[fPlayer].pt == PLAYER_PUBEVAL );
}

int humanDecision( int fPlayer)
{
    return  !botDecision(fPlayer);
}

int ginDecision(int fPlayer)
{
    float arOutputs[NUM_ROLLOUT_OUTPUTS];
    int i;

    for (i=0;i<NUM_ROLLOUT_OUTPUTS;i++)
	arOutputs[i] = 0.5; 
    SanityCheck(ms.anBoard, arOutputs);
    if (fPlayer==ms.fMove) 
	return  (ms.anScore[fPlayer] + ms.nCube *
		((arOutputs[OUTPUT_WIN] == 1.0f) +
		(arOutputs[OUTPUT_WINGAMMON] == 1.0f) +
		(arOutputs[OUTPUT_WINBACKGAMMON] == 1.0f))) >= ms.nMatchTo ;
    else
	return  (ms.anScore[fPlayer] + ms.nCube *
		((arOutputs[OUTPUT_WIN] == 0.0f) +
		(arOutputs[OUTPUT_LOSEGAMMON] == 1.0f) +
		(arOutputs[OUTPUT_LOSEBACKGAMMON] == 1.0f))) >= ms.nMatchTo ;
}

tctransition transitions[] = {
{ "dmpscore", dmpscoreDecision },
{ "dmp", dmpDecision },
{ "crawford", crawfordDecision }, 
{ "postcrawford", postcrawfordDecision },
/* { "newgame", newgameDecision }, */
{ "bot", botDecision },
{ "human", humanDecision },
{ "gin", ginDecision },
{ 0, 0}
};

#ifndef timeradd
#define timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif
#ifndef timersub
#define timersub(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)
#endif

static tcnode *tcHead = 0;

static int str2time( char *sz ) {

    int t=0;
    char *pch=sz;

    if( !sz  )
	return 0;

    for( ; *sz ; sz++ )
    {
	if( isdigit( *sz) ) continue;
	if ( ':' == *sz )
	{
	    *sz=0;
	    t += atoi(pch);
	    t *= 60;
	    *sz=':';
	    pch = sz+1;
	} else {
	    return 0;
	}
    }
    t += atoi(pch);
    return t;
}

static char *time2str(int t, char *sz)
{
	static char staticbuf[16];
	int s=t%60;
	int m=(t/60) % 60;
	int h=t/3600;

	if (!sz)
		sz=staticbuf;
		/* If you want thread safe or persistent - supply your
		   own buffer ... */

	*sz=0;
	if (h)
	{
	    sprintf(sz, "%d:%02d:%02d", h, m, s);
	} else {
	    sprintf(sz , "%d:%02d", m, s);
	}
	return sz;
}

static void tcCopy(timecontrol *dst, timecontrol *src)
{
    tctransitionnode *srcNode, *dstNode, *nextNode;

    free(dst->szName);
    free(dst->szNext);
    free(dst->szNextB);
    for (dstNode=dst->pTransitions;dstNode;dstNode=nextNode)
    {
	nextNode=dstNode->next;
	free(dstNode->szNext);
	free(dstNode);
    }
    *dst=*src;
    if (src->szName) dst->szName = strdup(src->szName);
    if (src->szNext) dst->szNext = strdup(src->szNext);
    if (src->szNextB) dst->szNextB = strdup(src->szNextB);
    srcNode=src->pTransitions;
    if (srcNode)
	dstNode = dst->pTransitions=malloc(sizeof(tctransition));
    while (srcNode)
    {
	dstNode->ptrans = srcNode->ptrans;
	dstNode->szNext = strdup(srcNode->szNext);
	srcNode = srcNode->next;
	if (srcNode)
	    dstNode = dstNode->next = malloc(sizeof(tctransition));
	else
	    dstNode->next = 0;
    }
}


static timecontrol *findOrDeleteTimeControl( char *sz, int del )
{
    tcnode **ppNode, **ppRefNode;
    tctransitionnode *pNode, *nextNode;

    if (!sz || !*sz ) return 0;
    ppNode=&tcHead;
    ppRefNode=0;
    while (*ppNode)
    {
	if (0 == strcasecmp(sz, (*ppNode)->ptc->szName) )
	{
		ppRefNode=ppNode; 
		break; /* exact match */
	}
	else if ( 0 == strncasecmp(sz, (*ppNode)->ptc->szName, strlen(sz)) )
		ppRefNode=ppNode; /* partial match */
	ppNode = &((*ppNode)->next);
    }

    if (!ppRefNode) return 0;

    if (del)
    {
    tcnode *next;
#if USE_GTK
    if ( fX )
      GTKRemoveTimeControl((*ppRefNode)->ptc->szName);
#endif
    next=(*ppRefNode)->next;
    free((*ppRefNode)->ptc->szName);
    free((*ppRefNode)->ptc->szNext);
    free((*ppRefNode)->ptc->szNextB);
    for (pNode=(*ppRefNode)->ptc->pTransitions;pNode;pNode=nextNode)
    {
	nextNode=pNode->next;
	free(pNode->szNext);
	free(pNode);
    }
    free((*ppRefNode)->ptc);
    free(*ppRefNode);
    *ppRefNode = next;
	return 0;
    }
    else
	return (*ppRefNode)->ptc;
} 

static timecontrol *findTimeControl( char *sz )
{
    return findOrDeleteTimeControl( sz, 0 );
}

static void unnameTimeControl( char *sz )
{
    findOrDeleteTimeControl( sz, 1 );
}

static void setNameModified( )
{
	char *p;
    if (!tc.szName) return;
    if (0 == strcmp(tc.szName+strlen(tc.szName) - strlen(_("(modified)")),
	_("(modified)")) ) 
	return;
    p = malloc(strlen(tc.szName)+12);
    sprintf(p, "%s %s", tc.szName, _("(modified)"));
    free (tc.szName);
    tc.szName = p;
}
  	 
static void nameTimeControl( char *sz )
{
    tcnode *pNode;
	timecontrol *ptc;

    if (!sz || !*sz) return;

    ptc = findTimeControl( sz );
    if (!ptc || strcmp(ptc->szName, sz))
    {
	ptc = calloc(sizeof(timecontrol), 1);
    }
#if USE_GTK
    else if ( fX )
	GTKRemoveTimeControl(ptc->szName);
#endif
    if (ptc && (pNode = calloc(sizeof(tcnode), 1)))
    {
	free (tc.szName);
	tc.szName = strdup(sz);
        tcCopy(ptc , &tc);
	pNode->next = tcHead;
	pNode->ptc = ptc;
	tcHead = pNode;
#if USE_GTK
	if ('.' != ptc->szName[0] && fX )
	    GTKAddTimeControl(ptc->szName);
#endif
    }
}

static void showTimeControl( timecontrol *ptc, int level, int levels )
{
    if (TC_NONE == ptc->timing )
    {
	outputf("%s\n", _(szTCOFF));
	return;
    }

    outputf(_("Time control: %s\n"), ptc->szName);
    outputf("%*s--------\n", 2*level,"");
    switch (ptc->timing)
    {
    case TC_PLAIN:
	outputf(_("%*sPlain timing\n"), 2*level,"");
	break;
    case TC_FISCHER:
	outputf(_("%*sFischer timing\n"), 2*level,"");
	break;
    case TC_BRONSTEIN:
	outputf(_("%*sBronstein timing\n"), 2*level,"");
	break;
    case TC_HOURGLASS:
	outputf(_("%*sHourglass timing\n"), 2*level,"");
	break;
    default:
      /* ignore */
      break;
    }
    outputf("%*s%-*s (%s): ", 2*level,"",
		30-strlen(szTCPENALTY),
		"Penalty points",
		szTCPENALTY);
    if (TC_LOSS == ptc->penalty)
	outputf(_("lose match\n"));
    else
	outputf("%9d\n", ptc->nPenalty);

    /* if (ptc->nAddedTime) */
	outputf("%*s%-*s (%s): %9s\n", 2*level,"",
		30-strlen(szTCTIME),
		"Added time",
		szTCTIME,
		time2str( ptc->nAddedTime, 0 ) );
    if (ptc->nPointAllowance)
	outputf("%*s%-*s (%s): %9s\n", 2*level,"",
		30-strlen(szTCPOINT),
		"Time per point",
		szTCPOINT,
		time2str(ptc->nPointAllowance, 0));
    if (ptc->nMoveAllowance ||
	TC_FISCHER == ptc->timing ||
	TC_BRONSTEIN == ptc->timing)
	outputf("%*s%-*s (%s): %9s\n", 2*level,"",
		30-strlen(szTCMOVE),
		"Time per move",
		szTCMOVE,
		time2str(ptc->nMoveAllowance, 0));
    if (ptc->dMultiplier != 1.0) 
	outputf("%*s%-*s (%s): %9s\n", 2*level,"",
		30-strlen(szTCMULT),
		"Scale old time by:",
		szTCMULT,
		time2str(ptc->dMultiplier, 0)
		);


    if (ptc->szNext) {
    	timecontrol *stc = findTimeControl( ptc->szNext);	
    	if (stc)  { 
	    if (level==levels)
	    {
		outputf(_("%*sNext Time Control: %s\n"), 2*level,"", stc->szName);
	    } else {
		outputf("%*sNext ", 2*level,"");
		showTimeControl(stc, level+1, levels);
	    }
	}
    }
    if (ptc->szNextB) {
    	timecontrol *stc = findTimeControl( ptc->szNextB);	
    	if (stc) { 
	    if (level==levels)
	    {
		outputf(_("%*sOpponent's Next Time Control: %s\n"), 2*level,"", stc->szName);
	    } else {
		outputf("%*sOpponent's Next", 2*level,"");
		showTimeControl(stc, level+1, levels);
	    }
	}
    }
    {
	tctransitionnode *pNode;
	for (pNode=ptc->pTransitions;pNode;pNode=pNode->next)
	{
	    outputf(_("%*sTransition: %s - %s\n"), 2*level,"", pNode->ptrans->szName, pNode->szNext);
	}
    }
    outputf("%*s--------\n", 2*level,"");
    if (1 == level) outputx();
}



extern void CommandSetTCName( char *sz ) {
    char *pch = NextToken ( &sz );
    if (pch)
	nameTimeControl(pch);
}

extern void CommandSetTCUnname( char *sz ) {
    char *pch = NextToken ( &sz );
    if (pch)
        unnameTimeControl(pch);
}

extern void CommandSetTCType( char *sz ) {
    char *pch = NextToken ( &sz );
    if (0 == strncasecmp(pch, szTCPLAIN, strlen(pch)))
	tc.timing=TC_PLAIN;
    else if (0 == strncasecmp(pch, szTCFISCHER, strlen(pch)))
	tc.timing=TC_FISCHER;
    else if (0 == strncasecmp(pch, szTCBRONSTEIN, strlen(pch)))
	tc.timing=TC_BRONSTEIN;
    else if (0 == strncasecmp(pch, szTCHOURGLASS, strlen(pch)))
	tc.timing=TC_HOURGLASS;
    else if (0 == strncasecmp(pch, szTCNONE, strlen(pch)))
	tc.timing=TC_NONE;
    else
	return;
    setNameModified();
} 

extern void CommandSetTCTime( char *sz ) {
    char *pch = NextToken ( &sz );
    tc.nAddedTime=str2time(pch);
    setNameModified();
}

extern void CommandSetTCPoint( char *sz ) {
    char *pch = NextToken ( &sz );
    tc.nPointAllowance=str2time(pch);
    setNameModified();
}

extern void CommandSetTCMove( char *sz ) {
    char *pch = NextToken ( &sz );
    tc.nMoveAllowance=str2time(pch);
    setNameModified();
}

	    
extern void CommandSetTCPenalty( char *sz ) {
    char *pch = NextToken ( &sz );
    if (0 == strncasecmp(pch, "lose", strlen(pch)))
    {
	tc.nPenalty=0;
	tc.penalty=TC_LOSS;
    }
    else
    {
        tc.nPenalty=atoi(pch);
	tc.penalty=TC_POINT;
    }
    setNameModified();
}

extern void CommandSetTCMultiplier( char *sz ) {
    tc.dMultiplier = ParseReal ( &sz );
    if (ERR_VAL == tc.dMultiplier)
    {
	tc.dMultiplier = 1.0;
    }
    setNameModified();
}

extern void CommandSetTCNext( char *sz ) {
    char *pch = NextToken ( &sz );
    free (tc.szNext);
    free (tc.szNextB);
    if (pch)
    {
	tc.szNext = strdup(pch);
    }
    else
    {
	tc.szNext = 0;
    }

    pch = NextToken ( &sz );
    if (pch)
    {
	tc.szNextB = strdup(pch);
    }
    else
    {
	tc.szNextB = 0;
    }
    setNameModified();
}

extern void CommandSetTimeControl( char *sz ) {
  if ((strcasecmp(sz, "off")))
    {
	timecontrol *ptc;
	if ((ptc = findTimeControl(sz)))
	{
	    outputf(_("Time control set to %s\n"), ptc->szName);
	    tcCopy( &tc, ptc);
#if USE_GTK
            if ( fX )
              GTKCheckTimeControl(ptc->szName);
#endif
	}
	else
	{
	    if (strlen(sz))
		outputerrf("No such time control: %s\n", sz);
	    else
		CommandShowTCList( sz );
	}
	
   }
   else
   {
	outputl(_(szTCOFF));
	tc.timing = TC_NONE;
#if USE_GTK
        if ( fX )
          GTKCheckTimeControl("Off");
#endif
   }
    outputx();
}

extern void CommandSetTCTransition( char *sz ) {
    tctransition *ptr=transitions;
    tctransitionnode *pNode, **ppRef=&tc.pTransitions;

    char *pch = NextToken ( &sz );
    if (!pch) return;

    while (strcmp(pch,ptr->szName))
    {
	if (!ptr->szName) return;
	ptr++;
    }

    pch = NextToken ( &sz );
    if (!pch) return;

    for (pNode = tc.pTransitions;pNode;pNode=pNode->next)
    {
	if (pNode->ptrans == ptr) break;
	ppRef = &pNode->next;
    }

    if (pNode)
    {
	if (strcmp(pch, pNode->szNext))
	{
	    free (pNode->szNext);
	    if (strcasecmp(pch, "off"))
	    {
		pNode->szNext=strdup(pch);
	    }
	    else
	    {
		*ppRef = pNode->next;
		free(pNode);
	    }
	}
    }
    else
    {
	*ppRef = malloc(sizeof(tctransitionnode));
	(*ppRef)->ptrans = ptr;
	(*ppRef)->szNext=strdup(pch);
	(*ppRef)->next=0;
    }
    setNameModified();
}

extern void CommandShowTimeControl( char *sz ) {
	timecontrol *ptc;
     char *name = NextToken ( &sz );
     int level = ParseNumber ( &sz );
     if (INT_MIN == level) level=1;
     ptc = findTimeControl(name);
     if (ptc)
	showTimeControl(ptc, 1, level);
     else
	showTimeControl(&tc, 1, level);
}

extern void CommandShowTCList( char *sz ) {
	tcnode *node;
   char *arg= NextToken ( &sz );
   int all = arg && ! strncasecmp(arg, "all", strlen(arg));
   outputf(_("Defined time controls:\n"));
   node=tcHead;
   while (node)
   {
	if (all || '.' != *(node->ptc->szName))
	outputf("  %s\n", node->ptc->szName);
	node=node->next;
   }
   outputx();
}
  

/* applyPenalty
 * @param matchstate pointer
 */

static int applyPenalty(matchstate *pms)
{
	timecontrol *newtc;
	playerclock *pgcPlayer=&pms->gc.pc[pms->fTurn],
	    *pgcOpp=&pms->gc.pc[!pms->fTurn];
	int penalty = 0;

	switch (pgcPlayer->tc.penalty ) {
	case TC_POINT:
		penalty = pgcPlayer->tc.nPenalty;
		break;
	case TC_LOSS:
		timerclear(&pgcPlayer->tvTimeleft); /*  not to reiterate applyPenalty */
		return pms->nMatchTo - pms->anScore[!pms->fTurn];
	default:
	  assert (1 == 0);
	  return 0;
	}

	newtc = findTimeControl(pgcPlayer->tc.szNextB);
	if (newtc) 
		pgcOpp->tc= *newtc;
	pgcOpp->tvTimeleft.tv_sec = (int)( pgcOpp->tvTimeleft.tv_sec * pgcOpp->tc.dMultiplier);
	pgcOpp->tvTimeleft.tv_sec += pgcOpp->tc.nAddedTime;

	newtc = findTimeControl(pgcPlayer->tc.szNext);
	if (newtc) 
		pgcPlayer->tc= *newtc;
	pgcPlayer->tvTimeleft.tv_sec = (int)( pgcPlayer->tvTimeleft.tv_sec * pgcPlayer->tc.dMultiplier);
	pgcPlayer->tvTimeleft.tv_sec += pgcPlayer->tc.nAddedTime;

	return penalty;
}


extern void InitGameClock(gameclock *pgc, timecontrol *ptc, int nPoints)
{
#ifdef TCDEBUG
printf("InitgameClock (match to: %d)\n", nPoints);
#endif
    int i;
    for (i=0;i<2;i++)
    {
        timerclear(&pgc->pc[i].tvStamp);
	pgc->pc[i].tvTimeleft.tv_sec = nPoints * ptc->nPointAllowance;
	pgc->pc[i].tvTimeleft.tv_sec += ptc->nAddedTime; 
	pgc->pc[i].tvTimeleft.tv_usec = 0; 
	pgc->pc[i].tc = *ptc;
    }
    timerclear(&pgc->pausedtime);
}


extern void HitGameClock(matchstate *pms)
{
#ifdef TCDEBUG
printf("HitGameClock: state:%d, turn: %d, ts0: (%d.%d), ts1: (%d.%d)\n",
	pms->gs, pms->fTurn,
	pms->gc.pc[0].tvStamp.tv_sec %1000, pms->gc.pc[0].tvStamp.tv_usec / 1000, 
	pms->gc.pc[1].tvStamp.tv_sec %1000, pms->gc.pc[1].tvStamp.tv_usec / 1000); 
#endif

    pms->gc.fPlayer = pms->fTurn;

    if ( pms->gs != GAME_PLAYING ||  pms->fTurn < 0 )
    {
	if timercmp(&pms->gc.pc[0].tvStamp, &pms->gc.pc[1].tvStamp, >)
	    pms->gc.pc[1].tvStamp = pms->gc.pc[0].tvStamp;
	else
	    pms->gc.pc[0].tvStamp = pms->gc.pc[1].tvStamp;
	return;
    }

    if (timercmp(&pms->gc.pc[!pms->fTurn].tvStamp, &pms->gc.pc[pms->fTurn].tvStamp, > ))
	return; /* rehit for same player */

    pms->gc.pc[!pms->fTurn].tvStamp = pms->gc.pc[pms->fTurn].tvStamp ;

    switch ( pms->gc.pc[pms->fTurn].tc.timing ) {
    case TC_FISCHER:
	pms->gc.pc[pms->fTurn].tvTimeleft.tv_sec += pms->gc.pc[pms->fTurn].tc.nMoveAllowance;
	break;
    default:
	break;
    }
}


extern void PauseGameClock(matchstate *pms)
{
    fprintf(stderr, "Not yet implemented\n");
    assert (0);
}

#if WIN32
double get_time();

int gettimeofday (struct timeval *tv, void * arg)
{
	double ms = get_time();

	tv->tv_sec = (long) ms / 1000;
	/* micro seconds */
	tv->tv_usec = (long) ((ms - (tv->tv_sec * 1000)) / 1000);

	return 0;
}
#endif

extern int CheckGameClock(matchstate *pms, struct timeval *tvp)
{
    int pen=0;
    struct timeval ts;
    playerclock *pgcPlayer, *pgcOpp;

    if (0 == tvp)
    {
	tvp = &ts;
	gettimeofday(tvp,0);
    }

    if ( pms->gs != GAME_PLAYING  || pms->gc.fPlayer < 0 )
    {
	 pms->gc.pc[0].tvStamp = pms->gc.pc[1].tvStamp = *tvp;
	 return 0;
    }

    pgcPlayer=&pms->gc.pc[pms->gc.fPlayer];
    pgcOpp=&pms->gc.pc[!pms->gc.fPlayer];

/* Check transitions */
   if (pms == &ms)
   {
	char *szNextPlayer=0, *szNextOpp=0;
	tctransitionnode *pNode;
	timecontrol *newtc;

	for(pNode=pgcPlayer->tc.pTransitions;pNode;pNode=pNode->next)
	{
	    if ( (*pNode->ptrans->pfDecision)(pms->gc.fPlayer) )
	    {
		szNextPlayer = pNode->szNext;
		break;
	    }
	}
	for(pNode=pgcOpp->tc.pTransitions;pNode;pNode=pNode->next)
	{
	    if ( (*pNode->ptrans->pfDecision)(!pms->gc.fPlayer) )
	    {
		szNextOpp= pNode->szNext;
		break;
	    }
	}

	if (szNextPlayer && (newtc = findTimeControl(szNextPlayer))) 
	{
	    pgcPlayer->tc= *newtc;
	    pgcPlayer->tvTimeleft.tv_sec = (int)( pgcPlayer->tvTimeleft.tv_sec * pgcPlayer->tc.dMultiplier);
	    pgcPlayer->tvTimeleft.tv_sec += pgcPlayer->tc.nAddedTime;
	}

	if (szNextOpp && (newtc = findTimeControl(szNextOpp))) 
	{
	    pgcOpp->tc= *newtc;
	    pgcOpp->tvTimeleft.tv_sec = (int)( pgcOpp->tvTimeleft.tv_sec * pgcOpp->tc.dMultiplier);
	    pgcOpp->tvTimeleft.tv_sec += pgcOpp->tc.nAddedTime;
	}
   }

/* Subtract time and check for fallen flag */
    {
	struct timeval used;


	if  (TC_NONE == pgcPlayer->tc.timing)
	    return 0;
	if (TC_UNKNOWN == pgcPlayer->tc.timing) 
	{
	    pms->gc.pc[0].tvTimeleft = pms->tvTimeleft[0];
	    pms->gc.pc[1].tvTimeleft = pms->tvTimeleft[1];
	    return 0;
	}

	/* Player's timestamp is reference for last hit.
	 * Opp's timestamp is last update */

	assert (timerisset(&pgcPlayer->tvStamp) &&  
	   ! timercmp(&pgcOpp->tvStamp, &pgcPlayer->tvStamp, <) );

    timersub(tvp, & pgcOpp->tvStamp, &used);
    switch ( pgcPlayer->tc.timing ) {
    case TC_BRONSTEIN:
	{
	struct timeval ref= pgcPlayer->tvStamp;
	ref.tv_sec += pgcPlayer->tc.nMoveAllowance;
	if ( timercmp(tvp, &ref, <) ) {
		timerclear(&used);
	} else if ( timercmp(&pgcOpp->tvStamp, &ref, <) ) {
	    timersub(tvp, &ref, &used);
	}
	}
	break;
    case TC_HOURGLASS:
	timeradd(&pgcOpp->tvTimeleft, &used, &pgcOpp->tvTimeleft);
	break;
    case TC_FISCHER:
    case TC_PLAIN:
    default:
	break;
    }
    timersub(&pgcPlayer->tvTimeleft, &used, &pgcPlayer->tvTimeleft);
    while ( pgcPlayer->tvTimeleft.tv_sec < 0 )
	pen += applyPenalty(pms);

    pms->tvTimeleft[0] = pms->gc.pc[0].tvTimeleft;
    pms->tvTimeleft[1] = pms->gc.pc[1].tvTimeleft;
    pgcOpp->tvStamp = *tvp;

    if (pen) {
    moverecord *pmr;
	pmr = malloc( sizeof( movetime ) );
	pmr->mt = MOVE_TIME;
	pmr->t.sz = 0;
	pmr->t.fPlayer = ms.fTurn;
	pmr->t.tl[0] = ms.gc.pc[0].tvTimeleft;
	pmr->t.tl[1] = ms.gc.pc[1].tvTimeleft;
	pmr->t.nPoints = pen;
	AddMoveRecord( pmr );
   }
   return pen;
}
}

extern char *FormatClock(struct timeval *ptl, char *buf)
{
static char staticBuf[20];
    char *szClock = buf ? buf :  staticBuf;
    long sec=ptl->tv_sec;
    int h,m,s;
    int neg;
    if ((neg=(sec<0))) sec = -sec;
    s = sec%60;
    sec/=60;
    m = sec%60;
    h = sec/60;
    sprintf(szClock, "%s%02d:%02d:%02d", neg?"-":" ", h, m, s);
    return szClock;
}


#if USE_GUI
#if USE_GTK
extern gboolean UpdateClockNotify(gpointer *p)
#else
extern int UpdateClockNotify(event *pev, void *p)
#endif
#else
extern int UpdateClockNotify(void *p)
#endif
{
 /* not last move - don't update */
    if (!plLastMove || plLastMove->plNext != plGame)
	return 1;

#ifdef TCDEBUG
printf("UpdateClockNotify - game state: %d\n", ms.gs);
#endif

    if ( GAME_PLAYING != ms.gs ) return 1;

    CheckGameClock(&ms, 0);

#if USE_GTK
    if (fX)
	GTKUpdateClock();
#endif
	return 1;
}

extern void SaveTimeControlSettings( FILE *pf )
{
	tcnode *pNode;
   if ( NULL == pf ) 
	return;

    pNode=tcHead;
    while (pNode)
    {
        tctransitionnode *tNode;

	if (TC_LOSS == pNode->ptc->penalty)
	    fprintf(pf, "set tcpenalty lose\n");
	else
	    fprintf(pf, "set tcpenalty %d\n", pNode->ptc->nPenalty);
	switch ( pNode->ptc->timing ) {
	case TC_PLAIN:
	    fprintf(pf, "set tctype plain \n");
	    break;
	case TC_BRONSTEIN:
	    fprintf(pf, "set tctype bronstein\n");
	    break;
	case TC_FISCHER:
	    fprintf(pf, "set tctype fischer\n");
	    break;
	case TC_HOURGLASS:
	    fprintf(pf, "set tctype hourglass\n");
	    break;
	case TC_NONE:
	default:
	    fprintf(pf, "set tctype none\n");
	    break;
	}
	
	/* save transitions */
        for (tNode=pNode->ptc->pTransitions; tNode; tNode=tNode->next)
        {
	    fprintf(pf, "set tctransition %s %s\n", tNode->ptrans->szName, tNode->szNext);
	}
	
	fprintf(pf, "set tctime %d\n"
		"set tcpointtime %d\n"
		"set tcmultiplier %f\n"
		"set tcmovetime %d\n"
		"set tcnext %s %s\n"
		"set tcname %s\n",
		pNode->ptc->nAddedTime,
		pNode->ptc->nPointAllowance,
		pNode->ptc->dMultiplier,
		pNode->ptc->nMoveAllowance,
		(pNode->ptc->szNext)? pNode->ptc->szNext : "" ,
		(pNode->ptc->szNext)? pNode->ptc->szNextB : "",
		pNode->ptc->szName);
	pNode = pNode->next;
    }

    if (TC_NONE == tc.timing)
	fprintf(pf, "set tc off\n");
    else
	fprintf(pf, "set tc %s\n", tc.szName);
	
}

extern void SetDefaultTC ()
{
	;
}
#endif

