/*****************************************************************************
 *
 * Copyright 1999 Digi International (www.digi.com)
 *     James Puzzo  <jamesp at digi dot com>
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
 *     $Id: dgrp_net_ops.c,v 1.14 2016/01/26 19:05:39 pberger Exp $
 *
 *  Description:
 *
 *     Handle the file operations required for the "network" devices.
 *     Includes those functions required to register the "net" devices
 *     in "/proc".
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/

#include "linux_ver_fix.h"

#include <linux/types.h>
#include <linux/string.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
#include <linux/device.h>
#endif
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/cred.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/slab.h>
#endif

#include "dgrp_net_ops.h"
#include "dgrp_dpa_ops.h"
#define MYFLIPLEN	TBUF_MAX

#include "digirp.h"              /* User ioctl API */
#include "drp.h"
#include "dgrp_common.h"
#include "dgrp_sysfs.h"


/*****************************************************************
 *    File specific "global" variables and declarations required
 *    for the poller.
 *****************************************************************/

static int    globals_initialized = 0;/* Set on the _first_ register */
static long   node_active_count;	/* one count for each open or
					 * pending open device, and one for
					 * the poller
					 */
static long   poll_round;		/* Timer rouding factor */
static ulong poll_time;			/* Time of next poll */

static void poll_start_timer(ulong time);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void poll_handler(ulong dummy);
static struct timer_list poll_timer = { function: poll_handler };
#else
static void poll_handler(struct timer_list *dummy);
static struct timer_list poll_timer;
#endif

/*
 *  Generic helper function declarations
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static int test_perm(int mode, int op);
#endif
static void   parity_scan(struct ch_struct *ch, unsigned char *cbuf,
				unsigned char *fbuf, int *len);

/*
 *  File operation declarations
 */
static int dgrp_net_open(struct inode *, struct file *);
static int dgrp_net_release(struct inode *, struct file *);
static ssize_t dgrp_net_read(struct file *, char *, size_t, loff_t *);
static ssize_t dgrp_net_write(struct file *, const char *, size_t, loff_t *);
static long dgrp_net_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int dgrp_net_select(struct file *file, struct poll_table_struct *table);

/*
 *  Inode operation declarations
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int net_chk_perm(struct inode *, int, unsigned int);
#else
static int net_chk_perm(struct inode *, int);
#endif
#endif

static struct file_operations net_ops = {
	.owner   =  THIS_MODULE,	/* owner   */
	.read    =  dgrp_net_read,	/* read	   */
	.write   =  dgrp_net_write,	/* write   */
	.poll    =  dgrp_net_select,	/* poll or select */
	.unlocked_ioctl =  dgrp_net_ioctl,	/* ioctl   */
	.open    =  dgrp_net_open,	/* open    */
	.release =  dgrp_net_release,	/* release */
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static struct inode_operations net_inode_ops = {
	.permission = net_chk_perm
};
#endif


/*****************************************************************************
*
* Function:
*
*    register_net_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- Node for PS whose daemon communicate device is being created
*    root -- Directory where net device will be create (/proc/dgrp/net)
*
* Return Values:
*
*    0       -- Success
*    -ENOMEM -- Couldn't create the /proc entry
*
* Description:
*
*    TODO
*
******************************************************************************/

int register_net_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	char buf[3];
	long len;
	struct proc_dir_entry *de;

	if (!globals_initialized) {
		globals_initialized = 1;
		spin_lock_init(&GLBL(poll_lock));
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
		init_timer(&poll_timer);
#else
		timer_setup(&poll_timer, poll_handler, 0);
#endif
	}

	ID_TO_CHAR(node->nd_ID, buf);

	len = strlen(buf);

	dbg_net_trace(INIT, ("register net: %s\n", buf));

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root);
	if (!de)
		return -ENOMEM;

	de->data = (void *) node;
	de->proc_iops = &net_inode_ops;
	de->proc_fops = &net_ops;
#else
	de = dgrp_create_proc_entry(buf, 0600 | S_IFREG, root, &net_ops, (void *)node);
#endif
	node->nd_net_de = de;
	sema_init(&node->nd_net_semaphore, 1);
	sema_init(&node->nd_writebuf_semaphore, 1);
	node->nd_state = NS_CLOSED;
	dgrp_create_node_class_sysfs_files(node);

	return 0;
}



/*****************************************************************************
*
* Function:
*
*    unregister_net_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    node -- Node for PS whose net communication device is being removed
*    root -- The directory in which the net device is being removed from
*
* Return Values:
*
*    0       -- Success
*    -EINVAL -- Node was not configured for net communication
*    -EBUSY  -- Net device is already in use
*
* Description:
*
*    TODO
*
******************************************************************************/

int unregister_net_device(struct nd_struct *node, struct proc_dir_entry *root)
{
	struct proc_dir_entry *de;

	if (!node->nd_net_de)
		return -EINVAL;

	de = node->nd_net_de;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	/* Don't unregister proc entries that are still being used.. */
	if ((atomic_read(&de->count)) != 1) {
		printk(KERN_ALERT "proc entry in use... Not removing...\n");
		return -EBUSY;
	}

	dbg_net_trace(UNINIT, ("unregister net: %s\n", de->name));

	dgrp_remove_proc_entry(de);
#else
	dgrp_remove_proc_entry(node, root);
#endif
	node->nd_net_de = NULL;
	dgrp_remove_node_class_sysfs_files(node);

	return 0;
}



/*****************************************************************************
*
* Function:
*
*    dgrp_dump
*
* Author:
*
*    Gene Olson      (HP-UX version)
*    James A. Puzzo  (ported to Linux)
*
* Parameters:
*
*    mem -- Memory location which should be printed to the console
*    len -- Number of bytes to be dumped
*
* Return Values:
*
*    none
*
* Description:
*
*    Prints memory for debugging purposes.
*
******************************************************************************/

static void dgrp_dump(uchar *mem, int len)
{
	int i;
	int n;
	char b[66];

	b[48] = ' ';
	b[49] = ' ';

	for (i = 0; i < len; i += 16) {
		for (n = 0; n < 16; n++) {
			if (i + n < len) {
				int m = mem[i + n];

				b[3*n+0] = "0123456789ABCDEF"[m >> 4];
				b[3*n+1] = "0123456789ABCDEF"[m & 0xf];
				b[3*n+2] = ' ';

				b[n+50] = (0x20 <= m && m <= 0x7e) ? m : '.';
			} else {
				b[3*n+0] = ' ';
				b[3*n+1] = ' ';
				b[3*n+2] = ' ';

				b[n+50]  = 0;
			}
		}

		dbg_net_trace(OUTPUT, ("net dump: %4x| %s\n", i, b));
	}
}

/*
 *      dgrp_read_data_block:
 *
 *      Read part or all of the control or data portion of a message.
 *
 *      "buf_size" is the size of the buffer to put it in.  "data_len"
 *      is the amount of data to be handled.  If buf_size is less than
 *      data_len, only buf_size bytes will be moved, and the rest will
 *      be skipped.
 *
 *      Always returns 0; Honest.
 *
 */

static int
dgrp_read_data_block(struct ch_struct *ch, void *flipbuf, int flipbuf_size, int data_len)
{
	int count;
	int t;
	int n;

	/*
	 * 	Copy the data from the circular queue rbuf to flipbuf
	 */
	count = min(flipbuf_size, data_len);

	if (count) {
		t = RBUF_MAX - ch->ch_rout;
		n = count;

		if (n >= t) {
			memcpy(flipbuf, ch->ch_rbuf + ch->ch_rout, t);
			flipbuf += t;
			n -= t;
			ch->ch_rout = 0;
		}

		memcpy(flipbuf, ch->ch_rbuf + ch->ch_rout, n);
		flipbuf += n;
		ch->ch_rout += n;
	}

	return count;
}

