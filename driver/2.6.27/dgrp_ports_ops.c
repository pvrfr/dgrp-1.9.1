/*****************************************************************************
 *
 * Copyright 1999-2000 Digi International (www.digi.com)
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
 *****************************************************************************/

/****************************************************************************
 *
 *  Filename:
 *
 *     $Id: dgrp_ports_ops.c,v 1.6 2014/03/28 17:21:28 pberger Exp $
 *
 *  Description:
 *
 *     Handle the file operations required for the /proc/dgrp/ports/...
 *     devices.  Basically gathers tty status for the node and returns it.
 *
 *  Author:
 *
 *     James A. Puzzo
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
#include "dgrp_ports_ops.h"


/* Generic helper function declarations */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static int test_perm(int mode, int op);
#endif


/* File operation declarations */
static int dgrp_ports_open(struct inode *, struct file *);
static int dgrp_ports_release(struct inode *, struct file *);
static ssize_t dgrp_ports_read(struct file *, char *, size_t, loff_t *);
static ssize_t dgrp_ports_write(struct file *, const char *, size_t, loff_t *);


/* Inode operation declarations */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int ports_chk_perm(struct inode *, int, unsigned int);
#else
static int ports_chk_perm(struct inode *, int);
#endif
#endif

static struct file_operations ports_ops = {
	.owner   =   THIS_MODULE,	/* owner   */
	.read    =   dgrp_ports_read,	/* read	   */
	.write   =   dgrp_ports_write,	/* write   */
	.open    =   dgrp_ports_open,	/* open    */
	.release =   dgrp_ports_release	/* release */
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static struct inode_operations ports_inode_ops = {
	.permission = ports_chk_perm
};
#endif




/*****************************************************************************
*
* Function:
*
*    register_ports_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- Node for PortServer for which we gathering information
*    root -- /proc directory into which we are placing our ports device
*
* Return Values:
*
*    0       -- Success
*    -ENOMEM -- Couldn't add /proc device
*
* Description:
*
*    Adds the /proc/dgrp/ports/ID device and registers its entry points
*
******************************************************************************/

int register_ports_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	char buf[3];
	int len;
	struct proc_dir_entry *de;

	ID_TO_CHAR(node->nd_ID, buf);

	len = strlen(buf);

	dbg_ports_trace(INIT, ("register proc/dgrp/ports/%s\n", buf));

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root);
	if (!de)
		return -ENOMEM;

	de->data = (void *) node;
	de->proc_iops = &ports_inode_ops;
	de->proc_fops = &ports_ops;
#else
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root, &ports_ops, (void *)node);
	if (!de)
		return -ENOMEM;

#endif

	node->nd_ports_de = de;
	sema_init(&node->nd_ports_semaphore, 1);
	node->nd_ports_inuse = 0;

	return 0;
}



/*****************************************************************************
*
* Function:
*
*    unregister_ports_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- Node for PortServer for which we gathering information
*    root -- /proc directory from which we are removing our ports device
*
* Return Values:
*
*    0       -- Success
*    -EINVAL -- Device wasn't registered for ports information gathering
*    -EBUSY  -- Ports info gathering device was in use
*
* Description:
*
*    Removes the /proc/dgrp/ports/ID device and removes its entry points
*
******************************************************************************/

int unregister_ports_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	struct proc_dir_entry *de;

	if (!node->nd_ports_de)
		return -EINVAL;

	de = node->nd_ports_de;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	/* Don't unregister proc entries that are still being used.. */
	if ((atomic_read(&de->count)) != 1) {
		printk(KERN_ALERT "proc entry in use... Not removing...\n");
		return -EBUSY;
	}

	dbg_ports_trace(UNINIT, ("unregister /proc/dgrp/ports/%s\n", de->name));

	dgrp_remove_proc_entry(de);
#else
	dgrp_remove_proc_entry(node, root);
#endif
	node->nd_ports_de = NULL;

	return 0;
}



/*****************************************************************************
*
* Function:
*
*    dgrp_ports_open
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
*    Open the /proc/dgrp/ports/... device for a particular PortServer
*
******************************************************************************/

