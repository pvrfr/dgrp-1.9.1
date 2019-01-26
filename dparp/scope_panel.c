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
 * $Id: scope_panel.c,v 1.15 2005/08/15 15:57:29 scottk Exp $
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
#include <ctype.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>

#include "dpa_rp.h"
#include "dpa_os.h"

#include "curses_helpers.h"
#include "general_panels.h"
#include "scope_panel.h"
#include "info_panel.h"

#include "capture.h"

#include "attribs.h"


/************************************************************************
 * Decodes an unsigned 4-byte buffer quantity.
 ************************************************************************/

int decode_u4(uchar* buf)
{
#if LE
    return ((int) buf[0] +
            ((int) buf[1] << 8) +
            ((int) buf[2] << 16) +
            ((int) buf[3] << 24));
#else
    return (((int) buf[0] << 24) +
            ((int) buf[1] << 16) +
            ((int) buf[2] <<  8) +
            ((int) buf[3]      ));
#endif
}


int handle_dpa_read(struct deviceinfo *unit)
{
	int idx;
	int off;
	int bytes_to_move;
	CAPTURE_BUFFER *capbuf;
	char sizestr[10];
	char *buff, *ptr;
	char type;
	unsigned int size = 0;
	int n;
	int total;
	int attempts;

	n = read(unit->dpa_fd, &type, sizeof(char));

	if (n != 1) {
		return 0;
	}

	ptr = sizestr;
	total = 0;
	for (attempts = 0; attempts < 10; attempts++) {
		n = read(unit->dpa_fd, ptr + total, sizeof(unsigned int) - total);
		if (n > 0) {
			total += n;
		}
		if (total == sizeof(unsigned int)) {
			break;
		}
 	}

	if (attempts > 10) {
		printf("ERROR\n");
		exit(-1);
		return -1;
	}

	size = decode_u4((uchar *) sizestr);

	buff = (char *) malloc(size);
	ptr = buff;

	total = 0;
	for (attempts = 0; attempts < 10; attempts++) {
		n = read(unit->dpa_fd, ptr + total, size - total);
		if (n > 0) {
			total += n;
		}
		if (total >= (int) size) {
			break;
		}
 	}

	if (attempts > 10) {
		printf("ERROR\n");
		exit(-1);
		return -1;
	}

	bytes_to_move = size;

	capbuf = (type == 1) ? &rx_capture : &tx_capture;

	idx = bytes_to_move;
	off = 0;

	while (idx) {

		capbuf->buf[capbuf->tail] = buff[off];
		off++;

		capbuf->tail = (capbuf->tail + 1) & capbuf->mask;
		if (capbuf->tail == capbuf->head)
			capbuf->head = (capbuf->head + 1) & capbuf->mask;

		idx--;
	}

	free(buff);
	return 0;
}




/**********************************************************************
*
*  Routine Name:	handle_scope
*
*  Function:	   capture data, display "live" updates
*                 returns 0 if should return to original caller,
*                         1 if should switch to "other" (examine) handler
*
**********************************************************************/

