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
 * $Id: channel_panel.c,v 1.20 2016/02/09 03:36:34 pberger Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */

# include <curses.h>

#ifdef AIX
# include "digi_panel.h"
#else
# include <panel.h>
#endif

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>

#include "dpa_rp.h"
#include "dpa_os.h"

#include "curses_helpers.h"
#include "general_panels.h"
#include "channel_panel.h"

#include "info_panel.h"
#include "loopback_panel.h"
#include "scope_panel.h"
#include "exam_panel.h"

#include "attribs.h"


/**********************************************************************
*
*  Routine Name:	print_channel
*
*  Function:	   display information about a specific port
*
**********************************************************************/

void 
print_channel(struct deviceinfo *unit)
{
	int i = 0, option = EOF, scr_opt = 0;

	int rc;
	fd_set rfds;
	struct timeval tv;
	int max_fd;

	struct digi_node node;
	struct digi_chan chan;
	int port = 0;
	char ttyname[100];


	int d_invokes_capture = 1;   /* 0 if 'D' should invoke "exam_panel" */
	                             /* 1 if 'D' should invoke "scope_panel" */


	WINDOW *chanwin = GetWin(ChanWin);

	/* 
	 * scr_opt is just used to determine which field to 
	 * highlight.
	 */

	show_panel (GetPan(ChanWin));
	update_panels ();
	doupdate ();

	DPAOpenDevice(unit);

	while ((option != 'Q') && (option != 'q') && (option != ESC))
	{

		/* Get port info for current port */
		DPAGetNode(unit, &node);
		DPAGetChannel(unit, &chan, port);

		if (DPAGetPortName(unit, &node, &chan, port, ttyname) == NULL) {
			ttyname[0] = '\0';
		}

		max_fd = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);

		mvwprintw (chanwin, 1, 2, "Device Description: %-54.54s", node.nd_ps_desc);

		mvwprintw (chanwin, 5, 35, "Name: %-10.10s", ttyname);

		wattrset (chanwin, make_attr (A_BOLD, WHITE, BLACK));

		mvwprintw (chanwin, 5, 56, "Status: %-10.10s",
		           chan.ch_open ? "Open" : "Closed");
		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));

		if (scr_opt == 1)
		{
			wattrset (chanwin, make_attr (A_REVERSE | A_STANDOUT, WHITE, BLACK));
			mvwprintw (chanwin, 3, 5, "Unit IP Address: %-15.15s", unit->host);
			wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
			mvwprintw (chanwin, 5, 8, "Port Number : %-2d", port + 1);
		}
		else if (scr_opt == 2)
		{
			wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
			mvwprintw (chanwin, 3, 5, "Unit IP Address: %-15.15s", unit->host);
			wattrset (chanwin, make_attr (A_REVERSE | A_STANDOUT, WHITE, BLACK));
			mvwprintw (chanwin, 5, 8, "Port Number : %-2d", port + 1);
			wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
		}
		else
		{
			mvwprintw (chanwin, 3, 5, "Unit IP Address: %-15.15s", unit->host);
			mvwprintw (chanwin, 5, 8, "Port Number : %-2d", port + 1);
		}

		if (!vanilla) wattrset (chanwin, make_attr (A_ALTCHARSET, GREEN, BLACK));
		else wattrset (chanwin, make_attr (A_NORMAL, GREEN, BLACK));

		for (i = 0; i < 62; i++)
			mvwaddch (chanwin, 6, 8 + i, mapchar(ACS_HLINE));
		mvwaddch (chanwin, 6, 7, mapchar(ACS_ULCORNER));
		mvwaddch (chanwin, 6, 70, mapchar(ACS_URCORNER));
		mvwaddch (chanwin, 7, 7, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 7, 70, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 8, 7, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 8, 70, mapchar(ACS_VLINE));

		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
		mvwprintw (chanwin, 12, 24, "Signal Active = ");
		wattrset (chanwin, make_attr (A_STANDOUT, WHITE, GREEN));
		waddstr (chanwin, "X");

		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
		mvwaddstr (chanwin, 12, 46, "Inactive =");
		wattrset (chanwin, make_attr (A_NORMAL, GREEN, BLACK));
		mvwaddstr (chanwin, 12, 57, "_");
		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));

		mvwaddstr (chanwin, 8, 13,
				   "Tx:             RTS  CTS  DSR  DCD  DTR  RI  OFC  IFC");
		mvwaddstr (chanwin, 9, 13,
				   "Rx:                                                  ");

		if (!vanilla)
			wattrset (chanwin, make_attr (A_ALTCHARSET, GREEN, BLACK));
		else
			wattrset (chanwin, make_attr (A_NORMAL, GREEN, BLACK));

		mvwaddch (chanwin, 9, 7, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 9, 70, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 10, 7, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 10, 70, mapchar(ACS_VLINE));
		mvwaddch (chanwin, 11, 7, mapchar(ACS_LLCORNER));
		mvwaddch (chanwin, 11, 70, mapchar(ACS_LRCORNER));

		for (i = 0; i < 62; i++)
			mvwaddch (chanwin, 11, 8 + i, mapchar(ACS_HLINE));

		wattrset (chanwin, make_attr (A_NORMAL, GREEN, BLACK));

		mvwprintw (chanwin, 8, 17, "%-10d", chan.ch_txcount);
		mvwprintw (chanwin, 9, 17, "%-10d", chan.ch_rxcount);

		{
			int msigs=6;
			int   mbits[] = { MS_RTS, MS_CTS, MS_DSR, MS_DCD, MS_DTR, MS_RI };
			char *mhstr[] = { "X",    "X",    "X",    "X",    "X",    "X"   };
			char *mlstr[] = { "_",    "_",    "_",    "_",    "_",    "_"   };
			int   mpos[]  = { 30,     35,     40,     45,     50,     55    };
			int sig=0;

			for(sig = 0; sig < msigs; sig++)
			{
				if (chan.ch_s_mstat & mbits[sig])
				{
					wattrset (chanwin, make_attr (A_STANDOUT, WHITE, GREEN));
					mvwprintw (chanwin, 9, mpos[sig], mhstr[sig]);
				}
				else
				{
					wattrset (chanwin, make_attr (A_NORMAL, GREEN, BLACK));
					mvwprintw (chanwin, 9, mpos[sig], mlstr[sig]);
				}
			}
		}
		{
			int events=2;
			int   ebits[] = { EV_OPALL, EV_IPALL };
			char *ehstr[] = { "X",      "X"      };
			char *elstr[] = { "_",      "_"      };
			int   epos[]  = { 59,       64       };
			int ev=0;

			for(ev=0; ev<events; ev++)
			{
				if (chan.ch_s_estat & ebits[ev])
				{
					wattrset (chanwin, make_attr (A_STANDOUT, WHITE, GREEN));
					mvwprintw (chanwin, 9, epos[ev], ehstr[ev]);
				}
				else
				{
					wattrset (chanwin, make_attr (A_NORMAL, GREEN, BLACK));
					mvwprintw (chanwin, 9, epos[ev], elstr[ev]);
				}
			}
		}

		wattrset (chanwin, make_attr (A_BOLD, WHITE, BLACK));
		mvwaddstr (chanwin, 14, 1, "  Input Modes ");
		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
		wprintw (chanwin, "                                                           ");
		wmove (chanwin, 14, 15);

		if (chan.ch_s_iflag & IF_IGNBRK)
			wprintw (chanwin, ":IGNBRK");
		if (chan.ch_s_iflag & IF_BRKINT)
			wprintw (chanwin, ":BRKINT");
		if (chan.ch_s_iflag & IF_IGNPAR)
			wprintw (chanwin, ":IGNPAR");
		if (chan.ch_s_iflag & IF_PARMRK)
			wprintw (chanwin, ":PARMRK");
		if (chan.ch_s_iflag & IF_INPCK)
			wprintw (chanwin, ":INPCK");
		if (chan.ch_s_iflag & IF_ISTRIP)
			wprintw (chanwin, ":ISTRIP");
		if (chan.ch_s_iflag & IF_IXON)
			wprintw (chanwin, ":IXON");
		if (chan.ch_s_iflag & IF_IXANY)
			wprintw (chanwin, ":IXANY");
		if (chan.ch_s_iflag & IF_IXOFF)
			wprintw (chanwin, ":IXOFF");
		if (chan.ch_s_xflag & XF_XIXON)
			wprintw (chanwin, ":IXONA");
		if (chan.ch_s_xflag & XF_XTOSS)
			wprintw (chanwin, ":ITOSS");
		if (chan.ch_s_iflag & IF_DOSMODE)
			wprintw (chanwin, ":DOSMODE");

		wprintw (chanwin, ":");
		wattrset (chanwin, make_attr (A_BOLD, WHITE, BLACK));
		mvwprintw (chanwin, 15, 1, " Output Modes ");
		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
		wprintw (chanwin, "                                                           ");
		wmove (chanwin, 15, 15);

		if (chan.ch_s_xflag & XF_XCASE)
			wprintw (chanwin, ":XCASE");
		if (chan.ch_s_oflag & OF_OLCUC)
			wprintw (chanwin, ":OLCUC");
		if (chan.ch_s_oflag & OF_ONLCR)
			wprintw (chanwin, ":ONLCR");
		if (chan.ch_s_oflag & OF_OCRNL)
			wprintw (chanwin, ":OCRNL");
		if (chan.ch_s_oflag & OF_ONOCR)
			wprintw (chanwin, ":ONOCR");
		if (chan.ch_s_oflag & OF_ONLRET)
			wprintw (chanwin, ":ONLRET");

		/* TODO -- tab expansion / TABDLY interpretation */

		wprintw (chanwin, ":");
		wattrset (chanwin, make_attr (A_BOLD, WHITE, BLACK));
		mvwprintw (chanwin, 16, 1, "Control Modes ");
		wattrset (chanwin, make_attr (A_NORMAL, WHITE, BLACK));
		/*
		 * clear the field, then write the new one
		 */
		wprintw (chanwin, "                                                              ");
		wmove (chanwin, 16, 15);

		if (chan.ch_open) {
/* Jira RP-77: In early stages of a port open, ch_open is set before ch_s_brate
 * is initialized, which, undetected, causes an arithmetic exception here.
 */
                	if (chan.ch_s_brate)
                		wprintw (chanwin, ":%d Baud", PDIVIDEND / chan.ch_s_brate);
			else
                		wprintw (chanwin, ":?? Baud");

			switch (chan.ch_s_cflag & CF_CSIZE)
			{
			case CF_CS5:
				wprintw (chanwin, ":5 Char Bits");
				break;
			case CF_CS6:
				wprintw (chanwin, ":6 Char Bits");
				break;
			case CF_CS7:
				wprintw (chanwin, ":7 Char Bits");
				break;
			case CF_CS8:
				wprintw (chanwin, ":8 Char Bits");
				break;
			default:
				wprintw (chanwin, ":No Char Bits");
				break;
			}

			if (chan.ch_s_cflag & CF_CSTOPB)
				wprintw (chanwin, ":2 Stop Bits");
			else
				wprintw (chanwin, ":1 Stop Bits");

			if (chan.ch_s_cflag & CF_PARENB)
			{
				if (chan.ch_s_xflag & XF_XPAR)
				{
					if (chan.ch_s_cflag & CF_PARODD)
						wprintw (chanwin, ":Parity Odd");
					else
						wprintw (chanwin, ":Parity Even");
				}
				else
				{
					if (chan.ch_s_cflag & CF_PARODD)
						wprintw (chanwin, ":Parity Space");
					else
						wprintw (chanwin, ":Parity Mark");
				}
			}
			else
			{
				wprintw (chanwin, ":No Parity");
			}
		}
		else {
                	wprintw (chanwin, ":");
		}

		wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));

		commandline (clbuf, helpstr, "ESC=Quit", lrudstr,
		             "T=LoopBack", "D=DataMonitor", "^P=Print", NULL);
		mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);

		wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		wrefresh (GetWin(MainWin));
		wrefresh (chanwin);

		change_term (0, TIMEOUT);

		option = EOF;

		tv.tv_sec = 0;  tv.tv_usec = 500000;
		rc = select( max_fd + 1, &rfds, NULL, NULL, &tv );

		if (rc < 0)
		{
			fprintf (stderr, "FATAL ERROR: select failure.\n");
			EndCurses (-13);
			/* FIXME: This code will not be executed as 
			   EndCurses() calls exit(). */
			exit (-2);
		}
		else if (rc > 0)
		{
			if (FD_ISSET(0, &rfds))
			{
				option = getch();
			}

		}
		else
		{
			scr_opt = 0;
		}

		/*
		 * If the user hasn't selected anything keep doing the 
		 * original screen. 
		 */

		switch (option)
		{
		case EOF:
			break;

		case '':
			scr_opt = 0;
			refresh_screen ();
			break;

#ifdef KEY_LEFT
		case KEY_LEFT:
#endif
		case 'H':
		case 'h':

			port--;
			if (port < 0)
				port = unit->nports - 1;

			scr_opt = 2;
			break;

#ifdef KEY_RIGHT
		case KEY_RIGHT:
#endif
		case 'L':
		case 'l':

			port++;
			if (port >= (int) unit->nports)
				port = 0;

			scr_opt = 2;
			break;

#ifdef KEY_UP
		case KEY_UP:
#endif
		case 'K':
		case 'k':
			{
				int curr_unit = unit->unit_number;
				do {
					curr_unit--;
					if (curr_unit < 0)
						curr_unit = num_devices - 1;
				}
				while (device_info[curr_unit].status != UP);

				unit = &device_info[curr_unit];

				if (port >= (int) unit->nports)
					port = unit->nports - 1;
			}

			scr_opt = 1;
			break;

#ifdef KEY_DOWN
		case KEY_DOWN:
#endif
		case 'J':
		case 'j':
			{
				int curr_unit = unit->unit_number;
				do {
					if (curr_unit < num_devices - 1)
						curr_unit++;
					else
						curr_unit = 0;
				}
				while (device_info[curr_unit].status != UP);

				unit = &device_info[curr_unit];

				if (port >= (int) unit->nports)
					port = unit->nports - 1;

			}

			scr_opt = 1;
			break;

		case 'q':
		case 'Q':
		case ESC:
			hide_panel (GetPan(ChanWin));
			erase_win (ChanWin);
			update_panels ();
			doupdate ();
			scr_opt = 0;
			break;

		case 'T':
		case 't':
			scr_opt = 0;

			hide_panel (GetPan(ChanWin));
			update_panels ();
			doupdate ();

			handle_loopback(unit, &node, &chan, port);

			change_term (0, TIMEOUT);

			show_panel (GetPan(ChanWin));
			update_panels ();
			doupdate ();
			wrefresh (chanwin);
			break;

		case 'D':
		case 'd':
			scr_opt = 0;

			{
				int invoke_one = 1;

				while (invoke_one)
				{
					if (d_invokes_capture)
					{
						invoke_one = handle_scope (unit, &node,  &chan, port);
					}
					else
					{
						invoke_one = handle_exam (unit, &node, port);
					}

					if (invoke_one)
						d_invokes_capture = 1 - d_invokes_capture;
				}
			}

			touchwin (chanwin);
			wrefresh (chanwin);
			update_panels ();
			doupdate ();
			break;


#ifdef KEY_PRINT
		case KEY_PRINT:
#endif
		case '':
			scr_opt = 0;

			screen_save (ChanWin, logfile);
			touchwin (chanwin);
			wrefresh (chanwin);
			update_panels ();
			doupdate ();
			break;

		case KEY_F (1):		   /*  Help info  */
		case '?':
			scr_opt = 0;
			info_screen (dpa_info2, dpa_info2_len, NULL);
			update_panels ();
			doupdate ();
			break;

		default:
			scr_opt = 0;
			mvwprintw (chanwin, 16, 60, "Invalid key");
			wrefresh (chanwin);
			sleep (1);
			mvwprintw (chanwin, 16, 60, "           ");
			wrefresh (chanwin);
			break;
		}						   /* End Case */
	}							   /* End While */
}
