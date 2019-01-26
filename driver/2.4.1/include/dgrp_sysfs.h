/*
 * Copyright 2003 Digi International (www.digi.com)
 *	Scott H Kilau <Scott_Kilau at digi dot com>
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
 *	NOTE: THIS IS A SHARED HEADER. DO NOT CHANGE CODING STYLE!!!
 */

#ifndef __DGRP_SYSFS_H
#define __DGRP_SYSFS_H

/* Determine whether we support SYSFS or not... */
#include "linux_ver_fix.h"


#ifdef DGRP_SYSFS_SUPPORT

#include <linux/device.h>

extern void dgrp_create_class_sysfs_files(void);
extern void dgrp_remove_class_sysfs_files(void);

extern void dgrp_create_node_class_sysfs_files(struct nd_struct *nd);
extern void dgrp_remove_node_class_sysfs_files(struct nd_struct *nd);

extern void dgrp_create_tty_sysfs(struct un_struct *un, struct device *c);
extern void dgrp_remove_tty_sysfs(struct device *c);

#else /* DGRP_SYSFS_SUPPORT */

#define dgrp_create_class_sysfs_files() do {} while (0)
#define dgrp_remove_class_sysfs_files() do {} while (0)

#define dgrp_create_node_class_sysfs_files(a) do {} while (0)
#define dgrp_remove_node_class_sysfs_files(a) do {} while (0)

#define dgrp_create_tty_sysfs(a, b) do {} while (0)
#define dgrp_remove_tty_sysfs(a) do {} while (0)

#endif /* DGRP_SYSFS_SUPPORT */


#endif