static void show_flags(struct tty_struct *tty)
{
	dbg_net_trace(INPUT, ("show_flags: "
			"L_ECHO:%d, "
			"L_ICANON:%d, "
			"L_IEXTEN:%d, "
			"L_ISIG:%d, "
			"I_BRKINT:%d, "
			"I_ICRNL:%d, "
			"I_INPCK:%d, "
			"I_ISTRIP:%d, "
			"I_IXON:%d, "
			"(C_CSIZE(tty) != CS8):%d, "
			"C_PARENB:%d, "
			"O_OPOST:%d, "
			"(MIN_CHAR(tty) != 1):%d, "
			"(TIME_CHAR(tty) != 0):%d\n",

			L_ECHO(tty),
			L_ICANON(tty),
			L_IEXTEN(tty),
			L_ISIG(tty),
			I_BRKINT(tty),
			I_ICRNL(tty),
			I_INPCK(tty),
			I_ISTRIP(tty),
			I_IXON(tty),
			(C_CSIZE(tty) != CS8),
			C_PARENB(tty),
			O_OPOST(tty),
			(MIN_CHAR(tty) != 1),
			(TIME_CHAR(tty) != 0)));

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
/* After 3.8.0, "real_raw" was moved out of tty_struct and into
 * n_tty.c's private struct n_tty_data.  So... here we make our
 * own raw mode assessment (based on Stevens' Advanced Programming
 * in the UNIX Environment, chapter 18 definition).
 */
static unsigned char
dgrp_is_real_raw(struct tty_struct *tty)
{
	show_flags(tty);

	if (L_ECHO(tty) || L_ICANON(tty) || L_IEXTEN(tty) || L_ISIG(tty) ||
			I_BRKINT(tty) || I_ICRNL(tty) ||I_INPCK(tty) ||
			I_ISTRIP(tty) || I_IXON(tty) ||
			C_CSIZE(tty) != CS8 || C_PARENB(tty) ||
			O_OPOST(tty) ||
			MIN_CHAR(tty) != 1 || TIME_CHAR(tty) != 0 ) {

			dbg_net_trace(INPUT, ("dgrp_is_real_raw: IS NOT RAW\n"));
        	return 0;       /* Not raw mode */
	} else {
			dbg_net_trace(INPUT, ("dgrp_is_real_raw: IS RAW\n"));
        	return 1;       /* Raw mode */
	}

}
#endif


/*****************************************************************************
*
* Function:
*
*    dgrp_input
*
* Author:
*
*    Gene Olson      (HP-UX version)
*    James A. Puzzo  (ported to Linux)
*
* Parameters:
*
*    ch -- channel structure for which data is being read and sent to the
*          line discipline
*
* Return Values:
*
*    none
*
* Description:
*
*    Copys the rbuf to the flipbuf and sends to line discipline.
*    Sends input buffer data to the line discipline.
*
*    There are several modes to consider here:
*    rawreadok, tty->real_raw, and IF_PARMRK
*
******************************************************************************/


static void dgrp_input(struct ch_struct *ch)
{
	struct nd_struct *nd;
	struct tty_struct *tty;
	int remain;
	int data_len;
	int len;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	int flip_len;
#endif
	int tty_count;
	ulong lock_flags;
	struct tty_ldisc *ld;
	uchar  *myflipbuf;
	uchar  *myflipflagbuf;
	unsigned char l_real_raw;

	if (!ch) {
		dbg_net_trace(INPUT, ("bogus input channel\n"));
		return;
	}

	nd = ch->ch_nd;

	if (!nd) {
		dbg_net_trace(INPUT, ("bogus input channel\n"));
		return;
	}

	DGRP_LOCK(nd->nd_lock, lock_flags);

	myflipbuf = nd->nd_inputbuf;
	myflipflagbuf = nd->nd_inputflagbuf;

	if (!ch->ch_open_count && !(ch->ch_tun).un_blocked_open) {
		ch->ch_rout = ch->ch_rin;
		DGRP_UNLOCK(nd->nd_lock, lock_flags);
		dbg_net_trace(INPUT, ("no open channels!\n"));
		return;
	}

	if (ch->ch_tun.un_flag & UN_CLOSING) {
		ch->ch_rout = ch->ch_rin;
		DGRP_UNLOCK(nd->nd_lock, lock_flags);
		dbg_net_trace(INPUT, ("unit closing, discarding input data\n"));
		return;
	}

	tty = (ch->ch_tun).un_tty;


	if (!tty || tty->magic != TTY_MAGIC) {
		ch->ch_rout = ch->ch_rin;
		DGRP_UNLOCK(nd->nd_lock, lock_flags);
		dbg_net_trace(INPUT, ("bad tty struct, discarding input data\n"));
		return;
	}

	tty_count = tty->count;
	if (!tty_count) {
		ch->ch_rout = ch->ch_rin;
		DGRP_UNLOCK(nd->nd_lock, lock_flags);
		dbg_net_trace(INPUT, ("TTY count is 0, discarding input data\n"));
		return;
	}


#if LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)
	if (tty->closing || test_bit(TTY_CLOSING, &tty->flags))
#else
	if (tty->closing || !tty->count)
#endif
	{
		ch->ch_rout = ch->ch_rin;
		DGRP_UNLOCK(nd->nd_lock, lock_flags);
		dbg_net_trace(INPUT, ("TTY is closing, discarding input data\n"));
		return;
	}

	DGRP_UNLOCK(nd->nd_lock, lock_flags);

	dbg_net_trace(INPUT, ("input(%x) start\n", nd->nd_ID));
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	l_real_raw = tty->real_raw;
#else
	l_real_raw = dgrp_is_real_raw(tty);
#endif
	dbg_net_trace(INPUT, ("dgrp_input l_real_raw:%d\n", l_real_raw));

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	/* Decide how much data we can send into the tty layer */
	if (GLBL(rawreadok) && l_real_raw)

		flip_len = MYFLIPLEN;
	else
		flip_len = TTY_FLIPBUF_SIZE;
#endif

	/* data_len should be the number of chars that we read in */
	data_len = (ch->ch_rin - ch->ch_rout) & RBUF_MASK;
	remain = data_len;

	/* len is the amount of data we are going to transfer here */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	len = min(data_len, flip_len);

	/* take into consideration length of ldisc */
	len = min(len, (N_TTY_BUF_SIZE - 1) - tty->read_cnt);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	len = tty_buffer_request_room(tty, data_len);
#else
	len = tty_buffer_request_room(&ch->port, data_len);
#endif

	ld = tty_ldisc_ref(tty);

#ifdef TTY_DONT_FLIP
	/*
	 * If the DONT_FLIP flag is on, don't flush our buffer, and act
	 * like the ld doesn't have any space to put the data right now.
	 */
	if (test_bit(TTY_DONT_FLIP, &tty->flags))
		len = 0;
#endif

	/*
	 * If we were unable to get a reference to the ld,
	 * don't flush our buffer, and act like the ld doesn't
	 * have any space to put the data right now.
	 */
	if (!ld) {
		len = 0;
	} else {
		/*
		 * If ld doesn't have a pointer to a receive_buf function,
		 * flush the data, then act like the ld doesn't have any
		 * space to put the data right now.
		 */
		if (!ld->ops->receive_buf) {
			DGRP_LOCK(nd->nd_lock, lock_flags);
			ch->ch_rout = ch->ch_rin;
			DGRP_UNLOCK(nd->nd_lock, lock_flags);
			len = 0;
		}
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
	dbg_net_trace(INPUT, ("rawreadok(%d) real_raw(%d) flip_len(%d) data_len(%d) len(%d)\n",
		GLBL(rawreadok), l_real_raw, flip_len, data_len, len));
#else
	dbg_net_trace(INPUT, ("rawreadok(%d) real_raw(%d) data_len(%d) len(%d)\n",
		GLBL(rawreadok), l_real_raw, data_len, len));
#endif



	/* Check DPA flow control */
	if (nd->nd_dpa_debug && nd->nd_dpa_flag & DPA_WAIT_SPACE &&
		 nd->nd_dpa_port == MINOR(tty_devnum(ch->ch_tun.un_tty))) {
		len = 0;
	}

	if (len && !(ch->ch_flag & CH_RXSTOP)) {
		dbg_net_trace(INPUT, ("OK, not CH_RXSTOP parmrk(%x)\n",
				ch->ch_iflag & IF_PARMRK));

		dgrp_read_data_block(ch, myflipbuf, len, len);

		/*
		 * In high performance mode, we don't have to update
		 * flag_buf or any of the counts or pointers into flip buf.
		 */
		if (!GLBL(rawreadok) || !l_real_raw) {
			if (I_PARMRK(tty) || I_BRKINT(tty) || I_INPCK(tty))
				parity_scan(ch, myflipbuf, myflipflagbuf, &len);
			else
				memset(myflipflagbuf, TTY_NORMAL, len);
		}


		dbg_net_trace(INPUT, ("len(%d)\n", len));

		if (nd->nd_dpa_debug && nd->nd_dpa_port == PORT_NUM(MINOR(tty_devnum(tty))))
			dgrp_dpa_data(nd, 1, myflipbuf, len);

		/*
		 * If we're doing raw reads, jam it right into the
		 * line disc bypassing the flip buffers.
		 */
		if (GLBL(rawreadok) && l_real_raw) {
			dbg_net_trace(INPUT,
					("Have rawreadok and l_real_raw! len:%d\n", len));
			ld->ops->receive_buf(tty, myflipbuf, NULL, len);
		} else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
			len = tty_buffer_request_room(tty, len);
			tty_insert_flip_string_flags(tty, myflipbuf, myflipflagbuf, len);

			/* Tell the tty layer its okay to "eat" the data now */
			tty_flip_buffer_push(tty);
#else
			len = tty_buffer_request_room(&ch->port, len);
			tty_insert_flip_string_flags(&ch->port, myflipbuf, myflipflagbuf,
					len);

			/* Tell the tty layer its okay to "eat" the data now */
			tty_flip_buffer_push(&ch->port);
#endif
		}

		ch->ch_rxcount += len;
	}

	if (ld)
		tty_ldisc_deref(ld);

	/*
	 * Wake up any sleepers (maybe dgrp close) that might be waiting
	 * for a channel flag state change.
	 */
	wake_up_interruptible(&ch->ch_flag_wait);

	dbg_net_trace(INPUT, ("input(%x) complete remain=%d\n",
	    nd->nd_ID, remain));

	dbg_net_trace(INPUT, ("input(%x) complete\n", nd->nd_ID));
}


/*****************************************************************************
*
*  parity_scan
*
*  Loop to inspect each single character or 0xFF escape.
*
*  if PARMRK & ~DOSMODE:
*     0xFF  0xFF           Normal 0xFF character, escaped
*                          to eliminate confusion.
*     0xFF  0x00  0x00     Break
*     0xFF  0x00  CC       Error character CC.
*     CC                   Normal character CC.
*
*  if PARMRK & DOSMODE:
*     0xFF  0x18  0x00     Break
*     0xFF  0x08  0x00     Framing Error
*     0xFF  0x04  0x00     Parity error
*     0xFF  0x0C  0x00     Both Framing and Parity error
*
*  TODO:  do we need to do the XMODEM, XOFF, XON, XANY processing??
*         as per protocol
*
******************************************************************************/

static void
parity_scan(struct ch_struct *ch, unsigned char *cbuf, unsigned char *fbuf, int *len)
{
	int l = *len;
	int count = 0;
	int DOS = ((ch->ch_iflag & IF_DOSMODE) == 0 ? 0 : 1);

	/*
	 * cout = character buffer for output
	 * fout = flag buffer for output
	 */
	unsigned char *in, *cout, *fout;
	unsigned char c;

	in = cbuf;
	cout = cbuf;
	fout = fbuf;

	dbg_net_trace(INPUT, ("in parity_scan, l(%d)\n", l));

	while (l--) {
		c = *in++ ;
		switch (ch->ch_pscan_state) {
		default:
			/* reset to sanity and fall through */
			ch->ch_pscan_state = 0 ;

		case 0:
			/* No FF seen yet */
			if (c == (unsigned char) '\377') {
				/* delete this character from stream */
				dbg_net_trace(INPUT, ("parity_scan: read first 0xFF\n"));
				ch->ch_pscan_state = 1;
			} else {
				*cout++ = c;
				*fout++ = TTY_NORMAL;
				count += 1;
			}
			break;

		case 1:
			/* first FF seen */
			if (c == (unsigned char) '\377') {
				/* doubled ff, transform to single ff */
				dbg_net_trace(INPUT, ("parity_scan: read second 0xFF\n"));
				*cout++ = c;
				*fout++ = TTY_NORMAL;
				count += 1;
				ch->ch_pscan_state = 0;
			} else {
				/* save value examination in next state */
				dbg_net_trace(INPUT, ("parity_scan: read second char\n"));
				ch->ch_pscan_savechar = c;
				ch->ch_pscan_state = 2;
			}
			break;
		case 2:
			/* third character of ff sequence */
			*cout++ = c;
			if (DOS) {
				if (ch->ch_pscan_savechar & 0x10) {
					dbg_net_trace(INPUT, ("parity_scan: received TTY_BREAK\n"));
					*fout++ = TTY_BREAK;
				} else if (ch->ch_pscan_savechar & 0x08) {
					dbg_net_trace(INPUT, ("parity_scan: received TTY_FRAME\n"));
					*fout++ = TTY_FRAME;
				} else {
					/*
					 * either marked as a parity error,
					 * indeterminate, or not in DOSMODE
					 * call it a parity error
					 */
					dbg_net_trace(INPUT, ("parity_scan: received TTY_PARITY\n"));
					*fout++ = TTY_PARITY;
				}
			} else {  /* not DOSMODE */

				/* case FF XX ?? where XX is not 00 */
				if (ch->ch_pscan_savechar & 0xff) {
					/* this should not happen */
					dbg_net_trace(INPUT, ("parity_scan: error!!!\n"));
					*fout++ = TTY_PARITY;
				}
				/* case FF 00 XX where XX is not 00 */
				else if (c & 0xff) {
					dbg_net_trace(INPUT, ("parity_scan: TTY_PARITY\n"));
					*fout++ = TTY_PARITY;
				}
				/* case FF 00 00 */
				else {
					dbg_net_trace(INPUT, ("parity_scan: received TTY_BREAK\n"));
					*fout++ = TTY_BREAK;
				}
			}
			count += 1;
			ch->ch_pscan_state = 0;
		}
	}
	*len = count;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_net_idle
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd -- node structure for the PS which needs its communications idled
*
* Return Values:
*
*    none
*
* Description:
*
*    Idles the network connection.
*
******************************************************************************/

static void dgrp_net_idle(struct nd_struct *nd)
{
	struct ch_struct *ch;
	int i;

	nd->nd_tx_work = 1;

	nd->nd_state = NS_IDLE;
	nd->nd_flag = 0;

	i = nd->nd_seq_out;

	for (;;) {
		if (nd->nd_seq_wait[i] != 0) {
			nd->nd_seq_wait[i] = 0;

			wake_up_interruptible(&nd->nd_seq_wque[i]);
		}

		if (i == nd->nd_seq_in)
			break;

		i = (i + 1) & SEQ_MASK;
	}

	nd->nd_seq_out = nd->nd_seq_in;

	nd->nd_unack = 0;
	nd->nd_remain = 0;

	nd->nd_tx_module = 0x10;
	nd->nd_rx_module = 0x00;

	for (i = 0, ch = nd->nd_chan; i < CHAN_MAX; i++, ch++) {
		ch->ch_state = CS_IDLE;

		ch->ch_otype = 0;
		ch->ch_otype_waiting = 0;
	}
}



/*****************************************************************************
*
* Function:
*
*    dgrp_chan_count
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd -- pointer to a node structure
*    n  -- what the new node channel count should be
*
* Return Values:
*
*    none
*
* Description:
*
*    Adjusts the node channel count.  If new ports have appeared, it tries
*    to signal those processes that might have been waiting for ports to
*    appear.  If ports have disappeared it tries to signal those processes
*    that might be hung waiting for a response for the now non-existant port.
*
******************************************************************************/

static void dgrp_chan_count(struct nd_struct *nd, int n)
{
	struct ch_struct *ch;
	uchar *tbuf;
	uchar *rbuf;
	int i;

	/*
	 *  Increase the number of channels, waking up any
	 *  threads that might be waiting for the channels
	 *  to appear.
	 */

	if (n > nd->nd_chan_count) {
		for (i = nd->nd_chan_count; i < n;) {
			ch = nd->nd_chan + i;

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
			spinunlock(&nd->nd_lock);
#endif

			tbuf = kmalloc(TBUF_MAX, GFP_KERNEL);
			rbuf = kmalloc(RBUF_MAX, GFP_KERNEL);

			if (tbuf == 0 || rbuf == 0) {
			        if (tbuf != 0)
				        kfree(tbuf);
				  
				if (rbuf != 0)
				        kfree(rbuf);

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
				spinlock(&nd->nd_lock);
#endif
				break;
			}

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
			spinlock(&nd->nd_lock);
#endif
			assert(ch->ch_tbuf == 0 && ch->ch_rbuf == 0);

			ch->ch_tbuf = tbuf;
			ch->ch_rbuf = rbuf;

			{
				struct device *classp;
				char name[DEVICE_NAME_SIZE];
				int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
				classp = tty_register_device(nd->nd_serial_ttdriver, i, NULL);
#else
				classp = tty_port_register_device(&ch->port, nd->nd_serial_ttdriver, i, NULL);
#endif
				ch->ch_tun.un_sysfs = classp;
				snprintf(name, DEVICE_NAME_SIZE, "tty_%d", i);

				dgrp_create_tty_sysfs(&ch->ch_tun, classp);
				ret = sysfs_create_link(&nd->nd_class_dev->kobj,
					&classp->kobj, name);

				/*
				 * NOTE: We don't support "cu" devices anymore,
				 * so you will notice we don't register them
				 * here anymore.
				 */
				if (GLBL(register_prdevices)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
					classp = tty_register_device(nd->nd_xprint_ttdriver, i, NULL);
#else
					classp = tty_port_register_device(&ch->port, nd->nd_xprint_ttdriver, i, NULL);
#endif
					ch->ch_pun.un_sysfs = classp;
					snprintf(name, DEVICE_NAME_SIZE, "pr_%d", i);

					dgrp_create_tty_sysfs(&ch->ch_pun, classp);
					ret = sysfs_create_link(&nd->nd_class_dev->kobj,
						&classp->kobj, name);
				}
			}
			nd->nd_chan_count = ++i;

			wake_up_interruptible(&ch->ch_flag_wait);
		}
	}

	/*
	 *  Decrease the number of channels, and wake up
	 *  any threads that might be waiting on the
	 *  channels that vanished.
	 */

	else if (n < nd->nd_chan_count) {
		for (i = nd->nd_chan_count; --i >= n;) {
			ch = nd->nd_chan + i;

			/*
			 *  Make any open ports inoperative.
			 */
			ch->ch_state = CS_IDLE;

			ch->ch_otype = 0;
			ch->ch_otype_waiting = 0;

			/*
			 *  Only "HANGUP" if we care about carrier
			 *  transitions and we are already open.
			 */
			if ((ch->ch_open_count != 0) &&
			   !(ALWAYS_FORCES_CARRIER(ch->ch_category) ||
			      (ch->ch_digi.digi_flags & DIGI_FORCEDCD) ||
			      (ch->ch_flag & CH_CLOCAL))) {
				ch->ch_flag |= CH_HANGUP;
				dbg_net_trace(POLL, ("calling carrier "
				 "from chan count\n"));
				dgrp_carrier(ch);
			}

			/*
			 * Unlike the CH_HANGUP flag above, use another
			 * flag to indicate to the RealPort state machine
			 * that this port has disappeared.
			 */
			if (ch->ch_open_count != 0)
				ch->ch_flag |= CH_PORT_GONE;

			wake_up_interruptible(&ch->ch_flag_wait);

			nd->nd_chan_count = i;

			tbuf = ch->ch_tbuf;
			rbuf = ch->ch_rbuf;

			ch->ch_tbuf = 0;
			ch->ch_rbuf = 0;

			nd->nd_chan_count = i;

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
			spinunlock(&nd->nd_lock);
#endif

			kfree(tbuf);
			kfree(rbuf);

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
			spinlock(&nd->nd_lock);
#endif

			{
				char name[DEVICE_NAME_SIZE];
				dgrp_remove_tty_sysfs(ch->ch_tun.un_sysfs);
				snprintf(name, DEVICE_NAME_SIZE, "tty_%d", i);
				sysfs_remove_link(&nd->nd_class_dev->kobj, name);
				tty_unregister_device(nd->nd_serial_ttdriver, i);

				/*
				 * NOTE: We don't support "cu" devices anymore,
				 * So you will notice we don't register them
				 * here anymore.
				 */

				if (GLBL(register_prdevices)) {
					dgrp_remove_tty_sysfs(ch->ch_pun.un_sysfs);
					snprintf(name, DEVICE_NAME_SIZE, "pr_%d", i);
					sysfs_remove_link(&nd->nd_class_dev->kobj, name);
					tty_unregister_device(nd->nd_xprint_ttdriver, i);
				}
			}
		}
	}

}


/*****************************************************************************
*
* Function:
*
*    dgrp_monitor
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

static void dgrp_monitor(struct nd_struct *nd, uchar *buf, int nbuf)
{
	int n;
	int r;
	int rtn;

	/*
	 *  Grab monitor lock.
	 */
	down(&nd->nd_mon_semaphore);

	/*
	 *  Loop while data remains.
	 */
	while (nbuf > 0 && nd->nd_mon_buf != 0) {
		/*
		 *  Determine the amount of available space left in the
		 *  buffer.  If there's none, wait until some appears.
		 */

		n = (nd->nd_mon_out - nd->nd_mon_in - 1) & MON_MASK;

		if (n == 0) {

			nd->nd_mon_flag |= MON_WAIT_SPACE;

			up(&nd->nd_mon_semaphore);

			/*
			 * Go to sleep waiting until the condition becomes true.
			 */
			rtn = wait_event_interruptible(nd->nd_mon_wqueue,
				((nd->nd_mon_flag & MON_WAIT_SPACE) == 0));

			/*
			 *  We can't exit here if we receive a signal, since
			 *  to do so would trash the debug stream.
			 */

			down(&nd->nd_mon_semaphore);

			continue;
		}

		/*
		 * Copy as much data as will fit.
		 */

		if (n > nbuf)
			n = nbuf;

		r = MON_MAX - nd->nd_mon_in;

		if (r <= n) {
			memcpy(nd->nd_mon_buf + nd->nd_mon_in, buf, r);

			n -= r;

			nd->nd_mon_in = 0;

			buf += r;
			nbuf -= r;
		}

		memcpy(nd->nd_mon_buf + nd->nd_mon_in, buf, n);

		nd->nd_mon_in += n;

		buf += n;
		nbuf -= n;

		assert(nd->nd_mon_in < MON_MAX);

		/*
		 *  Wakeup any thread waiting for data
		 */

		if (nd->nd_mon_flag & MON_WAIT_DATA) {
			nd->nd_mon_flag &= ~MON_WAIT_DATA;
			wake_up_interruptible(&nd->nd_mon_wqueue);
		}
	}

	/*
	 *  Release the monitor lock.
	 */
	up(&nd->nd_mon_semaphore);
}


/*****************************************************************************
*
* Function:
*
*    dgrp_encode_time
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd  -- pointer to a node structure
*    buf -- destination for time
*
* Return Values:
*
*    none
*
* Description:
*
*    Encodes "rpdump" time into a 4-byte quantity.  Time is measured since
*    open.
*
******************************************************************************/

static void dgrp_encode_time(struct nd_struct *nd, uchar *buf)
{
	ulong t;

	/*
	 *  Convert time in HZ since open to time in milliseconds
	 *  since open.
	 */
	t = jiffies - nd->nd_mon_lbolt;
	t = 1000 * (t / HZ) + 1000 * (t % HZ) / HZ;

	dgrp_encode_u4(buf, (uint)(t & 0xffffffff));
}



/*****************************************************************************
*
* Function:
*
*    dgrp_monitor_message
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd      -- pointer to a node structure
*    message -- message to be written to the message buffer
*
* Return Values:
*
*    none
*
* Description:
*
*    Builds an "rpdump" style message.
*
******************************************************************************/

static void dgrp_monitor_message(struct nd_struct *nd, char *message)
{
	uchar header[7];
	int n;

	header[0] = RPDUMP_MESSAGE;

	dgrp_encode_time(nd, header + 1);

	n = strlen(message);
	dgrp_encode_u2(header + 5, n);

	dgrp_monitor(nd, header, sizeof(header));
	dgrp_monitor(nd, (uchar *) message, n);
}



/*****************************************************************************
*
* Function:
*
*    dgrp_monitor_reset
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd -- pointer to node structure
*
* Return Values:
*
*    none
*
* Description:
*
*    Note a reset in the monitoring buffer.
*
******************************************************************************/

static void dgrp_monitor_reset(struct nd_struct *nd)
{
	uchar header[5];

	header[0] = RPDUMP_RESET;

	dgrp_encode_time(nd, header + 1);

	dgrp_monitor(nd, header, sizeof(header));
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
*    type -- type of message to be logged in the message buffer
*    buf  -- buffer of data to be logged in the message buffer
*    size -- number of bytes in the "buf" buffer
*
* Return Values:
*
*    none
*
* Description:
*
*    Builds a monitor data packet.
*
******************************************************************************/

static void dgrp_monitor_data(struct nd_struct *nd, int type, uchar *buf, int size)
{
	uchar header[7];

	header[0] = type;

	dgrp_encode_time(nd, header + 1);

	dgrp_encode_u2(header + 5, size);

	dgrp_monitor(nd, header, sizeof(header));
	dgrp_monitor(nd, buf, size);
}



/*****************************************************************************
*
* Function:
*
*    dgrp_net_open
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
*    Open the NET device for a particular PortServer
*
******************************************************************************/

static int dgrp_net_open(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	int     rtn = 0;
	ulong  lock_flags;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	struct proc_dir_entry *de;
#endif

	dbg_net_trace(OPEN, ("net open(%p) start\n", file->private_data));
	
	rtn = try_module_get(THIS_MODULE);
	if (!rtn) {
		dbg_net_trace(OPEN, ("net open(%p) failed, could not get and increment module count\n",
			file->private_data));
		return -EAGAIN;
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

	nd = (struct nd_struct *) de->data;
#else
	nd = (struct nd_struct *) PDE_DATA(inode);

#endif
	if (!nd) {
		rtn = -ENXIO;
		goto done;
	}

	file->private_data = (void *) nd;

	/*
	 *  This is an exclusive access device.  If it is already
	 *  open, return an error.
	 */

	/*
	 *  Grab the NET lock.
	 */
	down(&nd->nd_net_semaphore);

	if (nd->nd_state != NS_CLOSED) {
		rtn = -EBUSY;
		goto unlock;
	}

	/*
	 *  Initialize the link speed parameters.
	 */

	nd->nd_link.lk_fast_rate = UIO_MAX;
	nd->nd_link.lk_slow_rate = UIO_MAX;

	nd->nd_link.lk_fast_delay = 1000;
	nd->nd_link.lk_slow_delay = 1000;

	nd->nd_link.lk_header_size = 46;

	/*
	 *  Allocate the network read/write buffer.
	 */

	nd->nd_iobuf = kmalloc(UIO_MAX + 10, GFP_KERNEL);
	if (!nd->nd_iobuf) {
		rtn = -ENOMEM;
		goto unlock;
	}

	/*
	 * Allocate a buffer for doing the copy from user space to
	 * kernel space in the write routines.
	 */
	nd->nd_writebuf = kmalloc(WRITEBUFLEN, GFP_KERNEL);
	if (!nd->nd_writebuf) {
		rtn = -ENOMEM;
		goto unlock;
	}

	/*
	 * Allocate a buffer for doing the copy from kernel space to
	 * tty buffer space in the read routines.
	 */
	nd->nd_inputbuf = kmalloc(MYFLIPLEN, GFP_KERNEL);
	if (!nd->nd_inputbuf) {
		rtn = -ENOMEM;
		goto unlock;
	}

	/*
	 * Allocate a buffer for doing the copy from kernel space to
	 * tty buffer space in the read routines.
	 */
	nd->nd_inputflagbuf = kmalloc(MYFLIPLEN, GFP_KERNEL);
	if (!nd->nd_inputflagbuf) {
		rtn = -ENOMEM;
		goto unlock;
	}

	/*
	 *  The port is now open, so move it to the IDLE state
	 */

	dgrp_net_idle(nd);

	nd->nd_tx_time = jiffies;

	/*
	 *  If the polling routing is not running, start it running here
	 */
	DGRP_LOCK(GLBL(poll_lock), lock_flags);

	if (++node_active_count == 1) {
		node_active_count++;

		poll_time = jiffies + dgrp_jiffies_from_ms(dgrp_poll_tick);

		poll_start_timer(poll_time);
	}

	DGRP_UNLOCK(GLBL(poll_lock), lock_flags);

	dgrp_monitor_message(nd, "Net Open");


unlock:
	/*
	 *  Release the NET lock.
	 */
	up(&nd->nd_net_semaphore);

done:
	dbg_net_trace(OPEN, ("net open(%p) return %d\n", file->private_data, rtn));

	if (rtn)
		module_put(THIS_MODULE);

	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_net_release
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
*    0 or Error (standard Linux release behavior)
*
* Description:
*
*    Close the NET device for a particular PortServer.
*
******************************************************************************/

static int dgrp_net_release(struct inode *inode, struct file *file)
{
	struct nd_struct *nd;
	uchar *iobuf;
	ulong  lock_flags;

	dbg_net_trace(CLOSE, ("net close(%p) start\n", file->private_data));

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd)
		goto done;

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
/*	spinlock(&nd->nd_lock); */


	/*
	 *  Grab the NET lock.
	 */
	down(&nd->nd_net_semaphore);

	/*
	 *  Before "closing" the internal connection, make sure all
	 *  ports are "idle".
	 */
	dgrp_net_idle(nd);

	nd->nd_state = NS_CLOSED;
	nd->nd_flag = 0;

	/*
	 *  TODO ... must the wait queue be reset on close?
	 *  should any pending waiters be reset?
	 *  Let's decide to assert that the waitq is empty... and see
	 *  how soon we break.
	 */
	assert(!waitqueue_active(&nd->nd_tx_waitq));

	nd->nd_send = 0;

	/*
	 *  Deallocate the network IO buffer.
	 */
	iobuf = nd->nd_iobuf;
	nd->nd_iobuf = 0;

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
/*	spinunlock( &nd->nd_lock ); */

	kfree(iobuf);

	/*
	 *  Deallocate the write buffer.
	 */
	iobuf = nd->nd_writebuf;
	nd->nd_writebuf = 0;

	kfree(iobuf);

	/*
	 *  Deallocate the read buffers.
	 */
	iobuf = nd->nd_inputbuf;
	nd->nd_inputbuf = 0;
	kfree(iobuf);

	iobuf = nd->nd_inputflagbuf;
	nd->nd_inputflagbuf = 0;
	kfree(iobuf);


/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
/*	spinlock(&nd->nd_lock); */

	/*
	 *  Set the active port count to zero.
	 */
	dgrp_chan_count(nd, 0);

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
/*	spinunlock(&nd->nd_lock); */

	/*
	 *  Release the NET lock.
	 */
	up(&nd->nd_net_semaphore);

	/*
	 *  Cause the poller to stop scheduling itself if this is
	 *  the last active node.
	 */
	DGRP_LOCK(GLBL(poll_lock), lock_flags);

	node_active_count--;
	if (node_active_count == 1) {
		del_timer(&poll_timer);
		node_active_count = 0;
	}

	DGRP_UNLOCK(GLBL(poll_lock), lock_flags);

done:
	down(&nd->nd_net_semaphore);

	dgrp_monitor_message(nd, "Net Close");

	up(&nd->nd_net_semaphore);

	dbg_net_trace(CLOSE, ("net close(%p) return\n", file->private_data));

	module_put(THIS_MODULE);
	file->private_data = NULL;
	return 0;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_send
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd   -- pointer to a node structure
*    tmax -- maximum bytes to transmit
*
* Return Values:
*
*    Number of bytes sent
*
* Description:
*
*    Builds a packet for transmission to the server.
*
*
* NOTE TO LINUX KERNEL HACKERS:
*
*	Yes, this is a long function.  Very long.
*
*	This function is a constant among all of Digi's Unix RealPort drivers,
*	and for bug fixing purposes it is critical it stays exactly like it is!
*	Please do not try to split it up into a bunch of smaller functions!
*
*	Thanks, Scott.
*
******************************************************************************/

static int dgrp_send(struct nd_struct *nd, long tmax)
{
	struct ch_struct *ch;
	uchar *b;
	uchar *buf;
	uchar *mbuf;
	long mod;
	long port;
	long send;
	long maxport;
	long lastport;
	long rwin;
	long in;
	long n;
	long t;
	long ttotal;
	long tchan;
	long tsend;
	long tsafe;
	long work;
	long send_sync;
	long wanted_sync_port;
	ushort tdata[CHAN_MAX];
	long used_buffer;

	mod = 0;
	port = 0;
	work = 0;
	lastport = -1;
	wanted_sync_port = -1;

	ch = nd->nd_chan;

	buf = mbuf = b = nd->nd_iobuf + UIO_BASE;

	send_sync = nd->nd_link.lk_slow_rate < UIO_MAX;

	ttotal = 0;
	tchan = 0;

	memset(tdata, '\0', sizeof(tdata));

	/*
	 * If there are any outstanding requests to be serviced, service them here.
	 */
	if (nd->nd_send & NR_PASSWORD) {

		/*
		 *  Send Password response.
		 */

		b[0] = 0xfc;
		b[1] = 0x20;
		dgrp_encode_u2(b + 2, strlen(nd->password));
		b += 4;
		b += strlen(nd->password);
		nd->nd_send &= ~(NR_PASSWORD);
	}


	/*
	 *  Loop over all modules to generate commands, and determine
	 *  the amount of data queued for transmit.
	 */

	for (mod = 0; port < nd->nd_chan_count; mod++) {
		/*
		 *  If this is not the current module, enter a module select
		 *  code in the buffer.
		 */

		if (mod != nd->nd_tx_module)
			mbuf = ++b;

		/*
		 *  Loop to process one module.
		 */

		maxport = port + 16;

		if (maxport > nd->nd_chan_count)
			maxport = nd->nd_chan_count;

		for (; port < maxport; port++, ch++) {
			/*
			 *  Switch based on channel state.
			 */

			switch (ch->ch_state) {
			/*
			 *  Send requests when the port is closed, and there
			 *  are no Open, Close or Cancel requests expected.
			 */

			case CS_IDLE:
				{
					/*
					 * Wait until any open error code
					 * has been delivered to all
					 * associated ports.
					 */

					if (ch->ch_open_error != 0) {
						if (ch->ch_wait_count[ch->ch_otype] != 0) {
							work = 1;
							break;
						}

						ch->ch_open_error = 0;
					}

					/*
					 *  Wait until the channel HANGUP flag is reset
					 *  before sending the first open.  We can only
					 *  get to this state after a server disconnect.
					 */

					if ((ch->ch_flag & CH_HANGUP) != 0)
						break;

					/*
					 *  If recovering from a TCP disconnect, or if
					 *  there is an immediate open pending, send an
					 *  Immediate Open request.
					 */
#if 0
					if (ch->ch_open_count != 0 ||
#else
					if ((ch->ch_flag & CH_PORT_GONE) ||
#endif
					    ch->ch_wait_count[OTYPE_IMMEDIATE] != 0) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 10;
						b[2] = 0;
						b += 3;

						ch->ch_state = CS_WAIT_OPEN;
						ch->ch_otype = OTYPE_IMMEDIATE;
						break;
					}

					/*
					 *  If there is no Persistent or Incoming Open on
					 *  the wait list in the server, and a thread is
					 *  waiting for a Persistent or Incoming Open,
					 *  send a Persistent or Incoming Open Request.
					 */

					if (ch->ch_otype_waiting == 0) {
						if (ch->ch_wait_count[OTYPE_PERSISTENT] != 0) {
							b[0] = 0xb0 + (port & 0xf);
							b[1] = 10;
							b[2] = 1;
							b += 3;

							ch->ch_state = CS_WAIT_OPEN;
							ch->ch_otype = OTYPE_PERSISTENT;
						} else if (ch->ch_wait_count[OTYPE_INCOMING] != 0) {
							b[0] = 0xb0 + (port & 0xf);
							b[1] = 10;
							b[2] = 2;
							b += 3;

							ch->ch_state = CS_WAIT_OPEN;
							ch->ch_otype = OTYPE_INCOMING;
						}
						break;
					}

					/*
					 *  If a Persistent or Incoming Open is pending in
					 *  the server, but there is no longer an open
					 *  thread waiting for it, cancel the request.
					 */

					if (ch->ch_wait_count[ch->ch_otype_waiting] == 0) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 10;
						b[2] = 4;
						b += 3;

						ch->ch_state = CS_WAIT_CANCEL;
						ch->ch_otype = ch->ch_otype_waiting;
					}
				}
				break;

			/*
			 *  Send port parameter queries.
			 */
			case CS_SEND_QUERY:
				{
				dbg_net_trace(READ, ("case CS_SEND_QUERY, ch_flag(%x)\n", ch->ch_flag));
					/*
					 *  Clear out all FEP state that might remain
					 *  from the last connection.
					 */

					ch->ch_flag |= CH_PARAM;

					ch->ch_flag &= ~CH_RX_FLUSH;

					ch->ch_expect = 0;

					ch->ch_s_tin   = 0;
					ch->ch_s_tpos  = 0;
					ch->ch_s_tsize = 0;
					ch->ch_s_treq  = 0;
					ch->ch_s_elast = 0;

					ch->ch_s_rin   = 0;
					ch->ch_s_rwin  = 0;
					ch->ch_s_rsize = 0;

					ch->ch_s_tmax  = 0;
					ch->ch_s_ttime = 0;
					ch->ch_s_rmax  = 0;
					ch->ch_s_rtime = 0;
					ch->ch_s_rlow  = 0;
					ch->ch_s_rhigh = 0;

					ch->ch_s_brate = 0;
					ch->ch_s_iflag = 0;
					ch->ch_s_cflag = 0;
					ch->ch_s_oflag = 0;
					ch->ch_s_xflag = 0;

					ch->ch_s_mout  = 0;
					ch->ch_s_mflow = 0;
					ch->ch_s_mctrl = 0;
					ch->ch_s_xon   = 0;
					ch->ch_s_xoff  = 0;
					ch->ch_s_lnext = 0;
					ch->ch_s_xxon  = 0;
					ch->ch_s_xxoff = 0;

					/*
					 *  Send Sequence Request.
					 */

					b[0] = 0xb0 + (port & 0xf);
					b[1] = 14;
					b += 2;

					/*
					 *  Configure Event Conditions Packet.
					 */

					b[0] = 0xb0 + (port & 0xf);
					b[1] = 42;
					dgrp_encode_u2(b + 2, 0x02c0);
					b[4] = (DM_DTR | DM_RTS | DM_CTS |
						DM_DSR | DM_RI | DM_CD);
					b += 5;

					/*
					 *  Send Status Request.
					 */

					b[0] = 0xb0 + (port & 0xf);
					b[1] = 16;
					b += 2;

					/*
					 *  Send Buffer Request.
					 */

					b[0] = 0xb0 + (port & 0xf);
					b[1] = 20;
					b += 2;

					/*
					 *  Send Port Capability Request.
					 */

					b[0] = 0xb0 + (port & 0xf);
					b[1] = 22;
					b += 2;

					ch->ch_expect = (RR_SEQUENCE |
							RR_STATUS  |
							RR_BUFFER |
							RR_CAPABILITY);

					ch->ch_state = CS_WAIT_QUERY;

					/*
					 *  Raise modem signals.
					 */

					b[0] = 0xb0 + (port & 0xf);
					b[1] = 44;

					if (ch->ch_flag & CH_PORT_GONE)
						b[2] = ch->ch_s_mout  = ch->ch_mout;
					else
						b[2] = ch->ch_s_mout  = ch->ch_mout = DM_DTR | DM_RTS;

					b[3] = ch->ch_s_mflow = 0;
					b[4] = ch->ch_s_mctrl = ch->ch_mctrl = 0;
					b += 5;

					if (ch->ch_flag & CH_PORT_GONE)
						ch->ch_flag &= ~CH_PORT_GONE;

				}
				break;

			/*
			 *  Handle normal open and ready mode.
			 */

			case CS_READY:

				/*
				 *  If the port is not open, and there are no
				 *  no longer any ports requesting an open,
				 *  then close the port.
				 */

				dbg_net_trace(READ, ("case CS_READY, ch_flag(%x)\n", ch->ch_flag));
				if (ch->ch_open_count == 0 &&
				    ch->ch_wait_count[ch->ch_otype] == 0) {
					goto send_close;
				}

				/*
				 *  Process waiting input.
				 *
				 *  If there is no one to read it, discard the data.
				 *
				 *  Otherwise if we are not in fastcook mode, or if
				 *  there is a fastcook thread waiting for data, send
				 *  the data to the line discipline.
				 */

				if (ch->ch_rin != ch->ch_rout) {
					if (ch->ch_tun.un_open_count == 0 ||
					     (ch->ch_tun.un_flag & UN_CLOSING) ||
					    (ch->ch_cflag & CF_CREAD) == 0) {
						ch->ch_rout = ch->ch_rin;
					} else if ((ch->ch_flag & CH_FAST_READ) == 0 ||
							ch->ch_inwait != 0) {
						dgrp_input(ch);

						if (ch->ch_rin != ch->ch_rout)
							work = 1;
					}
				}

				/*
				 *  Handle receive flush, and changes to
				 *  server port parameters.
				 */

				if ((ch->ch_flag & (CH_RX_FLUSH | CH_PARAM)) != 0) {
					/*
					 *  If we are in receive flush mode, and enough
					 *  data has gone by, reset receive flush mode.
					 */

					if ((ch->ch_flag & CH_RX_FLUSH) != 0) {
						if (((ch->ch_flush_seq - nd->nd_seq_out) & SEQ_MASK) >
						    ((nd->nd_seq_in - nd->nd_seq_out) & SEQ_MASK)) {
							dbg_net_trace(READ, ("dgrp send(%x:%x) RX has flushed\n",
								MAJOR(tty_devnum(ch->ch_tun.un_tty)),
								MINOR(tty_devnum(ch->ch_tun.un_tty))));
							ch->ch_flag &= ~CH_RX_FLUSH;
						} else {
							dbg_net_trace(READ, ("dgrp send(%x:%x) Waiting to flush RX\n",
								MAJOR(tty_devnum(ch->ch_tun.un_tty)),
								MINOR(tty_devnum(ch->ch_tun.un_tty))));
							work = 1;
						}
					}

					/*
					 *  Send TMAX, TTIME.
					 */

					if (ch->ch_s_tmax  != ch->ch_tmax ||
					    ch->ch_s_ttime != ch->ch_ttime) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 48;
						dgrp_encode_u2(b + 2, ch->ch_s_tmax  = ch->ch_tmax);
						dgrp_encode_u2(b + 4, ch->ch_s_ttime = ch->ch_ttime);
						b += 6;
					}

					/*
					 *  Send RLOW, RHIGH.
					 */

					if (ch->ch_s_rlow  != ch->ch_rlow ||
					    ch->ch_s_rhigh != ch->ch_rhigh) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 45;
						dgrp_encode_u2(b + 2, ch->ch_s_rlow  = ch->ch_rlow);
						dgrp_encode_u2(b + 4, ch->ch_s_rhigh = ch->ch_rhigh);
						b += 6;
					}

					/*
					 *  Send BRATE, CFLAG, IFLAG, OFLAG, XFLAG.
					 */

					if (ch->ch_s_brate != ch->ch_brate ||
					    ch->ch_s_cflag != ch->ch_cflag ||
					    ch->ch_s_iflag != ch->ch_iflag ||
					    ch->ch_s_oflag != ch->ch_oflag ||
					    ch->ch_s_xflag != ch->ch_xflag) {
						b[0] = 0xb0 + (port & 0xf); b[1] = 40;
						dgrp_encode_u2(b +  2, ch->ch_s_brate = ch->ch_brate);
						dgrp_encode_u2(b +  4, ch->ch_s_cflag = ch->ch_cflag);
						dgrp_encode_u2(b +  6, ch->ch_s_iflag = ch->ch_iflag);
						dgrp_encode_u2(b +  8, ch->ch_s_oflag = ch->ch_oflag);
						dgrp_encode_u2(b + 10, ch->ch_s_xflag = ch->ch_xflag);
						b += 12;
					}

					/*
					 *  Send MOUT, MFLOW, MCTRL.
					 */

					if (ch->ch_s_mout  != ch->ch_mout  ||
					    ch->ch_s_mflow != ch->ch_mflow ||
					    ch->ch_s_mctrl != ch->ch_mctrl) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 44;
						b[2] = ch->ch_s_mout  = ch->ch_mout;
						b[3] = ch->ch_s_mflow = ch->ch_mflow;
						b[4] = ch->ch_s_mctrl = ch->ch_mctrl;
						b += 5;
					}

					/*
					 *  Send Flow control characters.
					 */

					if (ch->ch_s_xon   != ch->ch_xon   ||
					    ch->ch_s_xoff  != ch->ch_xoff  ||
					    ch->ch_s_lnext != ch->ch_lnext ||
					    ch->ch_s_xxon  != ch->ch_xxon  ||
					    ch->ch_s_xxoff != ch->ch_xxoff) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 46;
						b[2] = ch->ch_s_xon   = ch->ch_xon;
						b[3] = ch->ch_s_xoff  = ch->ch_xoff;
						b[4] = ch->ch_s_lnext = ch->ch_lnext;
						b[5] = ch->ch_s_xxon  = ch->ch_xxon;
						b[6] = ch->ch_s_xxoff = ch->ch_xxoff;
						b += 7;
					}

					/*
					 *  Send RMAX, RTIME.
					 */

					if (ch->ch_s_rmax != ch->ch_rmax ||
					    ch->ch_s_rtime != ch->ch_rtime) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 47;
						dgrp_encode_u2(b + 2, ch->ch_s_rmax  = ch->ch_rmax);
						dgrp_encode_u2(b + 4, ch->ch_s_rtime = ch->ch_rtime);
						b += 6;
					}

					ch->ch_flag &= ~CH_PARAM;
					wake_up_interruptible(&ch->ch_flag_wait);
				}


				/*
				 *  Handle action commands.
				 */

				if (ch->ch_send != 0) {
					/* int send = ch->ch_send & ~ch->ch_expect; */
					send = ch->ch_send & ~ch->ch_expect;
					dbg_net_trace(READ, ("case CS_READY, handle Open Request Packets\n"));

					/*
					 *  Send character immediate.
					 */

					if ((send & RR_TX_ICHAR) != 0) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 60;
						b[2] = ch->ch_xon;
						b += 3;

						ch->ch_expect |= RR_TX_ICHAR;
					}

					/*
					 *  BREAK request.
					 */

					if ((send & RR_TX_BREAK) != 0) {
						if (ch->ch_break_time != 0) {
							b[0] = 0xb0 + (port & 0xf);
							b[1] = 61;
							dgrp_encode_u2(b + 2, ch->ch_break_time);
							b += 4;

							ch->ch_expect |= RR_TX_BREAK;
							ch->ch_break_time = 0;
						} else {
							ch->ch_send &= ~RR_TX_BREAK;
							ch->ch_flag &= ~CH_TX_BREAK;
							wake_up_interruptible(&ch->ch_flag_wait);
						}
					}

					/*
					 *  Flush input/output buffers.
					 */

					if ((send & (RR_RX_FLUSH | RR_TX_FLUSH)) != 0) {
						b[0] = 0xb0 + (port & 0xf);
						b[1] = 62;
						b[2] = ((send & RR_TX_FLUSH) == 0 ? 1 :
							(send & RR_RX_FLUSH) == 0 ? 2 : 3);
						dbg_net_trace(OUTPUT, ("dgrp send(%x:%x) sending FLUSH command %d\n",
							MAJOR(tty_devnum(ch->ch_tun.un_tty)),
							MINOR(tty_devnum(ch->ch_tun.un_tty)), b[2]));
						b += 3;

						if (send & RR_RX_FLUSH) {
							ch->ch_flush_seq = nd->nd_seq_in;
							ch->ch_flag |= CH_RX_FLUSH;
							work = 1;
							send_sync = 1;
							wanted_sync_port = port;
						}

						ch->ch_send &= ~(RR_RX_FLUSH | RR_TX_FLUSH);
					}

					/*
					 *  Pause input/output.
					 */

					if ((send & (RR_RX_STOP | RR_TX_STOP)) != 0) {
						int f = 0;

						if ((send & RR_TX_STOP) != 0) {
							f |= EV_OPU;
							dbg_net_trace(OUTPUT, ("dgrp send(%x:%x) pausing transmitter\n",
								MAJOR(tty_devnum(ch->ch_tun.un_tty)),
								MINOR(tty_devnum(ch->ch_tun.un_tty))));
						}

						if ((send & RR_RX_STOP) != 0) {
							f |= EV_IPU;
							dbg_net_trace(OUTPUT, ("dgrp send(%x:%x) pausing receiver\n",
								MAJOR(tty_devnum(ch->ch_tun.un_tty)),
								MINOR(tty_devnum(ch->ch_tun.un_tty))));
						}

						b[0] = 0xb0 + (port & 0xf);
						b[1] = 63;
						b[2] = f;

						b += 3;

						ch->ch_send &= ~(RR_RX_STOP | RR_TX_STOP);
					}

					/*
					 *  Start input/output.
					 */

					if ((send & (RR_RX_START | RR_TX_START)) != 0) {
						int f = 0;

						if ((send & RR_TX_START) != 0) {
							dbg_net_trace(OUTPUT, ("dgrp send(%x:%x) starting transmitter\n",
								MAJOR(tty_devnum(ch->ch_tun.un_tty)),
								MINOR(tty_devnum(ch->ch_tun.un_tty))));
							f |= EV_OPU | EV_OPS | EV_OPX;
						}

						if ((send & RR_RX_START) != 0) {
							dbg_net_trace(OUTPUT, ("dgrp send(%x:%x) starting receiver\n",
								MAJOR(tty_devnum(ch->ch_tun.un_tty)),
								MINOR(tty_devnum(ch->ch_tun.un_tty))));
							f |= EV_IPU | EV_IPS;
						}

						b[0] = 0xb0 + (port & 0xf);
						b[1] = 64;
						b[2] = f;

						b += 3;

						ch->ch_send &= ~(RR_RX_START | RR_TX_START);
					}
				}


				/*
				 *  Send a window sequence to acknowledge received data.
				 */

				rwin = (ch->ch_s_rin +
					((ch->ch_rout - ch->ch_rin - 1) & RBUF_MASK));

				n = (rwin - ch->ch_s_rwin) & 0xffff;

				if (n >= RBUF_MAX / 4) {
					b[0] = 0xa0 + (port & 0xf);
					dgrp_encode_u2(b + 1, ch->ch_s_rwin = rwin);
					b += 3;
				}

				/*
				 *  If the terminal is waiting on LOW water or EMPTY,
				 *  and the condition is now satisfied, call the
				 *  line discipline to put more data in the buffer.
				 */

				n = (ch->ch_tin - ch->ch_tout) & TBUF_MASK;

				if ((ch->ch_tun.un_flag & (UN_EMPTY|UN_LOW)) != 0) {
					if ((ch->ch_tun.un_flag & UN_LOW) != 0 ?
					    (n <= TBUF_LOW) :
/* Under 3.X kernels, the tpos == tin sometimes fails so we don't
 * wakeup on UN_EMPTY */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
					    (n == 0 && ch->ch_s_tpos == ch->ch_s_tin)
#else
					    (n == 0)
#endif
							) {
						ch->ch_tun.un_flag &= ~(UN_EMPTY|UN_LOW);

						if (waitqueue_active(&((ch->ch_tun.un_tty)->write_wait)))
							wake_up_interruptible(&((ch->ch_tun.un_tty)->write_wait));
						#ifdef SERIAL_HAVE_POLL_WAIT
						if (waitqueue_active(&((ch->ch_tun.un_tty)->poll_wait)))
							wake_up_interruptible(&((ch->ch_tun.un_tty)->poll_wait));
						#endif
						tty_wakeup(ch->ch_tun.un_tty);
						n = (ch->ch_tin - ch->ch_tout) & TBUF_MASK;
					}
				}

				/*
				 * If the printer is waiting on LOW water, TIME, EMPTY
				 * or PWAIT, and is now ready to put more data in the
				 * buffer, call the line discipline to do the job.
				 */

				if (ch->ch_pun.un_open_count &&
				    (ch->ch_pun.un_flag &
				    (UN_EMPTY|UN_TIME|UN_LOW|UN_PWAIT)) != 0) {

					if ((ch->ch_pun.un_flag & UN_LOW) != 0 ?
					    (n <= TBUF_LOW) :
					    (ch->ch_pun.un_flag & UN_TIME) != 0 ?
					    TimeGE(jiffies, ch->ch_waketime) :
					    (n == 0 && ch->ch_s_tpos == ch->ch_s_tin) &&
					    ((ch->ch_pun.un_flag & UN_EMPTY) != 0 ||
					    ((ch->ch_tun.un_open_count &&
					      ch->ch_tun.un_tty->ops->chars_in_buffer) ?
					     (ch->ch_tun.un_tty->ops->chars_in_buffer)(ch->ch_tun.un_tty) == 0
					     : 1
					    )
					    )) {
						ch->ch_pun.un_flag &= ~(UN_EMPTY | UN_TIME | UN_LOW | UN_PWAIT);

						if (waitqueue_active(&((ch->ch_pun.un_tty)->write_wait)))
							wake_up_interruptible(&((ch->ch_pun.un_tty)->write_wait));
						#ifdef SERIAL_HAVE_POLL_WAIT
						if (waitqueue_active(&((ch->ch_pun.un_tty)->poll_wait)))
							wake_up_interruptible(&((ch->ch_pun.un_tty)->poll_wait));
						#endif
						tty_wakeup(ch->ch_pun.un_tty);
						n = (ch->ch_tin - ch->ch_tout) & TBUF_MASK;

					} else if ((ch->ch_pun.un_flag & UN_TIME) != 0) {
						work = 1;
					}
				}


				/*
				 *  Determine the max number of bytes this port can
				 *  send, including packet header overhead.
				 */

				t = ((ch->ch_s_tsize + ch->ch_s_tpos - ch->ch_s_tin) & 0xffff);

				if (n > t)
					n = t;

				if (n != 0) {
					n += (n <= 8 ? 1 : n <= 255 ? 2 : 3);

					tdata[tchan++] = n;
					ttotal += n;
				}
				break;

			/*
			 *  Close the port.
			 */

send_close:
			case CS_SEND_CLOSE:
				{
					b[0] = 0xb0 + (port & 0xf);
					b[1] = 10;
					b[2] = ch->ch_otype == OTYPE_IMMEDIATE ? 3 : 4;
					b += 3;

					ch->ch_state = CS_WAIT_CLOSE;
				}
				break;

			/*
			 *  Wait for a previous server request.
			 */

			case CS_WAIT_OPEN:
			case CS_WAIT_CANCEL:
			case CS_WAIT_FAIL:
			case CS_WAIT_QUERY:
			case CS_WAIT_CLOSE:
				break;

			default:
				assert(0);
			}
		}

		/*
		 *  If a module select code is needed, drop one in.  If space
		 *  was reserved for one, but none is needed, recover the space.
		 */

		if (mod != nd->nd_tx_module) {
			if (b != mbuf) {
				mbuf[-1] = 0xf0 | mod;
				nd->nd_tx_module = mod;
			} else {
				b--;
			}
		}
	}

	/*
	 *  Adjust "tmax" so that under worst case conditions we do
	 *  not overflow either the daemon buffer or the internal
	 *  buffer in the loop that follows.   Leave a safe area
	 *  of 64 bytes so we start getting asserts before we start
	 *  losing data or clobbering memory.
	 */

	n = UIO_MAX - UIO_BASE;

	if (tmax > n)
		tmax = n;

	tmax -= 64;

	tsafe = tmax;

	/*
	 *  Allocate space for 5 Module Selects, 1 Sequence Request,
	 *  and 1 Set TREQ for each active channel.
	 */

	tmax -= 5 + 3 + 4 * nd->nd_chan_count;

	/*
	 *  Further reduce "tmax" to the available transmit credit.
	 *  Note that this is a soft constraint;  The transmit credit
	 *  can go negative for a time and then recover.
	 */

	n = nd->nd_tx_deposit - nd->nd_tx_charge - nd->nd_link.lk_header_size;

	if (tmax > n)
		tmax = n;

	/*
	 *  Finally reduce tmax by the number of bytes already in
	 *  the buffer.
	 */

	tmax -= b - buf;

	/*
	 *  Suspend data transmit unless every ready channel can send
	 *  at least 1 character.
	 */
	if (tmax < 2 * nd->nd_chan_count) {
		tsend = 1;

	} else if (tchan > 1 && ttotal > tmax) {

		/*
		 *  If transmit is limited by the credit budget, find the
		 *  largest number of characters we can send without driving
		 *  the credit negative.
		 */

		long tm = tmax;
		int tc = tchan;
		int try;

		tsend = tm / tc;

		for (try = 0; try < 3; try++) {
			int i;
			int c = 0;

			for (i = 0; i < tc; i++) {
				if (tsend < tdata[i])
					tdata[c++] = tdata[i];
				else
					tm -= tdata[i];
			}

			if (c == tc)
				break;

			tsend = tm / c;

			if (c == 1)
				break;

			tc = c;
		}

		tsend = tm / nd->nd_chan_count;

		assert(tsend >= 2);
		if (tsend < 2)
			tsend = 1;

	} else {
		/*
		 *  If no budgetary constraints, or only one channel ready
		 *  to send, set the character limit to the remaining
		 *  buffer size.
		 */

		tsend = tmax;
	}

	tsend -= (tsend <= 9) ? 1 : (tsend <= 257) ? 2 : 3;

	/*
	 *  Loop over all channels, sending queued data.
	 */

	port = 0;
	ch = nd->nd_chan;
	used_buffer = tmax;

	for (mod = 0; port < nd->nd_chan_count; mod++) {
		/*
		 *  If this is not the current module, enter a module select
		 *  code in the buffer.
		 */

		if (mod != nd->nd_tx_module)
			mbuf = ++b;

		/*
		 *  Loop to process one module.
		 */

		maxport = port + 16;

		if (maxport > nd->nd_chan_count)
			maxport = nd->nd_chan_count;

		for (; port < maxport; port++, ch++) {
			if (ch->ch_state != CS_READY)
				continue;

			lastport = port;

			n = (ch->ch_tin - ch->ch_tout) & TBUF_MASK;

			/*
			 *  If there is data that can be sent, send it.
			 */

			if (n != 0 && used_buffer > 0) {
				t = (ch->ch_s_tsize + ch->ch_s_tpos - ch->ch_s_tin) & 0xffff;

				if (n > t)
					n = t;

				if (n > tsend) {
					work = 1;
					n = tsend;
				}

				if (n > used_buffer) {
#if 0
					printk(KERN_ALERT "Data Push: n: %ld tsend: %ld used: %ld cur: %ld\n", n, tsend, used_buffer, port);
#endif
					work = 1;
					n = used_buffer;
				}

				if (n <= 0)
					continue;

				/*
				 *  Create the correct size transmit header,
				 *  depending on the amount of data to transmit.
				 */

				if (n <= 8) {

					b[0] = ((n - 1) << 4) + (port & 0xf);
					b += 1;

				} else if (n <= 255) {

					b[0] = 0x80 + (port & 0xf);
					b[1] = n;
					b += 2;

				} else {

					b[0] = 0x90 + (port & 0xf);
					dgrp_encode_u2(b + 1, n);
					b += 3;
				}

				ch->ch_s_tin = (ch->ch_s_tin + n) & 0xffff;

				/*
				 *  Copy transmit data to the packet.
				 */

				t = TBUF_MAX - ch->ch_tout;

				if (n >= t) {
					memcpy(b, ch->ch_tbuf + ch->ch_tout, t);
					b += t;
					n -= t;
					used_buffer -= t;
					ch->ch_tout = 0;
					dbg_net_trace(OUTPUT, ("updating the ch_tout pointer to (%d)\n",
						ch->ch_tout));
				}

				memcpy(b, ch->ch_tbuf + ch->ch_tout, n);
				b += n;
				used_buffer -= n;
				ch->ch_tout += n;
				dbg_net_trace(OUTPUT, ("updating the ch_tout pointer to (%d)\n",
					ch->ch_tout));

				n = (ch->ch_tin - ch->ch_tout) & TBUF_MASK;
			}

			/*
			 *  Wake any terminal unit process waiting in the
			 *  dgrp_write routine for low water.
			 */

			if (n > TBUF_LOW)
				continue;

			if ((ch->ch_flag & CH_LOW) != 0) {
				ch->ch_flag &= ~CH_LOW;
				wake_up_interruptible(&ch->ch_flag_wait);
			}

			/* selwakeup tty_sel */
			{

				if (ch->ch_tun.un_open_count) {
					struct tty_struct *tty = (ch->ch_tun.un_tty);

					if (waitqueue_active(&tty->write_wait))
						wake_up_interruptible(&tty->write_wait);

					#ifdef SERIAL_HAVE_POLL_WAIT
					if (waitqueue_active(&tty->poll_wait))
						wake_up_interruptible(&tty->poll_wait);
					#endif
					tty_wakeup(tty);
				}


				if (ch->ch_pun.un_open_count) {
					struct tty_struct *tty = (ch->ch_pun.un_tty);

					if (waitqueue_active(&tty->write_wait))
						wake_up_interruptible(&tty->write_wait);
					#ifdef SERIAL_HAVE_POLL_WAIT
					if (waitqueue_active(&tty->poll_wait))
						wake_up_interruptible(&tty->poll_wait);
					#endif
					tty_wakeup(tty);
				}
			}

			/*
			 *  Do EMPTY processing.
			 */

			if (n != 0)
				continue;

			if ((ch->ch_flag & (CH_EMPTY | CH_DRAIN)) != 0 ||
			    (ch->ch_pun.un_flag & UN_EMPTY) != 0) {
				/*
				 *  If there is still data in the server, ask the server
				 *  to notify us when its all gone.
				 */

				if (ch->ch_s_treq != ch->ch_s_tin) {
					b[0] = 0xb0 + (port & 0xf);
					b[1] = 43;
					dgrp_encode_u2(b + 2, ch->ch_s_treq = ch->ch_s_tin);
					b += 4;
				}

				/*
				 *  If there is a thread waiting for buffer empty,
				 *  and we are truly empty, wake the thread.
				 */

				else if ((ch->ch_flag & CH_EMPTY) != 0 &&
					(ch->ch_send & RR_TX_BREAK) == 0) {
					ch->ch_flag &= ~CH_EMPTY;

					wake_up_interruptible(&ch->ch_flag_wait);
				}
			}
		}

		/*
		 *  If a module select code is needed, drop one in.  If space
		 *  was reserved for one, but none is needed, recover the space.
		 */

		if (mod != nd->nd_tx_module) {
			if (b != mbuf) {
				mbuf[-1] = 0xf0 | mod;
				nd->nd_tx_module = mod;
			} else {
				b--;
			}
		}
	}

	/*
	 *  Send a synchronization sequence associated with the last open
	 *  channel that sent data, and remember the time when the data was
	 *  sent.
	 */

	in = nd->nd_seq_in;

	if ((send_sync || nd->nd_seq_wait[in] != 0) && lastport >= 0) {
		uchar *bb = b;

		/*
		 * Attempt the use the port that really wanted the sync.
		 * This gets around a race condition where the "lastport" is in
		 * the middle of the close() routine, and by the time we
		 * send this command, it will have already acked the close, and
		 * thus not send the sync response.
		 */
		if (wanted_sync_port >= 0)
			lastport = wanted_sync_port;
		/*
		 * Set a flag just in case the port is in the middle of a close,
		 * it will not be permitted to actually close until we get an
		 * sync response, and clear the flag there.
		 */
		ch = nd->nd_chan + lastport;
		ch->ch_flag |= CH_WAITING_SYNC;

		mod = lastport >> 4;

		if (mod != nd->nd_tx_module) {
			bb[0] = 0xf0 + mod;
			bb += 1;

			nd->nd_tx_module = mod;
		}

		bb[0] = 0xb0 + (lastport & 0x0f);
		bb[1] = 12;
		bb[2] = in;
		bb += 3;

		nd->nd_seq_size[in] = bb - buf;
		nd->nd_seq_time[in] = jiffies;

		if (++in >= SEQ_MAX)
			in = 0;

		if (in != nd->nd_seq_out) {
			b = bb;
			nd->nd_seq_in = in;
			nd->nd_unack += b - buf;
		}
	}

	/*
	 *  If there are no open ports, a sync cannot be sent.
	 *  There is nothing left to wait for anyway, so wake any
	 *  thread waiting for an acknowledgement.
	 */

	else if (nd->nd_seq_wait[in] != 0) {
		nd->nd_seq_wait[in] = 0;

		wake_up_interruptible(&nd->nd_seq_wque[in]);
	}

	/*
	 *  If there is no traffic for an interval of IDLE_MAX, then
	 *  send a single byte packet.
	 */

	if (b != buf) {
		nd->nd_tx_time = jiffies;
	} else if ((ulong)(jiffies - nd->nd_tx_time) >= IDLE_MAX) {
		*b++ = 0xf0 | nd->nd_tx_module;
		nd->nd_tx_time = jiffies;
	}

	n = b - buf;

	assert(n < tsafe);
	if (n > tsafe) {
#if 0
		printk(KERN_ALERT "n: %ld tsafe: %ld tsend: %ld ttotal: %ld tmax: %ld tchan: %ld\n",
			n, tsafe, tsend, ttotal, tmax, tchan);
#endif
	}

	if (tsend < 0) {
		dbg_net_trace(OUTPUT, ("net send err: tx_credit=%d, tmax=%d, "
					"tchan=%d, tsend=%d, sent=%d\n",
					nd->nd_tx_credit, tmax, tchan, tsend, n));

		dgrp_dump(buf, n);
	}

	nd->nd_tx_work = work;

	return n;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_net_read
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
*    Data to be sent TO the PortServer from the "async." half of the driver.
*
******************************************************************************/

