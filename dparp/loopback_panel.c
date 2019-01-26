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
 * $Id: loopback_panel.c,v 1.21 2005/07/31 07:19:56 scottk Exp $
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
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>

#include <termios.h>

#include "dpa_rp.h"
#include "dpa_os.h"

#include "curses_helpers.h"
#include "general_panels.h"
#include "loopback_panel.h"


#include "attribs.h"


static int next_result = 0;
static int test_cases = 0;
static int test_passes = 0;


int
tty_open(char *name, struct termios *sv_tios)
{
        int ret;
	int fd;
        struct termios tios;
        
	if (access (name, F_OK) || (fd = open(name, O_RDWR | O_NONBLOCK) ) < 0) {
		return -1;
	}

	/* get stty settings */
	if ((ret = tcgetattr(fd, &tios)) < 0 ) {
		close(fd);
		return -1;
	}
        
	*sv_tios = tios;        /* Save a copy so we can restore settings later */

	/* default values */
	tios.c_iflag &= ~(IXON|IXOFF|INLCR|ICRNL);
	tios.c_oflag &= ~(OPOST|ONLCR|OCRNL);
	tios.c_oflag |= ONLRET;
	tios.c_cflag &= ~(CBAUD|CSIZE|CSTOPB|PARENB);
	tios.c_cflag |= CLOCAL|CREAD|B9600|CS8;
        tios.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHOKE|ECHONL|ECHOCTL|ISIG|IEXTEN);

	/* Make reads return immediately with data or after 1 second without */
	tios.c_cc[VMIN] = 0;
	tios.c_cc[VTIME] = 10;  /* 10ths of second */

	/* set stty settings */
	if ((ret = tcsetattr(fd, TCSAFLUSH, &tios)) < 0 ) {
		return -1;
	}

	return fd;
}


void
tty_close(int fd, struct termios *sv_tios)
{
	tcsetattr(fd, TCSAFLUSH, sv_tios);
	close(fd);
}


/**********************************************************************
*
*  Routine Name:        neo_test_send
*
*  Function:    Send test data on the specified port with normal system
*               writes.
*
**********************************************************************/
int
test_send(char *test_data, int len, int fd)
{
	int ret;
	ret = write(fd, test_data, len);
	return 0;
}


/**********************************************************************
*
*  Routine Name:        neo_test_recv
*               
*  Function:    Receive test data on the specified port with normal
*               system reads
*  
**********************************************************************/
int
test_recv(char *dt, int mx_ln, int wt_sec, int fd)
{
	time_t tloc, elapsed;
	int nbytes, ret;

	time( &tloc );
	nbytes = 0;
	do {
		ret = read(fd, (char *) (dt + nbytes), mx_ln-nbytes);

		if (ret > 0) {
			nbytes += ret;
		}
	} while ( nbytes < mx_ln && ((elapsed = (time(NULL) - tloc)) < wt_sec));

	*(dt + nbytes) = '\0';

	return nbytes;
}



#define TBUFSIZ 256



/**********************************************************************
*
*  Routine Name:	handle_loopback
*
*  Function:	   execute a loopback test for port
*
**********************************************************************/

