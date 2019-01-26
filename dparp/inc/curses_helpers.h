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
 * $Id: curses_helpers.h,v 1.2 2005/07/31 07:23:22 scottk Exp $
 *
 */


#ifndef _CURSES_HELPERS_H
#define _CURSES_HELPERS_H

#define  GetBegX(w)   ((w)->winx)
#define  GetBegY(w)   ((w)->winy)
#define  GetWidth(w)  ((w)->winw)
#define  GetHeight(w) ((w)->winh)
#define  GetWin(w)    ((w)->win)
#define  GetPan(w)    ((w)->pan)

#define BLACK	COLOR_BLACK
#define RED	COLOR_RED
#define GREEN	COLOR_GREEN
#define YELLOW	COLOR_YELLOW
#define BLUE	COLOR_BLUE
#define MAGENTA	COLOR_MAGENTA
#define CYAN	COLOR_CYAN
#define WHITE	COLOR_WHITE

#define	ESC	'\033'
#define	RETURN	'\015'

#define TIMEOUT	5      /* 5 tenths of a second (500 milliseconds) */


typedef struct {
	WINDOW *win;
	PANEL  *pan;

	int winx;
	int winy;
	int winw;  /* width  */
	int winh;  /* height */
} DigiWin;


/* 
 * Global variables.
 */

extern char *propeller[];
extern int vanilla;       /* When "1", use -, |, and + for boxes and tees */


/* 
 * Prototypes.
 */

DigiWin *new_digi_win (int, int, int, int);
void delete_digi_win (DigiWin *);
void erase_win (DigiWin *);
void TermDepSetup (void);
void StartCurses (void);
void EndCurses (int exitval);
void solidbox (DigiWin *);
void digibox (DigiWin *);
int  mapchar (int acs_char);
void fillwin (DigiWin *, int);
int center (DigiWin *, int);
void refresh_screen (void);
void change_term (int, int);
void restore_term (void);

#endif /* _CURSES_HELPERS_H */
