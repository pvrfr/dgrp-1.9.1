/*****************************************************************************
 *
 * Copyright 1999 Digi International (www.digi.com)
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

#include "linux_ver_fix.h"

#include <linux/tty.h>

/*
 *  PortServer includes
 */
#include "dgrp_common.h"


/************************************************************************/
/*									*/
/*	/proc/dgrp/{*} functions						*/
/*									*/
/************************************************************************/
#include <linux/proc_fs.h>


/*
 *	The directory entry /proc/dgrp
 */
static struct proc_dir_entry *ProcDgRp;

extern void dgrp_specproc_init(struct proc_dir_entry *root);
extern void dgrp_specproc_uninit(struct proc_dir_entry *root);

/*
 *	Register the basic /proc/dgrp files that appear whenever
 *	the driver is loaded.
 */
void
dgrp_proc_register_basic(void)
{
	/*
	 *	Register /proc/dgrp
	 */
	ProcDgRp = FN(create_proc_entry)(PROCSTR, S_IFDIR, &proc_root);

	dgrp_specproc_init(ProcDgRp);
}


/*
 *	Unregister all of our /proc/dgrp/{*} devices
 */
void
dgrp_proc_unregister_all(void)
{

	#define NUKE_ONE(P) \
		if (P) { FN(remove_proc_entry)(P); P = NULL; } else

	dgrp_specproc_uninit(ProcDgRp);

	NUKE_ONE(ProcDgRp);
}
