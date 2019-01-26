/*
 * Copyright 2005 Digi International (www.digi.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: exam_panel.h,v 1.5 2005/07/31 07:23:22 scottk Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


#ifndef _EXAM_PANEL_H
#define _EXAM_PANEL_H

#include "dpa_os.h"

int handle_exam (struct deviceinfo *unit, struct digi_node *node, int port);

#endif /* _EXAM_PANEL_H */
