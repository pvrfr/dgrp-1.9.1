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
 *     $Id: dgrp_mon_ops.h,v 1.1 2008/12/22 20:00:54 scottk Exp $
 *
 *  Description:
 *
 *     Declarations for externally accessible data members and structures
 *     of the "mon" devices.
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/

#include <linux/proc_fs.h>
#include "drp.h"

int register_mon_device(struct nd_struct *, struct proc_dir_entry *);
int unregister_mon_device(struct nd_struct *, struct proc_dir_entry *);
