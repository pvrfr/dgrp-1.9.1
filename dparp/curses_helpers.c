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
 * $Id: curses_helpers.c,v 1.11 2005/07/31 07:19:56 scottk Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


# include <curses.h>

#include <termios.h>

#ifdef AIX
# include "digi_panel.h" 
#else
# include <panel.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef CYGWIN
#include <sys/ioctl.h>
#endif

#include "dpa_rp.h"

#include "curses_helpers.h"
#include "general_panels.h"

#include "attribs.h"


static int USE_CURSES = FALSE;

extern long HasAttr;

static struct termios tbuf;	/* Terminal Structure			*/
static struct termios sbuf;	/* Saved Terminal Structure		*/

char *propeller[] =
{
	"|",
	"/",
	"-",
	"\\",
	"DOWN"
};

int vanilla = FALSE;


/**********************************************************************
*
*  Routine Name:	new_digi_win
*
*  Function:	Creates a new paneled window, with Digi parameters
*              recorded.
*
**********************************************************************/

DigiWin * 
new_digi_win(int y, int x, int starty, int startx)
{
	DigiWin *newone = (DigiWin *)malloc(sizeof(DigiWin));

	if( newone )
	{
		newone->winx = startx;
		newone->winy = starty;
		newone->winw = x;
		newone->winh = y;
		newone->win = newwin (y, x, starty, startx);
		newone->pan = new_panel (newone->win);
	}

	return newone;
}


/**********************************************************************
*
*  Routine Name:	delete_digi_win
*
*  Function:	Frees memory associated with Digi window.
*
**********************************************************************/

void
delete_digi_win(DigiWin *dw)
{
	del_panel (GetPan(dw));
	delwin (GetWin(dw));
	free (dw);
}


/**********************************************************************
*
*  Routine Name:	erase_win
*
*  Function:	Overwrites all chars on the panel (not the border)
*
**********************************************************************/

void 
erase_win(DigiWin *dw)
{
	int y, x, lines1,  cols;
	WINDOW *w = GetWin(dw);

	lines1 = GetHeight(dw);
	cols = GetWidth(dw);

	for (y = 1; y < lines1 - 1; y++)
		for (x = 1; x < cols - 1; x++)
			if (!((y == (LINES - 1)) && (x == (COLS - 1))))
				mvwaddstr (w, y, x, " ");
}



/**********************************************************************
*
*  Routine Name:	TermDepSetup
*
*  Function:	
*
**********************************************************************/

void
TermDepSetup(void)
{
	char *termtype = getenv ("TERM");

	/*
	 * when the term type is one of the following, we do not want to
	 * tell the user to use keys that are probably not mapped correctly
	 */
	if (!strncmp (termtype, "xterm", 5) ||
		!strncmp (termtype, "vt100", 5) || 
#if 0
		!strncmp (termtype, "sun", 3) || 
#endif
		!strncmp (termtype, "wy", 2))
	{
		strcpy (helpstr, "?=Help");
		strcpy (lrudstr, "h/j/k/l=Move");
		strcpy (updnstr, "j/k=Move");
		strcpy (pgdnstr, "N=Next Page");
		strcpy (pgupstr, "P=Previous Page");
	}
	else
	{
		strcpy (helpstr, "F1=Help");
		strcpy (lrudstr, "Arrows=Move");
		strcpy (updnstr, "Arrows=Move");
		strcpy (pgdnstr, "N=Next Page");
		strcpy (pgupstr, "P=Previous Page");
	}

	if (debug)
	{
		fprintf (dfp, "\n***** TermDepSetup *****\n");
		fprintf (dfp, "TERM = %s\n", termtype);
		fprintf (dfp, "Help String = %s\n", helpstr);
		fprintf (dfp, "L/R/U/D Arrows = %s\n", lrudstr);
		fprintf (dfp, "U/D Arrows = %s\n", updnstr);
		fprintf (dfp, "Page Down = %s\n", pgdnstr);
		fprintf (dfp, "Page Up = %s\n", pgupstr);
		fflush (dfp);
	}
}



/**********************************************************************
*
*  Routine Name:	StartCurses
*
*  Function:	Start curses
*
**********************************************************************/

void 
StartCurses(void)
{
	struct termios tios;

	USE_CURSES = TRUE;
	initscr ();
	nonl ();

	if (LINES < 24 || COLS < 80)
	{
		fprintf( stderr, "Display must be at least 24 lines by 80 columns to run this tool.\n" );
		EndCurses (-20);
	}

	if (tcgetattr(0, &tios) < 0)
	{
		EndCurses (-1);			   /* die horribly */
	}

	tios.c_cc[VMIN] = 1;
	tios.c_cc[VTIME] = 0;
	tios.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);

	if (tcsetattr(0, TCSAFLUSH, &tios) < 0)
	{
		EndCurses (-2);			   /* die horribly */
	}

	noecho ();
	cbreak ();
	keypad (stdscr, TRUE);
	start_color ();

