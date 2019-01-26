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
 * $Id: node_panel.c,v 1.11 2005/07/31 07:19:56 scottk Exp $
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
#include "dpa_os.h"

#include "curses_helpers.h"
#include "general_panels.h"
#include "exam_panel.h"
#include "info_panel.h"
#include "capture.h"

#include "attribs.h"

/*
 * NAME: read_descriptor
 *      
 * FUNCTION: Reads a descriptor from the VPD for a device
 *         
 * EXECUTION ENVIRONMENT:
 *         
 *  This function is used to decode VPD which is stored in the format
 *  used by the system devices ( CPU's, Planars etc. )
 *          
 * NOTES:
 *  VPD is stored as a series of descriptors, each of which
 * is encoded as follows:
 * 
 * Byte 0 = '*'
 * Byte 1,2 = mnemonic      ( E.g. "TM", "Z1", etc )
 * Byte 3 = Total length / 2
 * Byte 4.. = data
 *      
 *  E.g.:  Byte#     0    1    2    3    4    5    6    7    8    9
 *         Ascii    '*'  'Z'  '1'       '0'  '1'  '2'  '0'  '0'  '1'
 *         Hex       2A   5A   31   05   30   31   32   30   30   31
 *         Oct      052  132  061  005  060  061  062  060  060  061
 *  
 * RETURNS:
 *          
 *  A pointer to the static char array "result" is returned, with
 *  the array being empty if the descriptor was not present in the
 *  VPD passed in.
 */

char *read_descriptor(char *vpd, char *name, char *result)
{       
	char *res_ptr;
	int bytecount;

	res_ptr = result;
	*res_ptr = '\0';

	while( *vpd == '*' ) {
		if ( ( vpd[1] == name[0] ) && ( vpd[2] == name[1] ) ) {
			/* This is the correct descriptor */
			bytecount = ((int)vpd[3] << 1 ) - 4;

			vpd += 4;  

			while( bytecount-- )
				*res_ptr++ = *vpd++;

			*res_ptr = '\0';
		}
		else
			/* Skip to next descriptor */
			vpd += ( (int)vpd[3] << 1 );
	}

	return result;
}


/**********************************************************************
*
*  Routine Name:	print_node
*
*  Function:	   display capture buffers
*                 returns 0 if should return to original caller,
*                         1 if should switch to "other" (capture) handler
*
**********************************************************************/

int 
print_node(struct deviceinfo *unit)
{
	int option = EOF;

	int rc;
	fd_set rfds;
	int max_fd;
	struct digi_node node;
	struct digi_vpd vpd;
	int retval = 0;
	WINDOW *nodewin = GetWin(NodeWin);

	show_panel (GetPan(NodeWin));
	update_panels ();
	doupdate ();
	curs_set(0);

	while ((option != 'Q') && (option != 'q') && (option != ESC))
	{
		memset(&node, 0, sizeof(struct digi_node));
		memset(&vpd,  0, sizeof(struct digi_vpd));

		DPAOpenDevice(unit);
		DPAGetNode(unit, &node);
		DPAGetVPD(unit, &vpd);
		DPACloseDevice(unit);

		max_fd = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);

		erase_win (NodeWin);

		wattrset (nodewin, make_attr (A_NORMAL, WHITE, BLACK));

		if (!vanilla)
			wattrset (nodewin, make_attr (A_ALTCHARSET, CYAN, BLACK));
		else
			wattrset (nodewin, make_attr (A_NORMAL, CYAN, BLACK));

		wattrset (nodewin, make_attr (A_NORMAL, GREEN, BLACK));

		sprintf (clbuf, " Device Information ");
		mvwprintw (nodewin, 1, center (NodeWin, strlen (clbuf)), clbuf);

		mvwprintw (nodewin, 3, 2, "Device Description : %-53.53s",
			node.nd_ps_desc);
                mvwprintw (nodewin, 4, 2, "Unit IP Address    : %-53.53s",
			unit->host);
                mvwprintw (nodewin, 5, 2, "Number of ports    : %-2d",
			(node.nd_state != 0) ? node.nd_chan_count : unit->nports);
                mvwprintw (nodewin, 6, 2, "Server Port number : %-7d",
			unit->port);
                mvwprintw (nodewin, 7, 2, "Encrypted Sessions : %-53.53s",
			unit->encrypt);
                mvwprintw (nodewin, 8, 2, "Wan Speed          : %-53.53s",
			unit->wanspeed);
                mvwprintw (nodewin, 9, 2, "Device ID          : %-53.53s",
			unit->devname);


		/* Display VPD info, if found */


		if (vpd.vpd_len) {
			char string[100];

			sprintf (clbuf, " VPD Information ");
			mvwprintw (nodewin, 11, center (NodeWin, strlen (clbuf)), clbuf);

			read_descriptor(vpd.vpd_data, "DS", string);
			mvwprintw (nodewin, 13, 2, "Product Name...... : %-53.53s",
				string);

			read_descriptor(vpd.vpd_data, "MF", string);
			mvwprintw (nodewin, 14, 2, "Manufacturer...... : %-53.53s",
				string);

			read_descriptor(vpd.vpd_data, "SN", string);
			mvwprintw (nodewin, 15, 2, "Serial Number..... : %-53.53s",
				string);

			read_descriptor(vpd.vpd_data, "PN", string);
			mvwprintw (nodewin, 16, 2, "Part Number....... : %-53.53s", 
				string);

			read_descriptor(vpd.vpd_data, "FN", string);
			mvwprintw (nodewin, 17, 2, "FRU Number........ : %-53.53s", 
				string);

			read_descriptor(vpd.vpd_data, "EC", string);
			mvwprintw (nodewin, 18, 2, "EC Number......... : %-53.53s", 
				string);
		}


		wattrset (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));


		commandline (clbuf, helpstr, "ESC=Go Back", NULL);
		mvwprintw (GetWin(MainWin), KEY_LINE, 0, clbuf);

		wattroff (GetWin(MainWin), make_attr (A_NORMAL, WHITE, BLUE));
		wrefresh (GetWin(MainWin));
		wrefresh (nodewin);

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

#ifdef KEY_PRINT
		case KEY_PRINT:
#endif
		case '':
			screen_save (NodeWin, logfile);
			touchwin (nodewin);
			wrefresh (nodewin);
			update_panels ();
			doupdate ();
			break;

		case KEY_F (1):		   /*  Help info  */
		case '?':
			info_screen (dpa_info5, dpa_info5_len, NULL);
			update_panels ();
			doupdate ();
			break;

		case 'q':
		case 'Q':
		case ESC:
			hide_panel(GetPan(NodeWin));
			erase_win(NodeWin);
			update_panels();
			doupdate();

			break;

		default:
			mvwprintw (nodewin, GetHeight(NodeWin) - 2, 60, "Invalid key");
			wrefresh (nodewin);
			sleep (1);
			break;
		}						   /* End Case */
	}							   /* End While */

	curs_set(1);
	hide_panel (GetPan(NodeWin));
	update_panels ();
	doupdate ();

	return retval;
}
