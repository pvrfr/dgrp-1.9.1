/*****************************************************************************
 *
 * Copyright 1999 Digi International (www.digi.com)
 *     James Puzzo <jamesp at digi dot com>
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
 *
 *	NOTE TO LINUX KERNEL HACKERS:  DO NOT REFORMAT THIS CODE!
 *
 *	This is shared code between Digi's CVS archive and the
 *	Linux Kernel sources.
 *	Changing the source just for reformatting needlessly breaks
 *	our CVS diff history.
 *
 *	Send any bug fixes/changes to:  Eng.Linux at digi dot com.
 *	Thank you.
 *
 *
 *****************************************************************************/

/****************************************************************************
 *
 *  Filename:
 *
 *     $Id: dgrp_dpa_ops.c,v 1.8 2014/03/28 17:21:28 pberger Exp $
 *
 *  Description:
 *
 *     Handle the file operations required for the "dpa" devices.
 *     Includes those functions required to register the "dpa" devices
 *     in "/proc".
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/

#include "linux_ver_fix.h"

#include <linux/tty.h>
#include <linux/poll.h>
#include <linux/cred.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/slab.h>
#endif

#include "drp.h"
#include "dgrp_common.h"
#include "dgrp_dpa_ops.h"


/* Generic helper function declarations */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static int test_perm(int mode, int op);
#endif


/* File operation declarations */
static int dgrp_dpa_open(struct inode *, struct file *);
static int dgrp_dpa_release(struct inode *, struct file *);
static ssize_t dgrp_dpa_read(struct file *, char *, size_t, loff_t *);
static ssize_t dgrp_dpa_write(struct file *, const char *, size_t count, loff_t *);
static long dgrp_dpa_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int dgrp_dpa_select(struct file *, struct poll_table_struct *);


/* Inode operation declarations */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int dpa_chk_perm(struct inode *, int, unsigned int);
#else
static int dpa_chk_perm(struct inode *, int);
#endif
#endif

static struct file_operations dpa_ops = {
	.owner   =  THIS_MODULE,	/* owner	*/
	.read    =  dgrp_dpa_read,	/* read		*/
	.write   =  dgrp_dpa_write,	/* write	*/
	.poll    =  dgrp_dpa_select,	/* poll or select */
	.unlocked_ioctl =  dgrp_dpa_ioctl,	/* ioctl	*/
	.open    =  dgrp_dpa_open,	/* open		*/
	.release =  dgrp_dpa_release,	/* release	*/
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static struct inode_operations dpa_inode_ops = {
	.permission = dpa_chk_perm
};
#endif



struct digi_node {
	uint	nd_state;		/* Node state: 1 = up, 0 = down. */
	uint	nd_chan_count;		/* Number of channels found */
	uint	nd_tx_byte;		/* Tx data count */
	uint	nd_rx_byte;		/* RX data count */
	uchar	nd_ps_desc[MAX_DESC_LEN]; /* Description from PS */
};

#define DIGI_GETNODE      (('d'<<8) | 249)	/* get board info */


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

#define DIGI_GETCHAN      (('d'<<8) | 248)	/* get channel info */


struct digi_vpd {
	int vpd_len;
	char vpd_data[VPDSIZE];
};

#define DIGI_GETVPD       (('d'<<8) | 246)	/* get VPD info */


struct digi_debug {
	int onoff;
	int port;
};

#define DIGI_SETDEBUG      (('d'<<8) | 247)	/* set debug info */



/*****************************************************************************
*
* Function:
*
*    register_dpa_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- pointer to the node structure for the PortServer being monitored
*    root -- /proc directory into which we will create the dpa device
*
* Return Values:
*
*    0       -- success
*    -ENOMEM -- failed to create the /proc entry
*
* Description:
*
*    Adds a /proc entry of the form /proc/dgrp/dpa/ID.  Also initializes
*    the monitoring specific variables of the node structure.
*
******************************************************************************/

int register_dpa_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	char buf[3];
	int len;
	struct proc_dir_entry *de;

	ID_TO_CHAR(node->nd_ID, buf);

	len = strlen(buf);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root);
	if (!de)
		return -ENOMEM;

	de->data = (void *) node;
	de->proc_iops = &dpa_inode_ops;
	de->proc_fops = &dpa_ops;
#else
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root, &dpa_ops, (void *)node);
	if (!de)
		return -ENOMEM;

#endif

	node->nd_dpa_de = de;
	spin_lock_init(&node->nd_dpa_lock);

	return 0;
}


/*****************************************************************************
*
* Function:
*
*    unregister_dpa_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- Node structure for PS being unregistered for monitoring
*    root -- /proc directory where "dpa" entry currently resides
*
* Return Values:
*
*    0       -- Success
*    -EINVAL -- Fail if node was not configured for monitoring
*    -EBUSY  -- Fail if dpa device is open
*
* Description:
*
*    Delete the monitoring /proc entry for the PS, i.e. /proc/dgrp/dpa/ID
*
******************************************************************************/

