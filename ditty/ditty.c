/* -*- linux-c -*- (Autoset linux indent style when using emacs) */

/*
 * Copyright 1999 Digi International (www.digi.com)
 *    Jeff Randall <Jeff_Randall@digi.com>
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
 */

static char *version = "$Id: ditty.c,v 1.12 2004/10/26 19:42:04 scottk Exp $";


#if !defined(NOT_STARTC_ASTOPC)
# define	STARTC_STOPC
#endif

#if !defined(NOT_ASTARTC_ASTOPC)
# define	ASTARTC_ASTOPC
#endif

#include <linux/version.h>
#if !defined(KERNEL_VERSION) /* defined in version.h in later kernels */
# define KERNEL_VERSION(a,b,c)  (((a) << 16) + ((b) << 8) + (c))
#endif


#include <sys/types.h>
#include <sys/wait.h>
#include <curses.h>
#include <termio.h>
#include <term.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <string.h>

#ifdef _DGRP_
#	include "digirp.h"
#else
#	include "rascon.h"
#endif

extern char *tgetstr();

#define	outdev	stdout

/* ----------------- Begin prototypes ------------------------ */
int digiset(int, digi_t *);
int digiget(int, digi_t *);
void ditty(int); 
void catch(void);



/*
 * Internal terminal on/off strings used with the ditty term parameter.
 */
#define TSIZ	8
#define NTERM	22

struct terms {
    char name[20];
    char on[TSIZ];
    char off[TSIZ];
} term_type[NTERM] = {
    "adm31",	"\033d#",  "\024",    /* ADM 31		*/
    "ansi",	"\033[5i", "\033[4i", /* ANSI terminal	    */
    "dg200",	"\036F?3", "\036F?2", /* DG 200		*/
    "dg210",	"\036F?3", "\036F?2", /* DG 210		*/
    "hz1500",	"\033d#",  "\024",    /* HZ 1500	*/
    "mc5",	"\033d#",  "\024",    /* mc5		*/
    "microterm",    "\033[5i", "\033[4i", /* Microterm	    */
    "multiterm",    "\033d#",  "\024",    /* Multiterm	    */
    "pcterm",	"\033`",   "\033a",   /* PC term	*/
    "tvi",	"\033`",   "\033a",   /* TVI 910/915/925/950/955 */
    "vp-a2",	"\0333",   "\0334",   /* ADDS Viewpoint A2  */
    "vp-60",	"\0333",   "\0334",   /* ADDS Viewpoint 60  */
    "vt52",	"\033W",   "\033X",   /* VT 52		*/
    "vt100",	"\033[5i", "\033[4i", /* VT 100		*/
    "vt220",	"\033[5i", "\033[4i", /* VT 220		*/
    "vt320",	"\033[5i", "\033[4i", /* VT 320		*/
    "vt420",	"\033[5i", "\033[4i", /* VT 420		*/
    "wang2x36",	"\373\361", "\373\360", /* wang 2x36 series */
    "wyse30",	"\030",    "\024",    /* wyse 30	*/
    "wyse50",	"\030",    "\024",    /* wyse 50	*/
    "wyse60",	"\033d#",  "\024",    /* wyse 60	*/
    "wyse75",	"\033[5i", "\033[4i"  /* wyse 75	*/
};
/* end internal term types */


long	sbaud[16] = {
	0,	50,	75,	110,
	134,	150,	200,	300,
	600,	1200,	1800,	2400,
	4800,	9600,	19200,	38400
};

long	fbaud[16] = {
	0,	     57600,  76800,  115200,
	131657, 153600, 230400, 460800,
	921600, 1200,   1800,   2400,
	4800,   9600,   19200,  38400
};

typedef	unsigned char	uchar;

#define STTY	"/bin/stty"
#define STTYA	"/bin/stty -a"

/*
 *	Command parameters.
 */
digi_t	digi;		/* digi parameters to set.		*/
int	flag0;		/* digi_flags to clear			*/
int	flag1;		/* digi_flags to set			*/
int	modem0;		/* Modem control bits to clear		*/
int	modem1;		/* Modem control bits to set		*/
int	flush[3];	/* Flush tty modes			*/
int	sendbreak;	/* Send a BREAK	flag			*/
int	tcxonc[4];	/* Stop/restart output/input modes	*/
char    *term;		/* Terminal type			*/

int edelay=-1;
int edelayset=0;
int newedelay=-1;

#ifdef	STARTC_STOPC
uchar	startc;		/* Start flow control character		*/
uchar	stopc;		/* Stop flow control character		*/
#endif

