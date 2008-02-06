/*
 * relational.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2004.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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

#if USE_GTK
#include "gtkrelational.h"
#include "gtkgame.h"
#include "gtkwindows.h"
#endif
#include <stdlib.h>
#include <string.h>

#include "relational.h"
#include "backgammon.h"
#include "positionid.h"
#include "rollout.h"
#include "analysis.h"
#include "dbprovider.h"
#include "util.h"
#include <glib/gi18n.h>

int RunQueryValue(DBProvider *pdb, const char *query)
{
	RowSet *rs;
	rs = pdb->Select(query);
	if (rs && rs->rows > 1)
	{
		int id = strtol (rs->data[1][0], NULL, 0);
		FreeRowset(rs);
		return id;
	}
	else
		return -1;
}

extern int RelationalMatchExists(DBProvider *pdb)
{
	char *buf = g_strdup_printf("match_id FROM session WHERE checksum = '%s'", GetMatchCheckSum());
	int ret = RunQueryValue(pdb, buf);
	g_free(buf);
	return ret;
}

static int GameOver(void)
{
  	int anFinalScore[2];
	int nMatch;
	const listOLD* firstGame = lMatch.plNext->p;
	if (firstGame)
	{
		const moverecord* pmr = firstGame->plNext->p;
		if (pmr)
		{
			g_assert(pmr->mt == MOVE_GAMEINFO);
			nMatch = pmr->g.nMatch;
			if (ms.nMatchTo)
			{	/* Match - check someone has won */
				return (getFinalScore(anFinalScore) && ((anFinalScore[0] >= nMatch)
					|| (anFinalScore[1] >= nMatch)));
			}
			else
			{	/* Session - check game over */
				return (ms.gs == GAME_OVER);
			}
		}
	}
	return FALSE;
}

static int GetNextId(DBProvider *pdb, const char *table)
{
	int next_id;
	/* fetch next_id from control table */
	char *buf = g_strdup_printf("next_id FROM control WHERE tablename = '%s'", table);
	next_id = RunQueryValue(pdb, buf);
	g_free(buf);

	if (next_id != -1)
	{	/* update control data with new next id */
		next_id++;
		buf = g_strdup_printf("UPDATE control SET next_id = %d WHERE tablename = '%s'", next_id, table);
		if (!pdb->UpdateCommand(buf))
			next_id = -1;
		g_free(buf);
	}
	else
	{	/* insert new id */
		next_id = 1;
		buf = g_strdup_printf("INSERT INTO control (tablename,next_id) VALUES ('%s',%d)", table, next_id);
		if (!pdb->UpdateCommand(buf))
			next_id = -1;
		g_free(buf);
	}
	return next_id;
}

int GetPlayerId(DBProvider *pdb, const char *player_name)
{
	char *buf = g_strdup_printf ("player_id from player where name = '%s'", player_name);
	int id = RunQueryValue(pdb, buf);
	g_free (buf);
	return id;
}

static int AddPlayer(DBProvider *pdb, const char *name)
{
	int id = GetPlayerId(pdb, name);
	if (id == -1)
	{	/* Add new player to database */
		id = GetNextId(pdb, "player");
		if (id != -1)
		{
			char *buf = g_strdup_printf("INSERT INTO player(player_id,name,notes) VALUES (%d, '%s', '')", id, name);
			if (!pdb->UpdateCommand(buf))
				id = -1;
			g_free(buf);
		}
	}
	return id;
}

int MatchResult(int nMatchTo)
{	/* Work out the result (-1,0,1) - (p0 win, unfinished, p1 win) */
	int result = 0;
  	int anFinalScore[2];
	if (nMatchTo && getFinalScore(anFinalScore))
	{
		if (anFinalScore[0] > nMatchTo)
			result = -1;
		else if (anFinalScore[1] > nMatchTo)
			result = 1;
	}
	return result;
}

float Ratio(float a, int b)
{
	if (b != 0)
		return a / (float)b;
	else
		return 0;
}

#define NS(x) (x == NULL) ? "NULL" : x