int unregister_dpa_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	struct proc_dir_entry *de;

	if (!node->nd_dpa_de)
		return -EINVAL;

	de = node->nd_dpa_de;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	/* Don't unregister proc entries that are still being used.. */
	if ((atomic_read(&de->count)) != 1) {
		printk(KERN_ALERT "proc entry in use... Not removing...\n");
		return -EBUSY;
	}

	dgrp_remove_proc_entry(de);
#else
	dgrp_remove_proc_entry(node, root);
#endif
	node->nd_dpa_de = NULL;

	return 0;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_dpa_open
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode, file (standard Linux open arguments)
*
* Return Values:
*
*    0 or Error (standard Linux open behavior)
*
* Description:
*
*    Open the DPA device for a particular PortServer
*
******************************************************************************/

static int dgrp_dpa_open(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	int rtn = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	struct proc_dir_entry *de;
#endif

	rtn = try_module_get(THIS_MODULE);
	if (!rtn)
		return -ENXIO;

	rtn = 0;

	if (!capable(CAP_SYS_ADMIN)) {
		rtn = -EPERM;
		goto done;
	}

	/*
	 *  Make sure that the "private_data" field hasn't already been used.
	 */
	if (file->private_data) {
		rtn = -EINVAL;
		goto done;
	}

	/*
	 *  Get the node pointer, and fail if it doesn't exist.
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	de = PDE(inode);
	if (!de) {
		rtn = -ENXIO;
		goto done;
	}
	nd = (struct nd_struct *)de->data;
#else
	nd = (struct nd_struct *)PDE_DATA(inode);
#endif
	if (!nd) {
		rtn = -ENXIO;
		goto done;
	}

	file->private_data = (void *) nd;

	/*
	 * Allocate the DPA buffer.
	 */

	if (nd->nd_dpa_buf != 0) {
		rtn = -EBUSY;
	} else {
		nd->nd_dpa_buf = kmalloc(DPA_MAX, GFP_KERNEL);

		if (nd->nd_dpa_buf == 0) {
			rtn = -ENOMEM;
		} else {
			nd->nd_dpa_out = 0;
			nd->nd_dpa_in = 0;
			nd->nd_dpa_lbolt = jiffies;
		}
	}

done:

	if (rtn)
		module_put(THIS_MODULE);
	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_dpa_release
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode, file (standard Linux close arguments)
*
* Return Values:
*
*    standard Linux close return values
*
* Description:
*
*    Close the DPA device for a particular PortServer
*
******************************************************************************/

static int dgrp_dpa_release(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	uchar *buf;
	unsigned long lock_flags;

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd)
		goto done;

	/*
	 *  Free the dpa buffer.
	 */

	DGRP_LOCK(nd->nd_dpa_lock, lock_flags);

	assert(nd->nd_dpa_buf != 0);

	buf = nd->nd_dpa_buf;

	nd->nd_dpa_buf = 0;
	nd->nd_dpa_out = nd->nd_dpa_in;

	/*
	 *  Wakeup any thread waiting for buffer space.
	 */

	if (nd->nd_dpa_flag & DPA_WAIT_SPACE) {
		nd->nd_dpa_flag &= ~DPA_WAIT_SPACE;
		wake_up_interruptible(&nd->nd_dpa_wqueue);
	}

	DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);

	kfree(buf);

done:
	module_put(THIS_MODULE);
	file->private_data = NULL;
	return 0;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_dpa_read
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    standard Linux read entry point arguments
*
* Return Values:
*
*    standard Linux read entry point return values
*
* Description:
*
*    Copy data from the monitoring buffer to the user, freeing space
*    in the monitoring buffer for more messages
*
******************************************************************************/

