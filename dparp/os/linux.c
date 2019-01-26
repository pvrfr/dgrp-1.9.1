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
 * $Id: linux.c,v 1.7 2005/07/31 07:23:59 scottk Exp $
 *
 */

/* ex:se ts=4 sw=4 sm: */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>

#include "dpa_rp.h"
#include "dpa_os.h"

#define VPDSIZE 512


char    *
sindex(char *string, char *group)
{
	char *ptr;

	if (!string || !group)
		return NULL;

	for (; *string; string++) {
		for (ptr = group; *ptr; ptr++) {
			if (*ptr == *string)
				return string;
		}
	}

	return NULL;
}


char    *
next_arg(char *str, char **new_ptr)
{
	char *ptr;

	if (!(str && *str))
		return NULL;

        /* Skip over all whitespace */
        while (*str == ' ' && *str == '\t' && *str != '\0')
                str++;

        if (*str == '\0')
                return NULL;

	ptr = str;
        
        while (*str != ' ' && *str != '\t' && *str != '\0')
        	str++;
        
	if (*str != '\0')
		*str++ = '\0';


	if (new_ptr)
		*new_ptr = str;

	return ptr;
}



/**********************************************************************
*
*  Routine Name:	FindDevices
*
*  Function:		OS specific function - Seeds the Global ProductList
*			array with all known products that are currently
*			set up in the RealPort driver.
*
*  Return Value:	 0 - All Okay.
*			-1 - Something bad happened.
*
**********************************************************************/

int
DPAFindDevices(void)
{
	FILE *fp;
	char *ptr;
	char *id, *ip, *ipport, *ports, *wanspeed, *encrypt, *tmp;
	char buff[1000];
	struct digi_node node;

	fp = fopen("/etc/dgrp.backing.store", "r");

	if (!fp)
		return -1;

	while (!feof(fp)) {
		fgets(buff, 1000, fp);

		if (buff[0] == '#')
			continue;

		ptr = buff;

		// zz scottkps2 16 auto default default default default never default

		id = next_arg(ptr, &ptr);

		if (!id || *id == '\0')
			continue;

		ip = next_arg(ptr, &ptr);
		if (!ip || *ip == '\0')
			continue;

		ports = next_arg(ptr, &ptr);
		if (!ports || *ports == '\0')
			continue;

		wanspeed = next_arg(ptr, &ptr);
		if (!wanspeed || *wanspeed == '\0')
			continue;

		ipport = next_arg(ptr, &ptr);
		if (!ipport || *ipport == '\0')
			continue;

		tmp = next_arg(ptr, &ptr);
		if (!tmp || *tmp == '\0')
			continue;

		tmp = next_arg(ptr, &ptr);
		if (!tmp || *tmp == '\0')
			continue;

		tmp = next_arg(ptr, &ptr);
		if (!tmp || *tmp == '\0')
			continue;

		encrypt = next_arg(ptr, &ptr);
		if (!encrypt || *encrypt == '\0')
			continue;


		sprintf(device_info[num_devices].devname, "%s", id);
		sprintf(device_info[num_devices].dpadevname, "/proc/dgrp/dpa/%s", id);

		device_info[num_devices].dpa_fd = -1;

		memset(&node, 0, sizeof(struct digi_node));
		DPAOpenDevice(&device_info[num_devices]);
		DPAGetNode(&device_info[num_devices], &node);
		DPACloseDevice(&device_info[num_devices]);

		if (node.nd_state != 0) {
			device_info[num_devices].status = UP;
			device_info[num_devices].nports = node.nd_chan_count;
		}
		else {
			device_info[num_devices].status = DOWN;
			device_info[num_devices].nports = atoi(ports);
		}

		device_info[num_devices].major = 0;

		sprintf(device_info[num_devices].host, ip);

		device_info[num_devices].port = atoi(ipport);
		if (device_info[num_devices].port == 0)
			device_info[num_devices].port = 771;

		sprintf(device_info[num_devices].encrypt, encrypt);
		sprintf(device_info[num_devices].wanspeed, wanspeed);

		device_info[num_devices].unit_number = num_devices;

		num_devices++;

	}

	fclose(fp);

	return 0;
}



/**********************************************************************
*
*  Routine Name:	GetPortName
*
*  Function:		OS specific function - Translates a given
*			device and port offset into a tty name
*			of that port on this system.
*
*  Return Value:	 0 - All Okay.
*			-1 - Something bad happened.
*
**********************************************************************/

