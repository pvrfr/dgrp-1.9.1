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
 * $Id: dpa_os.h,v 1.10 2005/11/15 19:49:17 scottk Exp $
 *
 */


#ifndef _DPA_OS_H_
#define _DPA_OS_H_

#if defined(LINUX) || defined(SOLARIS)
typedef unsigned char uchar;
#endif

#define MAX_DESC_LEN	100	/* Maximum length of stored PS description */
#define MAX_VPD_LEN	512	/* Maximum length of stored VPD */
#define DOWN 0
#define UP 1


/*
 * Structure that holds all the basic information about a device.
 * It is based only on what can be found in userspace by searching
 * various config files or databases.
 */
struct deviceinfo {
	int	unit_number;
	char	host[MAX_DESC_LEN];
	int	port;
	uint	nports;
	int	major;
	char	encrypt[MAX_DESC_LEN];
	char	wanspeed[MAX_DESC_LEN];
	char    devname[MAX_DESC_LEN];
	char    dpadevname[MAX_DESC_LEN];
	int     status;
	int	dpa_fd;
};



/*
 * Structure that holds more in depth information about a device.
 * The driver will provide us with this information.
 */
struct digi_node {
	uint	nd_state;		/* Node state: 1 = up, 0 = down. */
	uint	nd_chan_count;		/* Number of channels found */
	uint	nd_tx_byte;		/* Tx data count */
	uint	nd_rx_byte;		/* RX data count */
	char	nd_ps_desc[MAX_DESC_LEN]; /* Description from PS */  
};

#define DIGI_GETNODE      ('d'<<8) | 249          /* get board info          */



/*
 * Structure that holds in depth information about a channel on a device
 * The driver will provide us with this information.
 */
struct digi_chan {
	uint	ch_port;	/* Port number to get info on */
	uint	ch_open;	/* 1 if open, 0 if not */
	uint	ch_txcount;	/* TX data count  */
	uint	ch_rxcount;	/* RX data count  */
        uint	ch_s_brate;	/* Realport BRATE */
	uint	ch_s_estat;	/* Realport ELAST */
	uint	ch_s_cflag;	/* Realport CFLAG */
	uint	ch_s_iflag;	/* Realport IFLAG */
	uint	ch_s_oflag;	/* Realport OFLAG */
	uint	ch_s_xflag;	/* Realport XFLAG */
	uint	ch_s_mstat;	/* Realport MLAST */ 
};

#define DIGI_GETCHAN      ('d'<<8) | 248          /* get channel info        */



/*
 * Structure that holds the VPD for a product, if it exists.
 * The driver will provide us with this information.
 */
struct digi_vpd {
	int vpd_len;
	char vpd_data[MAX_VPD_LEN];
};

#define DIGI_GETVPD       ('d'<<8) | 246          /* get VPD info           */



/*
 * Structure that DPA will send into the driver to tell it if we
 * want to "debug" a port, and what port to debug.
 */
struct digi_debug {
	int onoff;
	int port;
};

#define DIGI_SETDEBUG     ('d'<<8) | 247          /* set debug info        */



/*
 * Required functions for each OS that DPA for RealPort supports.
 */

int DPAFindDevices(void);
int DPAOpenDevice(struct deviceinfo *device);
int DPACloseDevice(struct deviceinfo *device);
int DPAGetNode(struct deviceinfo *device, struct digi_node *node);
int DPAGetChannel(struct deviceinfo *device, struct digi_chan *chan, int port);
int DPAGetVPD(struct deviceinfo *device, struct digi_vpd *vpd);
int DPASetDebugForPort(struct deviceinfo *device, int onoff, int port);
char *DPAGetPortName(struct deviceinfo *device, struct digi_node *node,
		    struct digi_chan *chan, int port, char *result);


#endif