#ifdef	ASTARTC_ASTOPC
uchar	astartc;	/* Aux. start flow control character	*/
uchar	astopc;		/* Aux. stop flow control character	*/
#endif


#define MAXPARM	100		/* Max STTY parameters		*/
#define MAXTTY	255		/* Max ttys to set up		*/

int	nparm;			/* Number of parameters		*/
char	*parm[MAXPARM+2];	/* List of parameters to set	*/

int	ntty;			/* Number of ttys		*/
char	*tty[MAXTTY];		/* List of ttys to set		*/

int	print = 0;			/* Print the current (final)	*/
				/* tty settings			*/

int	custom_speed = 0;		/* current custom speed */
int	custom_speed_req = 0;		/* new custom speed is being set */
int	custom_speed_new = 0;		/* new custom speed */

#ifdef INCLUDE
/********************************************************
 *	fbaud - return the effective baud rate when "fastbaud"
 *  has been specified.  "baud" is an index into the slow
 *  "sbaud" array (e.g: 50 baud is at index 1, 1200 baud is
 *  at index 9, etc.).  When the fastbaud flag is set, some
 *  of these are used to specify selected higher speeds.
 *  E.g. "ditty fastbaud 110" specifies 230400 baud.
 ********************************************************/
long fbaud( int baud )
{
	long ret;

	switch( baud ) {
	case 1:			// 50 baud
		ret = 57600;
		break;
	case 2:			// 75 baud
		ret = 115200;
		break;
	case 3:			// 110 baud
		ret = 230400;
		break;
	case 4:			// 134 baud
		ret = 460800;
		break;
	case 5:			// 150 baud
		ret = 921800;
		break;
	case 6:			// 200 baud
		ret = 1228800;
		break;
	default:
		ret = sbaud[baud];
		break;
	}

	return( ret );

}
#endif //INCLUDE



/********************************************************
 *  Routine to expand curses definitions.		*
 ********************************************************/

unsigned char
getcode(dest, src)
char *src ;	    /* Source string	*/
char *dest ;		/* Destination string	*/
{
    register char *sp = src ;
    register char *dp = dest ;
    register char *ep = dp + DIGI_PLEN ;
    register int ch ;
    register int v ;
    register int i ;
    register int len ;

    /*
     *	Loop to process all characters.
     */

    for (;;)
    {
	switch (ch = *sp++)
	{

	/*
	 *  Either a null, or a non-escaped "|" terminates
	 *  the string.
	 */

	case 0:
	    len = dp - dest ;
	    while (dp < ep) *dp++ = 0 ;
	    return(len) ;

	/*
	 *  Process backslash escapes.
	 */

	case '\\':
	    ch = *sp++ ;

	    switch (ch)
	    {
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		v = ch - '0' ;
		i = 2 ;
		for (;;) {
		    ch = *sp ;
		    if (ch < '0' || ch > '9') break ;
		    v *= 8 ;
		    v += ch - '0' ;
		    sp++ ;
		    if (--i == 0) break ;
		    }
		ch = v ;
		break ;

	    case 'e':
	    case 'E':
		ch = '\033' ;
		break ;

	    default:
		break ;
		}

	    break ;

	/*
	 *  Process ^X control characters.
	 */

	case '^':
	    if (*sp != 0) ch = *sp++ & 0x1f ;
	    break ;
	}

	/*
	 *  Stuff the destination character.
	 */

	if (dp < ep) *dp++ = ch ;
    }
}

/********************************************************
 *  Routine to print a string unambiguously             *
 ********************************************************/

void
stprint(str, len) 
char *str ;         /* String to print  */
int len ;           /* String length    */
{
	while (--len >= 0) {
	if (*str <= 0x20 || *str >= 0x7f) {
		(void) fprintf(outdev, "\\%03o", *str & 0xff) ;
	}
	else (void) fputc(*str, outdev) ;
	str++ ;
	}
}

/********************************************************
 * setcustombaud - Set custom baud rate using the       *
 *                 DIGI_SETCUSTOMBAUD... ioctl          *
 ********************************************************/
int setcustombaud(int fd, int newspeed)
{
	return(ioctl(fd, DIGI_SETCUSTOMBAUD, &newspeed)) ;
}


/********************************************************
 *	digiset - Set Digiboard special parameters	*
 ********************************************************/

int digiset(int fd, digi_t *digi)
{
	return(ioctl(fd, DIGI_SETAW, digi)) ;
}



/********************************************************
 *	digiget - Get DigiBoard special parameters.	*
 ********************************************************/

