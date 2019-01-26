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
 *      NOTE TO LINUX KERNEL HACKERS:  DO NOT REFORMAT THIS CODE!
 *
 *      This is shared code between Digi's CVS archive and the
 *      Linux Kernel sources.
 *      Changing the source just for reformatting needlessly breaks
 *      our CVS diff history.
 *
 *      Send any bug fixes/changes to:  Eng.Linux at digi dot com.
 *      Thank you.
 *
 *****************************************************************************/

/****************************************************************************
 *
 *  Filename:
 *
 *     $Id: dgrp_common.h,v 1.3 2014/09/28 20:16:49 pberger Exp $
 *
 *  Description:
 *
 *     Declarations of global variables and functions which are either
 *     shared by the tty, mon, and net drivers; or which cross them
 *     functionally (like the node hashing functions).
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/


#ifndef __DGRP_COMMON_H
#define __DGRP_COMMON_H

#include "drp.h"

/************************************************************************
 *
 *	Driver identification, error and debugging statments
 *
 *	In theory, you can change all occurances of "dgrp" in the next
 *	six lines, and the driver function names and printk's will
 *	all automagically change.
 *
 ************************************************************************/

#define	PROCSTR		"dgrp"			/* /proc entries */
#define	USRLIBSTR	"/usr/lib/dg/dgrp"	/* /usr/lib/ entries */
#define	DEVSTR		"/dev/dg/dgrp"		/* /dev entries */
#define	DRVSTR		"dgrp"			/* Driver name string
						 * displayed by dbg_trace */


#define	GLBL(var)	dgrp_##var


/************************************************************************
 * Debug trace definitions.
 ************************************************************************/

#define DEBUG_OPEN		0x00000001
#define DEBUG_CLOSE		0x00000002
#define DEBUG_READ		0x00000004
#define DEBUG_WRITE		0x00000008

#define DEBUG_IOCTL		0x00000010
#define DEBUG_SELECT		0x00000020
#define DEBUG_INPUT		0x00000040
#define DEBUG_OUTPUT		0x00000080

#define DEBUG_INIT		0x00000100
#define DEBUG_UNINIT		0x00000200
#define DEBUG_POLL		0x00000400

#define DEBUG_KME		0x00008000

#define DEBUG_ALWAYS		(-1)
#define DEBUG_NEVER		(0)


#if defined(DGRP_TRACER)

extern void dgrp_tracef(const char *fmt, ...);

#define dbg_trace(expr)				\
{						\
	dgrp_tracef expr;			\
}

