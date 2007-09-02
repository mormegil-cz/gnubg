/*
 * gtkrelational.h
 *
 * by Christian Anthon <anthon@kiku.dk>, 2006.
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

#ifndef _GTKRELATIONAL_H_
#define _GTKRELATIONAL_H_
extern void GtkRelationalShowStats(gpointer p, guint n, GtkWidget * pw);
extern void GtkRelationalAddMatch(gpointer * p, guint n, GtkWidget * pw);
extern void GtkShowRelational(gpointer * p, guint n, GtkWidget * pw);
extern void GtkManageRelationalEnvs(gpointer * p, guint n, GtkWidget * pw);
#endif