int AddStats(DBProvider *pdb, int gm_id, int player_id, int player, const char *table, int nMatchTo)
{
	int ret;
	char *buf, *s_id, *s_cheq, *s_luck, *s_cube, *s_overall,
		*s_fibsdiff, *s_fibstotal, *s_fibscheq, *s_fibscube,
		*s_money, *s_time;
	int unforced, totalmoves;
	float errorskill, errorcost;
	float aaaar[ 3 ][ 2 ][ 2 ][ 2 ];

	int gms_id = GetNextId(pdb, table);
	if (gms_id == -1)
		return FALSE;

	/* identification */
	s_id = g_strdup_printf("%d,%d,%d", gms_id, gm_id, player_id);

	/* chequer play statistics */
	totalmoves = scMatch.anTotalMoves[player];
	unforced = scMatch.anUnforcedMoves[player];
	s_cheq = g_strdup_printf("%d,%d,%d,%d,%d,%d,%d,%f,%f,%f,%f,%d",
		totalmoves, unforced,
		scMatch.anMoves[player][SKILL_NONE], 0, scMatch.anMoves[player][SKILL_DOUBTFUL],
		scMatch.anMoves[player][SKILL_BAD], scMatch.anMoves[player][SKILL_VERYBAD],
		scMatch.arErrorCheckerplay[player][0], scMatch.arErrorCheckerplay[player][1],
		Ratio(scMatch.arErrorCheckerplay[player][0], unforced),
		Ratio(scMatch.arErrorCheckerplay[player][1], unforced),
		GetRating(Ratio(scMatch.arErrorCheckerplay[player][0], unforced)));

	/* luck statistics */
	s_luck = g_strdup_printf("%d,%d,%d,%d,%d,%f,%f,%f,%f,%d",
		scMatch.anLuck[player][LUCK_VERYGOOD],
		scMatch.anLuck[player][LUCK_GOOD],
		scMatch.anLuck[player][LUCK_NONE],
		scMatch.anLuck[player][LUCK_BAD],
		scMatch.anLuck[player][LUCK_VERYBAD],
		scMatch.arLuck[player][0],
		scMatch.arLuck[player][1],
		Ratio(scMatch.arLuck[player][0], totalmoves),
		Ratio(scMatch.arLuck[player][1], totalmoves),
		getLuckRating(Ratio(scMatch.arLuck[player][0], totalmoves)));

	/* cube statistics */
	getMWCFromError(&scMatch, aaaar );
	errorskill = aaaar[ CUBEDECISION ][ PERMOVE ][ player ][ NORMALISED ];
	errorcost = aaaar[ CUBEDECISION ][ PERMOVE ][ player ][ UNNORMALISED ];

	s_cube = g_strdup_printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%d",
		scMatch.anTotalCube[player], scMatch.anCloseCube[player],
		scMatch.anDouble[player], scMatch.anTake[player], scMatch.anPass[player],
		scMatch.anCubeMissedDoubleDP[player], scMatch.anCubeMissedDoubleTG[player], scMatch.anCubeWrongDoubleDP[player],
		scMatch.anCubeWrongDoubleTG[player], scMatch.anCubeWrongTake[player], scMatch.anCubeWrongPass[player],
		scMatch.arErrorMissedDoubleDP[player][0],
		scMatch.arErrorMissedDoubleTG[player][ 0 ],
		scMatch.arErrorWrongDoubleDP[player][ 0 ],
		scMatch.arErrorWrongDoubleTG[player][ 0 ],
		scMatch.arErrorWrongTake[player][ 0 ],
		scMatch.arErrorWrongPass[player][ 0 ],
		scMatch.arErrorMissedDoubleDP[player][1],
		scMatch.arErrorMissedDoubleTG[player][ 1 ],
		scMatch.arErrorWrongDoubleDP[player][ 1 ],
		scMatch.arErrorWrongDoubleTG[player][ 1 ],
		scMatch.arErrorWrongTake[player][ 1 ],
		scMatch.arErrorWrongPass[player][ 1 ],
		/* gnubg is giving per-move, not totals here */
		errorskill * scMatch.anCloseCube[player],
		errorcost * scMatch.anCloseCube[player],
		errorskill,
		errorcost,
		GetRating(errorskill));

	/* overall */
	s_overall = g_strdup_printf("%f,%f,%f,%f,%d,%f,%f,%f",
		errorskill * scMatch.anCloseCube[player] + scMatch.arErrorCheckerplay[player][0],
		errorcost * scMatch.anCloseCube[player] + scMatch.arErrorCheckerplay[player][1],
		Ratio(errorskill * scMatch.anCloseCube[player] + scMatch.arErrorCheckerplay[player][0],
				scMatch.anCloseCube[player] + unforced),
		Ratio(errorcost * scMatch.anCloseCube[player] + scMatch.arErrorCheckerplay[player][1],
				scMatch.anCloseCube[player] + unforced),
		GetRating(Ratio(errorskill * scMatch.anCloseCube[player] + scMatch.arErrorCheckerplay[player][0],
				scMatch.anCloseCube[player] + unforced)),
		scMatch.arActualResult[player], scMatch.arLuckAdj[player],
		Ratio(errorskill * scMatch.anCloseCube[player] + scMatch.arErrorCheckerplay[player][0],
				totalmoves + scMatch.anTotalMoves[!player]));

	/* for matches only */
	s_fibsdiff = s_fibstotal = s_fibscheq = s_fibscube = NULL;
	if (nMatchTo)
	{
		float r = 0.5f + scMatch.arActualResult[ player ] - scMatch.arLuck[ player ][ 1 ] + scMatch.arLuck[ !player ][ 1 ];
		if (r > 0.0f && r < 1.0f)
			s_fibsdiff = g_strdup_printf("%f", relativeFibsRating(r, nMatchTo));
	
		if (scMatch.fCube || scMatch.fMoves)
		{
			s_fibstotal = g_strdup_printf("%f", absoluteFibsRating( aaaar[ CHEQUERPLAY ][ PERMOVE ][ player ][ NORMALISED ],
					aaaar[ CUBEDECISION ][ PERMOVE ][ player ][ NORMALISED ], nMatchTo, rRatingOffset ));

			if (scMatch.anUnforcedMoves[player])
				s_fibscheq = g_strdup_printf("%f", absoluteFibsRatingChequer(aaaar[ CHEQUERPLAY ][ PERMOVE ][ player ][ NORMALISED ], nMatchTo));

			if ( scMatch.anCloseCube[player])
				s_fibscube = g_strdup_printf("%f", absoluteFibsRatingCube( aaaar[ CUBEDECISION ][ PERMOVE ][ player ][ NORMALISED ], nMatchTo));
         }
	}

	/* for money sessions only */
	if (scMatch.fDice && !nMatchTo && scMatch.nGames > 1)
	{
		s_money = g_strdup_printf("%f,%f,%f,%f",
			scMatch.arActualResult[ player ] / scMatch.nGames,
			1.95996f * sqrt( scMatch.arVarianceActual[ player ] / scMatch.nGames ),
			scMatch.arLuckAdj[ player ] / scMatch.nGames,
			1.95996f * sqrt( scMatch.arVarianceLuckAdj[ player ] / scMatch.nGames ) );
	}
	else
		s_money = g_strdup("NULL,NULL,NULL,NULL");

	/* time penalties */
	s_time = g_strdup("0, 0, 0");	/* Are these currently in the system? */

	buf = g_strdup_printf("INSERT INTO %s VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", table,
		s_id, s_cheq, s_luck, s_cube, s_overall, NS(s_fibsdiff), NS(s_fibstotal), NS(s_fibscheq), NS(s_fibscube), s_money, s_time);

	ret = pdb->UpdateCommand(buf);

	g_free(buf);
	g_free(s_id);
	g_free(s_cheq);
	g_free(s_luck);
	g_free(s_cube);
	g_free(s_overall);
	g_free(s_fibsdiff);
	g_free(s_fibstotal);
	g_free(s_fibscheq);
	g_free(s_fibscube);
	g_free(s_money);
	g_free(s_time);

	return ret;
}

