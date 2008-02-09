/*
 * relational.h
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

#ifndef _RELATIONAL_H_
#define _RELATIONAL_H_

#include <stddef.h>
#include <sys/types.h>

#define DB_VERSION 1

typedef struct _RowSet
{
	size_t cols, rows;
	char ***data;
	size_t *widths;
} RowSet;

extern RowSet* RunQuery(char *sz);
extern RowSet* MallocRowset(size_t rows, size_t cols);
extern void SetRowsetData(RowSet *rs, size_t row, size_t col, const char *data);
extern void FreeRowset(RowSet* pRow);
extern int RelationalUpdatePlayerDetails(const char* oldName, const char* newName, const char* newNotes);

extern float Ratio(float a, int b);

#endif /* _RELATIONAL_H_ */
