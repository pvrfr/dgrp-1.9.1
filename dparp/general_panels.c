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
 * $Id: general_panels.c,v 1.13 2016/02/09 03:36:34 pberger Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


#include <ctype.h>

# include <curses.h>

#ifdef AIX
# include "digi_panel.h"
#else
# include <panel.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef SCO
#include <stdarg.h>
#endif

#ifdef SOLARIS
#include <stdarg.h>
#endif

#ifdef AIX
#include <stdarg.h>
#endif

#include "dpa_rp.h"

#include "curses_helpers.h"
#include "general_panels.h"

#include "attribs.h"


static char rev_info[]="$Revision: 1.13 $";
static int FeatureRelease = 0;	/* increase when it is a new feature release */
static int MinorRev_level = 0;	/* level when the feature level increased */
                                /* Typically one less than current minor */
                                /* BEFORE committing changes to the file, */
                                /* since it will increment when the file is */
                                /* committed!!!!  */
static char VERSION_STR[20];


DigiWin *MainWin;
DigiWin *CRWin;
DigiWin *HeadWin;
DigiWin *IdentWin;
DigiWin *ChanWin;
DigiWin *LBWin;
DigiWin *ScopeWin;
DigiWin *ExamWin;
DigiWin *NodeWin;

int KEY_LINE;
char *clbuf;

char helpstr[STRSIZ];	/* "help" key line string		*/
char lrudstr[STRSIZ];	/* left/right/up/dn help string	*/
char updnstr[STRSIZ];	/* up/down key line string		*/
char pgdnstr[STRSIZ];	/* page down key line string	*/
char pgupstr[STRSIZ];	/* page up key line string		*/


static void release_number (void);
static void screen_print (DigiWin *, FILE *);


/**********************************************************************
*
*  Routine Name:	release_number
*
*  Function:	Create the correct release number
*
**********************************************************************/

void 
release_number(void)
{
	char tmp[10];
	int Release;
	int Level;

	Release = atoi(strchr(rev_info, ' ') + 1);
	Level = atoi(strchr(rev_info, '.') + 1);
	sprintf (VERSION_STR, "%d", Release);
	sprintf (tmp, ".%d.%d", FeatureRelease, Level - MinorRev_level);
	strcat (VERSION_STR, tmp);
}



/**********************************************************************
*
*  Routine Name:	AskYN
*
*  Function:	Prompts a user for a Yes/No answer to the given question
*
**********************************************************************/

int
AskYN (char *question)
{
	int length = strlen (question);
	int x, y, startx, starty;
	int spaces = 4;
	int rval = 0;
	int done = FALSE;

	DigiWin *DAskWin;
	WINDOW *AskWin;
	PANEL *AskPanel;

	x = length + spaces * 2;
	y = 5;
	startx = (COLS - x) / 2;
	starty = (LINES - y) / 2;

	DAskWin = new_digi_win (y, x, starty, startx);
	AskWin = GetWin(DAskWin);
	AskPanel = GetPan(DAskWin);

	wattrset (AskWin, make_attr (A_NORMAL, CYAN, BLACK));
	digibox (DAskWin);
	wattroff (AskWin, make_attr (A_NORMAL, CYAN, BLACK));

	wattrset (AskWin, make_attr (A_BOLD, WHITE, RED));
	mvwprintw (AskWin, 0, center (DAskWin, strlen (" User Input ")),
			   " User Input ");
	wattroff (AskWin, make_attr (A_BOLD, WHITE, RED));

	mvwprintw (AskWin, 2, 4, question);

	show_panel (AskPanel);
	update_panels ();
	doupdate ();
	wrefresh (AskWin);
	flushinp ();
	while (!done)
	{
		switch (toupper (wgetch (AskWin)))
		{
			case 'Y':
				rval = TRUE;
				done = TRUE;
				waddch (AskWin, 'Y');
				wrefresh (AskWin);
				break;
			case 'N':
				rval = FALSE;
				done = TRUE;
				waddch (AskWin, 'N');
				wrefresh (AskWin);
				break;
		}
	}

	if (debug)
	{
		fprintf (dfp, "***** Debug info from AskYN *****\n");
		fprintf (dfp, "Question: %s\n", question);
		fprintf (dfp, "Answer: %s\n", (rval) ? "TRUE" : "FALSE");
		fflush (dfp);
	}

	wrefresh (AskWin);
	sleep (1);
	hide_panel (AskPanel);
	wrefresh (AskWin);
	update_panels ();
	doupdate ();
	del_panel (AskPanel);
	delwin (AskWin);
	free(DAskWin);
	flushinp ();

	return rval;
}