int CreateDatabase(DBProvider *pdb)
{
	char buffer[10240];
	char *pBuf = buffer;
	char *szFile = BuildFilename("gnubg.sql");
	FILE *fp = fopen(szFile, "r");
	g_free(szFile);
	if (!fp)
		return FALSE;

	buffer[0] = '\0';
	while (!feof(fp))
	{
		char line[1024];
		char *pLine;
		line[0] = '\0';
		fgets(line, sizeof(line), fp);
		
		pLine = line + strlen(line) - 1;
		while (pLine >= line && isspace(*pLine))
		{
			*pLine = '\0';
			pLine--;
		} 

		pLine = line;
		while (isspace(*pLine))
			pLine++;

		if (pLine[0] != '-' || pLine[1] != '-')
		{
			size_t len = strlen(pLine);
			if (len > 0)
			{
				strcat(buffer, pLine);
				pBuf += len;
				if (pLine[len - 1] == ';')
				{
					if (!pdb->UpdateCommand(buffer))
						return FALSE;
					pBuf = buffer;
					buffer[0] = '\0';
				}
			}
		}
	}
	fclose(fp);

	pBuf = g_strdup_printf("INSERT INTO control VALUES ('version', %d)", DB_VERSION);
	pdb->UpdateCommand(pBuf);
	g_free(pBuf);

	pdb->Commit();

	return TRUE;
}

