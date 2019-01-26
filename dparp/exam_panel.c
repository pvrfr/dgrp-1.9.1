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
 * $Id: exam_panel.c,v 1.10 2005/07/31 07:19:56 scottk Exp $
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

#include "dpa_rp.h"
#include "curses_helpers.h"
#include "general_panels.h"
#include "exam_panel.h"
#include "info_panel.h"
#include "capture.h"

#include "attribs.h"


/**********************************************************************
*
*  Routine Name:	handle_exam
*
*  Function:	   display capture buffers
*                 returns 0 if should return to original caller,
*                         1 if should switch to "other" (capture) handler
*
**********************************************************************/

int 
handle_exam(struct deviceinfo *unit, struct digi_node *node, int port)
{
	int i = 0, j = 0, option = EOF;

	int rc;
	fd_set rfds;
	int max_fd;

	int retval = 0;

#define RD 0
#define WR 1
#define MAX_BPOS(cbp) (((cbp)->tail - (cbp)->head) & (cbp)->mask)
#define PICK_CH(cbp, idx) ((cbp)->buf[((cbp)->head + (idx)) & (cbp)->mask])

	CAPTURE_BUFFER *cbufs[2];
	int display_fp[2];
	int display_bpos[2];
	int num_paragraphs[2];

	int tmp_paragraph;

	int disp_lines;

	int cdir = RD;

	int bpos_maxwidth = 0;
	int hex_column = 0;
	int ascii_column = 0;

	WINDOW *examwin = GetWin(ExamWin);

#define TOP_ROW    0
#define DESC_LINE  1
#define COUNT_LINE 2
#define SEP_LINE   3
#define FIRST_DATA 4

	show_panel (GetPan(ExamWin));
	update_panels ();
	doupdate ();

	cbufs[RD] = &rx_capture;
	display_fp[RD] = 0;   /* Display first paragraph */
	display_bpos[RD] = 0;
	num_paragraphs[RD] = (MAX_BPOS(cbufs[RD]) / 16) +
	                     ((MAX_BPOS(cbufs[RD]) % 16) ? 1 : 0);

	cbufs[WR] = &tx_capture;
	display_fp[WR] = 0;   /* Display first paragraph */
	display_bpos[WR] = 0;
	num_paragraphs[WR] = (MAX_BPOS(cbufs[WR]) / 16) +
	                     ((MAX_BPOS(cbufs[WR]) % 16) ? 1 : 0);

	disp_lines = GetHeight(ExamWin) - (FIRST_DATA + 1);

	
	for (i=0; i < 2; i++)
	{
		char tmpbuf[14];

		sprintf (tmpbuf, "%lx", MAX_BPOS(cbufs[i]) );
		if (strlen (tmpbuf) > (unsigned)bpos_maxwidth)
			bpos_maxwidth = strlen (tmpbuf);
	}
	hex_column = bpos_maxwidth + 4;
	ascii_column = GetWidth(ExamWin) - 18;


	while ((option != 'Q') && (option != 'q') && (option != ESC))
	{
		int calc_bpos;

		max_fd = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);

		erase_win (ExamWin);

		wattrset (examwin, make_attr (A_NORMAL, WHITE, BLACK));
		sprintf (clbuf, " Examine %s Data ",
		         (cdir == RD) ? "Received" : "Transmit");
		mvwprintw (examwin, TOP_ROW, center (ExamWin, strlen (clbuf)), clbuf);


		if (!vanilla)
			wattrset (examwin, make_attr (A_ALTCHARSET, CYAN, BLACK));
		else
			wattrset (examwin, make_attr (A_NORMAL, CYAN, BLACK));

		for (i = 0; i < (GetWidth(ExamWin) - 2); i++)
			mvwaddch (examwin, SEP_LINE, 1 + i, mapchar(ACS_HLINE));
		mvwaddch (examwin, SEP_LINE, 0, mapchar(ACS_LTEE));
		mvwaddch (examwin, SEP_LINE, (GetWidth(ExamWin) - 1), mapchar(ACS_RTEE));


		calc_bpos = display_fp[cdir] * 16;

		for (i=0; i < disp_lines; i++)
		{
			if ((display_fp[cdir] + i) >= num_paragraphs[cdir])
				continue;

			wattrset (examwin, make_attr (A_NORMAL, GREEN, BLACK));
			mvwprintw (examwin, FIRST_DATA + i, 2, "%0*X:",
			           bpos_maxwidth, calc_bpos);

			for (j=0; j < 16; j++)
			{
				int attribute;
				unsigned char currch;

				if ((unsigned)calc_bpos >= MAX_BPOS(cbufs[cdir]))
					continue;

				if (calc_bpos == display_bpos[cdir])
					attribute = make_attr (A_REVERSE | A_STANDOUT, GREEN, BLACK);
				else
					attribute = make_attr (A_NORMAL, GREEN, BLACK);

				currch = PICK_CH(cbufs[cdir], calc_bpos);

				wattrset (examwin, attribute);
				mvwprintw (examwin, FIRST_DATA + i,
				           hex_column + (j * 3), "%02X", currch);
				if (isprint (currch))
					mvwprintw (examwin, FIRST_DATA + i,
					           ascii_column + j, "%c", currch);
				else
					mvwprintw (examwin, FIRST_DATA + i,
					           ascii_column + j, ".", currch);

				calc_bpos++;
			}
		}

		wattrset (examwin, make_attr (A_NORMAL, GREEN, BLACK));

		sprintf (clbuf, "IP Addr: %s  Port #: %d",
		         unit->host, port + 1);
		i = strlen (clbuf);
		wattrset (GetWin(ExamWin), make_attr (A_BOLD, WHITE, BLUE));
		mvwprintw (GetWin(ExamWin), DESC_LINE, 1, "%*s",
		           GetWidth(ExamWin) - 2, " ");
		mvwprintw (examwin, DESC_LINE, GetWidth(ExamWin) - i - 2, clbuf);
		mvwprintw (examwin, DESC_LINE, 2, "Desc: %-*.*s",
		           GetWidth(ExamWin) - i - 2 - 2 - 2 - 6,
		           GetWidth(ExamWin) - i - 2 - 2 - 2 - 6,
		           node->nd_ps_desc);

		wattrset (examwin, make_attr (A_NORMAL, GREEN, BLACK));

		sprintf (clbuf, "Total Captured: %6ld        Byte Position: %6d",
		         MAX_BPOS(cbufs[cdir]), display_bpos[cdir] + 1);
		mvwprintw (examwin, COUNT_LINE, center (ExamWin, strlen (clbuf)), clbuf);


		wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));

		commandline (clbuf, "C=Capture", lrudstr,
		             pgdnstr, pgupstr,
		             (cdir == RD) ? "T=Tx Buffer" : "R=Rx Buffer",
		             "ESC=Quit", NULL);
		mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);

		wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		wrefresh (GetWin(MainWin));
		wrefresh (examwin);

		change_term (0, TIMEOUT);

		option = EOF;

		rc = select( max_fd + 1, &rfds, NULL, NULL, NULL );

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

