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
 * $Id: main.c,v 1.11 2006/08/11 18:33:04 scottk Exp $
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
#include <string.h>
#include <sys/types.h>
#include <termios.h> 
#include <unistd.h>

#include <signal.h>

#ifdef CYGWIN
#include <getopt.h>
#endif

#include "dpa_rp.h"
#include "dpa_os.h"

#include "curses_helpers.h"
#include "general_panels.h"

#include "identify_panel.h"

#include "capture.h"


int debug = 0;		/* write out to debug file	*/
FILE *dfp;		/* debug file pointer		*/
char logfile[256];	/* Log File name		*/

int num_devices = 0;
struct deviceinfo device_info[MAX_PORTSERVERS];


CAPTURE_BUFFER rx_capture;
CAPTURE_BUFFER tx_capture;


/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

static void usage(void)
{
	printf (" usage: dpa_ip [-f logfile] [-v] [-b capture_buffer_size]\n");
	if(debug)
		fprintf(dfp, "dpa_ip - exiting abnomally #3\n");
	exit (1);
}

/**********************************************************************
*
*  Routine Name:	
*
*  Function:	
*
**********************************************************************/

int 
main (int argc, char **argv)
{
	int c;
	extern char *optarg;


	int copyright_msg = TRUE;

	unsigned long bufsize_req = CAPTURE_DEFAULT_SIZE;

	strcpy (logfile, "/tmp/dpalog");

	if (getuid () != 0)
	{
		fprintf (stderr, "\nYou must be a super-user to execute %s!\n\n", argv[0]);
		exit (1);
	}


	while ((c = getopt (argc, argv, "b:f:l:dm?v")) != -1)
	{
		switch (c)
		{
		case 'b':
			bufsize_req = strtoul(optarg, NULL, 0);
			break;
		case 'f':
		case 'l':
			strcpy (logfile, optarg);
			break;
		case 'm':
			copyright_msg = FALSE;
			break;
		case 'd':
			dfp = fopen ("/tmp/dpadebug", "w");
			debug = (dfp != NULL) ? TRUE : FALSE;
			break;
		case '?':
			usage ();
			break;
		case 'v':
			vanilla = TRUE;
			break;
		default:
			break;
		}					   /* End Case */
	}						   /* End While */


	/*
	 * Initialize capture buffers
	 */
	memset( &rx_capture, 0, sizeof( rx_capture ) );
	rx_capture.size = 1;
	while( rx_capture.size < bufsize_req ) rx_capture.size <<= 1;
	rx_capture.mask = rx_capture.size - 1;
	memcpy( &tx_capture, &rx_capture, sizeof( tx_capture ) );

	rx_capture.buf = (char *)malloc( rx_capture.size );
	tx_capture.buf = (char *)malloc( tx_capture.size );

	if( bufsize_req != rx_capture.size )
	{
		fprintf (stderr, "NOTE: capture buffers of size %ld were requested, %ld will be used.\n",
		         bufsize_req, rx_capture.size);
		sleep (1);
	}

	if( !rx_capture.buf || !tx_capture.buf )
	{
		fprintf (stderr, "Couldn't allocate capture buffers of size %ld\n", (long)rx_capture.buf);
		exit (1);
	}
	

	/*
	 * Call the OS specific way of getting the configuration.
	 */
	if (DPAFindDevices()) {
		fprintf(stderr, "Unable to find any devices...\n");
		exit(1);
	}

	/*
	 * Initialize the terminal and internal curses routines
	 */
	StartCurses();

	/*
	 * create all of the windows and panels
	 */
	CreateWins();

	/*
	 * Get the current terminal type, and do the right thing
	 */
	TermDepSetup();

	/*
	 * show copyright info
	 */
	if (copyright_msg)
		ShowCopyright();

	/*
	 * display the header info
	 */
	DisplayHeader();

	/*
	 * show all the boards, and allow user to select one
	 */
	print_identify ();

	/*
	 * everything is done, delete all the windows and panels
	 */
	DeleteWins();

	exit (0);
}