DBProvider *ConnectToDB(DBProviderType dbType)
{
	DBProvider *pdb = GetDBProvider(dbType);
	if (pdb)
	{
		int con = pdb->Connect(pdb->database, pdb->username, pdb->password);
		if (con < 0 )
			return NULL;

		if (con > 0 || CreateDatabase(pdb))
			return pdb;
	}
	return NULL;
}

extern void CommandRelationalAddMatch( char *sz )
{
	DBProvider *pdb;
	char *buf, *date;
	char warnings[1024];
	int match_id, existing_id, player_id0, player_id1;
	*warnings = '\0';

	if (ListEmpty(&lMatch))
	{
		outputl( _("No match is being played.") );
		return;
	}

	/* Warn if match is not finished or fully analyzed */
	if (!GameOver())
		strcat(warnings, _("The match is not finished\n"));
	if (!MatchAnalysed())
		strcat(warnings, _("All of the match is not analyzed\n"));

	if (*warnings)
	{
		strcat(warnings, _("\nAdd match anyway?"));
		if (!GetInputYN(warnings))
			return;
	}

	if ((pdb = ConnectToDB(dbProviderType)) == NULL)
		return;

	existing_id = RelationalMatchExists(pdb);
	if (existing_id != -1)
	{
		if (!GetInputYN(_("Match exists, overwrite?")))
			return;

		buf = g_strdup_printf("DELETE FROM matchstat WHERE match_id = %d", existing_id);
		pdb->UpdateCommand(buf);
		g_free(buf);
		buf = g_strdup_printf("DELETE FROM session WHERE match_id = %d", existing_id);
		pdb->UpdateCommand(buf);
		g_free(buf);
	}

	match_id = GetNextId(pdb, "session");
	player_id0 = AddPlayer(pdb, ap[0].szName);
	player_id1 = AddPlayer(pdb, ap[1].szName);
	if (match_id == -1 || player_id0 == -1 || player_id1 == -1)
	{
		outputl( _("Error adding match.") );
		return;
	}

	if( mi.nYear )
		date = g_strdup_printf("%04d-%02d-%02d", mi.nYear, mi.nMonth, mi.nDay);
	else
		date = NULL;

	buf = g_strdup_printf("INSERT INTO session(match_id, checksum, player_id0, player_id1, "
              "result, length, added, rating0, rating1, event, round, place, annotator, comment, date) "
              "VALUES (%d, '%s', %d, %d, %d, %d, CURRENT_TIMESTAMP, '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s')",
				match_id, GetMatchCheckSum(), player_id0, player_id1,
				MatchResult(ms.nMatchTo), ms.nMatchTo, NS(mi.pchRating[0]), NS(mi.pchRating[1]),
				NS(mi.pchEvent), NS(mi.pchRound), NS(mi.pchPlace), NS(mi.pchAnnotator), NS(mi.pchComment), NS(date));
	if (pdb->UpdateCommand(buf))
	{
		if (AddStats(pdb, match_id, player_id0, 0, "matchstat", ms.nMatchTo) &&
			AddStats(pdb, match_id, player_id1, 1, "matchstat", ms.nMatchTo))
			pdb->Commit();
	}
	g_free(buf);
	g_free(date);
	pdb->Disconnect();
}

