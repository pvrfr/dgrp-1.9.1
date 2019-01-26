/*****************************************************************************
 *
 * Copyright 1999-2003 Digi International (www.digi.com)
 *     Jeff Randall
 *     James Puzzo  <jamesp at digi dot com>
 *     Scott Kilau  <Scott_Kilau at digi dot com>
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

/* $Id: dgrp_driver.c,v 1.2 2014/03/27 21:26:52 pberger Exp $ */

/*
 *	Driver specific includes
 */
#include "linux_ver_fix.h"
#include <linux/tty.h>

/*
 *  PortServer includes
 */
#include "dgrp_common.h"
#include "dgrp_proc.h"
#include "dgrp_net_ops.h"
#include "dgrp_sysfs.h"

static char *version = DIGI_VERSION;

/*
 * Because this driver is supported on older versions of Linux
 * as well, lets be safe, and just make sure on this one.
 */
#if defined(MODULE_LICENSE)
MODULE_LICENSE("GPL");
#endif

MODULE_AUTHOR("Digi International, http://www.digi.com");
MODULE_DESCRIPTION("RealPort driver for Digi's ethernet-based serial connectivity product line");
MODULE_SUPPORTED_DEVICE("dgrp");


/*
 * insmod command line overrideable parameters
 *
 * NOTE: we use a set of macros to create the variables, which allows
 * us to specify the variable type, name, initial value, and description.
 * This makes things work swell for both 2.0 and 2.1+ kernels.
 *
 * NOTE: these are _only_ the "external" versions of these variables.
 * These are copied (via the initialization procedure) to the internal
 * versions, which have a GLBL prefix (as defined by dgrp_common.h).
 *
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
PARM_INT(rawreadok,		1,	0644,	"Bypass flip buffers on input");
#else
int rawreadok=0;
#endif
PARM_INT(register_cudevices,	1,	0644,	"Turn on/off registering legacy cu devices");
PARM_INT(register_prdevices,	1,	0644,	"Turn on/off registering transparent print devices");
PARM_INT(net_debug,		0,	0644,	"Turn on/off net debugging");
PARM_INT(mon_debug,		0,	0644,	"Turn on/off mon debugging");
PARM_INT(comm_debug,		0,	0644,	"Turn on/off comm debugging");
PARM_INT(tty_debug,		0,	0644,	"Turn on/off tty debugging");
PARM_INT(ports_debug,		0,	0644,	"Turn on/off debugging of /proc/dgrp/ports");

/* Driver load/unload functions */
static int dgrp_init_module(void);
static void dgrp_cleanup_module(void);

#include <linux/init.h>
module_init(dgrp_init_module);
module_exit(dgrp_cleanup_module);


/*
 * Start of driver.
 */
static int dgrp_start(void)
{
	int	rc = 0;

	dbg_trace(("For the tools package or updated drivers please visit http://www.digi.com\n"));

	/*
	 *  Initialize the hash tables for the PortServer structures
	 */
	nd_struct_init();

	/*
	 *  Initialize the poll variables for the poller.
	 */
	dgrp_poll_vars_init();

	/*
	 *  Copy the PARM variable to our driver globals
	 */
	GLBL(rawreadok) = rawreadok;
	GLBL(register_cudevices) = register_cudevices;
	GLBL(register_prdevices) = register_prdevices;
	GLBL(net_debug) = net_debug;
	GLBL(mon_debug) = mon_debug;
	GLBL(tty_debug) = tty_debug;
	GLBL(comm_debug) = comm_debug;
	GLBL(ports_debug) = ports_debug;

	rc = dgrp_create_class_sysfs_files();
	if (rc) {
	  dbg_trace (("%s:%d, dgrp ERROR exit, rc %x\n", __func__, __LINE__, rc));
	  return rc;
	}
	rc = dgrp_proc_register_basic();

	return rc;
}


/*
 * init_module()
 *
 * Module load.  This is where it all starts.
 */
int
dgrp_init_module(void)
{
	dbg_trace(("Digi RealPort driver version: %s\n", version));
	return dgrp_start();
}


/*
 *	Module unload.  This is where it all ends.
 */
void
dgrp_cleanup_module(void)
{
	/*
	 *	Attempting to free resources in backwards
	 *	order of allocation, in case that helps
	 *	memory pool fragmentation.
	 */
	dgrp_proc_unregister_all();

	dgrp_remove_class_sysfs_files();

	/*
	 *  Free any currently allocated PortServer structures
	 */
	nd_struct_cleanup();
}
