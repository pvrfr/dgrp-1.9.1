/*
 * Copyright 2004 Digi International (www.digi.com)
 *      Scott H Kilau <Scott_Kilau at digi dot com>
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
 *
 * 
 * $Id: dgrp_sysfs.c,v 1.1 2008/12/22 20:02:53 scottk Exp $   
 */

#include "linux_ver_fix.h"
   
#include "dgrp_dpa_ops.h"
#include "dgrp_common.h"
#include "dgrp_tty.h"
#include "dgrp_sysfs.h"

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/serial_reg.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>


/*
 * Unfortunately, I have no choice but to wrap this whole file around 
 * one big ifdef.
 *
 * Because of fundamental changes in the kernel SYSFS implementation
 * which ended up breaking all the existing SYSFS code in the driver,
 * I have no choice but to only fully support SYSFS on 2.6.18+...
 */
#ifdef DGRP_SYSFS_SUPPORT

#define PORTSERVER_DIVIDEND 1843200
#define SERIAL_TYPE_NORMAL      1
#define SERIAL_TYPE_CALLOUT     2
#define SERIAL_TYPE_XPRINT      3


static struct class *dgrp_class;
struct class_device *dgrp_class_global_settings_dev;
struct class_device *dgrp_class_nodes_dev;


static ssize_t dgrp_class_version_show(struct class *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", DIGI_VERSION);
}
static CLASS_ATTR(driver_version, 0400, dgrp_class_version_show, NULL);


static ssize_t dgrp_class_register_with_sysfs_show(struct class_device *c, char *buf)
{
#if defined(REGISTER_TTYS_WITH_SYSFS)
	return snprintf(buf, PAGE_SIZE, "1\n");
#else
	return snprintf(buf, PAGE_SIZE, "0\n");
#endif
}
static CLASS_DEVICE_ATTR(register_with_sysfs, 0400, dgrp_class_register_with_sysfs_show, NULL);


static ssize_t dgrp_class_rawreadok_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", GLBL(rawreadok));
}
static ssize_t dgrp_class_rawreadok_store(struct class_device *c, const char *buf, size_t count)
{
	sscanf(buf, "0x%x\n", &(GLBL(rawreadok)));
	return count;
}
static CLASS_DEVICE_ATTR(rawreadok, 0600, dgrp_class_rawreadok_show, dgrp_class_rawreadok_store);


static ssize_t dgrp_class_pollrate_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", GLBL(poll_tick));
}
static ssize_t dgrp_class_pollrate_store(struct class_device *c, const char *buf, size_t count)
{
	sscanf(buf, "0x%x\n", &(GLBL(poll_tick)));
	return count;
}
static CLASS_DEVICE_ATTR(pollrate, 0600, dgrp_class_pollrate_show, dgrp_class_pollrate_store);


static ssize_t dgrp_class_mon_debug_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx\n", GLBL(mon_debug));
}
static ssize_t dgrp_class_mon_debug_store(struct class_device *c, const char *buf, size_t  count)
{
	sscanf(buf, "0x%lx\n", &(GLBL(mon_debug)));
	return count;
}
static CLASS_DEVICE_ATTR(mon_debug, 0600, dgrp_class_mon_debug_show, dgrp_class_mon_debug_store);


static ssize_t dgrp_class_net_debug_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx\n", GLBL(net_debug));
}
static ssize_t dgrp_class_net_debug_store(struct class_device *c, const char *buf, size_t count)
{
	sscanf(buf, "0x%lx\n", &(GLBL(net_debug)));
	return count;
}
static CLASS_DEVICE_ATTR(net_debug, 0600, dgrp_class_net_debug_show, dgrp_class_net_debug_store);


static ssize_t dgrp_class_tty_debug_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx", GLBL(tty_debug));
}
static ssize_t dgrp_class_tty_debug_store(struct class_device *c, const char *buf, size_t count)
{
	sscanf(buf, "0x%lx\n", &(GLBL(tty_debug)));
	return count;
}
static CLASS_DEVICE_ATTR(tty_debug, 0600, dgrp_class_tty_debug_show, dgrp_class_tty_debug_store);