extern RowSet* MallocRowset(size_t rows, size_t cols)
{
	size_t i;
	RowSet* pRow = malloc(sizeof(RowSet));

	pRow->widths = malloc(cols * sizeof(int));
	memset(pRow->widths, 0, cols * sizeof(int));

	pRow->data = malloc(rows * sizeof(char*));
	for (i = 0; i < rows; i++)
	{
		pRow->data[i] = malloc(cols * sizeof(char*));
		memset(pRow->data[i], 0, cols * sizeof(char*));
	}

	pRow->cols = cols;
	pRow->rows = rows;

	return pRow;
}

const char *TestDB(DBProviderType dbType)
{
	char *ret = NULL;
	DBProvider *pdb;
	RowSet *rs;
	if ((pdb = ConnectToDB(dbType)) == NULL)
        return _("Database connection test failed, installation problem!\n");

	/* Check main table is present */
	rs = pdb->Select("COUNT(*) from session");
	if (rs)
	{	// Success
		FreeRowset(rs);
	}
	else
	    ret = ( _("Database table check failed!\n"
                "The session table is missing.") );

	pdb->Disconnect();
	return ret;
}

extern void CommandRelationalTest( char *sz )
{
	const char *err = TestDB(dbProviderType);
	if (err == NULL)
		outputl( _("Database test is successful!") );
	else
		outputl(err);
}

extern void CommandRelationalShowDetails (char *sz)
{
	DBProvider *pdb;
	gchar *buf, output[4096], query[2][200], *player_name;
	RowSet *rs;
	gint i, id;
	statcontext sc;

	if (!sz || !*sz || !(player_name = NextToken (&sz)))
    {
      outputl (_("You must specify a player name to list the details for "
		 "(see `help relational show details')."));
      return;
    }

	if ((pdb = ConnectToDB(dbProviderType)) == NULL)
		return;

	id = GetPlayerId(pdb, player_name);
	if (id == -1)
	{
		outputl (_("Player not found or player stats empty"));
		return;
	}
	sprintf (query[0], "where matchstat.player_id = %d", id);
	sprintf (query[1], "NATURAL JOIN session WHERE "
		"(session.player_id0 = %d OR session.player_id1 = %d) "
		"AND matchstat.player_id != %d", id, id, id);
	IniStatcontext (&sc);
	for (i = 0; i < 2; ++i)
	{
		buf = g_strdup_printf ("SUM(total_moves),"
				"SUM(unforced_moves),"
				"SUM(total_cube_decisions),"
				"SUM(close_cube_decisions),"
				"SUM(doubles),"
				"SUM(takes),"
				"SUM(passes),"
				"SUM(very_bad_moves),"
				"SUM(bad_moves),"
				"SUM(doubtful_moves),"
				"SUM(unmarked_moves),"
				"SUM(very_unlucky_rolls),"
				"SUM(unlucky_rolls),"
				"SUM(unmarked_rolls),"
				"SUM(lucky_rolls),"
				"SUM(very_lucky_rolls),"
				"SUM(missed_doubles_below_cp),"
				"SUM(missed_doubles_above_cp),"
				"SUM(wrong_doubles_below_dp),"
				"SUM(wrong_doubles_above_tg),"
				"SUM(wrong_takes),"
				"SUM(wrong_passes),"
				"SUM(chequer_error_total_normalised),"
				"SUM(error_missed_doubles_below_cp_normalised),"
				"SUM(error_missed_doubles_above_cp_normalised),"
				"SUM(error_wrong_doubles_below_dp_normalised),"
				"SUM(error_wrong_doubles_above_tg_normalised),"
				"SUM(error_wrong_takes_normalised),"
				"SUM(error_wrong_passes_normalised),"
				"SUM(luck_total_normalised)"
				"from matchstat " "%s", query[i]);
		rs = pdb->Select(buf);
		g_free (buf);
		if ((!rs) || !strtol (rs->data[1][0], NULL, 0))
		{
			outputl (_("Player not found or player stats empty"));
			return;
		}
		sc.anTotalMoves[i] = strtol (rs->data[1][0], NULL, 0);
		sc.anUnforcedMoves[i] = strtol (rs->data[1][1], NULL, 0);
		sc.anTotalCube[i] = strtol (rs->data[1][2], NULL, 0);
		sc.anCloseCube[i] = strtol (rs->data[1][3], NULL, 0);
		sc.anDouble[i] = strtol (rs->data[1][4], NULL, 0);
		sc.anTake[i] = strtol (rs->data[1][5], NULL, 0);
		sc.anPass[i] = strtol (rs->data[1][6], NULL, 0);
		sc.anMoves[i][SKILL_VERYBAD] = strtol (rs->data[1][7], NULL, 0);
		sc.anMoves[i][SKILL_BAD] = strtol (rs->data[1][8], NULL, 0);
		sc.anMoves[i][SKILL_DOUBTFUL] = strtol (rs->data[1][9], NULL, 0);
		sc.anMoves[i][SKILL_NONE] = strtol (rs->data[1][10], NULL, 0);
		sc.anLuck[i][LUCK_VERYBAD] = strtol (rs->data[1][11], NULL, 0);
		sc.anLuck[i][LUCK_BAD] = strtol (rs->data[1][12], NULL, 0);
		sc.anLuck[i][LUCK_NONE] = strtol (rs->data[1][13], NULL, 0);
		sc.anLuck[i][LUCK_GOOD] = strtol (rs->data[1][14], NULL, 0);
		sc.anLuck[i][LUCK_VERYGOOD] = strtol (rs->data[1][15], NULL, 0);
		sc.anCubeMissedDoubleDP[i] = strtol (rs->data[1][16], NULL, 0);
		sc.anCubeMissedDoubleTG[i] = strtol (rs->data[1][17], NULL, 0);
		sc.anCubeWrongDoubleDP[i] = strtol (rs->data[1][18], NULL, 0);
		sc.anCubeWrongDoubleTG[i] = strtol (rs->data[1][19], NULL, 0);
		sc.anCubeWrongTake[i] = strtol (rs->data[1][20], NULL, 0);
		sc.anCubeWrongPass[i] = strtol (rs->data[1][21], NULL, 0);
		sc.arErrorCheckerplay[i][0] = (float)g_strtod (rs->data[1][22], NULL);
		sc.arErrorMissedDoubleDP[i][0] = (float)g_strtod (rs->data[1][23], NULL);
		sc.arErrorMissedDoubleTG[i][0] = (float)g_strtod (rs->data[1][24], NULL);
		sc.arErrorWrongDoubleDP[i][0] = (float)g_strtod (rs->data[1][25], NULL);
		sc.arErrorWrongDoubleTG[i][0] = (float)g_strtod (rs->data[1][26], NULL);
		sc.arErrorWrongTake[i][0] = (float)g_strtod (rs->data[1][27], NULL);
		sc.arErrorWrongPass[i][0] = (float)g_strtod (rs->data[1][28], NULL);
		sc.arLuck[i][0] = (float)g_strtod (rs->data[1][29], NULL);
		FreeRowset(rs);
	}
	sc.fMoves = 1;
	sc.fCube = 1;
	sc.fDice = 1;
	DumpStatcontext (output, &sc, player_name, _("Opponents"), NULL, FALSE);
#if USE_GTK
  if (fX)
    {
      GTKTextWindow (output, _("Player statistics"), DT_INFO);
    }
  else
#endif
    {
      outputl (_("Player statistics\n\n"));
      outputl (output);
    }
}