int digiget(int fd, digi_t *digi)
{
	/*
	 * If this fails, we assume custom bauds simply aren't
	 * supported, which is equivalent to being off, which
	 * is custom_baud == 0
	 */
	if( ioctl(fd, DIGI_GETCUSTOMBAUD, &custom_speed) == -1 )
		custom_speed = 0;
	ioctl(fd, DIGI_GEDELAY, &edelay);
	return(ioctl(fd, DIGI_GETA, digi)) ;
}

	

/********************************************************
 *	ditty - Perform ditty operations.		*
 ********************************************************/

void ditty(int fd) /* File descriptor of tty */
{ /* Begin ditty */

        int		 i, modem, eol, baud;
	digi_t		 di;
	digiflow_t	 dflow;
	struct termio	 tbuf;
	long		 truebaud;
	char	*baudstr;


	if (term) {
		register int i;
		register int got = 0;
		for (i=0; i<NTERM; i++) {
			if (strcmp(term_type[i].name, term) == 0) {
				digi.digi_onlen = strlen(term_type[i].on);
				digi.digi_offlen = strlen(term_type[i].off);

				if ( digi.digi_onlen > DIGI_PLEN )
					digi.digi_onlen = DIGI_PLEN;
				if ( digi.digi_offlen > DIGI_PLEN )
					digi.digi_offlen = DIGI_PLEN;

				(void) strncpy(digi.digi_onstr, term_type[i].on, DIGI_PLEN) ;
				(void) strncpy(digi.digi_offstr, term_type[i].off, DIGI_PLEN) ;
				(void) strncpy(digi.digi_term,term, DIGI_TSIZ);
				got = 1;
				break;
			}
		}

		if (!got) {
			char buf[1200];
			char *bp = buf+1024;
			char *po;
			char *pf;

			if ( tgetent(buf, term) != 1 ||  (po = tgetstr("po", &bp)) == 0
			    ||  (pf = tgetstr("pf", &bp)) == 0) {
				(void) fprintf(stderr,
				"No printer on/off strings for term %s in terminfo.\n", term) ;
				fprintf(stderr, "ditty: invalid terminal type: %s\n", term);
				fprintf(stderr, "ditty: valid terminal types are: ");
				fprintf(stderr, "\n\t\t");
				for (i=0; i<NTERM; i++) {
					fprintf(stderr, "%s\t", term_type[i].name);
					if ( (i + 1) % 5 == 0) /* five per row */
						fprintf(stderr, "\n\t\t");
				}
				fprintf(stderr, "\n");

			} else {
				digi.digi_onlen = strlen(po) ;
				digi.digi_offlen = strlen(pf) ;

				if ( digi.digi_onlen > DIGI_PLEN )
					digi.digi_onlen = DIGI_PLEN ;
				if ( digi.digi_offlen > DIGI_PLEN )
					digi.digi_offlen = DIGI_PLEN ;

				(void) strncpy(digi.digi_onstr, po, DIGI_PLEN) ;
				(void) strncpy(digi.digi_offstr, pf, DIGI_PLEN) ;
				(void) strncpy(digi.digi_term,term, DIGI_TSIZ);
			}
		}
	}


	/*
	 * Flush I/O queues.
	 */
	for (i = 0 ; i < 3 ; i++) {
		if (flush[i]) {
			if (ioctl(fd, TCFLSH, i) == -1) {
				perror("TCFLSH") ;
				return ;
			}
		}
	}

	/*
	 *      Stop/restart output.
	 */
	for (i = 0 ; i < 4 ; i++) {
		if (tcxonc[i]) {
			if (ioctl(fd, TCXONC, i) == -1) {
				perror("TCXONC");
				return;
			}
		}
	}

	/*
	 * Send a break signal.
	 */
	if (sendbreak) {
		if (ioctl(fd, TCSBRK, 0) == -1)	{
			perror("TCSBRK") ;
			return ;
		}
	}

	/*
	 *	Set Digiboard parameters.
	 */
	if  (   flag0 | flag1
	    |   digi.digi_maxcps 
	    |   digi.digi_maxchar
	    |   digi.digi_bufsize
	    |   digi.digi_onlen
	    |   digi.digi_offlen
	    |   (term != NULL)
	    |   (edelayset != 0)
	)
	{
		if (digiget(fd, &di) == -1) {
			perror("DIGI_GETA") ;
			return ;
		}

		di.digi_flags &= ~flag0 ;
		di.digi_flags |= flag1 ;

		if (digi.digi_maxcps) di.digi_maxcps = digi.digi_maxcps ;
		if (digi.digi_maxchar) di.digi_maxchar = digi.digi_maxchar ;
		if (digi.digi_bufsize) di.digi_bufsize = digi.digi_bufsize ;

		if (digi.digi_offlen) {
			di.digi_offlen = digi.digi_offlen ;
			(void) strncpy(di.digi_offstr, digi.digi_offstr, DIGI_PLEN) ;
		}

		if (digi.digi_onlen) {
			di.digi_onlen = digi.digi_onlen ;
			(void) strncpy(di.digi_onstr, digi.digi_onstr, DIGI_PLEN) ;
		}

		if (strlen(digi.digi_term) )
			(void) strncpy(di.digi_term, digi.digi_term, DIGI_TSIZ) ;

		if (edelayset) {
			if(newedelay < 0)
				newedelay = 0;
			if (newedelay > 1000)
				newedelay = 1000;

			/* Set edelay */
			if (ioctl(fd, DIGI_SEDELAY, &newedelay) == -1) {
				perror("DIGI_SEDELAY");
				return;
			}
		}

		if (digiset(fd, &di) == -1) {
			perror("ditty") ;
			return ;
		}
	} /* End if flag0 or flag1 set */

	/*
	 * Set custom baud rate
	 */
	if (custom_speed_req) {
		if (setcustombaud(fd, custom_speed_new) == -1) {
			if( custom_speed_new ) {
				/*
				 *  Don't bother complaining if they are trying to turn
				 *  the feature OFF.
				 */
				perror("cspeed not supported") ;
				return ;
			}
		}
	}

	/*
	 * Set modem control lines.
	 */

	if (modem0) {
		if (ioctl(fd, TIOCMBIC, &modem0) == -1)	{
			perror("TIOCMBIC") ;
			return ;
		}
	}

	if (modem1 & TIOCM_RTS) {
		if (digiget(fd, &di) == -1) {
			perror("DIGI_GETA") ;
			return ;
		}

		if (di.digi_flags & RTSPACE)
			modem1 &= ~TIOCM_RTS;
	}

	if (modem1) {
		if (ioctl(fd, TIOCMBIS, &modem1) == -1)	{
			perror("TIOCMBIS") ;
			return ;
		}
	}

#ifdef	STARTC_STOPC
	if (startc || stopc) {
		digiflow_t	digiflow;

		if (ioctl(fd, DIGI_GETFLOW, &digiflow) == -1) {
			perror("DIGI_GETFLOW");
			return ;
		}

		if ( startc )
			digiflow.startc = startc;

		if ( stopc )
			digiflow.stopc = stopc;

		if (ioctl(fd, DIGI_SETFLOW, &digiflow) == -1) {
			perror("DIGI_SETFLOW") ;
			return ;
		}
	}
#endif /* STARTC_STOPC */

#ifdef	ASTARTC_ASTOPC
	if (astartc || astopc) {
		digiflow_t	digiflow;

		if (ioctl(fd, DIGI_GETAFLOW, &digiflow) == -1) {
			perror("DIGI_GETAFLOW");
			return ;
		}

		if (astartc)
			digiflow.startc = astartc;

		if (astopc)
			digiflow.stopc = astopc;

		if (ioctl(fd, DIGI_SETAFLOW, &digiflow) == -1) {
			perror("DIGI_SETAFLOW") ;
			return ;
		}
	}
#endif /* ASTARTC_ASTOPC */

	/*
	 * Set any stty parameters requested.
	 */

	if (nparm) { 
		int pid;

		parm[0] = "stty";

		while ((pid = fork()) == -1) sleep(1);

		if (pid == 0) {
			/* Do an stty */
			execvp(STTY, parm);
			/* If execvp works we wont get here */
			perror(STTY);
			exit(2);
		}
		while (wait((int *)0) != pid) ;
	}

	eol = 0 ;
	/*
	 * Display Digiboard parameters.
	 */

	/* Find out what flags are on or off on Digi. */
	/* Covers IXON, FASTBAUD, FORCEDCD, ALTPIN, AIXON */
	if (print && (digiget(fd, &di) != -1)) {
		/*
		 * used to calculate baud rates.
		 */
		
		/* --------------------------------------------------------
		 * It appears that the below call doesn't exist.  We need
		 * cflags here, either that are a baud rate.  We need to
		 * either retrieve the cflags from the driver or baud
		 * from stty
		 * --------------------------------------------------------
		 */
		if (ioctl(fd, TCGETA, &tbuf) == -1) {
			perror("TCGETA") ;
			return ;
		}

		/* The below returns an index into baud table RONNIE */
		baud = tbuf.c_cflag & B38400;

		/*
		 *  The cspeed rates take precedence, followed by the Linux
		 *  CBAUDEX mapping, then fastbaud mappings, and finally
		 *  the standard baud rates.
		 */
		baudstr = "" ;
		if( custom_speed ){
			truebaud = custom_speed;
			baudstr = "*" ;
		} else if( tbuf.c_cflag & CBAUDEX ) {

			/*
			 * Linux supports these high baud
			 * rates in its own way (without fastbaud)
			 */
 			switch( tbuf.c_cflag & CBAUD ) {
			case B57600:
				truebaud = 57600;
				break;
			case B115200:
				truebaud = 115200;
				break;
			case B230400:
				truebaud = 230400;
				break;
			case B460800:
				truebaud = 460800;
				break;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0) && defined(B921600)
			case B921600:
				truebaud = 921600;
				break;
#endif

#if 0
			case B1228800:
				/* B1228800 doesn't exist as of 2.2.12 */
				truebaud = 1228800;
				break;
#endif
			default :
				truebaud =  -1;
				break;
			}
		} else if( di.digi_flags & DIGI_FAST) { // If "fastbaud" is set
			truebaud = fbaud[baud];
		} else {
			truebaud = sbaud[baud];
		}

		(void) fprintf(outdev, "onstr ") ;
		stprint(di.digi_onstr, (int) di.digi_onlen) ;
		(void) fprintf(outdev, " offstr ") ;
		stprint(di.digi_offstr, (int) di.digi_offlen) ;

		(void) fprintf(outdev, " term ") ;
		stprint(di.digi_term, (int) strlen(di.digi_term)) ;
		(void) fprintf(outdev, "\n") ;  

		(void) fprintf(outdev, "maxcps %d maxchar %d bufsize %d ",
		di.digi_maxcps, di.digi_maxchar, di.digi_bufsize);

		if(edelay != -1)  /* DIGI_GEDELAY did not fail */
			(void) fprintf(outdev, "edelay %d", edelay);

		putc('\n', outdev);

		(void) fprintf(outdev, "%sforcedcd %saltpin %sfastbaud (%ld%s)",
			       (di.digi_flags & DIGI_FORCEDCD) ? "" : "-",
			       (di.digi_flags & DIGI_ALTPIN) ? "" : "-",
			       (di.digi_flags & DIGI_FAST) ? "" : "-",
			       truebaud, baudstr );

		(void) fprintf(outdev, " %sprinter",
			(di.digi_flags & DIGI_PRINTER) ? "" : "-");

		(void) fprintf(outdev, " %srtstoggle",
			(di.digi_flags & DIGI_RTS_TOGGLE) ? "" : "-");

		(void) fprintf(outdev, "\n") ;


#ifndef	COMXI
		/* comxi fep supports only rts and cts flow control.	*/
		(void) fprintf(outdev, "%srtspace %sdtrpace %sctspace "
			       "%sdsrpace %sdcdpace",
			       (di.digi_flags & RTSPACE) ? "" : "-",
			       (di.digi_flags & DTRPACE) ? "" : "-",
			       (di.digi_flags & CTSPACE) ? "" : "-",
			       (di.digi_flags & DSRPACE) ? "" : "-",
			       (di.digi_flags & DCDPACE) ? "" : "-");
#else
		(void) fprintf(outdev, "%sctspace %srtspace",
			       (di.digi_flags & CTSPACE) ? "" : "-",
			       (di.digi_flags & RTSPACE) ? "" : "-");

#endif /* COMXI */

		eol = 1;
	}

	if (eol) fprintf(outdev, "\n");


	eol = 0 ;
	/*
	 * Display modem parameters.
	 */
	
	if (print && ioctl(fd, TIOCMGET, &modem) != -1) {
		if (eol) (void) fprintf(outdev, " ");

		(void) fprintf(outdev,
			"DTR%c  RTS%c  CTS%c  CD%c  DSR%c  RI%c",
			(modem & TIOCM_DTR) ? '+' : '-',
			(modem & TIOCM_RTS) ? '+' : '-',
			(modem & TIOCM_CTS) ? '+' : '-',
			(modem & TIOCM_CD) ? '+' : '-',
			(modem & TIOCM_DSR) ? '+' : '-',
			(modem & TIOCM_RI) ? '+' : '-') ;
		eol = 1 ;
	}

	if (eol) fprintf(outdev, "\n") ;