#ifdef KEY_LEFT
		case KEY_LEFT:
#endif
		case 'H':
		case 'h':
			if (display_bpos[cdir] > 0)
				display_bpos[cdir]--;

			tmp_paragraph = display_bpos[cdir] / 16;

			if (tmp_paragraph < display_fp[cdir])
				display_fp[cdir]--;
			break;

#ifdef KEY_RIGHT
		case KEY_RIGHT:
#endif
		case 'L':
		case 'l':
			if ((unsigned)display_bpos[cdir] < (MAX_BPOS(cbufs[cdir]) - 1))
				display_bpos[cdir]++;

			tmp_paragraph = display_bpos[cdir] / 16;

			if (tmp_paragraph >= (display_fp[cdir] + disp_lines))
				display_fp[cdir]++;
			break;

#ifdef KEY_UP
		case KEY_UP:
#endif
		case 'K':
		case 'k':
			if (display_bpos[cdir] > 15)
				display_bpos[cdir] -= 16;

			tmp_paragraph = display_bpos[cdir] / 16;

			if (tmp_paragraph < display_fp[cdir])
				display_fp[cdir]--;
			break;

#ifdef KEY_DOWN
		case KEY_DOWN:
#endif
		case 'J':
		case 'j':
			if ((unsigned)display_bpos[cdir] < (MAX_BPOS(cbufs[cdir]) - 16))
				display_bpos[cdir] += 16;

			tmp_paragraph = display_bpos[cdir] / 16;

			if (tmp_paragraph >= (display_fp[cdir] + disp_lines))
				display_fp[cdir]++;
			break;

#ifdef KEY_NPAGE
		case KEY_NPAGE:
#endif
		case 'N':
		case 'n':
			if ((display_fp[cdir] + disp_lines) < num_paragraphs[cdir])
			{
				display_bpos[cdir] += (disp_lines * 16);
				display_fp[cdir] += disp_lines;

				if ((unsigned)display_bpos[cdir] >= MAX_BPOS(cbufs[cdir]))
					display_bpos[cdir] = MAX_BPOS(cbufs[cdir]) - 1;
			}
			break;

#ifdef KEY_PPAGE
		case KEY_PPAGE:
#endif
		case 'P':
		case 'p':
			if (display_fp[cdir] > 0)
			{
				display_bpos[cdir] -= (disp_lines * 16);
				display_fp[cdir] -= disp_lines;

				if (display_bpos[cdir] < 0)
					display_bpos[cdir] = 0;

				if (display_fp[cdir] < 0)
					display_fp[cdir] = 0;
			}
			break;

		case 'C':
		case 'c':
			retval = 1;
			option = 'q';
			break;

		case 'T':
		case 't':
			if (cdir == RD) cdir = WR;
			break;

		case 'R':
		case 'r':
			if (cdir == WR) cdir = RD;
			break;

#ifdef KEY_PRINT
		case KEY_PRINT:
#endif
		case '':
			screen_save (ExamWin, logfile);
			touchwin (examwin);
			wrefresh (examwin);
			update_panels ();
			doupdate ();
			break;

		case KEY_F (1):		   /*  Help info  */
		case '?':
			info_screen (dpa_info4, dpa_info4_len, NULL);
			update_panels ();
			doupdate ();
			break;

		case 'q':
		case 'Q':
		case ESC:
			break;

		default:
			mvwprintw (examwin, GetHeight(ExamWin) - 2, 60, "Invalid key");
			wrefresh (examwin);
			sleep (1);
			break;
		}						   /* End Case */
	}							   /* End While */

	hide_panel (GetPan(ExamWin));
	erase_win (ExamWin);
	update_panels ();
	doupdate ();

	return retval;
}