static int dgrp_ports_open(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	int rtn = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	struct proc_dir_entry *de;
#endif

	dbg_ports_trace(OPEN, ("ports open(%p) start\n", file->private_data));

	rtn = try_module_get(THIS_MODULE);
	if (!rtn) {
		dbg_ports_trace(OPEN, ("ports open(%p) failed, could not get and increment module count\n",
			file->private_data));
		return -ENXIO;
	}

	rtn = 0;

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

	/* Enforce exclusive access to "ports" device */
	down(&nd->nd_ports_semaphore);

	if (nd->nd_ports_inuse)
		rtn = -EBUSY;
	else
		nd->nd_ports_inuse++;

	up(&nd->nd_ports_semaphore);

done:
	dbg_ports_trace(OPEN, ("ports open(%p) return %d\n",
				file->private_data, rtn));

	if (rtn)
		module_put(THIS_MODULE);
	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_ports_release
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode, file (standard Linux release arguments)
*
* Return Values:
*
*    standard Linux release behavior
*
* Description:
*
*    Close the /proc/dgrp/ports/... device for a particular PortServer
*
******************************************************************************/

static int dgrp_ports_release(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;

	dbg_ports_trace(CLOSE, ("ports close(%p) start\n", file->private_data));

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd)
		goto done;

	/* Enforce exclusivity of the "ports" device */
	down(&nd->nd_ports_semaphore);

	nd->nd_ports_inuse = 0;

	up(&nd->nd_ports_semaphore);

	dbg_ports_trace(CLOSE, ("ports close(%p) return\n",
			file->private_data));

done:
	module_put(THIS_MODULE);
	file->private_data = NULL;
	return 0;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_ports_read
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
*    Read the data which describes the ports for the indicated PS.
*    The function builds each line of data on demand at runtime.
*    A single read will never return more than 1 display line of data.
*
******************************************************************************/

static ssize_t dgrp_ports_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct nd_struct *nd;

	static int currch = 0;
	static int currline = 0;
	static char linebuf[81];
	static int linepos = 0;

	int len;
	int left;
	int notdone;
	ssize_t res = 0;

	dbg_ports_trace(READ, ("ports read (%x)\n", file->private_data));

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd) {
		res = -ENXIO;
		goto done;
	}

	/*
	 *  Determine current position.
	 */
	if (*ppos == 0) {
		currch = 0;
		currline = 0;
		linepos = 0;
		linebuf[0] = 0;
	}

	if (!count) {
		res = 0;
		goto done;
	}

	/*
	 *  If we are at the beginning of a line, compose
	 *  the line.
	 */
	if (!linepos) {
		char tmp_id[20];

		if (currch >= nd->nd_chan_count) {
			res = 0;
			goto done;
		}

		switch (currline) {
		case 0:
		case 7:
			strcpy(linebuf, "#-----------------------------"
					"------------------------------"
					"----------\n");
			break;
		case 1:
		case 5:
			strcpy(linebuf, "#\n");
			break;
		case 2:
			ID_TO_CHAR(nd->nd_ID, tmp_id);
			sprintf(linebuf, "#  Ports for node \"%s\"\n", tmp_id);
			break;
		case 3:
			strcpy(linebuf, "#  The first four columns of the "
					"output are:\n");
			break;
		case 4:
			strcpy(linebuf, "#    port_num  tty_open_cnt  "
					"pr_open_cnt  tot_wait_cnt\n");
			break;
		case 6:
			strcpy(linebuf, "## ## ## ## MSTAT  IFLAG  OFLAG  "
					"CFLAG  BPS    DIGIFLAGS\n");
			break;
		default:
			{
				struct ch_struct *ch;
				struct un_struct *tun, *pun;
				unsigned int totcnt;

				ch = &nd->nd_chan[currch];
				tun = &(ch->ch_tun);
				pun = &(ch->ch_pun);

				/*
				 * If port is not open and no one is waiting to
				 * open it, the modem signal values can't be
				 * trusted, and will be zeroed.
				 */
				totcnt = tun->un_open_count +
					pun->un_open_count +
					ch->ch_wait_count[0] +
					ch->ch_wait_count[1] +
					ch->ch_wait_count[2];

				sprintf(linebuf, "%02d %02d %02d %02d "
					"0x%04X 0x%04X 0x%04X 0x%04X "
					"%-6d 0x%04X\n", currch,
					tun->un_open_count,
					pun->un_open_count,
					ch->ch_wait_count[0] +
					ch->ch_wait_count[1] +
					ch->ch_wait_count[2],
					(totcnt ? ch->ch_s_mlast : 0),
					ch->ch_s_iflag,
					ch->ch_s_oflag,
					ch->ch_s_cflag,
					(ch->ch_s_brate ? (1843200 / ch->ch_s_brate) : 0),
					ch->ch_digi.digi_flags);
				currch++;
				break;
			}
		}
	}

	notdone = 0;
	left = count;
	len = strlen(&linebuf[linepos]);

	if (len > left) {
		len = left;
		notdone = 1;
	}

	if (access_ok(VERIFY_WRITE, buf, len) == 0) {
		res = -EFAULT;
		goto done;
	}

	res = copy_to_user(buf, &linebuf[linepos], len);

	if (res) {
		res = -EFAULT;
		goto done;
	}

	if (notdone)
		linepos += len;
	else {
		linepos = 0;
		currline++;
	}

	res = len;
	*ppos += len;

done:
	dbg_ports_trace(READ, ("ports read (%p) return=%d\n", nd, res));

	return res;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_ports_write
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    standard Linux write entry point arguments
*
* Return Values:
*
*    -ENXIO
*
* Description:
*
*    It is never valid to write to the ports information gatherer
*
******************************************************************************/

static ssize_t dgrp_ports_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	return -ENXIO;
}



/*****************************************************************************
*
* Function:
*
*    ports_chk_perm
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode -- inode representing /proc/dgrp/ports/ID
*    op    -- operation for which we need to if there is permission
*
* Return Values:
*
*    0        -- Success, operation is permitted
*    -EACCESS -- Failure, operation is forbidden
*
* Description:
*
*    ports_chk_perm, and its helpers, determine whether a specified
*    operation is valid in the context of the supplied inode (both for
*    file permissions and ownership)
*
******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int ports_chk_perm(struct inode *inode, int op, unsigned int flags)
#else
static int ports_chk_perm(struct inode *inode, int op)
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
