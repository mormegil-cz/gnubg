/*
 * boarddim.h
 *
 * by Holger Bochnig <hbgg@gmx.net>, 2003
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

#ifndef _BOARDDIM_H_
#define _BOARDDIM_H_


#include "config.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define BOARD_WIDTH		108
#if USE_BOARD3D /* || USE_OLD_LAYOUT */
#define BOARD_HEIGHT		 72
#else
#define BOARD_HEIGHT		 82
#endif
#define BORDER_HEIGHT		  3
#define BORDER_WIDTH		  3
#define DIE_WIDTH		  7
#define DIE_HEIGHT		  7
#define CUBE_WIDTH		  8
#define CUBE_HEIGHT		  8
#define CUBE_LABEL_WIDTH	  6
#define CUBE_LABEL_HEIGHT	  6
#define RESIGN_WIDTH		  8
#define RESIGN_HEIGHT		  8
#define RESIGN_LABEL_WIDTH	  6
#define RESIGN_LABEL_HEIGHT	  6
#define ARROW_WIDTH		  5
#define ARROW_HEIGHT		  5
#define CHEQUER_WIDTH		  6
#define CHEQUER_HEIGHT		  6
#define CHEQUER_LABEL_WIDTH	  4
#define CHEQUER_LABEL_HEIGHT	  4
#define POINT_WIDTH		  6
#define POINT_HEIGHT		 30
#define BOARD_CENTER_WIDTH	 36
#define BOARD_CENTER_HEIGHT	  6
#define BEAROFF_WIDTH		 12
#define BEAROFF_HEIGHT		 30
#define BEAROFF_DIVIDER_HEIGHT	  6
#define BAR_WIDTH		 12


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BOARDDIM_H_ */
