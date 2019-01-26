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
 * $Id: capture.h,v 1.2 2005/07/31 07:23:22 scottk Exp $
 *
 */

#ifndef _CAPTURE_H
#define _CAPTURE_H

#define CAPTURE_DEFAULT_SIZE (128 * 1024)
#define CAPTURE_OVERFLOW     0x00000001

typedef struct {
	char *buf;

	unsigned long size;  /* Enforced to a power of 2 */
	unsigned long mask;
	unsigned long head;  /* Oldest byte in buffer */
	unsigned long tail;  /* Next "empty" spot in buffer */
	unsigned long flags; /* To notice overflow */
} CAPTURE_BUFFER;

extern CAPTURE_BUFFER rx_capture;
extern CAPTURE_BUFFER tx_capture;

#endif /* _CAPTURE_H */