static ssize_t dgrp_class_comm_debug_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx", GLBL(comm_debug));
}
static ssize_t dgrp_class_comm_debug_store(struct class_device *c, const char *buf, size_t count)
{
	sscanf(buf, "0x%lx\n", &(GLBL(comm_debug)));
	return count;
}
static CLASS_DEVICE_ATTR(comm_debug, 0600, dgrp_class_comm_debug_show, dgrp_class_comm_debug_store);


static ssize_t dgrp_class_ports_debug_show(struct class_device *c, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%lx", GLBL(ports_debug));
}
static ssize_t dgrp_class_ports_debug_store(struct class_device *c, const char *buf, size_t count)
{
	sscanf(buf, "0x%lx\n", &(GLBL(ports_debug)));
	return count;
}
static CLASS_DEVICE_ATTR(ports_debug, 0600, dgrp_class_ports_debug_show, dgrp_class_ports_debug_store);


static struct attribute *dgrp_sysfs_global_settings_entries[] = {
	&class_device_attr_pollrate.attr,
	&class_device_attr_rawreadok.attr,
	&class_device_attr_mon_debug.attr,
	&class_device_attr_net_debug.attr,
	&class_device_attr_tty_debug.attr,
	&class_device_attr_comm_debug.attr,
	&class_device_attr_ports_debug.attr,
	&class_device_attr_register_with_sysfs.attr,
	NULL
};


static struct attribute_group dgrp_global_settings_attribute_group = {
        .name = NULL,
        .attrs = dgrp_sysfs_global_settings_entries,
};



void dgrp_create_class_sysfs_files(void)
{
	int ret = 0;

	dgrp_class = class_create(THIS_MODULE, "digi_realport");
	ret = class_create_file(dgrp_class, &class_attr_driver_version);

	dgrp_class_global_settings_dev = class_device_create(dgrp_class, NULL, MKDEV(0, 0),
		NULL, "driver_settings");

	dgrp_class_nodes_dev = class_device_create(dgrp_class, NULL, MKDEV(0, 0),
		NULL, "nodes");

	ret = sysfs_create_group(&dgrp_class_global_settings_dev->kobj,  
		&dgrp_global_settings_attribute_group);
	if (ret) {
		printk(KERN_ERR "dgrp: failed to create sysfs global settings device attributes.\n");
		sysfs_remove_group(&dgrp_class_global_settings_dev->kobj, 
			&dgrp_global_settings_attribute_group);
		return;
	}

}


void dgrp_remove_class_sysfs_files(void)
{
	struct nd_struct *nd;

	for (nd = head_nd_struct; nd ; nd = nd->nd_inext) {
		dgrp_remove_node_class_sysfs_files(nd);
	}

	sysfs_remove_group(&dgrp_class_global_settings_dev->kobj, 
		&dgrp_global_settings_attribute_group);

	class_device_del(dgrp_class_nodes_dev);
	class_device_del(dgrp_class_global_settings_dev);

	class_remove_file(dgrp_class, &class_attr_driver_version);

	class_device_destroy(dgrp_class, MKDEV(0, 0));
	class_unregister(dgrp_class);
	class_destroy(dgrp_class);
}





static ssize_t dgrp_node_state_show(struct class_device *c, char *buf)
{
	struct nd_struct *nd;

	if (!c)
		return (0);
	nd = (struct nd_struct *) class_get_devdata(c);
	if (!nd)
		return (0);

	return snprintf(buf, PAGE_SIZE, "%s\n", ND_STATE_STR(nd->nd_state));
}
/*
 * Can't use CLASS_DEVICE_ATTR here, because I use
 * the name "state" in the ports info too.
 */
struct class_device_attribute class_device_attr_node_state =
        __ATTR(state_info, 0600, dgrp_node_state_show, NULL);


static ssize_t dgrp_node_description_show(struct class_device *c, char *buf)
{
	struct nd_struct *nd;

	if (!c)
		return (0);
	nd = (struct nd_struct *) class_get_devdata(c);
	if (!nd)
		return (0);

	if (nd->nd_state == NS_READY && nd->nd_ps_desc)
		return snprintf(buf, PAGE_SIZE, "%s\n", nd->nd_ps_desc);
	return 0;
}
static CLASS_DEVICE_ATTR(description_info, 0600, dgrp_node_description_show, NULL);