static ssize_t dgrp_dpa_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct nd_struct *nd;
	int n;
	int r;
	int offset = 0;
	int res = 0;
	ssize_t rtn = 0;
	unsigned long lock_flags;

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd) {
		rtn = -ENXIO;
		goto done;
	}

	/*
	 *  Wait for some data to appear in the buffer.
	 */

	DGRP_LOCK(nd->nd_dpa_lock, lock_flags);

	for (;;) {
		n = (nd->nd_dpa_in - nd->nd_dpa_out) & DPA_MASK;

		if (n != 0)
			break;

		nd->nd_dpa_flag |= DPA_WAIT_DATA;

		DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);

		/*
		 * Go to sleep waiting until the condition becomes true.
		 */
		rtn = wait_event_interruptible(nd->nd_dpa_wqueue,
			((nd->nd_dpa_flag & DPA_WAIT_DATA) == 0));

		if (rtn)
			goto done;

		DGRP_LOCK(nd->nd_dpa_lock, lock_flags);
	}

	/*
	 *  Read whatever is there.
	 */

	if (n > count)
		n = count;

	res = n;

	r = DPA_MAX - nd->nd_dpa_out;

	if (r <= n) {

		DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);
		rtn = copy_to_user(buf, nd->nd_dpa_buf + nd->nd_dpa_out, r);
		DGRP_LOCK(nd->nd_dpa_lock, lock_flags);

		if (rtn) {
			rtn = -EFAULT;
			DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);
			goto done;
		}

		nd->nd_dpa_out = 0;
		n -= r;
		offset = r;
	}

	DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);
	rtn = copy_to_user(buf + offset, nd->nd_dpa_buf + nd->nd_dpa_out, n);
	DGRP_LOCK(nd->nd_dpa_lock, lock_flags);

	if (rtn) {
		rtn = -EFAULT;
		DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);
		goto done;
	}

	nd->nd_dpa_out += n;

	*ppos += res;

	rtn = res;

	/*
	 *  Wakeup any thread waiting for buffer space.
	 */

	n = (nd->nd_dpa_in - nd->nd_dpa_out) & DPA_MASK;

	if (nd->nd_dpa_flag & DPA_WAIT_SPACE && (DPA_MAX - n) > DPA_HIGH_WATER) {
		nd->nd_dpa_flag &= ~DPA_WAIT_SPACE;
		wake_up_interruptible(&nd->nd_dpa_wqueue);
	}

	DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);

 done:
	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_dpa_write
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    standard Linux write entry point parameters
*
* Return Values:
*
*    -ENXIO
*
* Description:
*
*    It is always invalid to write to the monitoring port
*
******************************************************************************/

static ssize_t dgrp_dpa_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return -ENXIO;
}



static unsigned int dgrp_dpa_select(struct file *file, struct poll_table_struct *table)
{
	unsigned int retval = 0;
	struct nd_struct *nd = file->private_data;

	if (nd->nd_dpa_out != nd->nd_dpa_in)
		retval |= POLLIN | POLLRDNORM; /* Conditionally readable */

	retval |= POLLOUT | POLLWRNORM;        /* Always writeable */

	return retval;
}




/*****************************************************************************
*
* Function:
*
*    dgrp_dpa_ioctl
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode, file, cmd, arg (standard Linux ioctl arguments)
*
* Return Values:
*
*    -EINVAL
*
* Description:
*
*    It is always invalid to execute ioctls on the monitoring device
*
******************************************************************************/
static long dgrp_dpa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	struct nd_struct  *nd;
	void __user *uarg = (void __user *) arg;

	nd = file->private_data;

	switch (cmd) {
	case DIGI_GETCHAN:
		{
			struct digi_chan getchan;
			struct ch_struct *ch;
			unsigned int port;

			if (copy_from_user(&getchan, uarg, sizeof(struct digi_chan)))
				return -EFAULT;

			port = getchan.ch_port;

			if (port < 0 || port > nd->nd_chan_count)
				return -EINVAL;

			ch = nd->nd_chan + port;

			getchan.ch_open = (ch->ch_open_count > 0) ? 1 : 0;
			getchan.ch_txcount = ch->ch_txcount;
			getchan.ch_rxcount = ch->ch_rxcount;
			getchan.ch_s_brate = ch->ch_s_brate;
			getchan.ch_s_estat = ch->ch_s_elast;
			getchan.ch_s_cflag = ch->ch_s_cflag;
			getchan.ch_s_iflag = ch->ch_s_iflag;
			getchan.ch_s_oflag = ch->ch_s_oflag;
			getchan.ch_s_xflag = ch->ch_s_xflag;
			getchan.ch_s_mstat = ch->ch_s_mlast;

			if (copy_to_user(uarg, &getchan, sizeof(struct digi_chan)))
				return -EFAULT;
		}
		break;


	case DIGI_GETNODE:
		{
			struct digi_node getnode;

			getnode.nd_state = (nd->nd_state & NS_READY) ? 1 : 0;
			getnode.nd_chan_count = nd->nd_chan_count;
			getnode.nd_tx_byte = nd->nd_tx_byte;
			getnode.nd_rx_byte = nd->nd_rx_byte;

			memset(&getnode.nd_ps_desc, '\0', MAX_DESC_LEN);
			strncpy(getnode.nd_ps_desc, nd->nd_ps_desc, MAX_DESC_LEN);

			if (copy_to_user(uarg, &getnode, sizeof(struct digi_node)))
				return -EFAULT;
		}
		break;


	case DIGI_SETDEBUG:
		{
			struct digi_debug setdebug;

			if (copy_from_user(&setdebug, uarg, sizeof(struct digi_debug)))
				return -EFAULT;

			nd->nd_dpa_debug = setdebug.onoff;
			nd->nd_dpa_port = setdebug.port;
		}
		break;


	case DIGI_GETVPD:
		{
			struct digi_vpd vpd;

			if (nd->nd_vpd_len > 0) {
				vpd.vpd_len = nd->nd_vpd_len;
				memcpy(&vpd.vpd_data, &nd->nd_vpd, nd->nd_vpd_len);
			} else {
				vpd.vpd_len = 0;
			}

			if (copy_to_user(uarg, &vpd, sizeof(struct digi_vpd)))
				return -EFAULT;
		}
		break;
	}

	return 0;
}