int 
handle_scope (struct deviceinfo *unit, struct digi_node *node, struct digi_chan *chan, int port)
{
	int i = 0, j = 0, option = EOF;
	char ttyname[100];
	int rc;
	fd_set rfds;
	int max_fd;
	struct timeval tv;

	int retval = 0;

	int disp_lines;

	int rd_on = 1;
	int rd_lines = 0;
	int wr_on = 1;

   int hex_column = 0;
   int ascii_column = 0;

	CAPTURE_BUFFER *curbuf;	

	WINDOW *scopewin = GetWin(ScopeWin);

#define TITLE_LINE 0
#define DESC_LINE  1
#define SIG_LINE   2
#define SEP1_LINE  3
#define FIRST_DATA 4

	show_panel (GetPan(ScopeWin));
	update_panels ();
	doupdate ();

	rx_capture.head = rx_capture.tail = rx_capture.flags = 0;
	tx_capture.head = tx_capture.tail = tx_capture.flags = 0;

	disp_lines = GetHeight(ScopeWin) - (FIRST_DATA + 1);

	rd_on = wr_on = 1;
	rd_lines = ((disp_lines - 1) / 2) + ((disp_lines - 1) % 2);
	
	hex_column = 6;
	ascii_column = GetWidth(ScopeWin) - 22;

	DPASetDebugForPort(unit, 1, port);

	while ((option != 'Q') && (option != 'q') && (option != ESC))
	{
		int show_lines;
		int bytes_on_screen;
		int screen_pos;
		int start_line;
		int end_line;
		int truncated;
		int calc_bpos;

		max_fd = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);

		if (unit->dpa_fd != -1)
		{
			FD_SET(unit->dpa_fd, &rfds);
			max_fd = unit->dpa_fd;
		}
		else
		{
			retval = 1;
			break;
		}

		erase_win (ScopeWin);

		wattrset (scopewin, make_attr (A_NORMAL, WHITE, BLACK));
		sprintf (clbuf, " Monitor Port Traffic ");
		mvwprintw (scopewin, TITLE_LINE,
		           center (ScopeWin, strlen (clbuf)), clbuf);


		if (!vanilla)
			wattrset (scopewin, make_attr (A_ALTCHARSET, CYAN, BLACK));
		else
			wattrset (scopewin, make_attr (A_NORMAL, CYAN, BLACK));

		for (i = FIRST_DATA; i < (GetHeight(ScopeWin) - 2); i++)
		{
			mvwaddch (scopewin, i, 0, mapchar(ACS_VLINE));
			mvwaddch (scopewin, i, GetWidth(ScopeWin) - 1, mapchar(ACS_VLINE));
		}

		for (i = 0; i < (GetWidth(ScopeWin) - 2); i++)
			mvwaddch (scopewin, SEP1_LINE, 1 + i, mapchar(ACS_HLINE));
		mvwaddch (scopewin, SEP1_LINE, 0, mapchar(ACS_LTEE));
		mvwaddch (scopewin, SEP1_LINE,
		          (GetWidth(ScopeWin) - 1), mapchar(ACS_RTEE));

		if (rd_on && wr_on)
		{
			for (i = 0; i < (GetWidth(ScopeWin) - 2); i++)
				mvwaddch (scopewin, FIRST_DATA + rd_lines, 1 + i, mapchar(ACS_HLINE));
			mvwaddch (scopewin, FIRST_DATA + rd_lines, 0, mapchar(ACS_LTEE));
			mvwaddch (scopewin, FIRST_DATA + rd_lines,
			          (GetWidth(ScopeWin) - 1), mapchar(ACS_RTEE));
		}

		wattrset (scopewin, make_attr (A_NORMAL, CYAN, BLACK));

		if (rd_on && !wr_on)
		{
			sprintf (clbuf, " Rx Data ");
			mvwprintw (scopewin, SEP1_LINE, center (ScopeWin, strlen (clbuf)), clbuf);
		}
		else if (wr_on && !rd_on)
		{
			sprintf (clbuf, " Tx Data ");
			mvwprintw (scopewin, SEP1_LINE, center (ScopeWin, strlen (clbuf)), clbuf);
		}
		else
		{
			sprintf (clbuf, " Rx Data ");
			mvwprintw (scopewin, SEP1_LINE, center (ScopeWin, strlen (clbuf)), clbuf);
			sprintf (clbuf, " Tx Data ");
			mvwprintw (scopewin, FIRST_DATA + rd_lines,
			           center (ScopeWin, strlen (clbuf)), clbuf);
		}


		for (curbuf = &rx_capture;
		     curbuf;
		     curbuf = ( curbuf == &tx_capture ) ? NULL : &tx_capture)
		{
			if ((curbuf == &rx_capture && rd_on) ||
			    (curbuf == &tx_capture && wr_on))
			{
				if (curbuf == &rx_capture)
				{
					screen_pos = FIRST_DATA;
					show_lines = wr_on ? rd_lines : disp_lines;
					bytes_on_screen = show_lines * 16;
				}
				else
				{
					screen_pos = rd_on ? FIRST_DATA + rd_lines + 1 : FIRST_DATA;
					show_lines = rd_on ? disp_lines - rd_lines - 1 : disp_lines;
					bytes_on_screen = show_lines * 16;
				}
	
				if (((curbuf->head - curbuf->tail - 1) & curbuf->mask) != 0)
				{
					truncated = 1;
				}
				else
				{
					truncated = 0;
				}

				start_line = ((curbuf->tail / 16) - (show_lines - 1)) & (curbuf->mask >> 4);
				end_line   = (curbuf->tail / 16);

				if (truncated && (end_line < start_line))
					start_line = 0;
				
				calc_bpos = start_line * 16;
	
				for (i=0; i <= (end_line - start_line); i++)
				{
					for (j=0; j < 16; j++)
					{
						unsigned char currch = 0;
	
						if (truncated && ((unsigned)calc_bpos > curbuf->tail))
							continue;
	
						if ((unsigned)calc_bpos == curbuf->tail)
						{
							wattrset (scopewin, make_attr (A_REVERSE | A_STANDOUT, GREEN, BLACK));
	
							mvwprintw (scopewin, screen_pos + i,
							           hex_column + (j * 3), "__", currch);
							mvwprintw (scopewin, screen_pos + i,
							           ascii_column + j, "_", currch);
						}
						else
						{
							wattrset (scopewin, make_attr (A_NORMAL, GREEN, BLACK));
							currch = curbuf->buf[calc_bpos];
	
							mvwprintw (scopewin, screen_pos + i,
							           hex_column + (j * 3), "%02X", currch);
							if (isprint (currch))
								mvwprintw (scopewin, screen_pos + i,
								           ascii_column + j, "%c", currch);
							else
								mvwprintw (scopewin, screen_pos + i,
								           ascii_column + j, ".", currch);
						}
	
						calc_bpos = (calc_bpos + 1) & curbuf->mask;
					}
				}
			}
		}

		wattrset (scopewin, make_attr (A_NORMAL, GREEN, BLACK));

		/* Get port info for current port */
		DPAGetChannel(unit, chan, port);

		if (DPAGetPortName(unit, node, chan, port, ttyname) == NULL) {
			ttyname[0] = '\0';
		}

		/* Now display the port identification information */
		sprintf (clbuf, "IP Address: %-23.23s Port #: %d           Name: %s",
		         unit->host, port + 1, ttyname);
		i = strlen (clbuf);

		wattrset (GetWin(ScopeWin), make_attr (A_BOLD, WHITE, BLUE));
		mvwprintw (GetWin(ScopeWin), DESC_LINE, 1, "%*s",
		           GetWidth(ScopeWin) - 2, " ");

		mvwprintw (scopewin, DESC_LINE, 2, clbuf);

		wattrset (scopewin, make_attr (A_NORMAL, GREEN, BLACK));

		/* Now display modem signals */
		/* RTS  CTS  DSR  DCD  DTR  RI  OFC  IFC */
		/* 0    5    10   15   20   25  29   34  */
		/* starting at position 20 */
		{
			int msigs=6;
			int   mbits[] = { MS_RTS, MS_CTS, MS_DSR, MS_DCD, MS_DTR, MS_RI };
			char *mhstr[] = { "RTS",  "CTS",  "DSR",  "DCD",  "DTR",  "RI"  };
			char *mlstr[] = { "rts",  "cts",  "dsr",  "dcd",  "dtr",  "ri"  };
			int   mpos[]  = { 20,     25,     30,     35,     40,     45    };
			int sig=0;

			for(sig=0; sig<msigs; sig++)
			{
				if (chan->ch_s_mstat & mbits[sig])
				{
					wattrset (scopewin, make_attr (A_STANDOUT, WHITE, GREEN));
					mvwprintw (scopewin, SIG_LINE, mpos[sig], mhstr[sig]);
				}
				else
				{
					wattrset (scopewin, make_attr (A_NORMAL, GREEN, BLACK));
					mvwprintw (scopewin, SIG_LINE, mpos[sig], mlstr[sig]);
				}
			}
		}
		{
			int events=2;
			int   ebits[] = { EV_OPALL, EV_IPALL };
			char *ehstr[] = { "OFC",    "IFC"    };
			char *elstr[] = { "ofc",    "ifc"    };
			int   epos[]  = { 49,       54       };
			int ev=0;

			for(ev=0; ev<events; ev++)
			{
				if (chan->ch_s_estat & ebits[ev])
				{
					wattrset (scopewin, make_attr (A_STANDOUT, WHITE, GREEN));
					mvwprintw (scopewin, SIG_LINE, epos[ev], ehstr[ev]);
				}
				else
				{
					wattrset (scopewin, make_attr (A_NORMAL, GREEN, BLACK));
					mvwprintw (scopewin, SIG_LINE, epos[ev], elstr[ev]);
				}
			}
		}


		wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));

		commandline (clbuf, "E=Examine", "+/-=Resize",
		             (wr_on) ? "R=Rx Only" : "T=Tx Only",
		             (wr_on && rd_on) ? "T=Tx Only" : "B=Both",
		             "ESC=Quit", NULL);
		mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);

		wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		wrefresh (GetWin(MainWin));
		wrefresh (scopewin);

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

			if ((unit->dpa_fd >= 0) &&
			    FD_ISSET(unit->dpa_fd, &rfds))
			{
				handle_dpa_read(unit);
			}
		}
		else {
			continue;
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
			refresh_screen ();
			break;

		case 'E':
		case 'e':
			retval = 1;
			option = 'q';
			break;

		case 'T':
		case 't':
			wr_on = 1;
			rd_on = 0;
			break;

		case 'R':
		case 'r':
			wr_on = 0;
			rd_on = 1;
			break;

		case 'B':
		case 'b':
			wr_on = 1;
			rd_on = 1;
			break;

		case '+':
			if (!(wr_on && rd_on)) break;

			if( rd_lines < (disp_lines - 2) ) rd_lines++;
		
			break;

		case '-':
			if (!(wr_on && rd_on)) break;

			if( rd_lines > 1 ) rd_lines--;
		
			break;

#ifdef KEY_PRINT
		case KEY_PRINT:
#endif
		case '':
			screen_save (ScopeWin, logfile);
			touchwin (scopewin);
			wrefresh (scopewin);
			update_panels ();
			doupdate ();
			break;

		case KEY_F (1):		   /*  Help info  */
		case '?':
			info_screen (dpa_info3, dpa_info3_len, NULL);
			update_panels ();
			doupdate ();
			break;

		case 'q':
		case 'Q':
		case ESC:
			break;

		default:
			mvwprintw (scopewin, GetHeight(ScopeWin) - 2, 60, "Invalid key");
			wrefresh (scopewin);
			sleep (1);
			break;
		}						   /* End Case */
	}							   /* End While */

	/* Turn off debugging for port */
	DPASetDebugForPort(unit, 0, 0);

	hide_panel (GetPan(ScopeWin));
	erase_win (ScopeWin);
	update_panels ();
	doupdate ();

	return retval;
}
