/*****************************************************************************
 *
 * Copyright 1999 Digi International (www.digi.com)
 *     James Puzzo <jamesp at digi dot com>
 *     Scott Kilau <scottk at digi dot com>
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
 *     $Id: linux_ver_fix.h,v 1.1 2008/12/22 20:02:54 scottk Exp $
 *
 *  Description:
 *
 *     This header provides a number of common includes and definitions
 *     which assist in writing C code to be built on multiple versions of
 *     Linux.
 *
 *  Author:
 *
 *     James A. Puzzo
 *     Scott H. Kilau - 2.4.x / 2.6.x+ changes.
 *
 *****************************************************************************/

#ifndef __LINUX_VER_FIX_H
#define __LINUX_VER_FIX_H

#include <linux/version.h>

/* Paranoia */
#if !defined(KERNEL_VERSION) /* defined in version.h in later kernels */
# define KERNEL_VERSION(a,b,c)  (((a) << 16) + ((b) << 8) + (c))
#endif


/*
 *	Additional types needed by header files
 */
typedef unsigned char	uchar;


/*
 *	Some API's changed at linux version 2.1.x.  I'm not sure exactly
 *	when they changed during the 2.1.x development, but it doesn't
 *	matter to us.  Create some macros to preserve compatibility.
 *
 *	Here you can see what has changed between 2.0 and 2.1+. If you
 *	are reading/writing this file, you should be well aware of these
 *	macros or you won't be able to follow whats going on.  The
 *	alternative was #ifdef spaghetti.
 *
 *      Of course, even more changed in the 2.3.x timeframe... we only
 *      really care about later 2.3 and 2.4, so we ignore the fact that
 *      some of these code changes may not have occured yet in early 2.3.
 *
 *      This file has now become a grand document on the changes 
 *      the linux kernel has gone thru at the tty level over 
 *      the 2.x series of kernels.  -Scott
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

	/* Sparse stuff */
	# ifndef __user
	#  define __user
	#  define __kernel
	#  define __safe
	#  define __force
	#  define __chk_user_ptr(x) (void)0
	# endif

	#include <linux/module.h>

	#include <linux/sched.h>
	#include <linux/tty.h>
	#include <linux/tty_driver.h>
	#include <asm/uaccess.h>
	#include <linux/poll.h>

	#if !defined(DEFINE_SPINLOCK)
	# define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED  
	#endif

	#if !defined(TTY_FLIPBUF_SIZE)
	# define TTY_FLIPBUF_SIZE 512
	#endif

	/* Linux 2.6.9+ all have the new Alan Cox tty locking in it... */
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
		#define NEW_TTY_LOCKING
	#endif

	#ifndef NEW_TTY_LOCKING

		#define	tty_ldisc_flush(tty) \
			if ((tty)->ldisc.flush_buffer) { \
				(tty)->ldisc.flush_buffer((tty)); \
			}

		#define tty_wakeup(tty) \
			if ((test_bit(TTY_DO_WRITE_WAKEUP, &(tty)->flags)) && \
			    (tty)->ldisc.write_wakeup) { \
				((tty)->ldisc.write_wakeup)((tty)); \
			}

		#define tty_ldisc_ref(tty) (&(tty)->ldisc)

		#define tty_ldisc_deref(ld)

	#endif

	#define DGRP_MOD_INC_USE_COUNT(rtn) (rtn = 1)
	#define DGRP_MOD_DEC_USE_COUNT

	# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)

	#  define DGRP_MOD_INC_USE_COUNT_PROC(rtn) (rtn = try_module_get(THIS_MODULE))
	#  define DGRP_MOD_DEC_USE_COUNT_PROC (module_put(THIS_MODULE))

	# else

	#  define DGRP_MOD_INC_USE_COUNT_PROC(rtn) (rtn = 1)
	#  define DGRP_MOD_DEC_USE_COUNT_PROC

	# endif

	/* They removed suser call, now its capable() */
	#define suser(x) (capable(CAP_SYS_ADMIN))

	#define DGRP_MAJOR(x) (major(x))
	#define DGRP_MINOR(x) (minor(x))

	#define DGRP_TTY_MAJOR(tty)	(MAJOR(tty_devnum(tty)))
	#define DGRP_TTY_MINOR(tty)	(MINOR(tty_devnum(tty)))

	#define DGRP_GET_PDE(x) (PDE(inode))

	#define IOREMAP(ADDR, LEN)		ioremap(ADDR, LEN)
	#define IOUNMAP(ADDR)			iounmap(ADDR)
	#define COPY_FROM_USER(DST,SRC,LEN)	copy_from_user(DST,SRC,LEN)
	#define COPY_TO_USER(DST,SRC,LEN)	copy_to_user(DST,SRC,LEN)
	#define GET_USER(A1,A2)			get_user(A1,(unsigned int *)A2)
	#define PUT_USER(A1,A2)			put_user(A1,(unsigned long *)A2)

	#define	READ_PROTO \
		struct file *file, char *buf, size_t count, loff_t *ppos
	#define	READ_ARGS	file, buf, count, ppos
	#define	READ_RETURN_T	ssize_t

	#define	WRITE_PROTO \
		struct file *file, const char *buf, size_t count, loff_t *ppos
	#define	WRITE_ARGS	file, buf, count, ppos
	#define	WRITE_RETURN_T	ssize_t

	#define SELECT_NAME	poll
	#define SELECT_PROTO \
		struct file *file, struct poll_table_struct *table
	#define SELECT_ARGS	file, table
	#define SELECT_RETURN_T	unsigned int

	#define	CLOSE_RETURN_T	int
	#define	CLOSE_RETURN(X)	return(X)

	#define PARM_I_RDEV     file->f_dentry->d_inode->i_rdev
	#define PARM_GEN_IP     PDE(file->f_dentry->d_inode)
	#define PTR_POS()	ppos
	#define	GET_POS()	*ppos
	#define	ADD_POS(AMT)	*ppos += (AMT)
	#define	SET_POS(AMT)	*ppos = (AMT)

	#define	PCI_PRESENT()	pci_present()


	# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)

	#define	PARM_STR(VAR, INIT, PERM, DESC) \
		static char *VAR = INIT; \
		module_param(VAR, charp, PERM); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_INT(VAR, INIT, PERM, DESC) \
		static int VAR = INIT; \
		module_param(VAR, int, PERM); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_ULONG(VAR, INIT, PERM, DESC) \
		static ulong VAR = INIT; \
		module_param(VAR, long, PERM); \
		MODULE_PARM_DESC(VAR, DESC);

	# else

	#define	PARM_STR(VAR, INIT, PERM, DESC) \
		static char *VAR = INIT; \
		MODULE_PARM(VAR, "s"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_INT(VAR, INIT, PERM, DESC) \
		static int VAR = INIT; \
		MODULE_PARM(VAR, "i"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_ULONG(VAR, INIT, PERM, DESC) \
		static ulong VAR = INIT; \
		MODULE_PARM(VAR, "l"); \
		MODULE_PARM_DESC(VAR, DESC);

	# endif

	#define SPINLOCK_DECLARE(VAR)	spinlock_t VAR
	#define SPINLOCK_INIT(x)	spin_lock_init(&(x))

	#define DGRP_LOCK(x,y)		spin_lock_irqsave(&(x), y)
	#define DGRP_UNLOCK(x,y)	spin_unlock_irqrestore(&(x), y)

	#define TERMIOS_LINE_B4_CC	0

	#ifdef MODULE
		#define DGRP_USE_COUNT		GET_USE_COUNT(&__this_module)
	#else
		#define DGRP_USE_COUNT		0
	#endif

	#define PROC_DE_GET_COUNT(deP) (atomic_read(&((deP)->count)))
	#define PROC_DE_OPS_INIT(deP,iops,fops) \
		{ (deP)->proc_iops = (iops); \
		  (deP)->proc_fops = (fops); }

	typedef wait_queue_t		WAITQ;
	typedef wait_queue_head_t	WAITQ_HEAD;

	#define ALLOW_ALT_FAIL_OPEN 0
	#define ALT_FAIL_OPEN_DEFAULT 0

	#define DGRP_GET_TTY_COUNT(x) (x->count)

	/* Linux 2.6.16+ all have the new Alan Cox tty buffer changes in it... */
	# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
	#  define NEW_TTY_BUFFERING
	# endif

	/*
	 * Include support for SYSFS on all 2.6.18+ kernels.
	 */
	# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
	#  define DGRP_SYSFS_SUPPORT
	# else
	#  undef DGRP_SYSFS_SUPPORT
	# endif

	/*
	 * They really started reworking the naming convention of
	 * the MUTEX_INIT define, switching to one thing, and then
	 * another.
	 * This mess of ifdefs catalog the changes, while working
	 * around their naming mess
	 */
	# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
	# define DGRP_MUTEX_INITIALIZER(name)   __SEMAPHORE_INITIALIZER(name, 1)
	#else
	# define DGRP_MUTEX_INITIALIZER(name)   __MUTEX_INITIALIZER(name)
	#endif




#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)




	/* Sparse stuff */
	# ifndef __user
	#  define __user
	#  define __kernel
	#  define __safe
	#  define __force
	#  define __chk_user_ptr(x) (void)0
	# endif

	#include <linux/config.h>
	#include <linux/module.h>

	#include <linux/sched.h>
	#include <linux/tty.h>
	#include <linux/tty_driver.h>
	#include <asm/uaccess.h>
	#include <linux/poll.h>

	/* No SYSFS stuff in 2.4 */
	#undef DGRP_SYSFS_SUPPORT

	/* These functions exist ONLY in 2.6+ */
	#define tty_register_device(a, b, c) do {} while (0)
	#define tty_unregister_device(a, b) do {} while (0)

	#if !defined(DEFINE_SPINLOCK)
	# define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED  
	#endif

	/* Linux 2.4.29+ all have the new Alan Cox tty locking in it... */
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,29)
		#define NEW_TTY_LOCKING
	#endif

	#ifndef NEW_TTY_LOCKING

		#define	tty_ldisc_flush(tty) \
			if ((tty)->ldisc.flush_buffer) { \
				(tty)->ldisc.flush_buffer((tty)); \
			}

		#define tty_wakeup(tty) \
			if ((test_bit(TTY_DO_WRITE_WAKEUP, &(tty)->flags)) && \
			    (tty)->ldisc.write_wakeup) { \
				((tty)->ldisc.write_wakeup)((tty)); \
			}

		#define tty_ldisc_ref(tty) (&(tty)->ldisc)

		#define tty_ldisc_deref(ld)

	#endif

	#define DGRP_MOD_INC_USE_COUNT(rtn) \
	{ \
		MOD_INC_USE_COUNT; \
		rtn = 1; \
	}

	#define DGRP_MOD_DEC_USE_COUNT MOD_DEC_USE_COUNT

	#define DGRP_MOD_INC_USE_COUNT_PROC(rtn) \
	{ \
		MOD_INC_USE_COUNT; \
		rtn = 1; \
	}

	#define DGRP_MOD_DEC_USE_COUNT_PROC MOD_DEC_USE_COUNT

	#define DGRP_MAJOR(x) MAJOR(x)
	#define DGRP_MINOR(x) (x)
	#define DGRP_TTY_MAJOR(tty)	(MAJOR(tty->device))
	#define DGRP_TTY_MINOR(tty)	(MINOR(tty->device))

	#define DGRP_GET_PDE(x) ((struct proc_dir_entry *) (inode->u.generic_ip))

	#define IOREMAP(ADDR, LEN)		ioremap(ADDR, LEN)
	#define IOUNMAP(ADDR)			iounmap(ADDR)
	#define COPY_FROM_USER(DST,SRC,LEN)	copy_from_user(DST,SRC,LEN)
	#define COPY_TO_USER(DST,SRC,LEN)	copy_to_user(DST,SRC,LEN)
	#define GET_USER(A1,A2)			get_user(A1,(unsigned int *)A2)
	#define PUT_USER(A1,A2)			put_user(A1,(unsigned long *)A2)

	#define	READ_PROTO \
		struct file *file, char *buf, size_t count, loff_t *ppos
	#define	READ_ARGS	file, buf, count, ppos
	#define	READ_RETURN_T	ssize_t

	#define	WRITE_PROTO \
		struct file *file, const char *buf, size_t count, loff_t *ppos
	#define	WRITE_ARGS	file, buf, count, ppos
	#define	WRITE_RETURN_T	ssize_t

	#define SELECT_NAME	poll
	#define SELECT_PROTO \
		struct file *file, struct poll_table_struct *table
	#define SELECT_ARGS	file, table
	#define SELECT_RETURN_T	unsigned int

	#define	CLOSE_RETURN_T	int
	#define	CLOSE_RETURN(X)	return(X)

	#define	PARM_I_RDEV	file->f_dentry->d_inode->i_rdev
	#define	PARM_GEN_IP	file->f_dentry->d_inode->u.generic_ip
	#define PTR_POS()	ppos
	#define	GET_POS()	*ppos
	#define	ADD_POS(AMT)	*ppos += (AMT)
	#define	SET_POS(AMT)	*ppos = (AMT)

	#define	PCI_PRESENT()	pci_present()

	#define	PARM_STR(VAR, INIT, PERM, DESC) \
		static char *VAR = INIT; \
		MODULE_PARM(VAR, "s"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_INT(VAR, INIT, PERM, DESC) \
		static int VAR = INIT; \
		MODULE_PARM(VAR, "i"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_ULONG(VAR, INIT, PERM, DESC) \
		static ulong VAR = INIT; \
		MODULE_PARM(VAR, "l"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define SPINLOCK_DECLARE(VAR)	spinlock_t VAR
	#define SPINLOCK_INIT(x)	spin_lock_init(&(x))

	#define DGRP_LOCK(x,y)		spin_lock_irqsave(&(x), y)
	#define DGRP_UNLOCK(x,y)	spin_unlock_irqrestore(&(x), y)

	#define TERMIOS_LINE_B4_CC	0

	#ifdef MODULE
		#define DGRP_USE_COUNT		GET_USE_COUNT(&__this_module)
	#else
		#define DGRP_USE_COUNT		0
	#endif

	#define PROC_DE_GET_COUNT(deP) (atomic_read(&((deP)->count)))
	#define PROC_DE_OPS_INIT(deP,iops,fops) \
		{ (deP)->proc_iops = (iops); \
		  (deP)->proc_fops = (fops); }

	typedef wait_queue_t		WAITQ;
	typedef wait_queue_head_t	WAITQ_HEAD;

	/*
	 *  In some revisions within the 2.4 kernel series, there is a patch
	 *  which changes the behavior of a failed open.  In 2.0 and 2.2 and
	 *  most 2.4 revisions, a failed open will be followed _always_ by
	 *  a call to the driver's close routine.  In some 2.4 revisions,
	 *  the close will not be called (and therefore driver use counts
	 *  will be too high, and ports may become unusable).
	 *
	 *  In the case that the ALT_FAIL_OPEN code is allowed to exist,
	 *  some new parameters are required, like ALT_FAIL_OPEN_DEFAULT.
	 */
	#define ALLOW_ALT_FAIL_OPEN 1
	#define ALT_FAIL_OPEN_DEFAULT 0


	/*
	 * Contrary to what I have been led to believe by some of the
	 * kernel guru's, not all 2.4.x kernels have min/max macro's
	 * in the kernel.h header.
	 * I call your attention to the Red Hat AS 2.4.9-e.27 kernel.
	 */
	#ifndef min
		#define min(a,b) (((a)<(b))?(a):(b))
	#endif
	#ifndef max
		#define max(a,b) (((a)>(b))?(a):(b))
	#endif

	#if defined(REDHAT_AS_30)  || defined(REDHAT_ES_30) || defined(REDHAT_WS_30) || \
	    defined(REDHAT_AS_3)   || defined(REDHAT_ES_3)  || defined(REDHAT_WS_3)  || \
	    defined(REDHATAS30)    || defined(REDHATES30)   || defined(REDHATWS30)   || \
	    defined(REDHATAS3)     || defined(REDHATES3)    || defined(REDHATWS3)    || \
	    defined(REDHAT_FEDORA) || defined(FEDORA)       || \
	    defined(REDHAT_10)     || defined(REDHAT10)

		#define DGRP_GET_TTY_COUNT(x) (atomic_read(&(x->count)))
	#else

		#define DGRP_GET_TTY_COUNT(x) (x->count)

	#endif

	# define DGRP_MUTEX_INITIALIZER(name)   __MUTEX_INITIALIZER(name)



#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)




	/* Sparse stuff */
	# ifndef __user
	#  define __user
	#  define __kernel
	#  define __safe
	#  define __force
	#  define __chk_user_ptr(x) (void)0
	# endif

	#include <linux/config.h>
	#include <linux/module.h>

	#include <linux/sched.h>
	#include <asm/uaccess.h>
	#include <linux/poll.h>

	#if !defined(DEFINE_SPINLOCK)
	# define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED  
	#endif

	#ifndef NEW_TTY_LOCKING

		#define	tty_ldisc_flush(tty) \
			if ((tty)->ldisc.flush_buffer) { \
				(tty)->ldisc.flush_buffer((tty)); \
			}

		#define tty_wakeup(tty) \
			if ((test_bit(TTY_DO_WRITE_WAKEUP, &(tty)->flags)) && \
			    (tty)->ldisc.write_wakeup) { \
				((tty)->ldisc.write_wakeup)((tty)); \
			}

		#define tty_ldisc_ref(tty) (&(tty)->ldisc)

		#define tty_ldisc_deref(ld)

	#endif

	#define DGRP_MOD_INC_USE_COUNT(rtn) \
	{ \
		MOD_INC_USE_COUNT; \
		rtn = 1; \
	}

	#define DGRP_MOD_DEC_USE_COUNT MOD_DEC_USE_COUNT

	#define DGRP_MOD_INC_USE_COUNT_PROC(rtn) \
	{ \
		MOD_INC_USE_COUNT; \
		rtn = 1; \
	}

	#define DGRP_MOD_DEC_USE_COUNT_PROC MOD_DEC_USE_COUNT

	#define DGRP_MAJOR(x) MAJOR(x)
	#define DGRP_MINOR(x) (x)
	#define DGRP_TTY_MAJOR(tty)	(MAJOR(tty->device))
	#define DGRP_TTY_MINOR(tty)	(MINOR(tty->device))

	#define DGRP_GET_PDE(x) ((struct proc_dir_entry *) (inode->u.generic_ip))

	#define IOREMAP(ADDR, LEN)		ioremap(ADDR, LEN)
	#define IOUNMAP(ADDR)			iounmap(ADDR)
	#define COPY_FROM_USER(DST,SRC,LEN)	copy_from_user(DST,SRC,LEN)
	#define COPY_TO_USER(DST,SRC,LEN)	copy_to_user(DST,SRC,LEN)
	#define GET_USER(A1,A2)			get_user(A1,(unsigned int *)A2)
	#define PUT_USER(A1,A2)			put_user(A1,(unsigned long *)A2)

	#define	READ_PROTO \
		struct file *file, char *buf, size_t count, loff_t *ppos
	#define	READ_ARGS	file, buf, count, ppos
	#define	READ_RETURN_T	ssize_t

	#define	WRITE_PROTO \
		struct file *file, const char *buf, size_t count, loff_t *ppos
	#define	WRITE_ARGS	file, buf, count, ppos
	#define	WRITE_RETURN_T	ssize_t

	#define SELECT_NAME	poll
	#define SELECT_PROTO \
		struct file *file, struct poll_table_struct *table
	#define SELECT_ARGS	file, table
	#define SELECT_RETURN_T	unsigned int

	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,15)\
		&& LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
	#define SERIAL_HAVE_POLL_WAIT 1
		/* This allows the driver to wake up select or poll processes
		 * when write space becomes available.  Only later 2.2.x
		 * kernels require this 8-21-2001
		*/
	#endif

	#define	CLOSE_RETURN_T	int
	#define	CLOSE_RETURN(X)	return(X)

	#define	PARM_I_RDEV	file->f_dentry->d_inode->i_rdev
	#define	PARM_GEN_IP	file->f_dentry->d_inode->u.generic_ip
	#define PTR_POS()	ppos
	#define	GET_POS()	*ppos
	#define	ADD_POS(AMT)	*ppos += (AMT)
	#define	SET_POS(AMT)	*ppos = (AMT)

	#define	PCI_PRESENT()	pci_present()

	#define	PARM_STR(VAR, INIT, PERM, DESC) \
		static char *VAR = INIT; \
		MODULE_PARM(VAR, "s"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_INT(VAR, INIT, PERM, DESC) \
		static int VAR = INIT; \
		MODULE_PARM(VAR, "i"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define	PARM_ULONG(VAR, INIT, PERM, DESC) \
		static ulong VAR = INIT; \
		MODULE_PARM(VAR, "l"); \
		MODULE_PARM_DESC(VAR, DESC);

	#define SPINLOCK_DECLARE(VAR)	spinlock_t VAR
	#define SPINLOCK_INIT(x)	spin_lock_init(&(x))

	#define DGRP_LOCK(x,y)		spin_lock_irqsave(&(x), y)
	#define DGRP_UNLOCK(x,y)	spin_unlock_irqrestore(&(x), y)

	#define TERMIOS_LINE_B4_CC	0

	#ifdef MODULE
		#define DGRP_USE_COUNT		GET_USE_COUNT(&__this_module)
	#else
		#define DGRP_USE_COUNT		0
	#endif

	#define PROC_DE_GET_COUNT(deP) ((deP)->count)
	#define PROC_DE_OPS_INIT(deP,iops,fops) { (deP)->ops = (iops); }

	typedef struct wait_queue	WAITQ;
	typedef struct wait_queue *	WAITQ_HEAD;

	#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,2,17)
		/* starting at 2.2.18 these macros are included in the
		 * kernel header files
		 */
		#define init_MUTEX(mutexP)	{ *(mutexP) = MUTEX ; }
		#define DECLARE_WAITQUEUE(w,c)	WAITQ w = { c, NULL }
		#define init_waitqueue_head(qptr) { *qptr = NULL; }
	#endif

	#define ALLOW_ALT_FAIL_OPEN 0

	/*
	 * 2.2.x doesn't define these in the kernel headers,
	 * so we do it here.
	 */
	#define min(a,b) (((a)<(b))?(a):(b))
	#define max(a,b) (((a)>(b))?(a):(b))

	# define DGRP_MUTEX_INITIALIZER(mutex)  MUTEX



#else



	#error "this driver does not support anything below the 2.2 kernel series."



#endif

#endif /* __LINUX_VER_FIX_H */
