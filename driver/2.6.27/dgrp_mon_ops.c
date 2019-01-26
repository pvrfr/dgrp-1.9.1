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
 *     $Id: dgrp_mon_ops.c,v 1.7 2014/03/28 17:21:28 pberger Exp $
 *
 *  Description:
 *
 *     Handle the file operations required for the "monitor" devices.
 *     Includes those functions required to register the "mon" devices
 *     in "/proc".
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/

#include "linux_ver_fix.h"

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/cred.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/slab.h>
#endif

#include "drp.h"
#include "dgrp_common.h"
#include "dgrp_mon_ops.h"


/* Generic helper function declarations */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static int test_perm(int mode, int op);
#endif


/* File operation declarations */
static int dgrp_mon_open(struct inode *, struct file *);
static int dgrp_mon_release(struct inode *, struct file *);
static ssize_t dgrp_mon_read(struct file *, char *, size_t, loff_t *);
static ssize_t dgrp_mon_write(struct file *, const char *, size_t, loff_t *);
static long dgrp_mon_ioctl(struct file *file, unsigned int cmd, unsigned long arg);


/* Inode operation declarations */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int mon_chk_perm(struct inode *, int, unsigned int);
#else
static int mon_chk_perm(struct inode *, int);
#endif
#endif

static struct file_operations mon_ops = {
	.owner   = THIS_MODULE,		/* owner   */
	.read    = dgrp_mon_read,	/* read	   */
	.write   = dgrp_mon_write,	/* write   */
	.unlocked_ioctl = dgrp_mon_ioctl,	/* ioctl   */
	.open    = dgrp_mon_open,	/* open    */
	.release = dgrp_mon_release,	/* release */
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static struct inode_operations mon_inode_ops = {
	.permission = mon_chk_perm
};
#endif



/*****************************************************************************
*
* Function:
*
*    register_mon_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- pointer to the node structure for the PortServer being monitored
*    root -- /proc directory into which we will create the mon device
*
* Return Values:
*
*    0       -- success
*    -ENOMEM -- failed to create the /proc entry
*
* Description:
*
*    Adds a /proc entry of the form /proc/dgrp/mon/ID.  Also initializes
*    the monitoring specific variables of the node structure.
*
******************************************************************************/

int register_mon_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	char buf[3];
	int len;
	struct proc_dir_entry *de;

	ID_TO_CHAR(node->nd_ID, buf);

	len = strlen(buf);

	dbg_mon_trace(INIT, ("register mon: %s\n", buf));

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root);
	if (!de)
		return -ENOMEM;

	de->data = (void *) node;
	de->proc_iops = &mon_inode_ops;
	de->proc_fops = &mon_ops;
#else
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root, &mon_ops, (void *)node);
	if (!de)
		return -ENOMEM;

#endif

	node->nd_mon_de = de;

	sema_init(&node->nd_mon_semaphore, 1);

	return 0;
}


/*****************************************************************************
*
* Function:
*
*    unregister_mon_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- Node structure for PS being unregistered for monitoring
*    root -- /proc directory where "mon" entry currently resides
*
* Return Values:
*
*    0       -- Success
*    -EINVAL -- Fail if node was not configured for monitoring
*    -EBUSY  -- Fail if monitor device is open
*
* Description:
*
*    Delete the monitoring /proc entry for the PS, i.e. /proc/dgrp/mon/ID
*
******************************************************************************/

int unregister_mon_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	struct proc_dir_entry *de;

	if (!node->nd_mon_de)
		return -EINVAL;

	de = node->nd_mon_de;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	/* Don't unregister proc entries that are still being used.. */
	if ((atomic_read(&de->count)) != 1) {
		printk(KERN_ALERT "proc entry in use... Not removing...\n");
		return -EBUSY;
	}

	dbg_mon_trace(UNINIT, ("unregister mon: %s\n", de->name));

	dgrp_remove_proc_entry(de);
#else
	dgrp_remove_proc_entry(node, root);
#endif

	node->nd_mon_de = NULL;

	return 0;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_mon_open
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
*    Open the MON device for a particular PortServer
*
******************************************************************************/

