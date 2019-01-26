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
*     $Id: dgrp_specproc.h,v 1.1 2008/12/22 20:02:54 scottk Exp $
*
*  Description:
*
*     Describes the private structures used to manipulate the "special"
*     proc constructs (not read-only) used by the Digi RealPort software.
*     The concept is borrowed heavily from the "sysctl" interface of
*     the kernel.  I decided not to use the structures and functions
*     provided by the kernel for two reasons:
*
*       1. Due to the planned use of "/proc" in the RealPort driver, many
*          of the functions of the "sysctl" interface would go unused.
*          A simpler interface will be easier to maintain.
*
*       2. I'd rather divorce our "added package" from the kernel internals.
*          If the "sysctl" structures should change, I will be insulated
*          from those changes.  These "/proc" entries won't be under the
*          "sys" tree anyway, so there is no need to maintain a strict
*          dependence relationship.
*
*  Author:
*
*     James A. Puzzo
*
*****************************************************************************/

#ifndef _DGRP_RW_PROC_H
#define _DGRP_RW_PROC_H

/*
 *  The list of DGRP entries with r/w capabilities.  These
 *  magic numbers are used for identification purposes.
 */
enum {
        DGRP_CONFIG = 1,	/* Configure portservers */
        DGRP_NETDIR = 2,	/* Directory for "net" devices */
        DGRP_MONDIR = 3,	/* Directory for "mon" devices */
        DGRP_PORTSDIR = 4,	/* Directory for "ports" devices */
        DGRP_INFO = 5,		/* Get info. about the running module */
        DGRP_NODEINFO = 6,	/* Get info. about the configured nodes */
        DGRP_DPADIR = 7,	/* Directory for the "dpa" devices */
};

/*
 *  Directions for proc handlers
 */
enum {
        INBOUND = 1,		/* Data being written to kernel */
        OUTBOUND = 2,		/* Data being read from the kernel */

};

/*
 *  Each entry in a DGRP proc directory is described with a
 *  "dgrp_proc_entry" structure.  A collection of these
 *  entries (in an array) represents the members associated
 *  with a particular "/proc" directory, and is referred to
 *  as a table.  All "tables" are terminated by an entry with
 *  zeros for every member.
 *
 *  The structure members are as follows:
 *
 *    int magic              -- ID number associated with this particular
 *                              entry.  Should be unique across all of
 *                              DGRP.
 *
 *    const char *name       -- ASCII name associated with the /proc entry.
 *
 *    mode_t mode            -- File access permisssions for the /proc entry.
 *
 *    dgrp_proc_entry *child -- When set, this entry refers to a directory,
 *                              and points to the table which describes the
 *                              entries in the subdirectory
 *
 *    dgrp_proc_handler *read_handler -- When set, points to the fxn which
 *                                       handle outbound data flow
 *
 *    dgrp_proc_handler *write_handler -- When set, points to the fxn which
 *                                        handles inbound data flow
 *
 *    struct proc_dir_entry *de -- Pointer to the directory entry for this
 *                                 object once registered.  Used to grab
 *                                 the handle of the object for
 *                                 unregistration
 */

typedef struct dgrp_proc_entry dgrp_proc_entry;

typedef int dgrp_proc_handler (dgrp_proc_entry *table, int dir,
                               struct file *filp,      void *buffer,
                               READ_RETURN_T *lenp,    loff_t *ppos);

struct dgrp_proc_entry {
        int                magic;         /* Integer identifier */
        const char        *name;          /* ASCII identifier */
        mode_t             mode;          /* File access permissions */
        struct dgrp_proc_entry *child;    /* Child pointer */
        dgrp_proc_handler *read_handler;  /* Pointer to outbound data fxn */
        dgrp_proc_handler *write_handler; /* Pointer to inbound data fxn */
        struct proc_dir_entry *de;        /* proc entry pointer */

	struct semaphore   excl_sem;      /* Protects exclusive access var */
	int                excl_cnt;      /* Counts number of curr accesses */
};


#endif /* _DGRP_RW_PROC_H */