static ssize_t dgrp_node_hw_version_show(struct class_device *c, char *buf)
{
	struct nd_struct *nd;

	if (!c)
		return (0);
	nd = (struct nd_struct *) class_get_devdata(c);
	if (!nd)
		return (0);

	if (nd->nd_state == NS_READY)
		return snprintf(buf, PAGE_SIZE, "%d.%d\n", (nd->nd_hw_ver >> 8 ) & 0xff,
			nd->nd_hw_ver & 0xff);

	return 0;
}
static CLASS_DEVICE_ATTR(hw_version_info, 0600, dgrp_node_hw_version_show, NULL);

static ssize_t dgrp_node_hw_id_show(struct class_device *c, char *buf)
{
	struct nd_struct *nd;

	if (!c)
		return (0);
	nd = (struct nd_struct *) class_get_devdata(c);
	if (!nd)
		return (0);


	if (nd->nd_state == NS_READY)
		return snprintf(buf, PAGE_SIZE, "%d\n", nd->nd_hw_id);
	return 0;
}
static CLASS_DEVICE_ATTR(hw_id_info, 0600, dgrp_node_hw_id_show, NULL);

static ssize_t dgrp_node_sw_version_show(struct class_device *c, char *buf)
{
	struct nd_struct *nd;

	if (!c)
		return (0);
	nd = (struct nd_struct *) class_get_devdata(c);
	if (!nd)
		return (0);

	if (nd->nd_state == NS_READY)
		return snprintf(buf, PAGE_SIZE, "%d.%d\n", (nd->nd_sw_ver >> 8 ) & 0xff,
			nd->nd_sw_ver & 0xff);

	return 0;
}
static CLASS_DEVICE_ATTR(sw_version_info, 0600, dgrp_node_sw_version_show, NULL);



static struct attribute *dgrp_sysfs_node_entries[] = {
	&class_device_attr_node_state.attr,
	&class_device_attr_description_info.attr,
	&class_device_attr_hw_version_info.attr,
	&class_device_attr_hw_id_info.attr,
	&class_device_attr_sw_version_info.attr,
	NULL
};


static struct attribute_group dgrp_node_attribute_group = {
        .name = NULL,
        .attrs = dgrp_sysfs_node_entries,
};


void dgrp_create_node_class_sysfs_files(struct nd_struct *nd)
{
	int ret;
	char name[10];

	if (nd->nd_ID) {
		ID_TO_CHAR(nd->nd_ID, name);
	}
	else {
		sprintf(name, "node%ld", nd->nd_major);
	}
	nd->nd_class_dev = class_device_create(dgrp_class, dgrp_class_nodes_dev,
		MKDEV(0, nd->nd_major), NULL, name);

	nd->nd_class_ports_dev = class_device_create(dgrp_class, nd->nd_class_dev,
		MKDEV(0, 0), NULL, "ports");

	ret = sysfs_create_group(&nd->nd_class_dev->kobj, &dgrp_node_attribute_group);
	if (ret) {
		printk(KERN_ERR "dgrp: failed to create sysfs node device attributes.\n");
		sysfs_remove_group(&nd->nd_class_dev->kobj, &dgrp_node_attribute_group);
		return;
	}

	class_set_devdata(nd->nd_class_dev, nd);
}


void dgrp_remove_node_class_sysfs_files(struct nd_struct *nd)
{

	if (nd->nd_class_dev) {
		sysfs_remove_group(&nd->nd_class_dev->kobj, &dgrp_node_attribute_group);
		class_device_destroy(dgrp_class, MKDEV(0,nd->nd_major));
		nd->nd_class_dev = NULL;
	}

	
}



static ssize_t dgrp_tty_state_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);

	return snprintf(buf, PAGE_SIZE, "%s\n", un->un_open_count ? "Open" : "Closed");
}
static DEVICE_ATTR(state_info, 0600, dgrp_tty_state_show, NULL);


static ssize_t dgrp_tty_baud_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%d\n",
		un->un_open_count ? (PORTSERVER_DIVIDEND / ch->ch_s_brate) : 0);
}
static DEVICE_ATTR(baud_info, 0400, dgrp_tty_baud_show, NULL);


