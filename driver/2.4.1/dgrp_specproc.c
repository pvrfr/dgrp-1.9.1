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
 *****************************************************************************/

/****************************************************************************
 *
 *  Filename:
 *
 *     $Id: dgrp_specproc.c,v 1.2 2015/01/22 07:02:42 pberger Exp $
 *
 *  Description:
 *
 *     Handle the "config" proc entry for the linux realport device driver
 *     and provide slots for the "net" and "mon" devices
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *****************************************************************************/

#include "linux_ver_fix.h"
#include <linux/tty.h>

#include <linux/proc_fs.h>

#include <linux/ctype.h>
#include "dgrp_specproc.h"

#include "dgrp_common.h"
#include "dgrp_net_ops.h"
#include "dgrp_mon_ops.h"
#include "dgrp_ports_ops.h"
#include "dgrp_dpa_ops.h"
#include "dgrp_tty.h"

static dgrp_proc_entry dgrp_table[];

/* File operation declarations */

STATIC int            FN(gen_proc_open)  (struct inode *, struct file *);
STATIC CLOSE_RETURN_T FN(gen_proc_close) (struct inode *, struct file *);
STATIC READ_RETURN_T  FN(gen_proc_read)  (READ_PROTO);
STATIC WRITE_RETURN_T FN(gen_proc_write) (WRITE_PROTO);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int proc_chk_perm(struct inode *, int, struct nameidata *);
#else
static int proc_chk_perm(struct inode *, int);
#endif

static int parse_write_config(char *buf);



static struct file_operations proc_file_ops =
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	owner: THIS_MODULE,
#endif
	read: FN(gen_proc_read),	/* read		*/
	write: FN(gen_proc_write),	/* write	*/
	open: FN(gen_proc_open),	/* open		*/
	release: FN(gen_proc_close),	/* release	*/
};

static struct inode_operations proc_inode_ops =
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
	default_file_ops: &proc_file_ops,
#endif
	permission: proc_chk_perm
};


static void register_proc_table(dgrp_proc_entry *, struct proc_dir_entry *);
static void unregister_proc_table(dgrp_proc_entry *, struct proc_dir_entry *);

static dgrp_proc_entry dgrp_net_table[];
static dgrp_proc_entry dgrp_mon_table[];
static dgrp_proc_entry dgrp_ports_table[];
static dgrp_proc_entry dgrp_dpa_table[];

static dgrp_proc_handler write_config;
static dgrp_proc_handler read_config;

static dgrp_proc_handler write_info;
static dgrp_proc_handler read_info;

static dgrp_proc_handler read_nodeinfo;

static dgrp_proc_entry dgrp_table[] = {
	{DGRP_CONFIG,   "config", 0644, NULL, &read_config, &write_config,
	                NULL, DGRP_MUTEX_INITIALIZER(dgrp_table[0].excl_sem), 0},
	{DGRP_INFO,     "info", 0644, NULL, &read_info, &write_info,
	                NULL, DGRP_MUTEX_INITIALIZER(dgrp_table[1].excl_sem), 0},
	{DGRP_NODEINFO, "nodeinfo", 0644, NULL, &read_nodeinfo, NULL,
	                NULL, DGRP_MUTEX_INITIALIZER(dgrp_table[2].excl_sem), 0},
	{DGRP_NETDIR,   "net",   0500, dgrp_net_table},
	{DGRP_MONDIR,   "mon",   0500, dgrp_mon_table},
	{DGRP_PORTSDIR, "ports", 0500, dgrp_ports_table},
	{DGRP_DPADIR,   "dpa",   0500, dgrp_dpa_table},
	{0}
};

static struct proc_dir_entry *net_entry_pointer;
static struct proc_dir_entry *mon_entry_pointer;
static struct proc_dir_entry *dpa_entry_pointer;
static struct proc_dir_entry *ports_entry_pointer;

static dgrp_proc_entry dgrp_net_table[] = {
	{0}
};

static dgrp_proc_entry dgrp_mon_table[] = {
	{0}
};

static dgrp_proc_entry dgrp_ports_table[] = {
	{0}
};

static dgrp_proc_entry dgrp_dpa_table[] = {
	{0}
};

void dgrp_specproc_init(struct proc_dir_entry *root)
{
	register_proc_table(dgrp_table, root);
}

void dgrp_specproc_uninit(struct proc_dir_entry *root)
{
	unregister_proc_table(dgrp_table, root);
	net_entry_pointer = NULL;
	mon_entry_pointer = NULL;
	dpa_entry_pointer = NULL;
	ports_entry_pointer = NULL;
}