/*****************************************************************************
*
* Function:
*
*    dpa_chk_perm
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode -- inode structure representing the monitoring device
*    op    -- operation to test for permission
*
* Return Values:
*
*    0        -- Success, operation is permitted
*    -EACCESS -- Fail, operation is illegal
*
* Description:
*
*    dpa_chk_perm (and its helpers) determine whether a particular operation
*    on an inode is valid, based on file permissions and ownership
*
******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int dpa_chk_perm(struct inode *inode, int op, unsigned int flags)
#else
static int dpa_chk_perm(struct inode *inode, int op)
#endif
{
	return test_perm(inode->i_mode, op);
}

static int test_perm(int mode, int op)
{
	if (!current_euid())
		mode >>= 6;
	else if (dgrp_in_egroup_p(0))
		mode >>= 3;
	if ((mode & op & 0007) == op)
		return 0;
	if (capable(CAP_SYS_ADMIN))
		return 0;
	return -EACCES;
}
#endif




/*****************************************************************************
*
* Function:
*
*    dgrp_dpa
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd   -- pointer to a node structure
*    buf  -- buffer of data to copy to the monitoring buffer
*    nbuf -- number of bytes to transfer to the buffer
*
* Return Values:
*
*    none
*
* Description:
*
*    Called by the net device routines to send data to the device
*    monitor queue.  If the device monitor buffer is too full to
*    accept the data, it waits until the buffer is ready.
*
******************************************************************************/

static void dgrp_dpa(struct nd_struct *nd, uchar *buf, int nbuf)
{
	int n;
	int r;
	unsigned long lock_flags;

	/*
	 *  Grab DPA lock.
	 */
	DGRP_LOCK(nd->nd_dpa_lock, lock_flags);

	/*
	 *  Loop while data remains.
	 */
	while (nbuf > 0 && nd->nd_dpa_buf != 0) {

		n = (nd->nd_dpa_out - nd->nd_dpa_in - 1) & DPA_MASK;

		/*
		 * Enforce flow control on the DPA device.
		 */
		if (n < (DPA_MAX - DPA_HIGH_WATER))
			nd->nd_dpa_flag |= DPA_WAIT_SPACE;

		/*
		 * This should never happen, as the flow control above
		 * should have stopped things before they got to this point.
		 */
		if (n == 0) {
			DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);
			return;
		}

		/*
		 * Copy as much data as will fit.
		 */

		if (n > nbuf)
			n = nbuf;

		r = DPA_MAX - nd->nd_dpa_in;

		if (r <= n) {
			memcpy(nd->nd_dpa_buf + nd->nd_dpa_in, buf, r);

			n -= r;

			nd->nd_dpa_in = 0;

			buf += r;
			nbuf -= r;
		}

		memcpy(nd->nd_dpa_buf + nd->nd_dpa_in, buf, n);

		nd->nd_dpa_in += n;

		buf += n;
		nbuf -= n;

		assert(nd->nd_dpa_in < DPA_MAX);

		/*
		 *  Wakeup any thread waiting for data
		 */
		if (nd->nd_dpa_flag & DPA_WAIT_DATA) {
			nd->nd_dpa_flag &= ~DPA_WAIT_DATA;
			wake_up_interruptible(&nd->nd_dpa_wqueue);
		}
	}

	/*
	 *  Release the DPA lock.
	 */
	DGRP_UNLOCK(nd->nd_dpa_lock, lock_flags);
}




/*****************************************************************************
*
* Function:
*
*    dgrp_monitor_data
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd   -- pointer to a node structure
*    type -- type of message to be logged in the DPA buffer
*    buf  -- buffer of data to be logged in the DPA buffer
*    size -- number of bytes in the "buf" buffer
*
* Return Values:
*
*    none
*
* Description:
*
*    Builds a DPA data packet.
*
******************************************************************************/

void dgrp_dpa_data(struct nd_struct *nd, int type, uchar *buf, int size)
{
	uchar header[5];

	header[0] = type;

	dgrp_encode_u4(header + 1, size);

	dgrp_dpa(nd, header, sizeof(header));
	dgrp_dpa(nd, buf, size);
}