extern void CommandRelationalShowPlayers( char *sz )
{
	/* Use the Select command */
	CommandRelationalSelect("name AS Player FROM player ORDER BY name");
}

extern void CommandRelationalErase(char *sz)
{
	char *mq, buf[1024];
	DBProvider *pdb;
	char *player_name;
	int player_id;
	if (!sz || !*sz || !(player_name = NextToken(&sz)))
	{
		outputl( _("You must specify a player name to remove "
		"(see `help relational erase player').") );
		return;
	}
	if ((pdb = ConnectToDB(dbProviderType)) == NULL)
		return;

	player_id = GetPlayerId(pdb, player_name);
	if (player_id == -1)
	{
		outputl (_("Player not found or player stats empty"));
		return;
	}
	/* Get all matches involving player */
	mq = g_strdup_printf ("FROM session WHERE player_id0 = %d OR player_id1 = %d", player_id, player_id);

	/* first remove any matchstats */
	sprintf(buf, "DELETE FROM matchstat WHERE match_id in (select match_id %s)", mq);
	pdb->UpdateCommand(buf);

	/* then remove any matches */
	sprintf(buf, "DELETE %s", mq);
	pdb->UpdateCommand(buf);

	/* then the player */
	sprintf(buf, "DELETE FROM player WHERE player_id = %d", player_id);
	pdb->UpdateCommand(buf);

	pdb->Commit();
	pdb->Disconnect();
}