#ifdef	STARTC_STOPC
	if (print && ioctl(fd, DIGI_GETFLOW, &dflow) != -1) {
		(void) fprintf(outdev, "startc = 0x%x stopc = 0x%x\n",
			dflow.startc, dflow.stopc);
	}
#endif /* STARTC_STOPC */

#ifdef	ASTARTC_ASTOPC
	if (print && ioctl(fd, DIGI_GETAFLOW, &dflow) != -1) {
		(void) fprintf(outdev, "%saixon astartc = 0x%x "
			       "astopc = 0x%x\n",
			       (di.digi_flags & DIGI_AIXON) ? "" : "-",
			       dflow.startc, dflow.stopc);
	}
#endif /* ASTARTC_ASTOPC */

	/*
	 * Display stty parameters.
	 */

	fflush(outdev);

	if (print == 1)	{
	  if (system(STTY))
	    perror (STTY);
	}

	if (print == 2)	{
	  if (system(STTYA))
	    perror (STTYA);
	}

} /* End ditty */

/********************************************************
 *    catch - Stub to catch interrupts                  *
 ********************************************************/

void catch(void) {}

/********************************************************
 *     Main program to process parameters.              *
 ********************************************************/

int main(int argc, char **argv)
{ /* Begin Main */

	int		 i, fd, newfd;
	int		 ac = argc;
	char		 ttname[20], *ap, *tp;

	/*
	 * Process parameters.
	 */

	/* Begin for each arg */
	for (i = 1 ; i < argc ; i++) {
		ap = argv[i] ;

		if ((strcmp(ap, "maxcps") == 0) && (i+1 < argc)) {
			digi.digi_maxcps = atoi(argv[++i]) ;
		}
		else if ((strcmp(ap, "maxchar") == 0) && (i+1 < argc)) {
			digi.digi_maxchar = atoi(argv[++i]) ;
		}
		else if ((strcmp(ap, "bufsize") == 0) && (i+1 < argc)) {
			digi.digi_bufsize = atoi(argv[++i]) ;
		}
		else if ((strcmp(ap, "edelay") == 0) && (i+1 < argc)) {
			edelayset = 1;
			newedelay = atoi(argv[++i]) ;
		}
		else if ((strcmp(ap, "onstr") == 0) && (i+1 < argc)) {
			digi.digi_onlen = getcode(digi.digi_onstr, argv[++i]) ;
			(void) strcpy(digi.digi_term, "unknown");
		}
		else if ((strcmp(ap, "offstr") == 0) && (i+1 < argc)) {
			digi.digi_offlen = getcode(digi.digi_offstr, argv[++i]) ;
			(void) strcpy(digi.digi_term, "unknown");
		}
		else if ((strcmp(ap, "term") == 0) && (i+1 < argc)) {
			term = argv[++i] ;
		}


#ifdef	STARTC_STOPC
		else if (strcmp(ap, "startc") == 0 && i+1 < argc) {
			startc = (uchar) strtol(argv[++i], (char**)0, 0);
		}
		else if (strcmp(ap, "stopc") == 0 && i+1 < argc) {
			stopc = (uchar) strtol(argv[++i], (char**)0, 0);
		}
#endif

#ifdef	ASTARTC_ASTOPC
		else if (strcmp(ap, "astartc") == 0 && i+1 < argc) {
			astartc = (uchar) strtol(argv[++i], (char**)0, 0);
		}
		else if (strcmp(ap, "astopc") == 0 && i+1 < argc) {
			astopc = (uchar) strtol(argv[++i], (char**)0, 0);
		}
#endif

		else if (strcmp(ap, "-dtr") == 0) {
			modem0 |= TIOCM_DTR ;
		} else if (strcmp(ap, "dtr") == 0) {
			modem1 |= TIOCM_DTR ;
		} else if (strcmp(ap, "-rts") == 0) {
			modem0 |= TIOCM_RTS ;
		} else if (strcmp(ap, "rts") == 0) {
			modem1 |= TIOCM_RTS ;
		} else if (strcmp(ap, "rtspace") == 0) {
			flag1 |= RTSPACE;
		} else if (strcmp(ap, "-rtspace") == 0)	{
			flag0 |= RTSPACE;
		} else if (strcmp(ap, "ctspace") == 0) {
			flag1 |= CTSPACE;
		} else if (strcmp(ap, "-ctspace") == 0) {
			flag0 |= CTSPACE;
		} else if (strcmp(ap, "dtrpace") == 0) {
			flag1 |= DTRPACE;
		} else if (strcmp(ap, "-dtrpace") == 0)	{
			flag0 |= DTRPACE;
		} else if (strcmp(ap, "dsrpace") == 0) {
			flag1 |= DSRPACE;
		} else if (strcmp(ap, "-dsrpace") == 0)	{
			flag0 |= DSRPACE;
		} else if (strcmp(ap, "dcdpace") == 0) {
			flag1 |= DCDPACE;
		} else if (strcmp(ap, "-dcdpace") == 0)	{
			flag0 |= DCDPACE;
		} else if (strcmp(ap, "flushin") == 0) {
			flush[0]++;
		} else if (strcmp(ap, "flushout") == 0) {
			flush[1]++;
		} else if (strcmp(ap, "flush") == 0) {
			flush[2]++;
		} else if (strcmp(ap, "break") == 0) {
			sendbreak++ ;
		} else if (strcmp(ap, "stopout") == 0) {
			tcxonc[0]++ ;
		} else if (strcmp(ap, "startout") == 0) {
			tcxonc[1]++ ;
		} else if (strcmp(ap, "stopin") == 0) {
			tcxonc[2]++ ;
		} else if (strcmp(ap, "startin") == 0) {
			tcxonc[3]++ ;
		} else if (strcmp(ap, "fastbaud") == 0) {
			flag1 |= DIGI_FAST;
		} else if (strcmp(ap, "-fastbaud") == 0) {
			flag0 |= DIGI_FAST;
		} else if (strcmp(ap, "tfastbaud") == 0) {
			flag1 |= CBAUDEX;
		} else if (strcmp(ap, "-tfastbaud") == 0) {
			flag0 |= CBAUDEX;
		} else if (strcmp(ap, "printer") == 0) {
			flag1 |= DIGI_PRINTER;
		} else if (strcmp(ap, "-printer") == 0) {
			flag0 |= DIGI_PRINTER;
		} else if (strcmp(ap, "rtstoggle") == 0) {
			flag1 |= DIGI_RTS_TOGGLE;
		} else if (strcmp(ap, "-rtstoggle") == 0) {
			flag0 |= DIGI_RTS_TOGGLE;
		} else if (strcmp(ap, "forcedcd") == 0) {
			flag1 |= DIGI_FORCEDCD;
		} else if (strcmp(ap, "-forcedcd") == 0) {
			flag0 |= DIGI_FORCEDCD;
		}
#ifdef	ASTARTC_ASTOPC
		else if (strcmp(ap, "aixon") == 0) {
			flag1 |= DIGI_AIXON;
		} else if (strcmp(ap, "-aixon") == 0) {
			flag0 |= DIGI_AIXON;
		}
#endif
		else if (strcmp(ap, "altpin") == 0) {
			flag1 |= DIGI_ALTPIN;
		} else if (strcmp(ap, "-altpin") == 0) {
			flag0 |= DIGI_ALTPIN;
		}

		else if ((strcmp(ap, "cspeed") == 0) && (i+1 < argc))
		{
			custom_speed_req = 1;
			custom_speed_new = atoi(argv[++i]);
			if( custom_speed_new < 0 ) custom_speed_new = 0;
		}
		else if (strcmp(ap, "-cspeed") == 0)
		{
			custom_speed_req = 1;
			custom_speed_new = 0;
		}

#ifdef PARA
		else if ((strcmp(ap, "-p") == 0) || 
			 (strcmp(ap, "-parallel") == 0)) {
			parallel = 1;	/* interpret as parallel port */
		} else if (strcmp(ap, "output") == 0) {
			parallel = 1;
			flag0 |= DIGI_PP_INPUT;
		} else if (strcmp(ap, "input") == 0) {
			parallel = 1;
			flag1 |= DIGI_PP_INPUT;
		} else if (strcmp(ap, "noack") == 0) {
			parallel = 1;
			flag1 |= DSRPACE;
		} else if (strcmp(ap, "-noack") == 0) {
			parallel = 1;
			flag0 |= DSRPACE;
		} else if (strcmp(ap, "reset") == 0) {
			parallel = 1;
			preset = 1;
		} else if ((strcmp(ap, "-OP") == 0) || 
			   (strcmp(ap, "-op") == 0) ||
			   (strcmp(ap, "OP-") == 0) ||
			   (strcmp(ap, "op-") == 0)) {
			parallel = 1;
			modem0 |= TIOCM_DTR ;
		} else if ((strcmp(ap,  "OP") == 0) || 
			   (strcmp(ap,  "op") == 0) ||
			   (strcmp(ap, "+OP") == 0) ||
			   (strcmp(ap, "+op") == 0) ||
			   (strcmp(ap, "OP+") == 0) ||
			   (strcmp(ap, "op+") == 0)) {
			parallel = 1;
			modem1 |= TIOCM_DTR ;
		} else if ((strcmp(ap, "0.8") == 0) ||
			   (strcmp(ap, ".8") == 0)) {
			parallel = 1;
			pwidth = 13;
		} else if ((strcmp(ap, "1.0") == 0) || 
			   (strcmp(ap, "1") == 0)) {
			parallel = 1;
			pwidth = 12;
		} else if (strcmp(ap, "1.2") == 0) {
			parallel = 1;
			pwidth = 11;
		} else if (strcmp(ap, "1.6") == 0) {
			parallel = 1;
			pwidth = 10;
		} else if ((strcmp(ap, "2.0") == 0) ||
			   (strcmp(ap, "2") == 0)) {
			parallel = 1;
			pwidth = 9;
		} else if ((strcmp(ap, "3.0") == 0) ||
			   (strcmp(ap, "3") == 0)) {
			parallel = 1;
			pwidth = 8;
		} else if ((strcmp(ap, "4.0") == 0) ||
			   (strcmp(ap, "4") == 0)) {
			parallel = 1;
			pwidth = 7;
		} else if ((strcmp(ap, "5.0") == 0) || 
			   (strcmp(ap, "5") == 0)) {
			parallel = 1;
			pwidth = 6;
		} else if ((strcmp(ap, "10.0") == 0) ||
			   (strcmp(ap, "10") == 0)) {
			parallel = 1;
			pwidth = 5;
		} else if ((strcmp(ap, "20.0") == 0) ||
			   (strcmp(ap, "20") == 0)) {
			parallel = 1;
			pwidth = 4;
		} else if ((strcmp(ap, "30.0") == 0) ||
			   strcmp(ap, "30") == 0)) {
			parallel = 1;
			pwidth = 3;
		} else if ((strcmp(ap, "40.0") == 0) ||
			   (strcmp(ap, "40") == 0)) {
			parallel = 1;
			pwidth = 2;
		} else if (strcmp(ap, "50.0") == 0) {
			parallel = 1;
			pwidth = 1;
		}
#endif
		else if (strcmp(ap, "-a") == 0) {
			print = 2 ;
		} else if ((strcmp(ap, "-n") == 0) && (i+1 < argc)) {
			if (ntty >= MAXTTY) {
				(void) fprintf(stderr, 
					       "%s: too many tty devices "
					       "given!\n", argv[0]) ;
				exit(2) ;
			}
			tty[ntty++] = argv[++i];
			ac -= 2;
		} else if ((strncmp(ap, "/dev/", 5) == 0) ||
			   (strncmp(ap, "tty", 3) == 0) ||
			   (strncmp(ap, "cud", 3) == 0)) {
			if (ntty >= MAXTTY) {
				(void) fprintf(stderr, 
					       "%s: too many tty devices "
					       "given!\n", argv[0]) ;
				exit(2) ;
			}
			tty[ntty++] = ap ;
			ac-- ;
		} else 	{
			if (nparm >= MAXPARM) {
				(void) fprintf(stderr,
					       "%s: too many stty parameters "
					       "given!\n", argv[0]) ;
				exit(2) ;
			}
			parm[++nparm] = ap ;
		}

	} /* End for each arg */

	if (ac <= 1)
		print = 1;

	/*
	 * If no tty was specified, set options on
	 * standard output.
	 */

	if (!ntty) {
		ditty(0) ; /* standard input */
	} else { /* Begin tty's were specified */
		/*
		 * If a list of ttys were given, set options
		 * on all of them.
		 */

		(void) signal(SIGALRM, (void *)catch);

		/* Begin for each tty */
		for (i = 0 ; i < ntty ; i++) {
			
			tp = tty[i] ;
			if (tp[0] != '/') {
				(void) sprintf(ttname, "/dev/%s", tp) ;
				tp = ttname ;
			}

			(void) alarm(1) ;
			fd = open(tp, O_WRONLY | O_NDELAY) ;
			(void) alarm(0) ;

			if (fd < 0) {
				if (errno == EINTR) {
					(void) fprintf(stderr,
						       "%s: another process "
						       "waiting on open\n",
						       tp) ;
				}
				else perror(tp) ;
			} else {

				(void) close(0);
				/*
				 * This should always return file
				 * descriptor 0 (the lowest )
				 */
				newfd = dup(fd);

				/* 
				 * I don't really like the way this works.
				 * By closing stdin we guarantee that the
				 * next 'dup'ed file descriptor will be 0
				 * (Because dup always uses the lowest
				 * available descriptor	and stdin always
				 * has the lowest value of 0).  This is done
				 * so that the stty command 'exec'ed by ditty
				 * will be faked into believing that it is
				 * talking to standard input.  In this
				 * way he avoids sending arguments to
				 * the stty command.
				 */

				(void) close(fd);
				if (newfd < 0) {
					perror("dup failed ");
				}
				

				if (print && ntty > 1) {
					(void) fprintf(outdev,
						       "\n####### %s\n", tp) ;
				}

				/* Should always be 0 */
				ditty(newfd) ;
			}
		} /* End for each tty */
	} /* End tty's were specified */

	return(0);

} /* End Main */