static int dgrp_mon_open(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	int rtn = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	struct proc_dir_entry *de;
#endif

	dbg_mon_trace(OPEN, ("mon open(%p) start\n", file->private_data));

	rtn = try_module_get(THIS_MODULE);
	if (!rtn) {
		dbg_mon_trace(OPEN, ("mon open(%p) failed, could not get and increment module count\n",
			file->private_data));
		return -ENXIO;
	}

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
	 * Allocate the monitor buffer.
	 */

	/*
	 *  Grab the MON lock.
	 */
	down(&nd->nd_mon_semaphore);

	if (nd->nd_mon_buf != 0) {
		rtn = -EBUSY;
	} else {
		nd->nd_mon_buf = kmalloc(MON_MAX, GFP_KERNEL);

		if (nd->nd_mon_buf == 0) {
			rtn = -ENOMEM;
		} else {
			/*
			 *  Enter an RPDUMP file header into the buffer.
			 */

			uchar *buf = nd->nd_mon_buf;
			uint  time;
			struct timeval tv;

			strcpy(buf, RPDUMP_MAGIC);
			buf += strlen(buf) + 1;

			do_gettimeofday(&tv);

			/*
			 *  tv.tv_sec might be a 64 bit quantity.  Pare
			 *  it down to 32 bits before attempting to encode
			 *  it.
			 */
			time = (uint) (tv.tv_sec & 0xffffffff);

			dgrp_encode_u4(buf + 0, time);
			dgrp_encode_u2(buf + 4, 0);
			buf += 6;

			if (nd->nd_tx_module != 0) {
				buf[0] = RPDUMP_CLIENT;
				dgrp_encode_u4(buf + 1, 0);
				dgrp_encode_u2(buf + 5, 1);
				buf[7] = 0xf0 + nd->nd_tx_module;
				buf += 8;
			}

			if (nd->nd_rx_module != 0) {
				buf[0] = RPDUMP_SERVER;
				dgrp_encode_u4(buf + 1, 0);
				dgrp_encode_u2(buf + 5, 1);
				buf[7] = 0xf0 + nd->nd_rx_module;
				buf += 8;
			}

			nd->nd_mon_out = 0;
			nd->nd_mon_in  = buf - nd->nd_mon_buf;

			nd->nd_mon_lbolt = jiffies;
		}
	}

	up(&nd->nd_mon_semaphore);

done:
	dbg_mon_trace(OPEN, ("mon open(%p) return %d\n",
				file->private_data, rtn));

	if (rtn)
		module_put(THIS_MODULE);
	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_mon_release
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
*    Close the MON device for a particular PortServer
*
******************************************************************************/

static int dgrp_mon_release(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	uchar *buf;

	dbg_mon_trace(CLOSE, ("mon close(%p) start\n", file->private_data));

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd)
		goto done;

	/*
	 *  Free the monitor buffer.
	 */

	down(&nd->nd_mon_semaphore);

	assert(nd->nd_mon_buf != 0);

	buf = nd->nd_mon_buf;

	nd->nd_mon_buf = 0;
	nd->nd_mon_out = nd->nd_mon_in;

	kfree(buf);

	/*
	 *  Wakeup any thread waiting for buffer space.
	 */

	if (nd->nd_mon_flag & MON_WAIT_SPACE) {
		nd->nd_mon_flag &= ~MON_WAIT_SPACE;
		wake_up_interruptible(&nd->nd_mon_wqueue);
	}

	up(&nd->nd_mon_semaphore);

	/*
	 *  Make sure there is no thread in the middle of writing a packet.
	 */

	down(&nd->nd_net_semaphore);
	up(&nd->nd_net_semaphore);

	dbg_mon_trace(CLOSE, ("mon close(%p) return\n", file->private_data));

done:
	module_put(THIS_MODULE);
	file->private_data = NULL;
	return 0;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_mon_read
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

static ssize_t dgrp_mon_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct nd_struct *nd;
	int n;
	int r;
	int offset = 0;
	int res = 0;
	ssize_t rtn = 0;

	dbg_mon_trace(READ, ("mon read (%x)\n", file->private_data));

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

	down(&nd->nd_mon_semaphore);

	for (;;) {
		n = (nd->nd_mon_in - nd->nd_mon_out) & MON_MASK;

		if (n != 0)
			break;

		nd->nd_mon_flag |= MON_WAIT_DATA;

		up(&nd->nd_mon_semaphore);

		/*
		 * Go to sleep waiting until the condition becomes true.
		 */
		rtn = wait_event_interruptible(nd->nd_mon_wqueue,
			((nd->nd_mon_flag & MON_WAIT_DATA) == 0));

		if (rtn)
			goto done;

		down(&nd->nd_mon_semaphore);
	}

	/*
	 *  Read whatever is there.
	 */

	if (n > count)
		n = count;

	res = n;

	r = MON_MAX - nd->nd_mon_out;

	if (r <= n) {

		rtn = copy_to_user(buf, nd->nd_mon_buf + nd->nd_mon_out, r);
		if (rtn) {
			rtn = -EFAULT;
			up(&nd->nd_mon_semaphore);
			goto done;
		}

		nd->nd_mon_out = 0;
		n -= r;
		offset = r;
	}

	rtn = copy_to_user(buf + offset, nd->nd_mon_buf + nd->nd_mon_out, n);
	if (rtn) {
		rtn = -EFAULT;
		up(&nd->nd_mon_semaphore);
		goto done;
	}

	nd->nd_mon_out += n;

	*ppos += res;

	rtn = res;

	up(&nd->nd_mon_semaphore);

	/*
	 *  Wakeup any thread waiting for buffer space.
	 */

	if (nd->nd_mon_flag & MON_WAIT_SPACE) {
		nd->nd_mon_flag &= ~MON_WAIT_SPACE;
		wake_up_interruptible(&nd->nd_mon_wqueue);
	}

 done:
	dbg_mon_trace(READ, ("mon read (%p) count=%d return %d\n",
				nd, res, rtn));

	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_mon_write
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

static ssize_t dgrp_mon_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return -ENXIO;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_mon_ioctl
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
static long dgrp_mon_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}



/*****************************************************************************
*
* Function:
*
*    mon_chk_perm
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
*    mon_chk_perm (and its helpers) determine whether a particular operation
*    on an inode is valid, based on file permissions and ownership
*
******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int mon_chk_perm(struct inode *inode, int op, unsigned int flags)
#else
static int mon_chk_perm(struct inode *inode, int op)
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