extern void CommandRelationalEraseAll(char *sz)
{
	DBProvider *pdb;

	if( fConfirmSave && !GetInputYN( _("Are you sure you want to erase all "
				       "player records?") ) )
		return;

	if ((pdb = ConnectToDB(dbProviderType)) == NULL)
		return;

	/* first remove all matchstats */
	pdb->UpdateCommand("DELETE FROM matchstat");

	/* then remove all matches */
	pdb->UpdateCommand("DELETE FROM session");
      
	/* finally remove the players */
	pdb->UpdateCommand("DELETE FROM player");

	pdb->Commit();
	pdb->Disconnect();
}

extern void FreeRowset(RowSet* pRow)
{
	unsigned int i, j;
	free(pRow->widths);

	for (i = 0; i < pRow->rows; i++)
	{
		for (j = 0; j < pRow->cols; j++)
		{
			free (pRow->data[i][j]);
		}
		free(pRow->data[i]);
	}
	free(pRow->data);

	pRow->cols = pRow->rows = 0;
	pRow->data = NULL;
	pRow->widths = NULL;
}

extern RowSet* RunQuery(char *sz)
{
	DBProvider *pdb;
	if ((pdb = ConnectToDB(dbProviderType)) != NULL)
	{
		RowSet *rs = pdb->Select(sz);
		pdb->Disconnect();
		return rs;
	}
	return NULL;
}

extern void CommandRelationalSelect(char *sz)
{
#if !USE_GTK
	int i, j;
#endif
	RowSet *rs;

	if (!sz || !*sz)
	{
		outputl( _("You must specify a sql query to run.") );
		return;
	}

	rs = RunQuery(sz);
	if (!rs)
		return;

	if (rs->rows == 0)
	{
		outputl(_("No rows found.\n"));
		return;
	}

#if USE_GTK
	GtkShowQuery(rs);
#else
	for (i = 0; i < rs->rows; i++)
	{
		if (i == 1)
		{	/* Underline headings */
			char* line, *p;
			int k;
			int totalwidth = 0;
			for (k = 0; k < rs->cols; k++)
			{
				totalwidth += rs->widths[k] + 1;
				if (k != 0)
					totalwidth += 2;
			}
			line = malloc(totalwidth + 1);
			memset(line, '-', totalwidth);
			p = line;
			for (k = 0; k < rs->cols - 1; k++)
			{
				p += rs->widths[k];
				p[1] = '|';
				p += 3;
			}
			line[totalwidth] = '\0';
			outputl(line);
			free(line);
		}

		for (j = 0; j < rs->cols; j++)
		{
			if (j > 0)
				output(" | ");

			outputf("%*s", rs->widths[j], rs->data[i][j]);
		}
		outputl("");
	}
#endif
	FreeRowset(rs);
}

extern int RelationalUpdatePlayerDetails(int player_id, const char* newName, const char* newNotes)
{
	int ret = FALSE;
	int exist_id;
	DBProvider *pdb;
	if ((pdb = ConnectToDB(dbProviderType)) == NULL)
		return FALSE;

	exist_id = GetPlayerId(pdb, newName);
	if (exist_id != player_id && exist_id != -1)
	{	/* Can't change the name to an existing one */
		outputerrf( _("New player name already exists.") );
	}
	else
	{
		char *buf = g_strdup_printf("UPDATE player SET name = '%s', notes = '%s' WHERE player_id = %d",
									newName, newNotes, player_id);
		if (pdb->UpdateCommand(buf))
		{
			ret = 1;
			pdb->Commit();
		}
		else
			outputerrf( _("Error running database command") );
		g_free(buf);
	}
	pdb->Disconnect();
	return ret;
}

extern void CommandRelationalSetup(char * sz)
{
	char *apch[ 2 ];

	if ( ParseKeyValue( &sz, apch ) )
	{
		if (!StrCaseCmp(apch[0], "dbtype"))
			SetDBType(apch[1]);
		else
		{
			char *pc = apch[0];
			char *db = NextTokenGeneral(&pc, "-");
			SetDBParam(db, pc, apch[1]);
		}
	}
}
