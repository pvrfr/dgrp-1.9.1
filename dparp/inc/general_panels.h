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
 * $Id: general_panels.h,v 1.3 2005/07/31 07:23:22 scottk Exp $
 *
 */


#ifndef _GENERAL_PANELS_H
#define _GENERAL_PANELS_H

extern DigiWin *MainWin;
extern DigiWin *CRWin;
extern DigiWin *HeadWin;
extern DigiWin *IdentWin;
extern DigiWin *ChanWin;
extern DigiWin *LBWin;
extern DigiWin *ScopeWin;
extern DigiWin *ExamWin;
extern DigiWin *NodeWin;

extern int KEY_LINE;
extern char *clbuf;


#define STRSIZ 80

/*
 * the next strings are for the key line strings, this  tries
 * to be intelligent about the key options which are displayed
 * to the user
 */
extern char helpstr[STRSIZ];	/* "help" key line string		*/
extern char lrudstr[STRSIZ];	/* left/right/up/dn help string	*/
extern char updnstr[STRSIZ];	/* up/down key line string		*/
extern char pgdnstr[STRSIZ];	/* page down key line string	*/
extern char pgupstr[STRSIZ];	/* page up key line string		*/


/*
 * Function prototypes
 */
int AskYN (char *);
void ShowMsg (char *);
int commandline (char *s, ...);
void ShowCopyright (void);
void screen_save (DigiWin *, char *);
void DisplayHeader (void);
void CreateWins (void);
void DeleteWins (void);

#endif /* _GENERAL_PANELS_H */
