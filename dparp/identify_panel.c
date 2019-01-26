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
 * $Id: identify_panel.c,v 1.15 2005/07/31 07:19:56 scottk Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


# include <curses.h>

#ifdef AIX
# include "digi_panel.h"
#else
# include <panel.h>
#endif

#include <errno.h>

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <netinet/in.h>

#include "dpa_rp.h"
#include "dpa_os.h"

#include "curses_helpers.h"
#include "general_panels.h"
#include "identify_panel.h"
#include "channel_panel.h"
#include "node_panel.h"
#include "info_panel.h"


#include "attribs.h"



/**********************************************************************
*
*  Routine Name:	print_identify
*
*  Function:	   main data screen... includes list of devices
*  					to be managed
*
**********************************************************************/

void 
print_identify(void)
{
	int i;
	int option = EOF;
	static int first = TRUE;
	int curr_device = 0;
	int top_device = 0;
	int bot_device = 0;

	char *full_line;
	char line_template[81]; /* TODO -- dynamic */

	int rc;
	fd_set rfds;
	fd_set wfds;
	struct timeval tv;
	int max_fd;

#define BORDER_WIDTH 1
#define COLUMN_SPACE 2
#define INDEX_WIDTH 4
#define DEVICEID_WIDTH 12
#define PORT_WIDTH 12
#define IPADDR_WIDTH 25
#define STATE_WIDTH  13

	char ipaddr[IPADDR_WIDTH + 1];
	char state[STATE_WIDTH + 1];
	char deviceid[DEVICEID_WIDTH + 1];

	/*
	 *  Prepare screen to be filled in
	 */
	full_line = (char *)malloc( GetWidth(IdentWin) - 2 + 1 );

	memset (ipaddr, '\0', sizeof (ipaddr));
	memset (state, '\0', sizeof (state));
	memset (deviceid, '\0', sizeof (deviceid));
	memset (full_line, ' ', GetWidth(IdentWin) - 2);

	full_line[GetWidth(IdentWin) - 2] = 0;

	/*
	 *  Line template is an entire line, minus the borders... assumes
	 *  the column spaces will NOT be supplied as strings.
	 */
	sprintf( line_template, "%%%dd  %%-%d.%ds  %%-%d.%ds  %%-%dd  %%-%d.%ds  ",
	         INDEX_WIDTH,
	         DEVICEID_WIDTH, DEVICEID_WIDTH,
	         IPADDR_WIDTH, IPADDR_WIDTH,
	         PORT_WIDTH,
	         STATE_WIDTH, STATE_WIDTH);

	wattrset (GetWin(IdentWin), make_attr (A_BOLD, WHITE, BLUE));
	mvwprintw (GetWin(IdentWin), 1, BORDER_WIDTH, full_line);

	mvwprintw (GetWin(IdentWin), 1, BORDER_WIDTH + 
	           INDEX_WIDTH + COLUMN_SPACE, "Device ID" );
	mvwprintw (GetWin(IdentWin), 1, BORDER_WIDTH + 
	           INDEX_WIDTH + COLUMN_SPACE + DEVICEID_WIDTH + COLUMN_SPACE,
		   "IP Address/Host" );
	mvwprintw (GetWin(IdentWin), 1, BORDER_WIDTH + 
	           INDEX_WIDTH + COLUMN_SPACE + DEVICEID_WIDTH + 
		   COLUMN_SPACE + IPADDR_WIDTH + COLUMN_SPACE,
	           "Ports         Status  ");

	if (!vanilla)
		wattrset (GetWin(IdentWin), make_attr (A_ALTCHARSET, CYAN, BLACK));
	else
		wattrset (GetWin(IdentWin), make_attr (A_NORMAL, CYAN, BLACK));

	for (i = 0; i < GetWidth(IdentWin); i++)
		mvwaddch (GetWin(IdentWin), 2, i, mapchar(ACS_HLINE));
	mvwaddch (GetWin(IdentWin), 2, 0, mapchar(ACS_LTEE));
	mvwaddch (GetWin(IdentWin), 2, GetWidth(IdentWin) - 1, mapchar(ACS_RTEE));

	wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
	commandline (clbuf, helpstr, "ESC=Quit", "RETURN=Select",
		updnstr, "^P=Print", "D=Display Product Info", NULL);
	mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
	wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));

	show_panel (GetPan(IdentWin));
	update_panels ();
	doupdate ();
	wrefresh (GetWin(MainWin));
	wrefresh (GetWin(HeadWin));
	wrefresh (GetWin(IdentWin));

	/*
	 *  Main Loop
	 */
	while ((option != 'Q') && (option != 'q') && (option != ESC))
	{
		if (num_devices == 0)
		{
			EndCurses (-6);
			/* FIXME: This code will not be executed as 
			   EndCurses() calls exit(). */
			fprintf (stderr, "No devices listed in configuration file.\n");
			exit (-2);
		}

		max_fd = 0;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(0, &rfds);

		bot_device = top_device + GetHeight(IdentWin) - 5 - 1;
		for (i = 0; i < num_devices; i++)
		{
			int curs_row = 3 + i - top_device;

			char *state_str;

			if (i < top_device)
			{
				continue;
			}

			if (i > bot_device)
			{
				continue;
			}

			memset (full_line, ' ', GetWidth(IdentWin) - 2);
			full_line[GetWidth(IdentWin) - 2] = 0;

			if (i == curr_device)
			{
				wattrset (GetWin(IdentWin), make_attr (A_STANDOUT, WHITE, BLUE));
				mvwprintw (GetWin(IdentWin), curs_row, 1, full_line);
			}
			else
			{
				wattrset (GetWin(IdentWin), make_attr (A_NORMAL, GREEN, BLACK));
				mvwprintw (GetWin(IdentWin), curs_row, 1, full_line);
			}

			if (device_info[i].status == UP)
				state_str = "Available";
			else
				state_str = "Down";

			sprintf( full_line, line_template, i + 1, 
			         device_info[i].devname,
			         device_info[i].host,
			         device_info[i].nports,
			         state_str);


			mvwprintw (GetWin(IdentWin), curs_row, 1, full_line );

			wattrset (GetWin(IdentWin), make_attr (A_NORMAL, WHITE, BLACK));
		}

		if (first)
			wrefresh (GetWin(MainWin));
		
		first = FALSE;

		wattrset (GetWin(IdentWin), make_attr (A_NORMAL, WHITE, BLACK));
		wrefresh (GetWin(IdentWin));

		change_term (0, TIMEOUT);

		option = EOF;

		tv.tv_sec = 0;  tv.tv_usec = 500000;
		rc = select( max_fd + 1, &rfds, &wfds, NULL, &tv );

		if (rc < 0)
		{
			fprintf (stderr, "FATAL ERROR: select failure.\n");
			if( debug )
				fprintf (dfp, "Select failure, errno: %d\n", errno);
			EndCurses (-12);
			/* FIXME: This code will not be executed as 
			   EndCurses() calls exit(). */
			exit (-2);
		}
		else if (rc > 0)
		{
			if (FD_ISSET(0, &rfds))
				option = getch();

		}

		switch (option)
		{
		case EOF:
			break;

		case '':
			refresh_screen ();
			break;

#ifdef KEY_UP
		case KEY_UP:
#endif
		case 'k':
		case 'K':
			if (curr_device)
			{
				curr_device--;
				if (curr_device < top_device) top_device = curr_device;
			}
			else
			{
				curr_device = num_devices - 1;
				top_device = curr_device - GetHeight(IdentWin) + 5 + 1;
				if (top_device < 0) top_device = 0;
			}
			break;

#ifdef KEY_DOWN
		case KEY_DOWN:
#endif
		case 'j':
		case 'J':
			if (curr_device < num_devices - 1)
			{
				curr_device++;
				if (curr_device > bot_device) top_device++;
			}
			else
			{
				curr_device = 0;
				top_device = 0;
			}
			break;

		case 'Q':
		case 'q':
		case ESC:
			hide_panel (GetPan(IdentWin));
			break;

		case RETURN:

			if (device_info[curr_device].status == UP) {
				print_channel (&device_info[curr_device]);
			}
			else {
				beep ();
				mvwprintw (GetWin(IdentWin), GetHeight(IdentWin) - 2,
				           GetWidth(IdentWin) - 18, "Device Down.");
				wrefresh (GetWin(IdentWin));
				sleep (1);
				mvwprintw (GetWin(IdentWin), GetHeight(IdentWin) - 2,
				           GetWidth(IdentWin) - 18, "             ");
				wrefresh (GetWin(IdentWin));
			}

			show_panel (GetPan(IdentWin));
			update_panels();
			doupdate();

			break;

#ifdef KEY_PRINT
		case KEY_PRINT:
#endif
		case '':
			screen_save (IdentWin, logfile);
			touchwin (GetWin(IdentWin));
			wrefresh (GetWin(IdentWin));
			update_panels ();
			doupdate ();
			break;

		case KEY_F (1):		   /*  Help info  */
		case '?':
			info_screen (dpa_info1, dpa_info1_len, NULL);
			update_panels ();
			doupdate ();
			touchwin (GetWin(IdentWin));
			wrefresh (GetWin(IdentWin));
			break;

		case 'd':
		case 'D':

			print_node(&device_info[curr_device]);
			show_panel (GetPan(IdentWin));
			update_panels();
			doupdate();

			break;

		default:
			mvwprintw (GetWin(IdentWin), GetHeight(IdentWin) - 2,
			           GetWidth(IdentWin) - 16, "Invalid key");
			wrefresh (GetWin(IdentWin));
			sleep (1);
			mvwprintw (GetWin(IdentWin), GetHeight(IdentWin) - 2,
			           GetWidth(IdentWin) - 16, "           ");
			wrefresh (GetWin(IdentWin));
			break;
		}						   /* End Case */

		if (option != EOF)
		{
			wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));

			commandline (clbuf, helpstr, "ESC=Quit", "RETURN=Select",
			             updnstr, "^P=Print", "D=Display Product Info", NULL);
			mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
			wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
			wrefresh (GetWin(MainWin));
		}

	}							   /* End While Loop */
}