char *
DPAGetPortName(struct deviceinfo *device, struct digi_node *node, 
	    struct digi_chan *chan, int port, char *result)
{
	if (debug)
		printf("node: %p chan: %p\n", node, chan);
	sprintf(result, "tty%s%2.2d", device->devname, port);
	return result;
}



/**********************************************************************
*
*  Routine Name:	DPAOpenDevice
*
*  Function:		OS specific function - Opens the "dpa" device
*			for this device on this system.
*
*  Return Value:	>= 0 - All Okay.
*			  -1 - Something bad happened.
*
**********************************************************************/

int
DPAOpenDevice(struct deviceinfo *device)
{
	int fd;

	if ((fd = open(device->dpadevname, (O_RDWR | O_NONBLOCK))) < 0)
		return -1;

	device->dpa_fd = fd;
	return fd;
}



/**********************************************************************
*
*  Routine Name:	DPACloseDevice
*
*  Function:		OS specific function - Closes the "dpa" device
*			for this device on this system.
*
*  Return Value:	 0 - All Okay.
*
**********************************************************************/

int
DPACloseDevice(struct deviceinfo *device)
{
	if (device->dpa_fd >= 0) {
		close(device->dpa_fd);
		device->dpa_fd = -1;
	}
	return 0;
}

	

/**********************************************************************
*
*  Routine Name:	DPAGetNode
*
*  Function:		OS specific function - Fills the digi_node
*			structure.
*
*  Return Value:	 0 - All Okay.
*			-1 - Something bad happened.
*
**********************************************************************/

int
DPAGetNode(struct deviceinfo *device, struct digi_node *node)
{
	int ret;
	char *ptr;

	if (device->dpa_fd < 0)
		return -1;

        ret = ioctl(device->dpa_fd, DIGI_GETNODE, node);

	if (ret == -1) {
		return -1;
	}

	/*
	 * Swap in the IBM name if needed.
	 */
	if ((ptr = strstr(node->nd_ps_desc, "Version 82001249")) != NULL) {
		char newname[MAX_DESC_LEN];
		snprintf(newname, MAX_DESC_LEN, "LAN Attached RAN 16 %s", ptr);
		strncpy(node->nd_ps_desc, newname, MAX_DESC_LEN);
	}

	return 0;
}



/**********************************************************************
*
*  Routine Name:	DPAGetChannel
*
*  Function:		OS specific function - Fills the digi_chan
*			structure.
*
*  Return Value:	 0 - All Okay.
*			-1 - Something bad happened.
*
**********************************************************************/

int
DPAGetChannel(struct deviceinfo *device, struct digi_chan *chan, int port)
{
	int ret;

	if (device->dpa_fd < 0)
		return -1;

	chan->ch_port = port;

        ret = ioctl(device->dpa_fd, DIGI_GETCHAN, chan);

	if (ret == -1) {
		return -1;
	}

	if (chan->ch_open != 1) {
		chan->ch_s_brate = chan->ch_s_mstat = 0;
 		chan->ch_s_cflag = chan->ch_s_iflag = 0;
		chan->ch_s_oflag = chan->ch_s_xflag = 0;
	}

	return 0;
}



/**********************************************************************
*
*  Routine Name:	DPAGetVPD
*
*  Function:		OS specific function - Fills the digi_vpd
*			structure.
*
*  Return Value:	 0 - All Okay.
*			-1 - Something bad happened.
*
**********************************************************************/

int
DPAGetVPD(struct deviceinfo *device, struct digi_vpd *vpd)
{
	int rc;

	memset(vpd->vpd_data, 0, VPDSIZE);
	vpd->vpd_len = 0;

	if (device->dpa_fd >= 0) {

	        rc = ioctl(device->dpa_fd, DIGI_GETVPD, vpd);

		if (rc != -1) {
			return -1;
		}
	}

	return 0;
}



/**********************************************************************
*
*  Routine Name:	DPASetDebugForPort
*
*  Function:		OS specific function - Turns on driver relaying
*			of port data to DPA for a specific port.
*
*  Return Value:	 0 - All Okay.
*			-1 - Something bad happened.
*
**********************************************************************/

int
DPASetDebugForPort(struct deviceinfo *device, int onoff, int port)
{
	int ret;
	struct digi_debug setdebug;


	if (device->dpa_fd < 0)
		DPAOpenDevice(device);

	if (device->dpa_fd < 0)
		return -1;

	setdebug.port = port;
	setdebug.onoff = onoff;

        ret = ioctl(device->dpa_fd, DIGI_SETDEBUG, &setdebug);

	if (ret == -1) {
		return -1;
	}

	return 0;
}