#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

	if ((int)(tigetstr ("smul")) > 0)
		HasAttr |= A_UNDERLINE;
	if ((int)(tigetstr ("rev")) > 0)
		HasAttr |= A_REVERSE;
	if ((int)(tigetstr ("bold")) > 0)
		HasAttr |= A_BOLD;
#pragma GCC diagnostic pop

	KEY_LINE = (LINES >= 24) ? LINES - 1 : 23;
}



/**********************************************************************
*
*  Routine Name:	EndCurses
*
*  Function:	End curses
*
**********************************************************************/

void 
EndCurses(int exitval)
{
	if (USE_CURSES)
	{
		USE_CURSES = FALSE;
		/*
		 * FixMe - This should really only do closes, and then
		 * some other stuff if CreateWins () has been called
		 */
		DeleteWins ();
		if(debug)
			fprintf(dfp, "dpa - exiting abnormally #1 - %d\n", exitval);
		exit (exitval);
	}
}


/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
solidbox(DigiWin *dw)
{
	int y, x;
	WINDOW *w = GetWin(dw);

	wattron (w, make_attr (A_STANDOUT, BLACK, BLUE));
	for (x = 0; x < GetWidth(dw); x++)
	{
		mvwaddch (w, 0, x, ' ');
		mvwaddch (w, GetHeight(dw) - 1, x, ' ');
	}
	for (y = 0; y < GetHeight(dw); y++)
	{
		mvwaddch (w, y, 0, ' ');
		mvwaddch (w, y, GetWidth(dw) - 1, ' ');
	}

	wattroff (w, make_attr (A_STANDOUT, BLACK, BLUE));
}


/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
digibox(DigiWin *dw)
{
	WINDOW *w = GetWin(dw);

	wborder (w, mapchar(ACS_VLINE), mapchar(ACS_VLINE),
	            mapchar(ACS_HLINE), mapchar(ACS_HLINE),
	            mapchar(ACS_ULCORNER), mapchar(ACS_URCORNER),
	            mapchar(ACS_LLCORNER), mapchar(ACS_LRCORNER));
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

int
mapchar(int acs_char)
{
	int ret = ' ';

	if( !vanilla ) return acs_char;

	if((unsigned)acs_char == ACS_VLINE)    ret = '|';
	if((unsigned)acs_char == ACS_HLINE)    ret = '-';
	if((unsigned)acs_char == ACS_ULCORNER) ret = '+';
	if((unsigned)acs_char == ACS_URCORNER) ret = '+';
	if((unsigned)acs_char == ACS_LLCORNER) ret = '+';
	if((unsigned)acs_char == ACS_LRCORNER) ret = '+';
	if((unsigned)acs_char == ACS_RTEE)     ret = '+';
	if((unsigned)acs_char == ACS_LTEE)     ret = '+';
	if((unsigned)acs_char == ACS_BTEE)     ret = '+';
	if((unsigned)acs_char == ACS_TTEE)     ret = '+';
	if((unsigned)acs_char == ACS_PLUS)     ret = '+';

	return ret;
}


/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
fillwin(DigiWin *dw, int i)
{
	int y, x, lines1, cols;
	WINDOW *w = GetWin(dw);

	lines1 = GetHeight(dw);
	cols = GetWidth(dw);

	for (y = 0; y < lines1; y++)
		for (x = 0; x < cols; x++)
			if (!((y == (LINES - 1)) && (x == (COLS - 1))))
				mvwaddch (w, y, x, i);
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

int 
center(DigiWin *dwin, int str_width)
{
	int i;

	int win_width = GetWidth(dwin);

	i = (win_width / 2) - ((str_width + 1) / 2);
	if (i < 0)
		i = 0;

	return i;
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void
refresh_screen(void)
{
	touchwin (curscr);
	wrefresh (curscr);
}


/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
change_term(int vmin, int vtime)
{
	tcgetattr(0, &tbuf);
	tcgetattr(0, &sbuf);
	tbuf.c_cc[VMIN] = vmin;
	tbuf.c_cc[VTIME] = vtime;
	tcsetattr(0, TCSANOW, &tbuf);
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
restore_term(void)
{
	tcsetattr(0, TCSADRAIN, &sbuf);
}