/**********************************************************************
*
*  Routine Name:	ShowMsg
*
*  Function:	Displays a message in a window
*
**********************************************************************/

void 
ShowMsg (char *err)
{
	int str_len = strlen (err);
	int x, y, startx, starty;
	int newx;
	int spaces = 4;

	DigiWin *DShowMsgWin;
	WINDOW *ShowMsgWin;
	PANEL *ShowMsgPanel;

	x = str_len + spaces * 2;
	y = 6;

	startx = (GetWidth(MainWin) / 2) - x / 2;
	starty = (GetHeight(MainWin) / 2) - y / 2;

	DShowMsgWin = new_digi_win (y, x, starty, startx);
	ShowMsgPanel = GetPan(DShowMsgWin);
	ShowMsgWin = GetWin(DShowMsgWin);

	wattrset (ShowMsgWin, make_attr (A_NORMAL, CYAN, BLACK));

	digibox (DShowMsgWin);

	wattroff (ShowMsgWin, make_attr (A_NORMAL, CYAN, BLACK));

	wattrset (ShowMsgWin, make_attr (A_BOLD, WHITE, RED));
	mvwprintw (ShowMsgWin, 0, center (DShowMsgWin, strlen (" Message ")),
			   " Message ");
	wattroff (ShowMsgWin, make_attr (A_BOLD, WHITE, RED));

	mvwprintw (ShowMsgWin, 2, 4, err);
	newx = ((int) (x / 2))
		- ((int) (strlen ("Press ANY key to continue")) / 2);
	wattrset (ShowMsgWin, make_attr (A_STANDOUT | A_BOLD, WHITE, BLUE));
	mvwprintw (ShowMsgWin, y - 2, newx, "Press ANY key to continue");
	wattroff (ShowMsgWin, make_attr (A_STANDOUT | A_BOLD, WHITE, BLUE));

	if (debug)
	{
		fprintf (dfp, "***** Debug info from ShowMsg *****\n");
		fprintf (dfp, "Error message: %s\n", err);
		fprintf (dfp, "LINES: %d, COLUMNS: %d\n", LINES, COLS);
		fprintf (dfp, "startx: %d, starty: %d\n", startx, starty);
		fprintf (dfp, "height: %d, width: %d\n", y, x);
		fflush (dfp);
	}

	show_panel (ShowMsgPanel);
	change_term (1, 0);
	wgetch (ShowMsgWin);
	change_term (0, TIMEOUT);
	hide_panel (ShowMsgPanel);
	update_panels ();
	doupdate ();
	wrefresh (ShowMsgWin);
	del_panel (ShowMsgPanel);
	delwin (ShowMsgWin);
	free (DShowMsgWin);
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

int
commandline (char *s, ...)
{
	int i = 1, j = 0;
	int total_len = 0, num_args = 0;
	int remain = 0;				/* remaining spaces, not even distribution */
	int spaces = 0, leading = 0, trailing = 0;
	int err = 0;
	int diff = 0;
	int one_option = FALSE;
	va_list strings;
	char *p;
	char **args;
	char space_str[40] = "\0";
	char lspace_str[40] = "\0";
	char tspace_str[40] = "\0";

	int line_len = GetWidth(MainWin) - 1;

	if (s == NULL)
		return -1;

	/* erase previous contents */
	memset (s, '\0', line_len + 1);
	memset (space_str, '\0', strlen (space_str));
	memset (lspace_str, '\0', strlen (lspace_str));
	memset (tspace_str, '\0', strlen (tspace_str));

	/* copy first string */
	va_start (strings, s);
	if ((p = va_arg (strings, char *)) == NULL)
	{
		err = -1;
		return (err);
	}
	else
	{
		args = (char **) malloc (sizeof (char *));

		args[0] = (char *) malloc (strlen (p) + 1);
		strcpy (args[0], p);
		total_len = strlen (p);
	}

	/* append others */
	while ((p = va_arg (strings, char *)) != NULL)
	{
	  if ((args = (char **) realloc (args, i * sizeof (char *) + 1)) == NULL) {
		  if(debug) {
			  fprintf(dfp, "dpa - exiting abnomally #2\n");
		  }
		  exit (1);
	  }

		args[i] = (char *) malloc (strlen (p) + 1);
		total_len += strlen (p);
		if (total_len > line_len)
		{
			err = -1;
			return (err);
		}
		strcpy (args[i], p);
		i++;
	}
	va_end (strings);
	num_args = i;

	/*
	 * test for sufficient spacing, minimum of no leading or trailing
	 * spaces, just 2 spaces between the choices
	 */

	if (num_args >= 2)
	{
		spaces = (line_len - total_len) / (num_args - 1);
		remain = (line_len - total_len) % (num_args - 1);
	}
	else
	{
		spaces = 0;
		one_option = TRUE;
		remain = (line_len - total_len);
	}
	leading = remain / 2 + remain % 2;
	trailing = remain / 2;

	if ((spaces >= 2) || (one_option))
	{
		/*
		 * check for possible equal leading and trailing spaces, if we were 
		 * either one or 2 spaces short, take them from the beginning, and/or
		 * the end of the line
		 */
		if (!one_option)
		{
			spaces = (line_len - total_len) / (num_args + 1);
			remain = (line_len - total_len) % (num_args + 1);
			leading = spaces;
			trailing = spaces;
		}

		if (remain == num_args)
		{
			leading += 1;
			spaces += 1;
		}
		else if (remain == num_args - 1)
		{
			spaces += 1;
		}
		else if (!one_option)
		{
			diff = num_args - 1 - remain;
			leading -= diff / 2;
			trailing -= (diff / 2 + diff % 2);
			spaces += 1;

			if (leading < 0)
			{
				spaces -= 1;
				leading = spaces;
			}

			if (trailing < 0)
			{
				spaces -= 1;
				trailing = spaces;
			}
		}

		if (leading > 0)
		{
			strcpy (lspace_str, " ");
			for (i = 1; i < leading; i++)
				strcat (lspace_str, " ");
		}

		if (trailing > 0)
		{
			strcpy (tspace_str, " ");
			for (i = 1; i < trailing; i++)
				strcat (tspace_str, " ");
		}

		if (spaces > 0)
		{
			strcpy (space_str, " ");
			for (i = 1; i < spaces; i++)
				strcat (space_str, " ");
		}

		/*
		 * make the key line string
		 */
		strcpy (s, lspace_str);
		for (i = 0; i < num_args; i++)
		{
			strcat (s, args[i]);
			if (i < (num_args - 1))
				strcat (s, space_str);
		}
		strcat (s, tspace_str);
	}
	else
	{
		sprintf (s, "Option list is too long, need %d fewer characters",
				 total_len + (2 * (num_args - 1)) - line_len);
	}

	/*
	 * free the malloc'ed space
	 */

	for (j = 0; j < i; j++)
		free (args[j]);
	free (args);

	return (err);
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
ShowCopyright(void)
{
	WINDOW *w = GetWin(CRWin);

	wattrset (w, make_attr (A_NORMAL, GREEN, BLACK));
	mvwprintw (w, 2, 18, "RESTRICTED RIGHTS LEGEND");
	mvwprintw (w, 4, 4,
	 "Use, duplication, or disclosure by the Government is");
	mvwprintw (w, 5, 4,
	 "subject to restrictions as set forth in subparagraph");
	mvwprintw (w, 6, 4,
	 "(c)(1)(ii) of the Rights in Technical Data and");
	mvwprintw (w, 7, 4,
	 "Computer Software clause at DFARS 252.227-7013.");
	mvwprintw (w, 9, 17, "Digi International Inc.");
	mvwprintw (w, 10, 17, "11001 Bren Road E.");
	mvwprintw (w, 11, 17, "Minnetonka, MN 55343");

	wattrset (w, make_attr (A_STANDOUT | A_BOLD, WHITE, BLUE));
	mvwprintw (w, 15, 15, " Press ANY key to continue ");
	wattroff (w, make_attr (A_STANDOUT | A_BOLD, WHITE, BLUE));
	touchwin (w);
	keypad (w, TRUE);
	show_panel (GetPan(MainWin));
	show_panel (GetPan(CRWin));
	update_panels ();
	doupdate ();
	wrefresh (GetWin(MainWin));
	wrefresh (GetWin(CRWin));
	wgetch (GetWin(CRWin));
	hide_panel (GetPan(CRWin));
	update_panels ();
	doupdate ();
}


/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

static void 
screen_print (DigiWin *dwin, FILE *fp)
{
	chtype c;
	register int x, y;
	char str[80] = "\0";
	WINDOW *win = GetWin(dwin);

	for (x = 0; x < GetBegX(dwin); x++)
		strcat (str, " ");

	/* try to simulate relative position on the screen */
	for (y = 0; y < GetBegY(dwin) - 4; y++)
		fputc ('\n', fp);

	for (y = 0; y < GetHeight(dwin); y++)
	{
		fputs (str, fp);

		for (x = 0; x < GetWidth(dwin); x++)
		{
			int mapped_c;

			c = mvwinch (win, y, x);
			mapped_c = mapchar((int)c);

			if (mapped_c != ' ') c = (chtype)mapped_c;

			if (c != (c & ~A_ALTCHARSET))
			{
				c &= 0xffff;

				switch (c)
				{
					/* ansi term values */
				case 90:		   /* ULCORNER */
				case 63:		   /* URCORNER */
				case 64:		   /* LLCORNER */
				case 89:		   /* LRCORNER */
					c = '+';
					break;
				case 68:		   /* HLINE */
					c = '-';
					break;
				case 51:		   /* VLINE */
				case 67:		   /* LTEE */
				case 52:		   /* RTEE */
					c = '|';
					break;

					/* non-ansi term values */
				case 108:		   /* ULCORNER */
				case 107:		   /* URCORNER */
				case 109:		   /* LLCORNER */
				case 106:		   /* LRCORNER */
					c = '+';
					break;
				case 113:		   /* HLINE */
					c = '-';
					break;
				case 120:		   /* VLINE */
				case 116:		   /* LTEE */
				case 117:		   /* RTEE */
					c = '|';
					break;
				}
			}
			fputc (c, fp);
		}
		fputc ('\n', fp);
	}

	if (dwin != HeadWin)
	{
		fputc ('\n', fp);
		fputc ('\n', fp);
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
screen_save (DigiWin *dwin, char *file)
{
	DigiWin *Dwps_win;
	WINDOW *wps_win;
	PANEL *wps_panel;

	FILE *fp;
	char msg[256];
	char fmode[3] = "w";
	int quit = FALSE;
	int path_len = strlen (file);
	int x, startx;
	int valid_choice = FALSE;


	change_term (1, 0);

	if (!access (file, F_OK))	   /* File Exists */
	{
		x = path_len + 20;
		if (x < 26)
			x = 26;

		startx = (GetWidth(MainWin) / 2) - x / 2;
		if ((Dwps_win = new_digi_win (10, x, 9, startx)) != NULL)
		{
			wps_panel = GetPan(Dwps_win);
			wps_win = GetWin(Dwps_win);
			keypad (wps_win, 1);
		}
		else
		{
			ShowMsg ("Error creating a new window/panel");
			EndCurses (-5);
			return;
		}

		while (!valid_choice)
		{
			wattrset (wps_win, make_attr (A_NORMAL, BLACK, CYAN));
			fillwin (Dwps_win, ' ');
			digibox (Dwps_win);
			startx = x / 2 - 10;
			sprintf (msg, "File %s exists,", file);
			mvwprintw (wps_win, 2, 3, msg);
			mvwprintw (wps_win, 3, 3, "Choose Option:");
			mvwprintw (wps_win, 5, startx, "'A' = Append To File");
			mvwprintw (wps_win, 6, startx, "'O' = Overwrite File");
			mvwprintw (wps_win, 7, startx, "'Q' = Quit");
			wattroff (wps_win, make_attr (A_NORMAL, BLACK, CYAN));

			switch (wgetch (wps_win))
			{
			case 'A':
			case 'a':
				sprintf ((char *) fmode, (const char *) "a");
				valid_choice = TRUE;
				break;
			case 'O':
			case 'o':
				sprintf ((char *) fmode, (const char *) "w");
				valid_choice = TRUE;
				break;
			case 'Q':
			case 'q':
				quit = TRUE;
				valid_choice = TRUE;
				change_term (0, TIMEOUT);
				break;

			}					   /* End Switch */
		}
		hide_panel (wps_panel);
		del_panel (wps_panel);
		delwin (wps_win);
		free (Dwps_win);

		if (quit)
		{
			update_panels ();
			doupdate ();
			return;
		}
	}							   /* End If */

	if (!(fp = fopen (file, fmode)))
	{
		sprintf ((char *) msg, (const char *) "Error opening file %s", file);
		ShowMsg (msg);
		return;
	}
	screen_print (HeadWin, fp);
	screen_print (dwin, fp);

	sprintf ((char *) msg, (const char *) "Screen saved to %s", file);
	ShowMsg (msg);
	fclose (fp);
	change_term (0, TIMEOUT);
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
DisplayHeader(void)
{
	char msg[256];

	WINDOW *w = GetWin(HeadWin);

	release_number();

	wattrset (w, make_attr (A_NORMAL, WHITE, BLUE));
	fillwin (HeadWin, ' ');
	digibox (HeadWin);
	wattrset (w, make_attr (A_BOLD, YELLOW, BLUE));
	sprintf (msg, "Digi Port Authority for RealPort   version %s",
	         VERSION_STR);
	mvwprintw (w, 1, center (HeadWin, strlen (msg)), msg);
	sprintf (msg, "Copyright (c) 2005, Digi International Inc.");
	mvwprintw (w, 2, center (HeadWin, strlen (msg)), msg);
	wattroff (w, make_attr (A_BOLD, YELLOW, BLUE));
	show_panel (GetPan(MainWin));
	show_panel (GetPan(HeadWin));
	update_panels ();
	doupdate ();
	wrefresh (GetWin(MainWin));
	wrefresh (w);
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
CreateWins(void)
{
	int total_width = COLS;
	int header_height = 4;

	/*
	 *  The layout is always:
	 *
	 *    MainWin underneath all, filling the whole space
	 *    The bottom two lines of MainWin MUST be preserved,
	 *      for the "key" line and extra space underneath it.
	 *    A header panel fills the top 4 lines.
	 *    The "rest" of the space should therefore be:
	 *      LINES - 4 - 1 - 1 (header, space after header, keyline)
	 *      COLS - 2 (one column of space on either side)
	 */

	int primary_width = COLS - 2;
	int primary_height = LINES - header_height - 1 - 1;
	int primary_x = 1;
	int primary_y = 0 + header_height + 1;

	int CR_width = 60;
	int CR_height = 18;
	int CR_y = ( LINES - CR_height ) / 2;
	int CR_x = ( COLS - CR_width ) / 2;

	int chan_width  = 78;  /* Fixed width window */
	int chan_height = 18;  /* Fixed height window */
	int chan_x      = primary_x + ( primary_width - chan_width ) / 2;
	int chan_y      = primary_y + ( primary_height - chan_height ) / 2;

	KEY_LINE = LINES - 1;

	/* always display, this has the background pattern and colors */
	MainWin = new_digi_win(LINES, total_width, 0, 0);
	wattrset (GetWin(MainWin), make_attr (A_NORMAL, GREEN, WHITE));
	fillwin (MainWin, mapchar(177));
	wattroff (GetWin(MainWin), make_attr (A_NORMAL, GREEN, WHITE));
	keypad (GetWin(MainWin), TRUE);
	leaveok (GetWin(MainWin), TRUE);
	scrollok (GetWin(MainWin), FALSE);
	hide_panel (GetPan(MainWin));
	clbuf = (char *)malloc(GetWidth(MainWin) + 1);

	/* copyright panel */
	CRWin = new_digi_win(CR_height, CR_width, CR_y, CR_x);
	wattrset (GetWin(CRWin), make_attr (A_NORMAL, GREEN, BLACK));
	fillwin (CRWin, ' ');
	wattroff (GetWin(CRWin), make_attr (A_NORMAL, GREEN, BLACK));
	solidbox (CRWin);
	keypad (GetWin(CRWin), TRUE);
	leaveok (GetWin(CRWin), TRUE);
	hide_panel (GetPan(CRWin));

	/* DisplayHeader */
	HeadWin = new_digi_win(4, total_width, 0, 0);
	wattrset (GetWin(HeadWin), make_attr (A_NORMAL, WHITE, BLUE));
	fillwin (HeadWin, ' ');
	digibox (HeadWin);
	wattroff (GetWin(HeadWin), make_attr (A_NORMAL, WHITE, BLUE));
	keypad (GetWin(HeadWin), TRUE);
	hide_panel (GetPan(HeadWin));

	/* print_identify */
	IdentWin = new_digi_win(primary_height, primary_width,
	                        primary_y, primary_x);
	wattrset (GetWin(IdentWin), make_attr (A_NORMAL, CYAN, BLACK));
	digibox (IdentWin);
	wattroff (GetWin(IdentWin), make_attr (A_NORMAL, CYAN, BLACK));
	keypad (GetWin(IdentWin), TRUE);
	leaveok (GetWin(IdentWin), TRUE);
	hide_panel (GetPan(IdentWin));

	/* print_channel */
	ChanWin = new_digi_win(chan_height, chan_width,
	                       chan_y, chan_x);
	wattrset (GetWin(ChanWin), make_attr (A_NORMAL, CYAN, BLACK));
	digibox (ChanWin);
	wattroff (GetWin(ChanWin), make_attr (A_NORMAL, CYAN, BLACK));
	keypad (GetWin(ChanWin), TRUE);
	leaveok (GetWin(ChanWin), TRUE);
	hide_panel (GetPan(ChanWin));

	/* loop_back */
	LBWin = new_digi_win(chan_height, chan_width,
	                     chan_y, chan_x);
	wattrset (GetWin(LBWin), make_attr (A_NORMAL, CYAN, BLACK));
	digibox (LBWin);
	wattroff (GetWin(LBWin), make_attr (A_NORMAL, CYAN, BLACK));
	keypad (GetWin(LBWin), TRUE);
	leaveok (GetWin(LBWin), TRUE);
	hide_panel (GetPan(LBWin));

	/* data_scope */
	ScopeWin = new_digi_win(primary_height, chan_width,
	                        primary_y, chan_x);
	wattrset (GetWin(ScopeWin), make_attr (A_NORMAL, CYAN, BLACK));
	digibox (ScopeWin);
	wattroff (GetWin(ScopeWin), make_attr (A_NORMAL, CYAN, BLACK));
	keypad (GetWin(ScopeWin), TRUE);
	leaveok (GetWin(ScopeWin), FALSE);
	hide_panel (GetPan(ScopeWin));

	/* examine buffers */
	ExamWin = new_digi_win(primary_height, chan_width,
	                        primary_y, chan_x);
	wattrset (GetWin(ExamWin), make_attr (A_NORMAL, CYAN, BLACK));
	digibox (ExamWin);
	wattroff (GetWin(ExamWin), make_attr (A_NORMAL, CYAN, BLACK));
	keypad (GetWin(ExamWin), TRUE);
	leaveok (GetWin(ExamWin), FALSE);
	hide_panel (GetPan(ExamWin));

	/* Node Window */
	NodeWin = new_digi_win(primary_height, chan_width,
	                        primary_y, chan_x);
	wattrset (GetWin(NodeWin), make_attr (A_NORMAL, CYAN, BLACK));
	digibox (NodeWin);
	wattroff (GetWin(NodeWin), make_attr (A_NORMAL, CYAN, BLACK));
	keypad (GetWin(NodeWin), TRUE);
	leaveok (GetWin(NodeWin), FALSE);
	hide_panel (GetPan(NodeWin));

	update_panels ();
	doupdate ();
}



/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

void 
DeleteWins(void)
{
	hide_panel (GetPan(HeadWin));
	hide_panel (GetPan(MainWin));
	hide_panel (GetPan(IdentWin));
	update_panels ();
	doupdate ();


	delete_digi_win(MainWin);

	delete_digi_win(HeadWin);
	delete_digi_win(IdentWin);
	delete_digi_win(ChanWin);
	delete_digi_win(LBWin);
	delete_digi_win(ScopeWin);
	delete_digi_win(ExamWin);
	delete_digi_win(NodeWin);

	erase ();
	refresh ();

	/*
	 * put original term settings back
	 */
	restore_term ();
	endwin ();
}
