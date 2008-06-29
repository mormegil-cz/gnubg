
#include "config.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <glib/gi18n.h>
#include "backgammon.h"

extern const char *aszRatingList;
extern const char *aszLuckRatingList;
extern const char *aszSettingList;
extern const char *aszMoveFilterList;
extern const char *aszDoubleList;

const char *gnubg_gettext(const char *msgid)
{
	if (*msgid != ':')
		return gettext(msgid);
	else
	{	/* Remove context string */
		const char *start = strchr(msgid + 1, ':') + 1;
		while (isspace(*start))
			start++;
		return gettext(start);
	}
}

int translateList(const char *msgStrs, const char *list[], int numStrings)
{
	const char *nextid, *transList = gnubg_gettext(msgStrs);
	char *curElement;
	int i, msgLen;
	for (i = 0; i < numStrings; i++)
	{
		while (isspace(*transList))
			transList++;

		nextid = strchr(transList, ',');
		if (nextid)
			msgLen = nextid - transList;
		else
			msgLen = strlen(transList);

		list[i] = curElement = (char*)g_malloc(msgLen + 1);
		strncpy(curElement, transList, msgLen);
		curElement[msgLen] = '\0';

		transList = nextid + 1;
	}
	return TRUE;
}

static void freeList(const char *list[], int numStrings)
{
	int i;
	for (i = 0; i < numStrings; i++)
		g_free((void*)list[i]);
}

extern void InitTranslationLists()
{
	translateList(aszRatingList, aszRating, N_RATINGS);
	translateList(aszLuckRatingList, aszLuckRating, N_LUCKS);
	translateList(aszSettingList, aszSettings, NUM_SETTINGS);
	translateList(aszMoveFilterList, aszMoveFilterSettings, NUM_MOVEFILTER_SETTINGS);
	translateList(aszDoubleList, aszDoubleTypes, NUM_DOUBLE_TYPES);
}

extern void FreeTranslationLists()
{
	freeList(aszRating, N_RATINGS);
	freeList(aszLuckRating, N_LUCKS);
	freeList(aszSettings, NUM_SETTINGS);
	freeList(aszMoveFilterSettings, NUM_MOVEFILTER_SETTINGS);
	freeList(aszDoubleTypes, NUM_DOUBLE_TYPES);
}