static ssize_t dgrp_net_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct nd_struct *nd;
	long n;
	uchar *local_buf;
	uchar *b;
	ssize_t rtn = 0;

	dbg_net_trace(READ, ("net read(%p) start\n", file->private_data));

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);

	if (!nd) {
		rtn = -ENXIO;
		goto done;
	}


	if (count < UIO_MIN) {
		rtn = -EINVAL;
		goto done;
	}

	/*
	 *  Only one read/write operation may be in progress at
	 *  any given time.
	 */

	/*
	 *  Grab the NET lock.
	 */
	down(&nd->nd_net_semaphore);

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	spinlock(&nd->nd_lock);
#endif

	nd->nd_read_count++;

	nd->nd_tx_ready = 0;

	/*
	 *  Determine the effective size of the buffer.
	 */

	assert(nd->nd_remain <= UIO_BASE);

	b = local_buf = nd->nd_iobuf + UIO_BASE;

	/*
	 *  Generate data according to the node state.
	 */

	switch (nd->nd_state) {
	/*
	 *  Initialize the connection.
	 */

	case NS_IDLE:
		if (nd->nd_mon_buf != 0) {

			/* TODO : historical locking placeholder */
			/*
			 * In the HPUX version of the RealPort driver
			 * (which served as a basis for this driver)
			 * this locking code was used.  Saved if ever we
			 * need to review the locking under Linux.
			 */
#if 0
			spinunlock(&nd->nd_lock);
#endif

			dgrp_monitor_reset(nd);

			/* TODO : historical locking placeholder */
			/*
			 * In the HPUX version of the RealPort driver
			 * (which served as a basis for this driver)
			 * this locking code was used.  Saved if ever we
			 * need to review the locking under Linux.
			 */
#if 0
			spinlock(&nd->nd_lock);
#endif
		}

		/*
		 *  Request a Product ID Packet.
		 */

		b[0] = 0xfb;
		b[1] = 0x01;
		b += 2;

		nd->nd_expect |= NR_IDENT;

		/*
		 *  Request a Server Capability ID Response.
		 */

		b[0] = 0xfb;
		b[1] = 0x02;
		b += 2;

		nd->nd_expect |= NR_CAPABILITY;

		/*
		 *  Request a Server VPD Response.
		 */

		b[0] = 0xfb;
		b[1] = 0x18;
		b += 2;

		nd->nd_expect |= NR_VPD;

		nd->nd_state = NS_WAIT_QUERY;
		break;

	/*
	 *  We do serious communication with the server only in
	 *  the READY state.
	 */

	case NS_READY:
		b = dgrp_send(nd, count) + local_buf;
		break;

	/*
	 *  Send off an error after receiving a bogus message
	 *  from the server.
	 */

	case NS_SEND_ERROR:
		n = strlen(nd->nd_error);

		b[0] = 0xff;
		b[1] = n;
		memcpy(b + 2, nd->nd_error, n);
		b += 2 + n;

		dgrp_net_idle(nd);
		/*
		 *  Set the active port count to zero.
		 */
		dgrp_chan_count(nd, 0);
		break;
	}

	n = b - local_buf;

	if (n != 0) {
		nd->nd_send_count++;

		nd->nd_tx_byte   += n + nd->nd_link.lk_header_size;
		nd->nd_tx_charge += n + nd->nd_link.lk_header_size;
	}

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	spinunlock(&nd->nd_lock);
#endif

	assert(b <= nd->nd_iobuf + UIO_MAX);
	assert(n <= count);

	rtn = copy_to_user(buf, local_buf, n);
	if (rtn) {
		rtn = -EFAULT;
		up(&nd->nd_net_semaphore);
		goto done;
	}

	*ppos += n;

	rtn = n;

	if (nd->nd_mon_buf != 0)
		dgrp_monitor_data(nd, RPDUMP_CLIENT, local_buf, n);

	/*
	 *  Release the NET lock.
	 */
	up(&nd->nd_net_semaphore);

