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
 *****************************************************************************/

/****************************************************************************
 *
 *  Filename:
 *
 *     $Id: dgrp_common.c,v 1.3 2014/09/28 20:16:49 pberger Exp $
 *
 *  Description:
 *
 *     Definitions of global variables and functions which are either
 *     shared by the tty, mon, and net drivers; or which cross them
 *     functionally (like the poller).
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/

#include "linux_ver_fix.h"
#include <linux/errno.h>
#include <linux/tty.h>

#include "dgrp_common.h"
#include "dgrp_tty.h"
#include <linux/sched.h>	/* For in_egroup_p() */
#include <linux/sched/signal.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/slab.h>	/* For in_egroup_p() */
#endif



/*---------------------------------------------------------------------*
 *
 *  Global Variables
 *
 *---------------------------------------------------------------------*/

int dgrp_wait_control;		/* Wait for control complete */
int dgrp_wait_write;		/* Wait for write complete */
int dgrp_rawreadok;		/* Bypass flipbuf on input */
int dgrp_register_cudevices;	/* Turn on/off registering legacy cu devices */
int dgrp_register_prdevices;	/* Turn on/off registering transparent print */
int dgrp_poll_tick;		/* Poll interval - in ms */

spinlock_t dgrp_poll_lock;	/* Poll scheduling lock */


/* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
b_sema_t dgrp_allocate_semaphore;	/* Locks node allocation */
#endif


/*
 *  Debugging control variables
 */
unsigned long dgrp_net_debug    = 0;
unsigned long dgrp_tty_debug    = 0;
unsigned long dgrp_comm_debug   = 0;
unsigned long dgrp_mon_debug    = 0;
unsigned long dgrp_ports_debug  = 0;

/*
 *  Patchable constants.
 */
int dgrp_ttime = 100;			/* Realport ttime value */
int dgrp_rtime = 100;			/* Realport etime value */


/*-----------------------------------------------------------------------*
 *
 *  Common helper functions
 *
 *-----------------------------------------------------------------------*/

/*
 *  Support for the "assert" macro.
 */

void dgrp_assert_fail(char *file, int line)
{
	static int count = 0;

	if (count < 500) {
		dbg_trace(("assertion failure file=%s, line=%d\n", file, line));
		count++;
	}

	if (count == 500) {
		dbg_trace(("Too many messages.  No more will be displayed,\n"));
		dbg_trace(("though continued errors are likely.\n"));
		count++;
	}
}

/*
 *  Encodes 4 bytes of unsigned data.
 */

void dgrp_encode_u4(uchar *buf, uint data)
{
	buf[0] = data >> 24;
	buf[1] = data >> 16;
	buf[2] = data >> 8;
	buf[3] = data;
}


/*
 *  Encodes 2 bytes of unsigned data.
 */

void dgrp_encode_u2(uchar *buf, ushort data)
{
	buf[0] = data >> 8;
	buf[1] = data;
}


/*
 *  Decodes 2 bytes of unsigned data.
 */

ushort dgrp_decode_u2(uchar *buf)
{
	return (((uchar *)(buf))[0] << 8) | (((uchar *)(buf))[1]);
}


/*
 *  Allocate and then zero.
 */
void *dgrp_kzmalloc(size_t size, int priority)
{
	void	*p;

	p = kmalloc(size, priority);
	if (p)
		memset(p, 0, size);
	return p;
}


/*
 *  Sleep for a specified number of "ticks"
 *
 *  Returns 0 on success, !0 if interrupted
 */
int dgrp_sleep(ulong ticks)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(ticks);
	current->state = TASK_RUNNING;
	return signal_pending(current);
}



/*-----------------------------------------------------------------------*
 *
 *  Common operations (either used by more than one of net, mon, or tty,
 *                     or in interrupt context (i.e. the poller))
 *
 *-----------------------------------------------------------------------*/


/************************************************************************
 * Determines when CARRIER changes state and takes appropriate
 * action.
 ************************************************************************/
