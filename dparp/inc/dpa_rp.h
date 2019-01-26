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
 * $Id: dpa_rp.h,v 1.4 2010/02/10 04:18:08 scottk Exp $
 *
 */


#ifndef _DPA_RP_H_
#define _DPA_RP_H_

#include "dpa_os.h"	/* For deviceinfo */

/* Max number of PortServers we support with this application */
#define MAX_PORTSERVERS	4096

/* Default size of our strings */
#define STRSIZ		80

/* PortServer RealPort Dividend value */
#define PDIVIDEND	1843200

/* Control Flags */

#define CF_CBAUD        0x000f          /* Baud rate (not used) */
#define CF_CSIZE        0x0030          /* Character size */
#define CF_CS5          0x0000          /* 5 bits/character */
#define CF_CS6          0x0010          /* 6 bits/character */
#define CF_CS7          0x0020          /* 7 bits/character */
#define CF_CS8          0x0030          /* 8 bits/character */
#define CF_CSTOPB       0x0040          /* 2 stop bits */
#define CF_CREAD        0x0080          /* Enable read */
#define CF_PARENB       0x0100          /* Parity enable */
#define CF_PARODD       0x0200          /* Odd (space) parity */
#define CF_HUPCL        0x0400          /* Hang up on last close(not used) */
#define CF_CLOCAL       0x0800          /* Ignore DCD transitions(not used) */
#define CF_FRAME        0x1000          /* Port in framing mode (not used) */

/* Extra Flags */

#define XF_XPAR         0x0001          /* Enable mark/space parity */
#define XF_XMODEM       0x0002          /* Use in-band modem signalling (NI) */
#define XF_XCASE        0x0004          /* Special output char conversion (NI)*/
#define XF_XEDATA       0x0008          /* Error data is unmodified */
#define XF_XTOSS        0x0040          /* Toss IF_IXANY character */
#define XF_XIXON        0x2000          /* Enable xtra flow control characters*/
#define XF_XIXON_UNIX   0x0020          /* Due to UNIX driver implementation */
                                        /* a break from the Net C/X Spec. */
                                        /* this mask will also be supported. */

/* Input Processing Flags */

#define IF_IGNBRK       0x0001          /* Ignore received break */
#define IF_BRKINT       0x0002          /* Break interrupt */
#define IF_IGNPAR       0x0004          /* Ignore parity */
#define IF_PARMRK       0x0008          /* Mark parity */
#define IF_INPCK        0x0010          /* Enable input parity checking */
#define IF_ISTRIP       0x0020          /* Strip characters to 7 bits */
#define IF_IXON         0x0400          /* Enable output flow control chars */
#define IF_IXANY        0x0800          /* Restart output on any character */
#define IF_IXOFF        0x1000          /* Enable input flow control chars */
#define IF_DOSMODE      0x8000          /* Provide 16450 error data */

/* Output Processing Flags */

#define OF_OLCUC        0x0002          /* Lower case to upper case */
#define OF_ONLCR        0x0004          /* Newline to CRNL */
#define OF_OCRNL        0x0008          /* CR to CRNL */
#define OF_ONOCR        0x0010          /* No CR in column 0 */
#define OF_ONLRET       0x0020          /* Newline-return */
#define OF_TABDLY       0x1800          /* Tab handling */
#define OF_TAB0         0x0000          /* Tabs straight through */
#define OF_TAB3         0x1800          /* Expand tabs */

/* Modem Flags */

#define MS_DTR          0x01            /* Data Terminal Ready */
#define MS_RTS          0x02            /* Request to Send */
#define MF_RTSTOG       0x04            /* RTS Toggle flow control */
#define MF_RIPOWER      0x08            /* RI Power output */
#define MS_CTS          0x10            /* Clear to send  */
#define MS_DSR          0x20            /* Data Set Ready */
#define MS_RI           0x40            /* Ring Indicator */
#define MS_DCD          0x80            /* Data Carrier Detect */

/* Event Flags */

#define EV_OPU          0x0001          /*Output paused by client */
#define EV_OPS          0x0002          /*Output paused by reqular sw flowctrl*/
#define EV_OPX          0x0004          /*Output paused by extra sw flowctrl */
#define EV_OPH          0x0008          /*Output paused by hw flowctrl */
#define EV_IPU          0x0010          /*Input paused unconditionally by user*/
#define EV_IPS          0x0020          /*Input paused by high/low water marks*/
#define EV_TXB          0x0040          /*Transmit break pending */
#define EV_TXI          0x0080          /*Transmit immediate pending */
#define EV_TXF          0x0100          /*Transmit flowctrl char pending */
#define EV_RXB          0x0200          /*Break received */
#define EV_OPALL        0x000f          /*Output pause flags */
#define EV_IPALL        0x0030          /*Input pause flags */


/************** GLOBALS *****************/

extern int debug;			/* write out to debug file	*/
extern FILE *dfp;			/* debug file pointer		*/
extern char logfile[];			/* logfile name			*/
extern int num_devices;			/* Number of Devices found	*/
extern struct deviceinfo device_info[];	/* Array of Devices found	*/


#endif