/* test_perm does NOT grant the superuser all rights automatically, because
   some entries are readonly even to root. */

static int test_perm(int mode, int op)
{
	if (!current->euid)
		mode >>= 6;
	else if (FN(in_egroup_p(0)))
		mode >>= 3;
	if ((mode & op & 0007) == op)
		return 0;
	return -EACCES;
}


/*
 * /proc/sys support
 */
static int dgrp_proc_match(int len, const char *name, struct proc_dir_entry *de)
{
        if (!de || !de->low_ino)
                return 0;
        if (de->namelen != len)
                return 0;
        return !memcmp(name, de->name, len);
}


/*
 *  Scan the entries in table and add them all to /proc at the position
 *  referred to by "root"
 */
static void register_proc_table(dgrp_proc_entry *table,
				struct proc_dir_entry *root)
{
	struct proc_dir_entry *de;
	int len;
	mode_t mode;
	
	for (; table->magic; table++) {
		/* Can't do anything without a proc name. */
		if (!table->name)
			continue;
		/* Maybe we can't do anything with it... */
		if (!table->read_handler && !table->write_handler &&
		    !table->child) {
			printk(KERN_WARNING "DGRP PROC: Can't register %s\n",
				table->name);
			continue;
		}

		len = strlen(table->name);
		mode = table->mode;

		de = NULL;
		if (!table->child)
			mode |= S_IFREG;
		else {
			mode |= S_IFDIR;
			for (de = root->subdir; de; de = de->next) {
				if (dgrp_proc_match(len, table->name, de))
					break;
			}
			/* If the subdir exists already, de is non-NULL */
		}

		if (!de) {
			de = FN(create_proc_entry)(table->name, mode, root);
			if (!de)
				continue;
			de->data = (void *) table;
			if (!table->child) {
				PROC_DE_OPS_INIT(de, &proc_inode_ops, &proc_file_ops);
			}
		}
		table->de = de;
		if (de->mode & S_IFDIR)
			register_proc_table(table->child, de);

		if (table->magic == DGRP_NETDIR)
			net_entry_pointer = de;

		if (table->magic == DGRP_MONDIR)
			mon_entry_pointer = de;

		if (table->magic == DGRP_DPADIR)
			dpa_entry_pointer = de;

		if (table->magic == DGRP_PORTSDIR)
			ports_entry_pointer = de;
	}
}

/*
 * Unregister a /proc sysctl table and any subdirectories.
 */
static void unregister_proc_table(dgrp_proc_entry *table,
				  struct proc_dir_entry *root)
{
	struct proc_dir_entry *de;
	struct nd_struct *tmp;

	if (table == dgrp_net_table) 
		for (tmp = head_nd_struct; tmp; tmp = tmp->nd_inext) 
			if(tmp->nd_net_de)
				unregister_net_device(tmp, root);

	if (table == dgrp_mon_table)
		for (tmp = head_nd_struct; tmp; tmp = tmp->nd_inext) 
			if (tmp->nd_mon_de)
				unregister_mon_device(tmp, root);

	if (table == dgrp_dpa_table)
		for (tmp = head_nd_struct; tmp; tmp = tmp->nd_inext) 
			if (tmp->nd_dpa_de)
				unregister_dpa_device(tmp, root);

	if (table == dgrp_ports_table)
		for (tmp = head_nd_struct; tmp; tmp = tmp->nd_inext) 
			if (tmp->nd_ports_de)
				unregister_ports_device(tmp, root);