void dgrp_carrier(struct ch_struct *ch)
{
	struct nd_struct *nd;

	int virt_carrier = 0;
	int phys_carrier = 0;

	dbg_comm_trace(IOCTL, ("...somebody called carrier... \n"));
	/* fix case when the tty has already closed. */

	if (!ch)
		return;
	nd  = ch->ch_nd;
	if (!nd)
		return;

	/*
	 *  If we are currently waiting to determine the status of the port,
	 *  we don't yet know the state of the modem lines.  As a result,
	 *  we ignore state changes when we are waiting for the modem lines
	 *  to be established.  We know, as a result of code in dgrp_net_ops,
	 *  that we will be called again immediately following the reception
	 *  of the status message with the true modem status flags in it.
	 */
	if (ch->ch_expect & RR_STATUS) {
		dbg_comm_trace(IOCTL, ("dgrp_carrier(%x:%x) transition while querying "
					"status.  ch_expect (%x)\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty)),
					ch->ch_expect));
		return;
	}


/* TODO: historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
#if 0
	assert(owns_spinlock(&nd->nd_lock));
#endif

	/*
	 * If CH_HANGUP is set, we gotta keep trying to get all the processes
	 * that have the port open to close the port.
	 * So lets just keep sending a hangup every time we get here.
	 */
	if (ch->ch_flag & CH_HANGUP) {
		if (ch->ch_tun.un_open_count > 0)
			tty_hangup(ch->ch_tun.un_tty);
	}


	/*
	 *  Compute the effective state of both the physical and virtual
	 *  senses of carrier.
	 */

	if (ch->ch_s_mlast & DM_CD)
		phys_carrier = 1;

	if (ALWAYS_FORCES_CARRIER(ch->ch_category) ||
	    (ch->ch_s_mlast & DM_CD) ||
	    (ch->ch_digi.digi_flags & DIGI_FORCEDCD) ||
	    (ch->ch_flag & CH_CLOCAL)) {
		virt_carrier = 1;
	}

	dbg_comm_trace(IOCTL, ("dgrp_carrier(%x:%x) ch_flag (%x) phys (%d) virt (%d)\n",
				MAJOR(tty_devnum(ch->ch_tun.un_tty)),
				MINOR(tty_devnum(ch->ch_tun.un_tty)),
				ch->ch_flag, phys_carrier, virt_carrier));

	/*
	 *  Test for a VIRTUAL carrier transition to HIGH.
	 *
	 *  The CH_HANGUP condition is intended to prevent any action
	 *  except for close.  As a result, we ignore positive carrier
	 *  transitions during CH_HANGUP.
	 */
	if (((ch->ch_flag & CH_HANGUP)  == 0) &&
	    ((ch->ch_flag & CH_VIRT_CD) == 0) &&
	    (virt_carrier == 1)) {
		/*
		 * When carrier rises, wake any threads waiting
		 * for carrier in the open routine.
		 */

		dbg_comm_trace(IOCTL, ("dgrp_carrier(%x:%x) carrier rose\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty))));
		nd->nd_tx_work = 1;

		dbg_comm_trace(IOCTL, ("dgrp_carrier(%x:%x) sending wakeups\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty))));

		if (waitqueue_active(&(ch->ch_flag_wait)))
			wake_up_interruptible(&ch->ch_flag_wait);
	}

	/*
	 *  Test for a PHYSICAL transition to low, so long as we aren't
	 *  currently ignoring physical transitions (which is what "virtual
	 *  carrier" indicates).
	 *
	 *  The transition of the virtual carrier to low really doesn't
	 *  matter... it really only means "ignore carrier state", not
	 *  "make pretend that carrier is there".
	 */
	if ((virt_carrier == 0) &&
	    ((ch->ch_flag & CH_PHYS_CD) != 0) &&
	    (phys_carrier == 0)) {
		/*
		 * When carrier drops:
		 *
		 *   Do a Hard Hangup if that is called for.
		 *
		 *   Drop carrier on all open units.
		 *
		 *   Flush queues, waking up any task waiting in the
		 *   line discipline.
		 *
		 *   Send a hangup to the control terminal.
		 *
		 *   Enable all select calls.
		 */

		dbg_comm_trace(IOCTL, ("dgrp_carrier(%x:%x) carrier dropped\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty))));

		nd->nd_tx_work = 1;

		if (HARD_HANGUP(ch->ch_category))
			ch->ch_flag |= CH_HANGUP;

		ch->ch_flag &= ~(CH_LOW | CH_EMPTY | CH_DRAIN | CH_INPUT);

		dbg_comm_trace(IOCTL, ("dgrp_carrier(%x:%x) sending wakeups\n",
					MAJOR(tty_devnum(ch->ch_tun.un_tty)),
					MINOR(tty_devnum(ch->ch_tun.un_tty))));

		if (waitqueue_active(&(ch->ch_flag_wait)))
			wake_up_interruptible(&ch->ch_flag_wait);

		if (ch->ch_tun.un_open_count > 0) {
			dbg_comm_trace(IOCTL, ("dgrp_carrier: Sending tty hangup\n"));
			tty_hangup(ch->ch_tun.un_tty);
		}

		if (ch->ch_pun.un_open_count > 0) {
			dbg_comm_trace(IOCTL, ("dgrp_carrier: Sending pr hangup\n"));
			tty_hangup(ch->ch_pun.un_tty);
		}
	}

	/*
	 *  Make sure that our cached values reflect the current reality.
	 */
	if (virt_carrier == 1)
		ch->ch_flag |= CH_VIRT_CD;
	else
		ch->ch_flag &= ~CH_VIRT_CD;

	if (phys_carrier == 1)
		ch->ch_flag |= CH_PHYS_CD;
	else
		ch->ch_flag &= ~CH_PHYS_CD;

} /* dgrp_carrier */


/****************************************************************************
 *
 *     Describe a set of functions to manipulate both the set of
 *     portserver structures in a hash table, and also a doubly-linked
 *     list of structures for browsing.
 *
 *****************************************************************************/

/*
 *    To insert into hash table requires two letter ID and integer major
 *    number.  The major number will be used to calculate the hash number.
 *    The two letter ID is simply used for display of the major numbers.
 *
 *    The entries in the hash table are the portserver device structures
 *    themselves.  They are singly linked in the table.
 *
 *    All entries will also be included in a doubly-linked list (for
 *    iteration purposes).
 *
 *    The implicit assumption is that ALL entries will have unique
 *    major numbers.
 *
 */

/*
 *  The global hash and list structures
 */
struct nd_struct *hash_nd_struct[HASHMAX];
struct nd_struct *head_nd_struct;

/*
 *  ID manipulation macros (where c1 & c2 are unsigned characters, i is
 *  a long integer, and s is a character array of at least three members
 */

inline void ID_TO_CHAR(long i, char *s)
{
	s[0] = ((i & 0xff00)>>8);
	s[1] = (i & 0xff);
	s[2] = 0;
}

static inline long CHARS_TO_ID(char c1, char c2)
{
	long i;
	i = (((c1 & 0xff) << 8) | (c2 & 0xff));
	return i;
}

inline long CHAR_TO_ID(char *s)
{
	return CHARS_TO_ID(s[0], s[1]);
}


inline void nd_struct_init(void)
{
	int i;

	for (i = 0; i < HASHMAX; i++)
		hash_nd_struct[i] = NULL;

	head_nd_struct = NULL;
}

inline void nd_struct_cleanup(void)
{
	int i;
	struct nd_struct *tmp;

	while (head_nd_struct) {
		tmp = head_nd_struct->nd_inext;

		/*
		 *  Unregistration associated with this node
		 *  should occur here
		 */
		dgrp_tty_uninit(head_nd_struct);

		kfree(head_nd_struct);
		head_nd_struct = tmp;
	}

	for (i = 0; i < HASHMAX; i++)
		hash_nd_struct[i] = NULL;
}


/*
 *  Find the address of the pointer to the object, OR the pointer to
 *  an empty pointer where it belongs.
 */
static inline struct nd_struct **nd_struct_find(long major)
{
	long hash;
	struct nd_struct **tmp;

	hash = major % HASHMAX;

	for (tmp = &hash_nd_struct[hash]; *tmp; tmp = &((*tmp)->nd_hnext))
		if (major == (*tmp)->nd_major)
			break;

	return tmp;
}

inline int nd_struct_add(struct nd_struct *entry)
{
	struct nd_struct **ptr;

	ptr = nd_struct_find(entry->nd_major);

	/* If it is already in the hash table, fail */
	if (*ptr)
		return -EBUSY;

	entry->nd_hnext = NULL;
	*ptr = entry;

	if (head_nd_struct)
		head_nd_struct->nd_iprev = entry;

	entry->nd_inext = head_nd_struct;
	entry->nd_iprev = NULL;
	head_nd_struct = entry;

	return 0;
}

inline struct nd_struct *nd_struct_get(long major)
{
	struct nd_struct **tmp;

	tmp = nd_struct_find(major);

	return *tmp;
}

inline int nd_struct_del(struct nd_struct *entry)
{
	struct nd_struct **ptr;
	struct nd_struct *tmp;

	ptr = nd_struct_find(entry->nd_major);

	tmp = *ptr;

	/* If it isn't already in the hash table, fail */
	if (!tmp)
		return -ENODEV;

	*ptr = tmp->nd_hnext;

	if (entry->nd_inext)
		entry->nd_inext->nd_iprev = entry->nd_iprev;

	if (entry->nd_iprev)
		entry->nd_iprev->nd_inext = entry->nd_inext;
	else
		head_nd_struct = entry->nd_inext;

	return 0;
}



/****************************************************************************
 *
 *     Function and definitions designed to allow control of "tracing".
 *
 *****************************************************************************/

#define TRACE_TO_CONSOLE

#define SZ_MAXMSG     900

#if !defined(TRACE_TO_CONSOLE)
void dgrp_tracef(const char *fmt, ...)
{
	return;
}

#else /* defined(TRACE_TO_CONSOLE) */

void dgrp_tracef(const char *fmt, ...)
{
	va_list  ap;
	char     buf[SZ_MAXMSG+1];
	size_t   lenbuf;
	int      i;

	/* Format buf using fmt and arguments contained in ap. */
	va_start(ap, fmt);
	i = vsprintf(buf, fmt,  ap);
	va_end(ap);
	lenbuf = strlen(buf);

	printk(DRVSTR ": ");
	printk(buf);
}

#endif /* !defined(TRACE_TO_CONSOLE) */


/****************************************************************************
 *
 *     Functions for creating and removing proc entries consistently
 *     across kernel versions.
 *
 *****************************************************************************/

#include <linux/proc_fs.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
struct proc_dir_entry *dgrp_create_proc_entry(const char *name, mode_t mode,
						struct proc_dir_entry *parent)
{
	return create_proc_entry(name, mode, parent);
}
#else
struct proc_dir_entry *dgrp_create_proc_entry(const char *name, mode_t mode,
						struct proc_dir_entry *parent,
						struct file_operations *fops,
						void *data)
{
	return proc_create_data(name, mode, parent, fops, data);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
void dgrp_remove_proc_entry(struct proc_dir_entry *pde)
{
	if (!pde)
		return;

	remove_proc_entry(pde->name, pde->parent);

}
#else
void dgrp_remove_proc_entry(struct nd_struct *nd, struct proc_dir_entry *parent)
{
	char buf[3];

	ID_TO_CHAR(nd->nd_ID, buf);
	remove_proc_entry(buf, parent);

}
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
int dgrp_in_egroup_p(gid_t grp)
#else
int dgrp_in_egroup_p(kgid_t grp)
#endif
{
	return in_egroup_p(grp);
}
