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
 *     $Id: linux_ver_fix.h,v 1.1 2008/12/22 20:00:54 scottk Exp $
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
#include <linux/module.h>
#include <linux/tty.h>

#if !defined(KERNEL_VERSION) /* defined in version.h in later kernels */
# define KERNEL_VERSION(a, b, c)  (((a) << 16) + ((b) << 8) + (c))
#endif

#if !defined(TTY_FLIPBUF_SIZE)
# define TTY_FLIPBUF_SIZE 512
#endif

/* Sparse stuff */
#ifndef __user
# define __user
# define __kernel
# define __safe
# define __force
# define __chk_user_ptr(x) (void)0
#endif


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



#define DEVICE_NAME_SIZE 50

#define DGRP_LOCK(x, y)		spin_lock_irqsave(&(x), y)
#define DGRP_UNLOCK(x, y)	spin_unlock_irqrestore(&(x), y)


/*
 *	Additional types needed by header files
 */
typedef unsigned char	uchar;


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)



	/* Nothing Yet */


#else




	#error "this driver does not support anything below the 2.6.27 kernel series."




#endif

#endif /* __LINUX_VER_FIX_H */