void 
handle_loopback (struct deviceinfo *unit, struct digi_node *node, struct digi_chan *chan, int port)
{
	char full_line[81];

	int i = 0, option = EOF;

	struct termios sv_tios;
	char test_data[TBUFSIZ + 1];
	char read_data[TBUFSIZ + 1];
	char string[200], ttyname[200];

	int r = 3;
	int rwfd;

	WINDOW *lbwin = GetWin(LBWin);

#define TITLE_LINE  1
#define DESC_LINE   2
#define DESC2_LINE  3
#define SEP_LINE    4
#define FIRST_DATA  5
#define SEP2_LINE   15
#define RESULT_LINE 16

	show_panel (GetPan(LBWin));
	update_panels ();
	doupdate ();

	next_result = 0;
	test_cases = 0;
	test_passes = 0;

	if (DPAGetPortName(unit, node, chan, port, ttyname) == NULL) {
		ttyname[0] ='\0';
	}

	while (option == EOF)
	{
		erase_win (LBWin);
		wattrset (lbwin, make_attr (A_BOLD, WHITE, BLUE));
		mvwprintw (lbwin, TITLE_LINE,  1, "%-*.*s", 76, 76, " ");
		mvwprintw (lbwin, RESULT_LINE, 1, "%-*.*s", 76, 76, " ");
		mvwprintw (lbwin, TITLE_LINE, 32, " Loop Back Test ");
		sprintf (full_line, "Tests Executed: %-12d Passed: %-12d Failed: %d",
		         test_cases, test_passes, test_cases - test_passes);
		mvwprintw (lbwin, RESULT_LINE,
		           center(LBWin, strlen(full_line)), full_line);

		sprintf (clbuf, "Unit IP Address: %s       Port #: %d            Name: %s",
		         unit->host, port + 1, ttyname);
		i = strlen (clbuf);
		mvwprintw (GetWin(LBWin), DESC_LINE, 1, "%*s",
		           GetWidth(LBWin) - 2, " ");

		mvwprintw (GetWin(LBWin), DESC2_LINE, 1, "%*s",
		           GetWidth(LBWin) - 2, " ");

		mvwprintw (lbwin, DESC2_LINE, 2, clbuf);

		mvwprintw (lbwin, DESC_LINE, 2, "Device Description: %-*.*s",
		           GetWidth(LBWin) - 2 - 2 - 2 - 20,
		           GetWidth(LBWin) - 2 - 2 - 2 - 20,
		           node->nd_ps_desc);

		if (!vanilla)
			wattrset (lbwin, make_attr (A_ALTCHARSET, CYAN, BLACK));
		else
			wattrset (lbwin, make_attr (A_NORMAL, CYAN, BLACK));

		wmove (lbwin, SEP_LINE, 1);
		for (i = 0; i < 77; i++)
			waddch (lbwin, mapchar(ACS_HLINE));
		mvwaddch (lbwin, SEP_LINE, 0, mapchar(ACS_LTEE));
		mvwaddch (lbwin, SEP_LINE, 77, mapchar(ACS_RTEE));

		wmove (lbwin, SEP2_LINE, 1);
		for (i = 0; i < 77; i++)
			waddch (lbwin, mapchar(ACS_HLINE));
		mvwaddch (lbwin, SEP2_LINE, 0, mapchar(ACS_LTEE));
		mvwaddch (lbwin, SEP2_LINE, 77, mapchar(ACS_RTEE));

		wattrset (lbwin, make_attr (A_NORMAL, WHITE, BLACK));

		wrefresh (lbwin);

		wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		commandline (clbuf, "Press ANY key to Halt the test", NULL);
		mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);
		wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		wrefresh (GetWin(MainWin));

		change_term (0, 10);

		option = EOF;
		r = 5;

		if (chan->ch_open) {
			mvwprintw (lbwin, r++, 2, "***** Port is Busy.");
                        wrefresh(lbwin);
			test_cases++;
			sleep(1);
			goto user_input;
		}

		for (i = 0; i < 256; i++) {
			test_data[i] = (char) i;
		}

		test_data[TBUFSIZ]='\0';

		/* Get port name.  Can't run the test without it. */
		if (DPAGetPortName(unit, node, chan, port, ttyname) == NULL) {
                        mvwprintw (lbwin, r++, 2,
                        "***** Loop Back Test Failure. Port has no known tty name");
			test_cases++;
                        wrefresh (lbwin);
			sleep(1);
			goto user_input;
                }

		sprintf(string, "/dev/%s", ttyname);

		if( (rwfd = tty_open(string, &sv_tios )) < 0 ) {
			test_cases++;
			goto user_input;
		}

		tcflush(rwfd, TCIOFLUSH);

                if ((i = test_send (test_data, TBUFSIZ, rwfd)) != 0)
                {
                        mvwprintw (lbwin, r++, 2,
                        "***** Loop Back Test Failure=%d, Sending %d Bytes", i, TBUFSIZ);
                        wrefresh (lbwin);
                        tty_close (rwfd, &sv_tios);
                        test_cases++;
			goto user_input;
                }

                mvwprintw (lbwin, r++, 2, "Loop Back: %d Bytes Sent.", TBUFSIZ);
                wrefresh (lbwin);
                mvwprintw (lbwin, r++, 2, "Loop Back: Receiving %d Bytes.", TBUFSIZ);
                wrefresh (lbwin);

                if ((i = test_recv (read_data, TBUFSIZ, 5, rwfd)) != TBUFSIZ)
                {
                        mvwprintw (lbwin, r++, 2,
                                "***** Loop Back Failure=%d Receiving %d bytes.", i, TBUFSIZ);
                        wrefresh (lbwin);  
                        tty_close (rwfd, &sv_tios);
                        test_cases++;
			goto user_input;
                }


                /* Reset termios as before and close channel */
                tty_close (rwfd, &sv_tios);

                mvwprintw (lbwin, r++, 2, "Loop Back: Verifying %d bytes.", TBUFSIZ);
                wrefresh (lbwin);

                if (memcmp (test_data, read_data, TBUFSIZ))
                {
                        mvwprintw (lbwin, r++, 2,
                                "***** Loop Back Failure Verifying %d Bytes.", TBUFSIZ);
                        mvwprintw (lbwin, r++, 2, "***** Data Incorrectly Transferred.");
                        wrefresh (lbwin);
                        test_cases++;
			goto user_input;
                }
                else
                {
                        mvwprintw (lbwin, r++, 2, "Loop Back: Test Passed.");
                        wrefresh (lbwin);
                        test_cases++;
                        test_passes++;
                }


user_input:

		option = getch();


		/*
		 * If the user hasn't selected anything, loop.
		 * Otherwise, break.
		 */

		switch (option)
		{
		case EOF:
			break;

		case '':
			refresh_screen ();
			option = EOF;
			break;

#ifdef KEY_PRINT
		case KEY_PRINT:
#endif
		case '':
			screen_save (LBWin, logfile);
			touchwin (lbwin);
			wrefresh (lbwin);
			update_panels ();
			doupdate ();
			option = EOF;
			break;

		default:
			break;
		}						   /* End Case */
	}							   /* End While */

	hide_panel (GetPan(LBWin));
	update_panels ();
	doupdate ();
	return;
}