done:
	dbg_net_trace(READ, ("net read(%p) return %d\n",
				file->private_data, rtn));

	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_receive
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    nd -- pointer to a node structure
*
* Return Values:
*
*    none
*
* Description:
*
*    Decodes data packets received from the remote PortServer.
*
* NOTE TO LINUX KERNEL HACKERS:
*
*	Yes, this is a long function.  Very long.
*
*	This function is a constant among all of Digi's Unix RealPort drivers,
*	and for bug fixing purposes it is critical it stays exactly like it is!
*	Please do not try to split it up into a bunch of smaller functions!
*
*	Thanks, Scott.
*
******************************************************************************/

static void dgrp_receive(struct nd_struct *nd)
{
	struct ch_struct *ch;
	uchar *buf;
	uchar *b;
	uchar *dbuf;
	char *error = NULL;
	long port;
	long dlen;
	long plen;
	long remain;
	long n;
	long mlast;
	long elast;
	long mstat;
	long estat;

	char ID[3];

	nd->nd_tx_time = jiffies;

	ID_TO_CHAR(nd->nd_ID, ID);

	b = buf = nd->nd_iobuf;
	remain = nd->nd_remain;

	/*
	 *  Loop to process Realport protocol packets.
	 */

	while (remain > 0) {
		int n0 = b[0] >> 4;
		int n1 = b[0] & 0x0f;

		dbg_net_trace(INPUT, ("net receive(%d) %d %d\n",
				        b - buf, n0, n1));

		if (n0 <= 12) {
			port = (nd->nd_rx_module << 4) + n1;

			if (port >= nd->nd_chan_count) {
				error = "Improper Port Number";
				goto prot_error;
			}

			ch = nd->nd_chan + port;
		} else {
			port = -1;
			ch = 0;
		}

		/*
		 *  Process by major packet type.
		 */

		switch (n0) {

		/*
		 *  Process 1-byte header data packet.
		 */

		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			dlen = n0 + 1;
			plen = dlen + 1;

			dbuf = b + 1;
			goto data;

		/*
		 *  Process 2-byte header data packet.
		 */

		case 8:
			if (remain < 3)
				goto done;

			dlen = b[1];
			plen = dlen + 2;

			dbuf = b + 2;
			goto data;

		/*
		 *  Process 3-byte header data packet.
		 */

		case 9:
			if (remain < 4)
				goto done;

			dlen = dgrp_decode_u2(b + 1);
			plen = dlen + 3;

			dbuf = b + 3;

		/*
		 *  Common packet handling code.
		 */

data:
			nd->nd_tx_work = 1;

			/*
			 *  Otherwise data should appear only when we are
			 *  in the CS_READY state.
			 */

			if (ch->ch_state < CS_READY) {
				error = "Data received before RWIN established";
				goto prot_error;
			}

			/*
			 *  Assure that the data received is within the
			 *  allowable window.
			 */

			n = (ch->ch_s_rwin - ch->ch_s_rin) & 0xffff;

			if (dlen > n) {
				error = "Receive data overrun";
				goto prot_error;
			}

			/*
			 *  If we received 3 or less characters,
			 *  assume it is a human typing, and set RTIME
			 *  to 10 milliseconds.
			 *
			 *  If we receive 10 or more characters,
			 *  assume its not a human typing, and set RTIME
			 *  to 100 milliseconds.
			 */

			if (ch->ch_edelay != GLBL(rtime)) {
				if (ch->ch_rtime != ch->ch_edelay) {
					ch->ch_rtime = ch->ch_edelay;
					ch->ch_flag |= CH_PARAM;
				}
			} else if (dlen <= 3) {
				if (ch->ch_rtime != 10) {
					ch->ch_rtime = 10;
					ch->ch_flag |= CH_PARAM;
				}
			} else {
				if (ch->ch_rtime != GLBL(rtime)) {
					ch->ch_rtime = GLBL(rtime);
					ch->ch_flag |= CH_PARAM;
				}
			}

			/*
			 *  If a portion of the packet is outside the
			 *  buffer, shorten the effective length of the
			 *  data packet to be the amount of data received.
			 */

			if (remain < plen)
				dlen -= plen - remain;

			/*
			 *  Detect if receive flush is now complete.
			 */

			if ((ch->ch_flag & CH_RX_FLUSH) != 0 &&
			    ((ch->ch_flush_seq - nd->nd_seq_out) & SEQ_MASK) >=
			    ((nd->nd_seq_in    - nd->nd_seq_out) & SEQ_MASK)) {
				ch->ch_flag &= ~CH_RX_FLUSH;
			}

			/*
			 *  If we are ready to receive, move the data into
			 *  the receive buffer.
			 */

			ch->ch_s_rin = (ch->ch_s_rin + dlen) & 0xffff;

			dbg_net_trace(INPUT, ("in dgrp_receive(%x:%x) state(%x) cflag(%x) flag(%x)\n",
				MAJOR(tty_devnum(ch->ch_tun.un_tty)), MINOR(tty_devnum(ch->ch_tun.un_tty)),
				 ch->ch_state, ch->ch_cflag, ch->ch_flag));
			dbg_net_trace(INPUT, ("in dgrp_receive(%x:%x) open_count(%d) send(%x)\n",
				MAJOR(tty_devnum(ch->ch_tun.un_tty)), MINOR(tty_devnum(ch->ch_tun.un_tty)),
				ch->ch_tun.un_open_count, ch->ch_send));

			if (ch->ch_state == CS_READY &&
			    (ch->ch_tun.un_open_count != 0) &&
			    (ch->ch_tun.un_flag & UN_CLOSING) == 0 &&
			    (ch->ch_cflag & CF_CREAD) != 0 &&
			    (ch->ch_flag & (CH_BAUD0 | CH_RX_FLUSH)) == 0 &&
			    (ch->ch_send & RR_RX_FLUSH) == 0) {

				dbg_net_trace(INPUT, ("in dgrp_receive rin(%d) rout(%d)\n ",
					ch->ch_rin, ch->ch_rout));
				if (ch->ch_rin + dlen >= RBUF_MAX) {
					n = RBUF_MAX - ch->ch_rin;

					memcpy(ch->ch_rbuf + ch->ch_rin, dbuf, n);

					ch->ch_rin = 0;
					dbuf += n;
					dlen -= n;
				}

				memcpy(ch->ch_rbuf + ch->ch_rin, dbuf, dlen);

				ch->ch_rin += dlen;

				dbg_net_trace(INPUT, ("in dgrp_receive new rin(%d) new rout(%d)\n ",
					ch->ch_rin, ch->ch_rout));
				dbg_net_trace(INPUT, ("dgrp_receive MIN_CHAR(%d) TIME_CHAR(%d)\n ",
					MIN_CHAR(ch->ch_tun.un_tty), TIME_CHAR(ch->ch_tun.un_tty)));

				/*
				 *  If we are not in fastcook mode, or if there is
				 *  a fastcook thread waiting for data, send the data
				 *  to the line discipline.
				 */

				if ((ch->ch_flag & CH_FAST_READ) == 0 ||
				    ch->ch_inwait != 0) {
					dgrp_input(ch);
				}

				/*
				 *  If there is a read thread waiting in select, and we
				 *  are in fastcook mode, wake him up.
				 */

				if (waitqueue_active(&ch->ch_tun.un_tty->read_wait) &&
					(ch->ch_flag & CH_FAST_READ) != 0)
				   wake_up_interruptible(&ch->ch_tun.un_tty->read_wait);

				/*
				 * Wake any thread waiting in the fastcook loop.
				 */

				if ((ch->ch_flag & CH_INPUT) != 0) {
					ch->ch_flag &= ~CH_INPUT;

					wake_up_interruptible(&ch->ch_flag_wait);
				}
			}

			/*
			 *  Fabricate and insert a data packet header to
			 *  preceed the remaining data when it comes in.
			 */

			if (remain < plen) {
				dlen = plen - remain;
				b = buf;

				b[0] = 0x90 + n1;
				dgrp_encode_u2(b + 1, dlen);

				remain = 3;
				goto done;
			}
			break;

		/*
		 *  Handle Window Sequence packets.
		 */

		case 10:
			if (remain < (plen = 3))
				goto done;

			nd->nd_tx_work = 1;

			{
				ushort tpos   = dgrp_decode_u2(b + 1);

				ushort ack    = (tpos          - ch->ch_s_tpos) & 0xffff;
				ushort unack  = (ch->ch_s_tin  - ch->ch_s_tpos) & 0xffff;
				ushort notify = (ch->ch_s_treq - ch->ch_s_tpos) & 0xffff;

				if (ch->ch_state < CS_READY || ack > unack) {
					error = "Improper Window Sequence";
					goto prot_error;
				}

				ch->ch_s_tpos = tpos;

				if (notify <= ack)
					ch->ch_s_treq = tpos;
			}
			break;

		/*
		 *  Handle Command response packets.
		 */

		case 11:

			/*
			 * RealPort engine fix - 03/11/2004
			 *
			 * This check did not used to be here.
			 *
			 * We were using b[1] without verifying that the data
			 * is actually there and valid. On a split packet, it
			 * might not be yet.
			 *
			 * NOTE:  I have never actually seen the failure happen
			 *        under Linux,  but since I have seen it occur
			 *        under both Solaris and HP-UX,  the assumption
			 *        is that it *could* happen here as well...
			 */
			if (remain < 2)
				goto done;


			dbg_net_trace(INPUT, ("net receive case 11: b[1](%d), ch_state(%4x)\n",
					b[1], ch->ch_state));
			switch (b[1]) {

			/*
			 *  Handle Open Response.
			 */

			case 11:
			        if (remain < (plen = 6))
					goto done;

				nd->nd_tx_work = 1;

				{
					int req = b[2];
					int resp = b[3];
					port = dgrp_decode_u2(b + 4);

					if (port >= nd->nd_chan_count) {
						error = "Open channel number out of range";
						goto prot_error;
					}

					ch = nd->nd_chan + port;

					/*
					 *  How we handle an open response depends primarily
					 *  on our current channel state.
					 */

					switch (ch->ch_state) {
					case CS_IDLE:

						/*
						 *  Handle a delayed open.
						 */

						if (ch->ch_otype_waiting != 0 &&
						    req == ch->ch_otype_waiting &&
						    resp == 0) {
							ch->ch_otype = req;
							ch->ch_otype_waiting = 0;
							ch->ch_state = CS_SEND_QUERY;
							break;
						}
						goto open_error;

					case CS_WAIT_OPEN:

						/*
						 *  Handle the open response.
						 */

						if (req == ch->ch_otype) {
							switch (resp) {

							/*
							 *  On successful response, open the
							 *  port and proceed normally.
							 */

							case 0:
								ch->ch_state = CS_SEND_QUERY;
								break;

							/*
							 *  On a busy response to a persistent open,
							 *  remember that the open is pending.
							 */

							case 1:
							case 2:
								if (req != OTYPE_IMMEDIATE) {
									ch->ch_otype_waiting = req;
									ch->ch_state = CS_IDLE;
									break;
								}

							/*
							 *  Otherwise the server open failed.  If
							 *  the Unix port is open, hang it up.
							 */

							default:
								if (ch->ch_open_count != 0) {
									ch->ch_flag |= CH_HANGUP;
									dbg_net_trace(INPUT, ("calling carrier after open fail\n"));
									dgrp_carrier(ch);
									ch->ch_state = CS_IDLE;
									break;
								}

								ch->ch_open_error = resp;
								ch->ch_state = CS_IDLE;

								wake_up_interruptible(&ch->ch_flag_wait);
							}
							break;
						}

						/*
						 *  Handle delayed response arrival preceeding
						 *  the open response we are waiting for.
						 */

						if (ch->ch_otype_waiting != 0 &&
						    req == ch->ch_otype_waiting &&
						    resp == 0) {
							ch->ch_otype = ch->ch_otype_waiting;
							ch->ch_otype_waiting = 0;
							ch->ch_state = CS_WAIT_FAIL;
							break;
						}
						goto open_error;


					case CS_WAIT_FAIL:

						/*
						 *  Handle response to immediate open arriving
						 *  after a delayed open success.
						 */

						if (req == OTYPE_IMMEDIATE) {
							ch->ch_state = CS_SEND_QUERY;
							break;
						}
						goto open_error;


					case CS_WAIT_CANCEL:
						/*
						 *  Handle delayed open response arriving before
						 *  the cancel response.
						 */

						if (req == ch->ch_otype_waiting &&
						    resp == 0) {
							ch->ch_otype_waiting = 0;
							break;
						}

						/*
						 *  Handle cancel response.
						 */

						if (req == 4 && resp == 0) {
							ch->ch_otype_waiting = 0;
							ch->ch_state = CS_IDLE;
							break;
						}
						goto open_error;


					case CS_WAIT_CLOSE:
						/*
						 *  Handle a successful response to a port
						 *  close.
						 */

						if (req >= 3) {
							ch->ch_state = CS_IDLE;
/* Jira RP-80 uncovered an intermittent problem where fast looping port
 * open/closes (see client's DupLock test script) could result in a case
 * where the wait_event_interruptible() in dgrp_close() would be called
 * while we're already in CS_WAIT_CLOSE state (and CH_PARAM is still set),
 * which causes us to wait forever.  So... clear flag and add a wake_up here.
 */
ch->ch_flag &= ~CH_PARAM;
wake_up_interruptible(&ch->ch_flag_wait);
							break;
						}
						goto open_error;

open_error:
					default:
						{
							error = "Improper Open Response";
							goto prot_error;
						}
					}
				}
				break;

			/*
			 *  Handle Synchronize Response.
			 */

			case 13:
				if (remain < (plen = 3))
					goto done;
				{
					int seq = b[2];
					int s;

					/*
					 * If channel was waiting for this sync response,
					 * unset the flag, and wake up anyone waiting
					 * on the event.
					 */
					if (ch->ch_flag & CH_WAITING_SYNC) {
						ch->ch_flag &= ~(CH_WAITING_SYNC);
						wake_up_interruptible(&ch->ch_flag_wait);
					}

					if (((seq - nd->nd_seq_out) & SEQ_MASK) >=
					    ((nd->nd_seq_in - nd->nd_seq_out) & SEQ_MASK)) {
						break;
					}

					for (s = nd->nd_seq_out;; s = (s + 1) & SEQ_MASK) {
						if (nd->nd_seq_wait[s] != 0) {
							nd->nd_seq_wait[s] = 0;

							wake_up_interruptible(&nd->nd_seq_wque[s]);
						}

						nd->nd_unack -= nd->nd_seq_size[s];

						assert(nd->nd_unack >= 0);

						if (s == seq)
							break;
					}

					nd->nd_seq_out = (seq + 1) & SEQ_MASK;
				}
				break;

			/*
			 *  Handle Sequence Response.
			 */

			case 15:
				if (remain < (plen = 6))
					goto done;

				{
				/* Record that we have received the Sequence
				 * Response, but we aren't interested in the
				 * sequence numbers.  We were using RIN like it
				 * was ROUT and that was causing problems,
				 * fixed 7-13-2001 David Fries. See comment in
				 * drp.h for ch_s_rin variable.
					int rin = dgrp_decode_u2(b + 2);
					int tpos = dgrp_decode_u2(b + 4);
				*/

					ch->ch_send   &= ~RR_SEQUENCE;
					ch->ch_expect &= ~RR_SEQUENCE;
				}
				goto check_query;

			/*
			 *  Handle Status Response.
			 */

			case 17:
				if (remain < (plen = 5))
					goto done;

				{
					ch->ch_s_elast = dgrp_decode_u2(b + 2);
					ch->ch_s_mlast = b[4];

					ch->ch_expect &= ~RR_STATUS;
					ch->ch_send   &= ~RR_STATUS;

					/*
					 *  CH_PHYS_CD is cleared because something _could_ be
					 *  waiting for the initial sense of carrier... and if
					 *  carrier is high immediately, we want to be sure to
					 *  wake them as soon as possible.
					 */
					ch->ch_flag &= ~CH_PHYS_CD;

					dbg_net_trace(INPUT, ("calling carrier after"
							" status response\n"));
					dgrp_carrier(ch);
				}
				goto check_query;

			/*
			 *  Handle Line Error Response.
			 */

			case 19:
				if (remain < (plen = 14))
					goto done;

				break;

			/*
			 *  Handle Buffer Response.
			 */

			case 21:
				if (remain < (plen = 6))
					goto done;

				{
					ch->ch_s_rsize = dgrp_decode_u2(b + 2);
					ch->ch_s_tsize = dgrp_decode_u2(b + 4);

					ch->ch_send   &= ~RR_BUFFER;
					ch->ch_expect &= ~RR_BUFFER;
				}
				goto check_query;

			/*
			 *  Handle Port Capability Response.
			 */

			case 23:
				if (remain < (plen = 32))
					goto done;

				{
					ch->ch_send   &= ~RR_CAPABILITY;
					ch->ch_expect &= ~RR_CAPABILITY;
				}

			/*
			 *  When all queries are complete, set those parameters
			 *  derived from the query results, then transition
			 *  to the READY state.
			 */

check_query:
				if (ch->ch_state == CS_WAIT_QUERY &&
				    (ch->ch_expect & (RR_SEQUENCE |
							RR_STATUS |
							RR_BUFFER |
							RR_CAPABILITY)) == 0) {
					ch->ch_tmax  = ch->ch_s_tsize / 4;

					if (ch->ch_edelay == GLBL(ttime))
						ch->ch_ttime = GLBL(ttime);
					else
						ch->ch_ttime = ch->ch_edelay;

					ch->ch_rmax = ch->ch_s_rsize / 4;

					if (ch->ch_edelay == GLBL(rtime))
						ch->ch_rtime = GLBL(rtime);
					else
						ch->ch_rtime = ch->ch_edelay;

					ch->ch_rlow  = 2 * ch->ch_s_rsize / 8;
					ch->ch_rhigh = 6 * ch->ch_s_rsize / 8;

					ch->ch_state = CS_READY;

					nd->nd_tx_work = 1;
					wake_up_interruptible(&ch->ch_flag_wait);

				}
				break;

			default:
				goto decode_error;
			}
			break;

		/*
		 *  Handle Events.
		 */
		case 12:

			if (remain < (plen = 4))
				goto done;

			mlast = ch->ch_s_mlast;
			elast = ch->ch_s_elast;

			mstat = ch->ch_s_mlast = b[1];
			estat = ch->ch_s_elast = dgrp_decode_u2(b + 2);

			/*
			 *  Handle modem changes.
			 */

			if (((mstat ^ mlast) & DM_CD) != 0) {
				dbg_net_trace(INPUT, ("calling carrier after"
					" case 12, carrier detected\n"));
				dgrp_carrier(ch);
			}


			/*
			 *  Handle received break.
			 */

			if ((estat & ~elast & EV_RXB) != 0 &&
			    (ch->ch_tun.un_open_count != 0) &&
			    I_BRKINT(ch->ch_tun.un_tty) &&
			    !(I_IGNBRK(ch->ch_tun.un_tty))) {

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
				tty_buffer_request_room(ch->ch_tun.un_tty, 1);
				tty_insert_flip_char(ch->ch_tun.un_tty, 0, TTY_BREAK);
				tty_flip_buffer_push(ch->ch_tun.un_tty);
#else
				tty_buffer_request_room(&ch->port, 1);
				tty_insert_flip_char(&ch->port, 0, TTY_BREAK);
				tty_flip_buffer_push(&ch->port);

#endif
				dbg_net_trace(INPUT, ("(%x:%x) received a break, sending to ld\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty))));

			} else {
				dbg_net_trace(INPUT, ("(%x:%x) received a break, but is ignoring\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty))));
			}

			/*
			 *  On transmit break complete, if more break traffic
			 *  is waiting then send it.  Otherwise wake any threads
			 *  waiting for transmitter empty.
			 */

			if ((~estat & elast & EV_TXB) != 0 &&
			    (ch->ch_expect & RR_TX_BREAK) != 0) {

				nd->nd_tx_work = 1;

				ch->ch_expect &= ~RR_TX_BREAK;

				if (ch->ch_break_time != 0) {
					ch->ch_send |= RR_TX_BREAK;
				} else {
					ch->ch_send &= ~RR_TX_BREAK;
					ch->ch_flag &= ~CH_TX_BREAK;
					wake_up_interruptible(&ch->ch_flag_wait);
				}
			}
			break;

		case 13:
		case 14:
			error = "Unrecognized command";
			goto prot_error;

		/*
		 *  Decode Special Codes.
		 */

		case 15:
			switch (n1) {
			/*
			 *  One byte module select.
			 */

			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				plen = 1;
				nd->nd_rx_module = n1;
				break;

			/*
			 *  Two byte module select.
			 */

			case 8:
				if (remain < (plen = 2))
					goto done;

				nd->nd_rx_module = b[1];
				break;

			/*
			 *  ID Request packet.
			 */

			case 11:
				if (remain < 4)
					goto done;

				plen = dgrp_decode_u2(b + 2);

				if (plen < 12 || plen > 1000) {
					error = "Response Packet length error";
					goto prot_error;
				}

				nd->nd_tx_work = 1;

				switch (b[1]) {
				/*
				 *  Echo packet.
				 */

				case 0:
					nd->nd_send |= NR_ECHO;
					break;

				/*
				 *  ID Response packet.
				 */

				case 1:
					nd->nd_send |= NR_IDENT;
					break;

				/*
				 *  ID Response packet.
				 */

				case 32:
					nd->nd_send |= NR_PASSWORD;
					break;

				}
				break;

			/*
			 *  Various node-level response packets.
			 */

			case 12:
				if (remain < 4)
					goto done;

				plen = dgrp_decode_u2(b + 2);

				if (plen < 4 || plen > 1000) {
					error = "Response Packet length error";
					goto prot_error;
				}

				nd->nd_tx_work = 1;

				switch (b[1]) {
				/*
				 *  Echo packet.
				 */

				case 0:
					nd->nd_expect &= ~NR_ECHO;
					break;

				/*
				 *  Product Response Packet.
				 */

				case 1:
					{
						int desclen;

						nd->nd_hw_ver = (b[8] << 8) | b[9];
						nd->nd_sw_ver = (b[10] << 8) | b[11];
						nd->nd_hw_id = b[6];
						desclen = ((plen - 12) > MAX_DESC_LEN) ? MAX_DESC_LEN :
							plen - 12;
						strncpy(nd->nd_ps_desc, b + 12, desclen);
						nd->nd_ps_desc[desclen] = 0;
					}

					nd->nd_expect &= ~NR_IDENT;
					break;

				/*
				 *  Capability Response Packet.
				 */

				case 2:
					{
						int nn = dgrp_decode_u2(b + 4);

						if (nn > CHAN_MAX)
							nn = CHAN_MAX;

						dgrp_chan_count(nd, nn);
					}

					nd->nd_expect &= ~NR_CAPABILITY;
					break;

				/*
				 *  VPD Response Packet.
				 */

				case 15:
					/*
					 * NOTE: case 15 is here ONLY because the EtherLite
					 * is broken, and sends a response to 24 back as 15.
					 * To resolve this, the EtherLite firmware is now
					 * fixed to send back 24 correctly, but, for backwards
					 * compatibility, we now have reserved 15 for the
					 * bad EtherLite response to 24 as well.
					 */

					/* Fallthru! */

				case 24:

					/*
					 * If the product doesn't support VPD,
					 * it will send back a null IDRESP,
					 * which is a length of 4 bytes.
					 */
					if (plen > 4) {
						memcpy(nd->nd_vpd, b + 4, min(plen - 4, (long) VPDSIZE));
						nd->nd_vpd_len = min(plen - 4, (long) VPDSIZE);
					}

					nd->nd_expect &= ~NR_VPD;
					break;

				default:
					goto decode_error;
				}

				if (nd->nd_expect == 0 &&
				    nd->nd_state == NS_WAIT_QUERY) {
					nd->nd_state = NS_READY;
				}
				break;

			/*
			 *  Debug packet.
			 */

			case 14:
				if (remain < 4)
					goto done;

				plen = dgrp_decode_u2(b + 2) + 4;

				if (plen > 1000) {
					error = "Debug Packet too large";
					goto prot_error;
				}

				if (remain < plen)
					goto done;
				break;

			/*
			 *  Handle reset packet.
			 */

			case 15:
				if (remain < 2)
					goto done;

				plen = 2 + b[1];

				if (remain < plen)
					goto done;

				nd->nd_tx_work = 1;

				n = b[plen];
				b[plen] = 0;

				dbg_net_trace(INPUT, ("net receive: Received Reset "
						"from node %s - %s\n",
						ID, b + 2));

				b[plen] = n;

				error = "Client Reset Acknowledge";
				goto prot_error;

			default:
			        goto decode_error;
			}
			break;

		default:
			goto decode_error;
		}

		b += plen;
		remain -= plen;
	}

	/*
	 *  When the buffer is exhausted, copy any data left at the
	 *  top of the buffer back down to the bottom for the next
	 *  read request.
	 */

done:
	if (remain > 0 && b != buf)
		memcpy(buf, b, remain);

	nd->nd_remain = remain;			
	return;

/*
 *  Handle a decode error.
 */

decode_error:
	dbg_net_trace(INPUT, ("net receive: node %s decode error - %02x %02x %02x %02x\n",
		   ID, b[0], b[1], b[2], b[3]));

	error = "Protocol decode error";			

/*
 *  Handle a general protocol error.
 */

prot_error:
	dbg_trace(("net receive: Sent Reset to node %s - %s\n", ID, error));

	nd->nd_remain = 0;
	nd->nd_state = NS_SEND_ERROR;
	nd->nd_error = error;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_net_write
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
*    standard Linux write entry point return values
*
* Description:
*
*    Writes data to the network device.
*
*    A zero byte write indicates that the connection to the RealPort
*    device has been broken.
*
*    A non-zero write indicates data from the RealPort device.
*
******************************************************************************/

static ssize_t dgrp_net_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct nd_struct *nd;
	ssize_t rtn = 0;
	long n;
	long total = 0;

	dbg_net_trace(WRITE, ("net write(%p) start\n", file->private_data));

	/*
	 *  Get the node pointer, and quit if it doesn't exist.
	 */
	nd = (struct nd_struct *)(file->private_data);
	if (!nd) {
		rtn = -ENXIO;
		goto done;
	}


	/*
	 *  Grab the NET lock.
	 */
	down(&nd->nd_net_semaphore);

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	spinlock(&nd->nd_lock);
#endif

	nd->nd_write_count++;

	/*
	 *  Handle disconnect.
	 */

	if (count == 0) {
		dgrp_net_idle(nd);
		/*
		 *  Set the active port count to zero.
		 */
		dgrp_chan_count(nd, 0);
		goto unlock;
	}

	/*
	 *  Loop to process entire receive packet.
	 */

	while (count > 0) {
		n = UIO_MAX - nd->nd_remain;

		if (n > count)
			n = count;

		nd->nd_rx_byte += n + nd->nd_link.lk_header_size;

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
		spinunlock(&nd->nd_lock);
#endif

		rtn = copy_from_user(nd->nd_iobuf + nd->nd_remain, buf + total, n);
		if (rtn) {
			rtn = -EFAULT;
			goto unlock;
		}

		*ppos += n;

		total += n;

		count -= n;

		if (nd->nd_mon_buf != 0) {
			dgrp_monitor_data(nd, RPDUMP_SERVER,
					nd->nd_iobuf + nd->nd_remain, n);
		}

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
		spinlock(&nd->nd_lock);
#endif

		nd->nd_remain += n;

		dgrp_receive(nd);
	}

	rtn = total;

unlock:
	/*
	 *  Release the NET lock.
	 */
	up(&nd->nd_net_semaphore);

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	spinunlock(&nd->nd_lock);
#endif

done:
	dbg_net_trace(WRITE, ("net write(%p) return %d\n",
				file->private_data, rtn));

	return rtn;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_net_select
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    standard Linux select entry point arguments
*
* Return Values:
*
*    standard Linux select entry point return values
*
* Description:
*
*    Determine whether a device is ready to be read or written to, and
*    sleep if not.
*
******************************************************************************/

static unsigned int dgrp_net_select(struct file *file, struct poll_table_struct *table)
{
	unsigned int retval = 0;
	struct nd_struct *nd = file->private_data;

	dbg_net_trace(SELECT, ("net select(%p) start\n", nd));

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	spinlock(&nd->nd_lock);
#endif

	poll_wait(file, &nd->nd_tx_waitq, table);

	if (nd->nd_tx_ready)
		retval |= POLLIN | POLLRDNORM; /* Conditionally readable */

	retval |= POLLOUT | POLLWRNORM;        /* Always writeable */

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	spinunlock(&nd->nd_lock);
#endif

	dbg_net_trace(SELECT, ("net select(%p) return %x\n", nd, retval));

	return retval;
}


/*****************************************************************************
*
* Function:
*
*    dgrp_net_ioctl
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode -- pointer to inode associated with the net communication device
*    file  -- file pointer associated with the net communication device
*    cmd   -- integer ioctl command number
*    arg   -- ioctl argument.  May be an long integer value, or a pointer,
*             depending on the cmd parameter.
*
* Return Values:
*
*    standard Linux ioctl return values
*
* Description:
*
*    Implement those functions which allow the network daemon to control
*    the network parameters in the driver.  The ioctls include ones to
*    get and set the link speed parameters for the PortServer.
*
******************************************************************************/
static long dgrp_net_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct nd_struct  *nd;
	int    rtn  = 0;
	long   size = _IOC_SIZE(cmd);

	link_t link;

	nd = file->private_data;

	dbg_net_trace(IOCTL, ("net ioctl(%p) start\n", nd));


	if (_IOC_DIR(cmd) & _IOC_READ)
		rtn = access_ok(VERIFY_WRITE, (void *) arg, size);
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		rtn = access_ok(VERIFY_READ,  (void *) arg, size);

	if (rtn == 0)
		goto done;

	rtn = 0;

	switch (cmd) {
	case DIGI_SETLINK:
		if (size != sizeof(link_t)) {
			rtn = -EINVAL;
			break;
		}

		if (copy_from_user((void *)(&link), (void *)arg, size)) {
			rtn = -EFAULT;
			break;
		}

		if (link.lk_fast_rate < 9600)
			link.lk_fast_rate = 9600;

		if (link.lk_slow_rate < 2400)
			link.lk_slow_rate = 2400;

		if (link.lk_fast_rate > 10000000)
			link.lk_fast_rate = 10000000;

		if (link.lk_slow_rate > link.lk_fast_rate)
			link.lk_slow_rate = link.lk_fast_rate;

		if (link.lk_fast_delay > 2000)
			link.lk_fast_delay = 2000;

		if (link.lk_slow_delay > 10000)
			link.lk_slow_delay = 10000;

		if (link.lk_fast_delay < 60)
			link.lk_fast_delay = 60;

		if (link.lk_slow_delay < link.lk_fast_delay)
			link.lk_slow_delay = link.lk_fast_delay;

		if (link.lk_header_size < 2)
			link.lk_header_size = 2;

		if (link.lk_header_size > 128)
			link.lk_header_size = 128;

		link.lk_fast_rate /= 8 * 1000 / dgrp_poll_tick;
		link.lk_slow_rate /= 8 * 1000 / dgrp_poll_tick;

		link.lk_fast_delay /= dgrp_poll_tick;
		link.lk_slow_delay /= dgrp_poll_tick;

		nd->nd_link = link;

		break;

	case DIGI_GETLINK:
		if (size != sizeof(link_t)) {
			rtn = -EINVAL;
			break;
		}

		if (copy_to_user((void *)arg, (void *)(&nd->nd_link), size)) {
			rtn = -EFAULT;
			break;
		}

		break;

	default:
		rtn = -EINVAL;
		break;
	}

done:
	dbg_net_trace(IOCTL, ("net ioctl(%p) return %d\n", nd, rtn));

	return rtn;
}



/*****************************************************************************
*
* Function:
*
*    net_chk_perm
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    inode -- pointer to inode structure for the net communication device
*    op    -- the operation to be tested
*
* Return Values:
*
*    0        -- Success, operation is permitted
*    -EACCESS -- Failure, operation is not permitted
*
* Description:
*
*    The file permissions and ownerships are tested to determine whether
*    the operation "op" is permitted on the file pointed to by the inode.
*
******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) && LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
static int net_chk_perm(struct inode *inode, int op, unsigned int flags)
#else
static int net_chk_perm(struct inode *inode, int op)
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
*    poll_handler
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    dummy -- ignored
*
* Return Values:
*
*    none
*
* Description:
*
*    As each timer expires, it determines (a) whether the "transmit"
*    waiter needs to be woken up, and (b) whether the poller needs to
*    be rescheduled.
*
******************************************************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void poll_handler(ulong dummy)
#else
static void poll_handler(struct timer_list *dummy)
#endif
{
	struct nd_struct *nd;
	link_t *lk;
	ulong  freq;
	ulong  lock_flags;

	dbg_net_trace(POLL, ("net poll start time %d\n", jiffies));

	freq = 1000 / dgrp_poll_tick;

	poll_round += 17;

	if (poll_round >= freq)
		poll_round -= freq;

	/*
	 * Loop to process all open nodes.
	 *
	 * For each node, determine the rate at which it should
	 * be transmitting data.  Then if the node should wake up
	 * and transmit data now, enable the net receive select
	 * to get the transmit going.
	 */

	for (nd = head_nd_struct; nd; nd = nd->nd_inext) {

		lk = &nd->nd_link;

		/*
		 * Decrement statistics.  These are only for use with
		 * KME, so don't worry that the operations are done
		 * unlocked, and so the results are occassionally wrong.
		 */

		nd->nd_read_count  -= (nd->nd_read_count  + poll_round) / freq;
		nd->nd_write_count -= (nd->nd_write_count + poll_round) / freq;
		nd->nd_send_count  -= (nd->nd_send_count  + poll_round) / freq;
		nd->nd_tx_byte	   -= (nd->nd_tx_byte     + poll_round) / freq;
		nd->nd_rx_byte	   -= (nd->nd_rx_byte     + poll_round) / freq;

		/*
		 * Wake the daemon to transmit data only when there is
		 * enough byte credit to send data.
		 *
		 * The results are approximate because the operations
		 * are performed unlocked, and we are inspecting
		 * data asynchronously updated elsewhere.  The whole
		 * thing is just approximation anyway, so that should
		 * be okay.
		 */

		if (lk->lk_slow_rate >= UIO_MAX) {

			nd->nd_delay = 0;
			nd->nd_rate = UIO_MAX;

			nd->nd_tx_deposit = nd->nd_tx_charge + 3 * UIO_MAX;
			nd->nd_tx_credit  = 3 * UIO_MAX;

		} else {

			long rate;
			long delay;
			long deposit;
			long charge;
			long size;
			long excess;

			long seq_in = nd->nd_seq_in;
			long seq_out = nd->nd_seq_out;

			/*
			 * If there are no outstanding packets, run at the
			 * fastest rate.
			 */

			if (seq_in == seq_out) {
				delay = 0;
				rate = lk->lk_fast_rate;
			}

			/*
			 * Otherwise compute the transmit rate based on the
			 * delay since the oldest packet.
			 */

			else {
				/*
				 * The actual delay is computed as the
				 * time since the oldest unacknowledged
				 * packet was sent, minus the time it
				 * took to send that packet to the server.
				 */

				delay = ((jiffies - nd->nd_seq_time[seq_out])
					- (nd->nd_seq_size[seq_out] /
					lk->lk_fast_rate));

				/*
				 * If the delay is less than the "fast"
				 * delay, transmit full speed.  If greater
				 * than the "slow" delay, transmit at the
				 * "slow" speed.   In between, interpolate
				 * between the fast and slow speeds.
				 */

				rate =
				  (delay <= lk->lk_fast_delay ?
				    lk->lk_fast_rate :
				    delay >= lk->lk_slow_delay ?
				      lk->lk_slow_rate :
				      (lk->lk_slow_rate +
				       (lk->lk_slow_delay - delay) *
				       (lk->lk_fast_rate - lk->lk_slow_rate) /
				       (lk->lk_slow_delay - lk->lk_fast_delay)
				      )
				  );
			}

			nd->nd_delay = delay;
			nd->nd_rate = rate;

			/*
			 * Increase the transmit credit by depositing the
			 * current transmit rate.
			 */

			deposit = nd->nd_tx_deposit;
			charge  = nd->nd_tx_charge;

			deposit += rate;

			/*
			 * If the available transmit credit becomes too large,
			 * reduce the deposit to correct the value.
			 *
			 * Too large is the max of:
			 *		6 times the header size
			 * 		3 times the current transmit rate.
			 */

			size = 2 * nd->nd_link.lk_header_size;

			if (size < rate)
				size = rate;

			size *= 3;

			excess = deposit - charge - size;

			if (excess > 0)
				deposit -= excess;

			nd->nd_tx_deposit = deposit;
			nd->nd_tx_credit  = deposit - charge;

			/*
			 * Wake the transmit task only if the transmit credit
			 * is at least 3 times the transmit header size.
			 */

			size = 3 * lk->lk_header_size;

			if (nd->nd_tx_credit < size)
				continue;
		}


/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
		if (!cspinlock(&nd->nd_lock)) {
			drp_poll_nolock++;
			continue;
		}
#endif

		/*
		 * Enable the READ select to wake the daemon if there
		 * is useful work for the drp_read routine to perform.
		 */

		if (waitqueue_active(&nd->nd_tx_waitq) &&
		    (nd->nd_tx_work != 0 ||
		    (ulong)(jiffies - nd->nd_tx_time) >= IDLE_MAX)) {

			dbg_net_trace(POLL, ("net poll woke server(%p) "
			   "credit=%d\n", nd, nd->nd_tx_credit));

			nd->nd_tx_ready = 1;

			wake_up_interruptible(&nd->nd_tx_waitq);

			/* not needed */
			/* nd->nd_flag &= ~ND_SELECT; */
		}

/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
		spinunlock(&nd->nd_lock);
#endif
	}


	/*
	 * Schedule ourself back at the nominal wakeup interval.
	 */

	DGRP_LOCK(GLBL(poll_lock), lock_flags);

	if (--node_active_count > 0) {
		ulong time;

		node_active_count++;

		poll_time += dgrp_jiffies_from_ms(dgrp_poll_tick);

		time = poll_time - jiffies;

		if ((ulong)time >= 2 * dgrp_poll_tick)
			poll_time = jiffies + dgrp_jiffies_from_ms(dgrp_poll_tick);

		poll_start_timer(poll_time);
	}

	DGRP_UNLOCK(GLBL(poll_lock), lock_flags);

	dbg_net_trace(POLL, ("net poll complete\n"));

	return;
}

static void poll_start_timer(ulong time)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
	init_timer(&poll_timer);
	poll_timer.function = poll_handler;
	poll_timer.data = 0;
#else
	timer_setup(&poll_timer, poll_handler, 0);
#endif
	poll_timer.expires = time;
	add_timer(&poll_timer);
}

/*
 * Initializes any poller variables we need.
 */
void dgrp_poll_vars_init(void)
{
	/* Use default of 20ms for our poll rate */
	dgrp_poll_tick = 20;
}