#define dbg_mon_trace(flag, expr)		\
{						\
    if ((DEBUG_ ## flag & GLBL(mon_debug)) != 0)\
	dgrp_tracef expr;			\
}

#define dbg_net_trace(flag, expr)		\
{						\
    if ((DEBUG_ ## flag & GLBL(net_debug)) != 0)\
	dgrp_tracef expr;			\
}

#define dbg_tty_trace(flag, expr)		\
{						\
    if ((DEBUG_ ## flag & GLBL(tty_debug)) != 0)\
	dgrp_tracef expr;			\
}

#define dbg_comm_trace(flag, expr)		\
{						\
    if ((DEBUG_ ## flag & GLBL(comm_debug)) != 0)\
	dgrp_tracef expr;			\
}

#define dbg_ports_trace(flag, expr)		\
{						\
    if ((DEBUG_ ## flag & GLBL(ports_debug)) != 0)\
	dgrp_tracef expr;			\
}

#else /* DGRP_TRACER */

#define dbg_trace(expr)			{ printk(DRVSTR": "); printk(expr); }
#define dbg_mon_trace(flag, expr)	{}
#define dbg_net_trace(flag, expr)	{}
#define dbg_tty_trace(flag, expr)	{}
#define dbg_comm_trace(flag, expr)	{}
#define dbg_ports_trace(flag, expr)	{}

#endif



/************************************************************************
 * All global storage allocation.
 ************************************************************************/

extern ulong GLBL(mon_debug);               /* Monitor debug flags */
extern ulong GLBL(net_debug);               /* Network debug flags */
extern ulong GLBL(tty_debug);               /* TTY debug flags */
extern ulong GLBL(comm_debug);              /* Common code debug flags */
extern ulong GLBL(ports_debug);             /* /proc/dgrp/ports debug flags */

extern int GLBL(wait_control);          /* Wait for control complete */
extern int GLBL(wait_write);            /* Wait for write complete */
extern int GLBL(rawreadok);             /* Allow raw writing of input */
extern int GLBL(register_cudevices);	/* Turn on/off registering legacy cu devices */
extern int GLBL(register_prdevices);	/* Turn on/off registering transparent print devices */
extern int GLBL(poll_tick);             /* Poll interval - in ms */


extern spinlock_t (GLBL(poll_lock));   /* Poll scheduling lock */

#if 0 /* TODO : historical locking placeholder */
/*
 *  In the HPUX version of the RealPort driver (which served as a basis
 *  for this driver) this locking code was used.  Saved if ever we need
 *  to review the locking under Linux.
 */
extern b_sema_t GLBL(allocate_semaphore);  /* Locks node allocation */
#endif




/*
 * Patchable constants.
 */

extern int GLBL(ttime);			/* Realport ttime value */
extern int GLBL(rtime);			/* Realport rtime value */



/*-----------------------------------------------------------------------*
 *
 *  Declarations for common helper functions
 *
 *-----------------------------------------------------------------------*/

/*
 *  Support for assert() macro.
 */

void dgrp_assert_fail(char *file, int line);

#define assert(x) { if (!(x)) dgrp_assert_fail(__FILE__, __LINE__); }

/*
 *  Macro to handle different singal functions
 *
 *	a = (int) tty->pgrp (or current->pgrp...)
 * 	b = (int) SIGNAL
 * 	c = (int) when???  (1, on_exit,...)
 *
 *  some signal functions are
 *	gsignal(a, b, c)
 *	send_sig(a, b, c)
 *	kill_pg(a, b, c)
 */

#define DGRP_SIGNAL(a, b, c) \
		kill_pg(a, b, c)



/*
 *  Encoding and decoding of information in network data messages
 */

void	dgrp_encode_u4(uchar *buf, uint   data);
void	dgrp_encode_u2(uchar *buf, ushort data);
ushort	dgrp_decode_u2(uchar *buf);

/*
 *  Useful utility.
 */
void   *dgrp_kzmalloc(size_t size, int priority);
int     dgrp_sleep(ulong ticks);


/*-----------------------------------------------------------------------*
 *
 *  Declarations for common operations:
 *
 *      (either used by more than one of net, mon, or tty,
 *       or in interrupt context (i.e. the poller))
 *
 *-----------------------------------------------------------------------*/

void dgrp_carrier(struct ch_struct *ch);


/*-----------------------------------------------------------------------*
 *
 *  Declarations for node structure hashing operations
 *
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
 *-----------------------------------------------------------------------*/

/*
 *  ID manipulation macros (where c1 & c2 are characters, i is
 *  a long integer, and s is a character array of at least three members
 */

void ID_TO_CHAR(long i, char *s);
long CHAR_TO_ID(char *s);


/*
 *  Hash Table definitions
 */
#define HASHMAX 13

extern struct nd_struct *hash_nd_struct[HASHMAX];
extern struct nd_struct *head_nd_struct;

void   nd_struct_init(void);
void   nd_struct_cleanup(void);
int    nd_struct_add(struct nd_struct *entry);
struct nd_struct *nd_struct_get(long major);
int    nd_struct_del(struct nd_struct *entry);


/*
 *  "proc" table manipulation functions
 */
#include <linux/proc_fs.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
struct proc_dir_entry *dgrp_create_proc_entry(const char *name, mode_t mode,
					      struct proc_dir_entry *parent);
void dgrp_remove_proc_entry(struct proc_dir_entry *pde);
#else
struct proc_dir_entry *dgrp_create_proc_entry(const char *name, mode_t mode,
						struct proc_dir_entry *parent,
						struct file_operations *fops,
						void *data);
void dgrp_remove_proc_entry(struct nd_struct *nd, struct proc_dir_entry *parent);
#endif

/*
 *  wrapper for "in_egroup_p"
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
int dgrp_in_egroup_p(gid_t grp);
#else
int dgrp_in_egroup_p(kgid_t grp);
#endif


#endif /* __DGRP_COMMON_H */