static ssize_t dgrp_tty_msignals_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);

	if (ch->ch_open_count) {
		return snprintf(buf, PAGE_SIZE, "%s %s %s %s %s %s\n",
			(ch->ch_s_mlast & DM_RTS) ? "RTS" : "",
			(ch->ch_s_mlast & DM_CTS) ? "CTS" : "",
			(ch->ch_s_mlast & DM_DTR) ? "DTR" : "",
			(ch->ch_s_mlast & DM_DSR) ? "DSR" : "",
			(ch->ch_s_mlast & DM_CD) ? "DCD" : "",
			(ch->ch_s_mlast & DM_RI)  ? "RI"  : "");
	}
	return 0;
}
static DEVICE_ATTR(msignals_info, 0400, dgrp_tty_msignals_show, NULL);


static ssize_t dgrp_tty_iflag_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%x\n", ch->ch_s_iflag);
}
static DEVICE_ATTR(iflag_info, 0600, dgrp_tty_iflag_show, NULL);


static ssize_t dgrp_tty_cflag_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%x\n", ch->ch_s_cflag);
}
static DEVICE_ATTR(cflag_info, 0600, dgrp_tty_cflag_show, NULL);


static ssize_t dgrp_tty_oflag_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%x\n", ch->ch_s_oflag);
}
static DEVICE_ATTR(oflag_info, 0600, dgrp_tty_oflag_show, NULL);


static ssize_t dgrp_tty_digi_flag_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%x\n", ch->ch_digi.digi_flags);
}
static DEVICE_ATTR(digi_flag_info, 0600, dgrp_tty_digi_flag_show, NULL);


static ssize_t dgrp_tty_rxcount_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%d\n", ch->ch_rxcount);
}
static DEVICE_ATTR(rxcount_info, 0600, dgrp_tty_rxcount_show, NULL);


static ssize_t dgrp_tty_txcount_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct ch_struct *ch;
	struct un_struct *un;

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	return snprintf(buf, PAGE_SIZE, "%d\n", ch->ch_txcount);
}
static DEVICE_ATTR(txcount_info, 0600, dgrp_tty_txcount_show, NULL);


static ssize_t dgrp_tty_name_show(struct device *d, struct device_attribute *attr, char *buf)
{
	struct nd_struct *nd;
	struct ch_struct *ch;
	struct un_struct *un;
	char name[10];

	if (!d)
		return (0);
	un = (struct un_struct *) dev_get_drvdata(d);
	if (!un)
		return (0);
	ch = un->un_ch;
	if (!ch)
		return (0);
	nd = ch->ch_nd;
	if (!nd)
		return (0);

	ID_TO_CHAR(nd->nd_ID, name);

	return snprintf(buf, PAGE_SIZE, "%s%s%02d\n",
		un->un_type == SERIAL_TYPE_XPRINT ? "pr" : "tty",
		name, ch->ch_portnum);
}
static DEVICE_ATTR(custom_name, 0600, dgrp_tty_name_show, NULL);


static struct attribute *dgrp_sysfs_tty_entries[] = {
	&dev_attr_state_info.attr,
	&dev_attr_baud_info.attr,
	&dev_attr_msignals_info.attr,
	&dev_attr_iflag_info.attr,
	&dev_attr_cflag_info.attr,
	&dev_attr_oflag_info.attr,          
	&dev_attr_digi_flag_info.attr,
	&dev_attr_rxcount_info.attr,
	&dev_attr_txcount_info.attr,
	&dev_attr_custom_name.attr,
	NULL
};


static struct attribute_group dgrp_tty_attribute_group = {
        .name = NULL,
        .attrs = dgrp_sysfs_tty_entries,
};


void dgrp_create_tty_sysfs(struct un_struct *un, struct device *c)
{
	int ret;

	ret = sysfs_create_group(&c->kobj, &dgrp_tty_attribute_group);
	if (ret) {
		printk(KERN_ERR "dgrp: failed to create sysfs tty device attributes.\n");
		sysfs_remove_group(&c->kobj, &dgrp_tty_attribute_group);
		return;
	}

	dev_set_drvdata(c, un);

}

 
void dgrp_remove_tty_sysfs(struct device *c)
{
	sysfs_remove_group(&c->kobj, &dgrp_tty_attribute_group);
}




#endif  /* DGRP_SYSFS */

