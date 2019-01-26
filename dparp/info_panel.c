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
 * $Id: info_panel.c,v 1.7 2005/07/31 07:19:56 scottk Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


# include <curses.h>

#ifdef AIX
# include "digi_panel.h"
#else
# include <panel.h>
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "dpa_rp.h"
#include "curses_helpers.h"
#include "general_panels.h"
#include "info_panel.h"

#include "attribs.h"


/**********************************************************************
*
*  Routine Name:	info_screen
*
*  Function:      Display text in a scrolling box.  Text must be
*                 74 columns wide, and in an array of character
*                 pointers.
*
*                 An optional heading can be supplied as the
*                 third parameter.
*
**********************************************************************/

void
info_screen (strarray, strarraylen, heading)
	char *strarray[];
	int strarraylen;
	char *heading;
{
	DigiWin *DHelpWin;
	WINDOW  *HelpWin;
	int xpos;

	int line = 0;
	int limit = 16;
	int option = EOF;
	int startline = 0, endline = 0;
	int change = TRUE;
	int one_page = FALSE;

	endline = strarraylen - 1;

	xpos = ( GetWidth(MainWin) - 78 ) / 2;
	DHelpWin = new_digi_win( GetHeight(IdentWin), 78, GetBegY(IdentWin), xpos );

	if (DHelpWin == NULL)
	{
		ShowMsg ("Error creating a new window/panel");
		EndCurses (-3);
	}

	HelpWin = GetWin(DHelpWin);
	limit = GetHeight(DHelpWin) - 2;

	keypad (HelpWin, 1);
	wattrset (HelpWin, make_attr (A_NORMAL, BLACK, CYAN));
	digibox (DHelpWin);
	wattroff (HelpWin, make_attr (A_NORMAL, BLACK, CYAN));

	wattrset (HelpWin, make_attr (A_BOLD | A_STANDOUT, WHITE, RED));
	if (heading == NULL)
	{
		mvwprintw (HelpWin, 0, center (DHelpWin, strlen (" DPA HELP ")),
		 " DPA HELP ");
	}
	else
	{
		mvwprintw (HelpWin, 0, center (DHelpWin, strlen (heading)),
		 "%s", heading);
	}
	wattroff (HelpWin, make_attr (A_BOLD | A_STANDOUT, WHITE, RED));

	/* this will not time-out */
	change_term (1, 0);
	while ((option != 'Q') && (option != 'q') && (option != ESC))
	{
		/* Only do this if a new page is desired */
		if (change)
		{
			wattrset (HelpWin, make_attr (A_NORMAL, BLACK, CYAN));
			erase_win (DHelpWin);
			change = FALSE;

			for (line=0; line < limit; line++)
			{
				if( (startline + line) <= endline )
					mvwprintw (HelpWin, line + 1, 2, "%-74.74s",
					           strarray[startline + line]);
				else
					mvwprintw (HelpWin, line + 1, 2, "%-74.74s", " ");
			}
			wattroff (HelpWin, make_attr (A_NORMAL, BLACK, CYAN));
			wmove (GetWin(MainWin), KEY_LINE, 0);
			wrefresh (GetWin(MainWin));
		}

		wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		if (startline == 0)
		{
			if (endline < limit)  /* only one page */
			{
				one_page = TRUE;
				commandline (clbuf, "Press ANY key to continue.", NULL);
				mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
			}
			else
				/* first page */
			{
				commandline (clbuf, "ESC=Continue", updnstr, pgdnstr, NULL);
				mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
			}
		}
		else if (endline - startline < limit)	/* last page */
		{
			commandline (clbuf, "ESC=Continue", updnstr, pgupstr, NULL);
			mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
		}
		else
			/* a middle page */
		{
			commandline (clbuf, "ESC=Continue", updnstr, pgdnstr,
						 pgupstr, NULL);
			mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
		}

		wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));

		show_panel (GetPan(DHelpWin));
		update_panels ();
		doupdate ();

		switch (option = wgetch (HelpWin))
		{
		case EOF:
			break;

		case '':
			refresh_screen ();
			break;

#ifdef KEY_UP
		case KEY_PPAGE:
#endif
		case 'P':
		case 'p':
			if (startline > 0)
			{
				startline -= limit;
				if (startline < 0)
					startline = 0;
				change = TRUE;
			}
			break;

#ifdef KEY_DOWN
		case KEY_NPAGE:
#endif
		case 'N':
		case 'n':
			if (endline >= startline + limit)
			{
				startline += limit;
				change = TRUE;
			}
			break;

#ifdef KEY_DOWN
		case KEY_DOWN:
#endif
		case 'j':
		case 'J':
			if (endline >= startline + limit)
			{
				startline++;
				change = TRUE;
			}
			break;

#ifdef KEY_UP
		case KEY_UP:  
#endif
		case 'k':
		case 'K':
			if (startline > 0)
			{
				startline--;
				change = TRUE;
			}
			break;

		case 'Q':
		case 'q':
		case ESC:
			option = 'Q';
			break;

		default:
			if (one_page)
				option = 'Q';
			break;
		}						   /* Switch */
	}							   /* End While Loop */

	change_term (0, TIMEOUT);
	hide_panel (GetPan(DHelpWin));
	update_panels ();
	doupdate ();
	delete_digi_win(DHelpWin);
}