	for (; table->magic; table++) {
		if (!(de = table->de))
			continue;
		if (de->mode & S_IFDIR) {
			if (!table->child) {
				printk(KERN_ALERT "Help - malformed sysctl tree on free\n");
				continue;
			}
			unregister_proc_table(table->child, de);

			/* Don't unregister directories which still have entries.. */
			if (de->subdir)
				continue;
		}

		/* Don't unregoster proc entries that are still being used.. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
		if (PROC_DE_GET_COUNT(de) != 1)
#else
		if (PROC_DE_GET_COUNT(de))
#endif
			continue;


		FN(remove_proc_entry)(de);
		table->de = NULL;
	}
}

STATIC int FN(gen_proc_open) (struct inode *inode, struct file *file)
{
	struct proc_dir_entry *de;
	dgrp_proc_entry *entry;
	int ret = 0;
	
	de = (struct proc_dir_entry*) PARM_GEN_IP;
	if (!de || !de->data) {
		ret = -ENXIO;
		goto done;
	}

	entry = (struct dgrp_proc_entry *) de->data;
	if (!entry) {
		ret = -ENXIO;
		goto done;
	}

	down(&entry->excl_sem);

	if (entry->excl_cnt)
		ret = -EBUSY;
	else
		entry->excl_cnt++;

	up(&entry->excl_sem);

done:
	if (!ret) {
		DGRP_MOD_INC_USE_COUNT(ret);
		if (!ret)
			ret = -ENXIO;
		else
			ret = 0;
	}

	return ret;
}

STATIC CLOSE_RETURN_T FN(gen_proc_close) (struct inode *inode,
                                           struct file *file)
{
	struct proc_dir_entry *de;
	dgrp_proc_entry *entry;
	
	de = (struct proc_dir_entry *) PARM_GEN_IP;
	if (!de || !de->data) 
		goto done;

	entry = (struct dgrp_proc_entry *) de->data;
	if (!entry) 
		goto done;

	down(&entry->excl_sem);

	if(entry->excl_cnt)
		entry->excl_cnt = 0;

	up(&entry->excl_sem);

done:
	DGRP_MOD_DEC_USE_COUNT;
	CLOSE_RETURN(0);
}

STATIC READ_RETURN_T FN(gen_proc_read) (READ_PROTO)
{
	struct proc_dir_entry *de;
	dgrp_proc_entry *entry;
	dgrp_proc_handler *handler;
	READ_RETURN_T res;
	READ_RETURN_T error;
	
	de = (struct proc_dir_entry*) PARM_GEN_IP;
	if (!de || !de->data)
		return -ENXIO;

	entry = (struct dgrp_proc_entry *) de->data;
	if (!entry)
		return -ENXIO;

	/* Test for read permission */
	if (test_perm(entry->mode, 4))
		return -EPERM;

	res = count;

	handler = entry->read_handler;
	if (!handler)
		return -ENXIO;

	error = (*handler) (entry, OUTBOUND, file, buf, &res, PTR_POS());
	if (error)
		return error;

	return res;
}

STATIC WRITE_RETURN_T FN(gen_proc_write) (WRITE_PROTO)
{
	struct proc_dir_entry *de;
	dgrp_proc_entry *entry;
	dgrp_proc_handler *handler;
	WRITE_RETURN_T res;
	WRITE_RETURN_T error;
	
	de = (struct proc_dir_entry *) PARM_GEN_IP;
	if (!de || !de->data)
		return -ENXIO;

	entry = (struct dgrp_proc_entry *) de->data;
	if (!entry)
		return -ENXIO;

	/* Test for write permission */
	if (test_perm(entry->mode, 2))
		return -EPERM;

	res = count;

	handler = entry->write_handler;
	if (!handler)
		return -ENXIO;

	error = (*handler) (entry, INBOUND, file, (char *)buf,
	                     &res, PTR_POS());
	if (error)
		return error;

	return res;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
STATIC int proc_chk_perm(struct inode *inode, int op, struct nameidata *notused)
#else
static int proc_chk_perm(struct inode *inode, int op)
#endif
{
	return test_perm(inode->i_mode, op);
}


/*
 *  When writing configuration information, each "record" (i.e. each
 *  write) is treated as an independent request.  See the "parse"
 *  description for more details.
 */
static int write_config(dgrp_proc_entry *table, int dir,
                         struct file *filp,      void *buffer,
                         READ_RETURN_T *lenp,    loff_t *ppos)
{
	int  retval;
	long len;
	#define INBUFLEN 256
	char inbuf[INBUFLEN];

	if ((!*lenp) || (dir != INBOUND)) {
		*lenp = 0;
		return 0;
	}

	len = *lenp;

	if (len > INBUFLEN - 1)
		len = INBUFLEN - 1;

	if (access_ok(VERIFY_READ, buffer, len) == 0)
		return -EFAULT;

	if (COPY_FROM_USER(inbuf, buffer, len))
		return -EFAULT;

	inbuf[len] = 0;

	retval = parse_write_config(inbuf);

	if (retval > 0) {
		*lenp = retval;
		*ppos += retval;
	} else {
		*ppos += len;
	}
		
	return retval;
}

static int read_config(dgrp_proc_entry *table, int dir,
                        struct file *filp,      void *buffer,
                        READ_RETURN_T *lenp,    loff_t *ppos)
{
	static struct nd_struct *curr_ps = NULL;
	static int currline = 0;
	static char linebuf[81];
	static int linepos = 0;

	int len;
	int left;
	int notdone;

	if (*ppos == 0) {
		curr_ps = head_nd_struct;
		currline = 0;
		linepos = 0;
		linebuf[0] = 0;
	}

	if ((!*lenp) || (dir != OUTBOUND) || (!curr_ps)) {
		*lenp = 0;
		return 0;
	}

	/* Compose the next line if we are pointing to the beginning */
	if (!linepos) {
		char tmp_id[20];

		switch( currline ) {
		case 0:
		case 3:
			strcpy(linebuf, "#---------------------------------"
			                 "----------------------------------"
			                 "----------\n");
			break;
		case 1:
			strcpy(linebuf, "#                        Avail\n");
			break;
		case 2:
			strcpy(linebuf, "# ID  Major  State       Ports\n");
			break;
		default:
			ID_TO_CHAR(curr_ps->nd_ID, tmp_id);
/*
                                          "#                        Avail\n"
                                          "# ID  Major  State       Ports\n"
                                          "  xx  99999  xxxxxxxxxx  99999\n"
*/
			sprintf(linebuf, "  %-2.2s  %-5ld  %-10.10s  %-5d\n",
			         tmp_id, curr_ps->nd_major,
			         ND_STATE_STR(curr_ps->nd_state),
			         curr_ps->nd_chan_count
			       );
			curr_ps = curr_ps->nd_inext;
		}
	}

	notdone = 0;
	left = *lenp;
	len = strlen(&linebuf[linepos]);

	if (len > left) {
		len = left;
		notdone = 1;
	}
	
	if (access_ok(VERIFY_WRITE, buffer, len) == 0)
		return -EFAULT;
	if (COPY_TO_USER(buffer, &linebuf[linepos], len))
		return -EFAULT;

	if (notdone)
		linepos += len;
	else {
		linepos = 0;
		currline++;
	}

	*lenp = len;
	*ppos += len;
	
	return 0;
}


/*
 *  ------------------------------------------------------------------------
 *
 *  The following are the functions to parse input 
 *
 *  ------------------------------------------------------------------------
 */
#define SKIP_WS		\
	while (*c && !isspace(*c)) c++; \
	while (*c && isspace(*c)) c++

#define CHK(x)		{ retval=(x); if (retval < 0) return retval; }
#define CHK_PERIOD   	{ if (*c != '.') return -EINVAL; else c++; }

static int parse_id(char **c, char *cID)
{
	int tmp = **c;

	if (isalnum(tmp) || (tmp == '_'))
		cID[0] = tmp;
	else
		return -EINVAL;

	(*c)++; tmp = **c;

	if (isalnum(tmp) || (tmp == '_')) {
		cID[1] = tmp;
		(*c)++; 
	} else
		cID[1] = 0;

	return 0;
}

static int parse_long(char **c, long *val)
{
	long tmp = 0;

	if (!isdigit(**c))
		return -EINVAL;

	while (**c && isdigit(**c))
	{
		tmp = (10 * tmp) + (**c - '0');
		(*c)++;
	}

	*val = tmp;

	return 0;
}

static int parse_add_config(char *buf)
{
	char *c = buf;
	int  i, retval;
	struct nd_struct *new_nd;
	char cID[2];
	long ID;

	SKIP_WS;
	
	CHK(parse_id(&c, cID));
	ID=CHAR_TO_ID(cID);

	SKIP_WS;

	/*
	 *  Now create an nd_struct and link into the hash table.
	 */
	new_nd = FN(kzmalloc)(sizeof(struct nd_struct), GFP_KERNEL);
	if (!new_nd) {
		dbg_trace(("MALLOC FAILED: struct nd_struct (%d)\n", sizeof(struct nd_struct)));
		return -ENOMEM;
	}

	memset(new_nd, 0, sizeof(struct nd_struct));

	new_nd->nd_major = 0;
	new_nd->nd_ID = ID;

	SPINLOCK_INIT(new_nd->nd_lock);

	init_waitqueue_head(&(new_nd->nd_tx_waitq));
	init_waitqueue_head(&(new_nd->nd_mon_wqueue));
	init_waitqueue_head(&(new_nd->nd_dpa_wqueue));
	for (i = 0; i < SEQ_MAX; i++)
		init_waitqueue_head(&(new_nd->nd_seq_wque[i]));

	/* setup the structures to get the major number */
	retval = FN(tty_init)(new_nd);
	if (retval) {
		/* error! */
		dbg_trace(("ERROR from ttyinit: %d\n", retval));
		kfree(new_nd);
		return retval;
	}

	new_nd->nd_major = new_nd->nd_serial_ttdriver.major;

	dbg_comm_trace(INIT, ("TTY major: %d\n", new_nd->nd_serial_ttdriver.major));
	dbg_comm_trace(INIT, ("CU  major: %d\n", new_nd->nd_callout_ttdriver.major));
	dbg_comm_trace(INIT, ("XPR major: %d\n", new_nd->nd_xprint_ttdriver.major));

	retval = nd_struct_add( new_nd );
	if (retval) {
		kfree(new_nd);
		return retval;
	} 

	(void) register_net_device(new_nd, net_entry_pointer);
	(void) register_mon_device(new_nd, mon_entry_pointer);
	(void) register_dpa_device(new_nd, dpa_entry_pointer);
	(void) register_ports_device(new_nd, ports_entry_pointer);

	return (c - buf);
}

static int parse_del_config( char *buf )
{
	char *c = buf;
	int  retval;
	struct nd_struct *nd;
	char cID[2];
	long ID;
	long major;

	SKIP_WS;
	
	CHK(parse_id(&c, cID));
	ID=CHAR_TO_ID(cID);

	SKIP_WS;

	CHK(parse_long(&c, &major));

	SKIP_WS;


	nd = nd_struct_get(major);
	if (!nd)
		return -EINVAL;	

	if ((nd->nd_major != major) || (nd->nd_ID != ID))
		return -EINVAL;

	/* Check to see if the selected structure is in use */
	if (nd->nd_tty_ref_cnt)
		return -EBUSY;

	if (nd->nd_net_de)
		(void) unregister_net_device(nd, net_entry_pointer);

	if (nd->nd_mon_de)
		(void) unregister_mon_device(nd, mon_entry_pointer);

	if (nd->nd_ports_de)
		(void) unregister_ports_device(nd, ports_entry_pointer);

	if (nd->nd_dpa_de)
		(void) unregister_dpa_device(nd, ports_entry_pointer);

	FN(tty_uninit)(nd);

	retval = nd_struct_del(nd);
	if (retval)
		return retval;

	kfree(nd);

	return 0;
}

static int parse_chg_config(char *buf)
{
	return -EINVAL;
}

/*
 *  The passed character buffer represents a single configuration request.
 *  If the first character is a "+", it is parsed as a request to add a
 *     PortServer
 *  If the first character is a "-", it is parsed as a request to delete a
 *     PortServer
 *  If the first character is a "*", it is parsed as a request to change a
 *     PortServer
 *  Any other character (including whitespace) causes the record to be
 *     ignored.
 */
static int parse_write_config(char *buf)
{
	int retval;

	switch (buf[0]) {
	case '+':
		retval = parse_add_config(buf);
		break;
	case '-':
		retval = parse_del_config(buf);
		break;
	case '*':
		retval = parse_chg_config(buf);
		break;
	default:
		retval = -EINVAL;
	}

	return retval;
}


/*
 *  When writing to the "info" entry point, I actually allow one
 *  to modify certain variables.  This may be a sleazy overload
 *  of this /proc entry, but I don't want:
 *
 *     a. to clutter /proc more than I have to
 *     b. to overload the "config" entry, which would be somewhat
 *        more natural
 *     c. necessarily advertise the fact this ability exists
 *
 *  The continued support of this feature has not yet been
 *  guaranteed.
 *
 *  Writing operates on a "state machine" principle.
 *
 *  State 0: waiting for a symbol to start.  Waiting for anything
 *           which isn't " ' = or whitespace.
 *  State 1: reading a symbol.  If the character is a space, move
 *           to state 2.  If =, move to state 3.  If " or ', move
 *           to state 0.
 *  State 2: Waiting for =... suck whitespace.  If anything other
 *           than whitespace, drop to state 0.
 *  State 3: Got =.  Suck whitespace waiting for value to start.
 *           If " or ', go to state 4 (and remember which quote it
 *           was).  Otherwise, go to state 5.
 *  State 4: Reading value, within quotes.  Everything is added to
 *           value up until the matching quote.  When you hit the
 *           matching quote, try to set the variable, then state 0.
 *  State 5: Reading value, outside quotes.  Everything not " ' =
 *           or whitespace goes in value.  Hitting one of the
 *           terminators tosses us back to state 0 after trying to
 *           set the variable.
 */
typedef enum {
	NONE, INFO_INT, INFO_CHAR, INFO_SHORT,
	INFO_LONG, INFO_PTR, INFO_STRING, END
} info_proc_var_val;

static struct {
	char              *name;
	info_proc_var_val  type;
	int                rw;       /* 0=readonly */
	void              *val_ptr;
} info_vars[] = {
	{ "version",     INFO_STRING, 0, (void *)DIGI_VERSION },
#if defined(REGISTER_TTYS_WITH_SYSFS)
	{ "register_with_sysfs", INFO_STRING, 0, (void *) "1" },
#else
	{ "register_with_sysfs", INFO_STRING, 0, (void *) "0" },
#endif
	{ NULL, NONE, 0, NULL },
	{ "rawreadok",   INFO_INT,    1, (void *)&(GLBL(rawreadok)) },
	{ "pollrate",    INFO_INT,    1, (void *)&(GLBL(poll_tick)) },
#if ALLOW_ALT_FAIL_OPEN
	{ "alt_fail_open",INFO_INT,   1, (void *)&(GLBL(alt_fail_open)) },
#endif

	{ NULL, NONE, 0, NULL },
	{ "mon_debug",   INFO_LONG,   1, (void *)&(GLBL(mon_debug)) },
	{ "net_debug",   INFO_LONG,   1, (void *)&(GLBL(net_debug)) },
	{ "tty_debug",   INFO_LONG,   1, (void *)&(GLBL(tty_debug)) },
	{ "comm_debug",  INFO_LONG,   1, (void *)&(GLBL(comm_debug)) },
	{ "ports_debug", INFO_LONG,   1, (void *)&(GLBL(ports_debug)) },
	{ NULL, END, 0, NULL }
};

static void set_info_var(char *name, char *val)
{
	int i;
	unsigned long newval;
	unsigned char charval;
	unsigned short shortval;
	unsigned int intval;

	for (i = 0; info_vars[i].type != END; i++) {
		if(info_vars[i].name)
			if(!strcmp(name, info_vars[i].name))
				break;
	}

	if (info_vars[i].type == END)
		return;
	if (info_vars[i].rw == 0)
		return;
	if (info_vars[i].val_ptr == NULL)
		return;

	newval = simple_strtoul(val, NULL, 0 ); 

	switch (info_vars[i].type) {
	case INFO_CHAR:
		charval = newval & 0xff;
		dbg_trace(("Modifying %s (%lx) <= 0x%02x  (%d)\n",
		           name, (long)(info_vars[i].val_ptr ),
		           charval, charval));
		*(uchar *)(info_vars[i].val_ptr) = charval;
		break;
	case INFO_SHORT:
		shortval = newval & 0xffff;
		dbg_trace(("Modifying %s (%lx) <= 0x%04x  (%d)\n",
		           name, (long)(info_vars[i].val_ptr),
		           shortval, shortval));
		*(ushort *)(info_vars[i].val_ptr) = shortval;
		break;
	case INFO_INT:
		intval = newval & 0xffffffff;
		dbg_trace(("Modifying %s (%lx) <= 0x%08x  (%d)\n",
		           name, (long)(info_vars[i].val_ptr),
		           intval, intval));
		*(uint *)(info_vars[i].val_ptr) = intval;
		break;
	case INFO_LONG:
		dbg_trace(("Modifying %s (%lx) <= 0x%lx  (%ld)\n",
		           name, (long)(info_vars[i].val_ptr),
		           newval, newval));
		*(ulong *)(info_vars[i].val_ptr) = newval;
		break;
	case INFO_PTR:
	case INFO_STRING:
	case END:
	case NONE:
	default:
		break;
	}
}

static int write_info(dgrp_proc_entry *table, int dir,
                       struct file *filp,      void *buffer,
                       READ_RETURN_T *lenp,    loff_t *ppos)
{
	static int state = 0;
	#define MAXSYM 255
	static int sympos, valpos;
	static char sym[MAXSYM + 1];
	static char val[MAXSYM + 1];
	static int quotchar = 0;

	int i;

	long len;
	#define INBUFLEN 256
	char inbuf[INBUFLEN];

	if (*ppos == 0) {
		state = 0;
		sympos = 0; sym[0] = 0;
		valpos = 0; val[0] = 0;
		quotchar = 0;
	}

	if ((!*lenp) || (dir != INBOUND)) {
		*lenp = 0;
		return 0;
	}

	len = *lenp;

	if (len > INBUFLEN - 1)
		len = INBUFLEN - 1;

	if (access_ok(VERIFY_READ, buffer, len) == 0)
		return -EFAULT;
	if (COPY_FROM_USER(inbuf, buffer, len))
		return -EFAULT;

	inbuf[len] = 0;

	for (i = 0; i < len; i++) {
		unsigned char c = inbuf[i];

		switch (state) {
		case 0:
			quotchar = sympos = valpos = sym[0] = val[0] = 0;
			if (!isspace(c) && (c != '\"') &&
			    (c != '\'') && (c != '=')) {
				sym[sympos++] = c;
				state = 1;
				break;
			}
			break;
		case 1:
			if (isspace(c)) {
				sym[sympos] = 0;
				state = 2;
				break;
			}
			if (c == '=') {
				sym[sympos] = 0;
				state = 3;
				break;
			}
			if ((c == '\"' ) || ( c == '\'' )) {
				state = 0;
				break;
			}
			if (sympos < MAXSYM) sym[sympos++] = c;
			break;
		case 2:
			if (isspace(c)) break;
			if (c == '=') {
				state = 3;
				break;
			}
			if ((c != '\"') && (c != '\'')) {
				quotchar = sympos = valpos = sym[0] = val[0] = 0;
				sym[sympos++] = c;
				state = 1;
				break;
			}
			state = 0;
			break;
		case 3:
			if (isspace(c)) break;
			if (c == '=') {
				state = 0;
				break;
			}
			if ((c == '\"') || (c == '\'')) {
				state = 4;
				quotchar = c;
				break;
			}
			val[valpos++] = c;
			state = 5;
			break;
		case 4:
			if (c == quotchar) {
				val[valpos] = 0;
				set_info_var(sym, val);
				state = 0;
				break;
			}
			if (valpos < MAXSYM) val[valpos++] = c;
			break;
		case 5:
			if (isspace(c) || (c == '\"') ||
			    (c == '\'') || (c == '=')) {
				val[valpos] = 0;
				set_info_var(sym, val);
				state = 0;
				break;
			}
			if (valpos < MAXSYM) val[valpos++] = c;
			break;
		default:
			break;
		}
	}

	*lenp = len;
	*ppos += len;
		
	return len;
}


/*
 *  Return what is (hopefully) useful information about the
 *  driver.  For now, the returned format is built line by
 *  line, like other /proc entry points implemented in this
 *  driver.  I _could_ have used the simple read, but I
 *  did not.
 */

static int read_info(dgrp_proc_entry *table, int dir,
                      struct file *filp,      void *buffer,
                      READ_RETURN_T *lenp,    loff_t *ppos)
{
	static int currline = 0;
	static char linebuf[81];
	static int linepos = 0;

	static int maxnamelen = -1;

	int len;
	int left;
	int notdone;

	if (maxnamelen == -1) {
		int i;

		maxnamelen = 0;
		for (i = 0; info_vars[i].type != END; i++) {
			if (info_vars[i].name &&
			    (strlen(info_vars[i].name) > maxnamelen))
				maxnamelen = strlen(info_vars[i].name);
		}
	}

	if (*ppos == 0) {
		currline = 0;
		linepos = 0;
		linebuf[0] = 0;
	}

	if ((!*lenp) || (dir != OUTBOUND) ||
	    (info_vars[currline].type == END)) {
		*lenp = 0;
		return 0;
	}

	/* Compose the next line if we are pointing to the beginning */
	if (!linepos) {
		int padlen;

		if (info_vars[currline].name)
			padlen = maxnamelen - strlen( info_vars[currline].name ) + 2;
		else
			padlen = maxnamelen + 2;

		switch (info_vars[currline].type) {
		case INFO_CHAR:
			sprintf(linebuf, "%s:%*s0x%02x\t(%d)\n",
			         info_vars[currline].name,
						padlen, "",
			         *(char *)(info_vars[currline].val_ptr),
			         *(char *)(info_vars[currline].val_ptr)
			       );
			break;
		case INFO_INT:
			sprintf(linebuf, "%s:%*s0x%08x\t(%d)\n",
			         info_vars[currline].name,
						padlen, "",
			         *(int *)(info_vars[currline].val_ptr),
			         *(int *)(info_vars[currline].val_ptr)
			       );
			break;
		case INFO_SHORT:
			sprintf(linebuf, "%s:%*s0x%04x\t(%d)\n",
			         info_vars[currline].name,
						padlen, "",
			         *(short *)(info_vars[currline].val_ptr),
			         *(short *)(info_vars[currline].val_ptr)
			       );
			break;
		case INFO_LONG:
			sprintf(linebuf, "%s:%*s0x%0*lx\t(%ld)\n",
			         info_vars[currline].name,
						padlen, "",
			         (int)sizeof(long) * 2,
			         *(long *)(info_vars[currline].val_ptr),
			         *(long *)(info_vars[currline].val_ptr)
			       );
			break;
		case INFO_PTR:
			sprintf(linebuf, "%s:%*s0x%0*lx\n",
			         info_vars[currline].name,
						padlen, "",
			         (int)sizeof(ulong) * 2,
			         *(ulong *)(info_vars[currline].val_ptr)
			       );
			break;
		case INFO_STRING:
			sprintf(linebuf, "%s:%*s%s\n",
			         info_vars[currline].name,
						padlen, "",
			         (char *)info_vars[currline].val_ptr );
			break;
		default:
			strcpy(linebuf, "\n");
		}
	}

	notdone = 0;
	left = *lenp;
	len = strlen(&linebuf[linepos]);

	if (len > left) {
		len = left;
		notdone = 1;
	}
	
	if (access_ok(VERIFY_WRITE, buffer, len) == 0)
		return -EFAULT;
	if (COPY_TO_USER(buffer, &linebuf[linepos], len))
		return -EFAULT;

	if (notdone)
		linepos += len;
	else {
		linepos = 0;
		currline++;
	}

	*lenp = len;
	*ppos += len;
	
	return 0;
}


/*
 *  Return detailed information about the nodes, including
 *  versions and descriptions.
 */

static int read_nodeinfo(dgrp_proc_entry *table, int dir,
                          struct file *filp,      void *buffer,
                          READ_RETURN_T *lenp,    loff_t *ppos)
{
	static struct nd_struct *curr_ps = NULL;
	static int currline = 0;
	static char linebuf[82];	/* 80 chars + '\n' + '\0' */
	static int linepos = 0;

	char hwver[8];
	char swver[8];

	int len;
	int left;
	int notdone;

	if (*ppos == 0) {
		curr_ps = head_nd_struct;
		currline = 0;
		linepos = 0;
		linebuf[0] = 0;
	}

	if ((!*lenp) || (dir != OUTBOUND) || (!curr_ps)) {
		*lenp = 0;
		return 0;
	}

	/* Compose the next line if we are pointing to the beginning */
	if (!linepos ) {
		char tmp_id[20];

		switch(currline) {
		case 0:
		case 3:
			strcpy(linebuf, "#---------------------------------"
			                 "----------------------------------"
					 "------------\n");
			break;
		case 1:
			strcpy(linebuf, "#                 HW       HW   SW"
					"                                     OpenCount\n");
			break;
		case 2:
			strcpy(linebuf, "# ID  State       Version  ID   Version  "
					"Description                          ||\n");
			break;
		default:
			ID_TO_CHAR(curr_ps->nd_ID, tmp_id);

			if (curr_ps->nd_state == NS_READY)
			{
				sprintf(hwver, "%d.%d", ( curr_ps->nd_hw_ver >> 8 ) & 0xff,
				         curr_ps->nd_hw_ver & 0xff);
				sprintf(swver, "%d.%d", ( curr_ps->nd_sw_ver >> 8 ) & 0xff,
				         curr_ps->nd_sw_ver & 0xff);

				sprintf(linebuf,
					"  %-2.2s  %-10.10s  %-7.7s  %-3d  %-7.7s  %-35.35s  %02d\n",
					tmp_id,
					ND_STATE_STR(curr_ps->nd_state),
					hwver,
					curr_ps->nd_hw_id <= 999? curr_ps->nd_hw_id : 999,
					swver, curr_ps->nd_ps_desc,
					curr_ps->nd_open_count <= 99? curr_ps->nd_open_count : 99);
			}
			else
			{
				sprintf(linebuf, "  %-2.2s  %-10.10s  %-60.60s%02d\n",
					tmp_id,
					ND_STATE_STR(curr_ps->nd_state),
					" ",
					curr_ps->nd_open_count <= 99? curr_ps->nd_open_count : 99
					);
			}
			curr_ps = curr_ps->nd_inext;
		}
	}

	notdone = 0;
	left = *lenp;
	len = strlen(&linebuf[linepos]);

	if (len > left) {
		len = left;
		notdone = 1;
	}
	
	if (access_ok(VERIFY_WRITE, buffer, len) == 0)
		return -EFAULT;
	if (COPY_TO_USER(buffer, &linebuf[linepos], len))
		return -EFAULT;

	if (notdone)
		linepos += len;
	else {
		linepos = 0;
		currline++;
	}

	*lenp = len;
	*ppos += len;
	
	return 0;
}
