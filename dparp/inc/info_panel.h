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
 * $Id: info_panel.h,v 1.3 2005/07/31 07:23:22 scottk Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


#ifndef _INFO_PANEL_H
#define _INFO_PANEL_H

void info_screen (char *strarray[], int strarraylen, char *heading);

extern char *dpa_info1[];  extern int dpa_info1_len;
extern char *dpa_info2[];  extern int dpa_info2_len;
extern char *dpa_info3[];  extern int dpa_info3_len;
extern char *dpa_info4[];  extern int dpa_info4_len;
extern char *dpa_info5[];  extern int dpa_info5_len;

#endif /* _IDENTIFY_PANEL_H */
