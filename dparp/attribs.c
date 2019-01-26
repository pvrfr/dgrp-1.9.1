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
 * $Id: attribs.c,v 1.4 2005/07/31 07:17:41 scottk Exp $
 * 
 */


/* ex:se ts=4 sw=4 sm: */

/*
 *	This file serves to insulate ETI apps from the
 *	quirks of:
 *		SCO "ansi" console reverse+color bug fix
 *		SCO renumbering ETI colors
 *		SCO not having a ansi-bw
 *
 *	Also, allocates color pairs for you
 *
 *	TERMOPTS environment variable syntax:
 *		<OPTIONLIST>
 *		<TERM>:<OPTIONLIST>
 *		<TERM>:<OPTIONLIST>;<TERM>:<OPTIONLIST> ...
 *	<OPTIONLIST>:
 *		[!]sco_colors,[!]att_colors,[!]rev_colors,[!]bw
 *
 *	e.g.
 *		SCO ANSI COLOR with reverse color bug fix (ODT1.1)
 *			TERMOPTS=sco_colors,rev_colors
 *				[ default iff TERM==ansi ]
 *		SCO ANSI COLOR without reverse color bug fix (ODT1.0)
 *			TERMOPTS=sco_colors,!rev_colors
 *		SCO ANSI with gray scale or BW monitor:
 *			TERMOPTS=bw
 *
 * e.g.  wattrset(win, make_attr(A_STANDOUT, COLOR_WHITE, COLOR_BLUE) );
 *
 */
#include <string.h>
#include <curses.h>

/*
 *	Indicate to program if A_BOLD, A_UNDERLINE, A_REVERSE, and A_DIM
 *	are available.
 */
long	HasAttr;

#ifndef ISO_BLACK	/* Not being compiled on SCO */

#define ISO_BLACK	0
#define ISO_RED		1
#define ISO_GREEN	2
#define ISO_YELLOW	3
#define ISO_BLUE	4
#define ISO_MAGENTA	5
#define ISO_CYAN	6
#define ISO_WHITE	7

static int	color_map[8] =
{
	ISO_BLACK, ISO_BLUE, ISO_GREEN, ISO_CYAN,
	ISO_RED, ISO_MAGENTA, ISO_YELLOW, ISO_WHITE
};
#define	CURSES_USES_SCO_COLORS	0

#else	/* Being compiled on SCO */

static int	color_map[8] =
{
	IBM_BLACK, IBM_RED, IBM_GREEN, IBM_YELLOW,
	IBM_BLUE, IBM_MAGENTA, IBM_CYAN, IBM_WHITE
};
#define	CURSES_USES_SCO_COLORS	1

#endif

#ifndef COLOR_PAIR
#define	COLOR_PAIR(X)	0
#endif

typedef struct
{
	int	fg;
	int	bg;
} PAIRINFO;

static PAIRINFO	pairinfo[32];
static int	npairinfo = 0;

static int	sco_colors = 0;
static int	rev_colors = 0;
static int	bw = 0;
static int	ul = 1;

int
make_attr(int video_att, int fg_att, int bg_att)
{
	register int	i;
	register char	*p, *s, *t;
	char		*getenv();

	static int first_time = 1;

	if (first_time)
	{
		first_time = 0;

		t = getenv("TERM");
		if (!t)
			t = "unknown";

		p = getenv("TERMOPTS");

		if (!p)
		{
			if (strcmp(t, "ansi") == 0)
				p = "sco_colors,rev_colors";	/* Probly SCO */
			else
				p = "";
		}
		p = strdup(p);

		while ((s = strchr(p, ':')) != NULL)
		{	/* var: form */
			if ( (unsigned)(s-p) == strlen(t) && strncmp(p, t, s-p) == 0)
			{
				p = s+1;
				s = strchr(p, ';'); if (s) *s = 0;
				break;
			}
			p = s+1;
			s = strchr(p, ';'); if (s) p = s+1;
		}

		while ((s = strtok(p, ",:;")) != NULL)
		{
			p = 0;
			if (strcmp(s, "sco_colors") == 0)
				sco_colors = 1;
			else if (strcmp(s, "!sco_colors") == 0)
				sco_colors = 0;
			else if (strcmp(s, "att_colors") == 0)
				sco_colors = 0;
			else if (strcmp(s, "!att_colors") == 0)
				sco_colors = 1;
			else if (strcmp(s, "rev_colors") == 0)
				rev_colors = 1;
			else if (strcmp(s, "!rev_colors") == 0)
				rev_colors = 0;
			else if (strcmp(s, "bw") == 0)
				bw = 1;
			else if (strcmp(s, "!bw") == 0)
				bw = 0;
			else if (strcmp(s, "ul") == 0)
				ul = 1;
			else if (strcmp(s, "!ul") == 0)
				ul = 0;
		}
		if (!ul) HasAttr &= ~A_UNDERLINE;
	}
	
	if (bw || !has_colors() ) return (video_att);

	if ( (CURSES_USES_SCO_COLORS && !sco_colors)
		|| (!CURSES_USES_SCO_COLORS && sco_colors) )
	{
		fg_att = color_map[fg_att];
		bg_att = color_map[bg_att];
	}

	if ( (video_att & (A_REVERSE|A_STANDOUT) ) && rev_colors)
	{
		i = fg_att; fg_att = bg_att; bg_att = i;
	}

	for (i = 0; i < npairinfo; ++i)
	{
		if (pairinfo[i].fg == fg_att && pairinfo[i].bg == bg_att)
			break;
	}

	if (i >= npairinfo)
	{	/* New color pair */
		if (npairinfo == 32)
			fprintf(stderr, "Couldn't allocate color pair\n");
		pairinfo[i].fg = fg_att;
		pairinfo[i].bg = bg_att;
		init_pair(i+1, fg_att, bg_att);
		++npairinfo;
	}
	++i;

#ifdef SOLARIS
	return ((video_att & ~A_ALTCHARSET) | COLOR_PAIR(i));
#else
	return (video_att | COLOR_PAIR(i));
#endif
}
