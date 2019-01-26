
#============================================================================== 
#
# FILE:  dgrp_msgs.tcl
#
# Define all text messages, labels, and constants in one place
#
#
# 	dgrpVals = miscellaneous values
# 	dgrpFVals = constants for file names and argument lists
# 	dgrpMsg = text messages and labels
#
#============================================================================== 

global dgrpVals
global dgrpFVals
global dgrpMsg
global node
global ports

#-- dgrpVals -----------------------------------------------------------

# set gui_debug to 0 or 1
set dgrpVals(gui_debug)	0

# We will use the master_node_list to have JUST those portservers
# that are described in the /proc/dgrp/config and /etc/dgrp.backing.store
# files.  This is to get around the problem with unsetting array elements
# when the PortServer is deleted.
# The master_node_list should be put in alphabetical order after every insert.
set dgrpVals(master_node_list) { }
set dgrpVals(current_node_id) ""
set dgrpVals(node_arraynames) [list id ports ip status daemon major mode \
	linkspeed index ipport owner group encrypt encrypt_ipport]
set dgrpVals(port_arraynames) [list id tty_open_cnt pr_open_cnt \
	tot_wait_cnt mstat iflag oflag cflag digiflags description status]
set dgrpVals(refresh_update_delay) 2
set dgrpVals(status_toplevel_dying) 0
set dgrpVals(warned_about_closed) 0

#-- dgrpFVals -----------------------------------------------------------

set dgrpFVals(proc_config)	/proc/dgrp/config
set dgrpFVals(proc_ports_dir)	/proc/dgrp/ports
set dgrpFVals(proc_info)	/proc/dgrp/info

set dgrpFVals(dgrp_store)	/etc/dgrp.backing.store
set dgrpFVals(dgrp_root)	/usr/bin/dgrp/config
set dgrpFVals(tmp_file)		/tmp/dgrp_gui.tmp
set dgrpFVals(dgrp_cfg_node)	$dgrpFVals(dgrp_root)/dgrp_cfg_node 
set dgrpFVals(dgrp_cfg_node_options) " -v -v"
set dgrpFVals(logo)	[image create photo logo_icon \
	-file $dgrpFVals(dgrp_root)/dgrp.gif ]

# set dgrpFVals(tty_prefix) 	/dev/tty
# set dgrpFVals(cu_prefix)	/dev/cu
# set dgrpFVals(pr_prefix)	/dev/pr

#-- node( ,default)------------------------------------------------------

set node(id,default) 		{}
set node(ports,default)		16
set node(ip,default)		{}
set node(status,default) 	{}
set node(daemon,default) 	{}
set node(major,default) 	{}
set node(mode,default) 		0666
set node(linkspeed,default) 	auto
set node(index,default) 	-99
set node(ipport,default) 	771
set node(encrypt,default) 	never
set node(encrypt_ipport,default)	1027
set node(owner,default) 	0
set node(group,default)		0

#-- port( ,default)------------------------------------------------------
	
set port(type,default)		NA
set port(id,default)		NA
set port(tty_open_cnt,default)	NA
set port(pr_open_cnt,default)	NA
set port(tot_wait_cnt,default)	NA
set port(mstat,default)		NA
set port(iflag,default)		NA
set port(oflag,default)		NA
set port(cflag,default)		NA
set port(bps,default)		NA
set port(digiflags,default)	NA
set port(description,default)	NA
set port(status,default)	NA

#-- dgrpMsg ------------------------------------------------------------

# titles, strings

# the gui version numbers are the version numbers of the package
# it was distributed with
set dgrpMsg(gui_ver)	 	"1.9"
set dgrpMsg(gui_rev) 		"36"
set dgrpMsg(gui_date)	 	"02/08/2016"

set dgrpMsg(driver_ver)		""
set dgrpMsg(driver_ver_err)	"(Could Not Determine Driver Module Version)"
set dgrpMsg(driver_ver_ok)	"Loaded Driver Module Version: %1s"
set dgrpMsg(driver_ver_dne)	"(No Driver Module Loaded)"

set dgrpMsg(main_title) 	"Digi RealPort Manager"
set dgrpMsg(main_toptext)	{Configured Devices on %1$s}
set dgrpMsg(port_title)		"Digi RealPort Manager -- Ports"
set dgrpMsg(port_toptext)	{Configured Ports on RealPort Device %1$s}
set dgrpMsg(config_title) 	"Digi RealPort Device Settings"
set dgrpMsg(config_toptext)	{Settings for RealPort Device %1$s}
set dgrpMsg(linkspeed_title)	"Set Custom Link Speed"
set dgrpMsg(logger_title)	"Digi RealPort Command Logger"
set dgrpMsg(port_status_title)	"Port Status"
set dgrpMsg(port_status_toptext) "Port Status"

set dgrpMsg(ps_headings) \
	"id  Ports Major      IP Address  Status            Link Speed       Encrypt"

set dgrpMsg(port_headings) \
	"Port   Status   Speed  Description"

#============================================================================== 

# labels 

set dgrpMsg(id_label)		"RealPort ID:"
set dgrpMsg(ip_label)		"IP Address or Name:"
set dgrpMsg(ports_label)	"Number of Ports:"
set dgrpMsg(speed_label)	"Link Speed:"
set dgrpMsg(major_label)	"Major Number:"
set dgrpMsg(ipport_label)	"IP Port:"
set dgrpMsg(mode_label)		"Access Mode:"
set dgrpMsg(owner_label)	"Owner:"
set dgrpMsg(group_label)	"Group:"
set dgrpMsg(encrypt_label)	"Encrypt Session:"
set dgrpMsg(encrypt_ipport_label) "Encrypt IP Port:"

set dgrpMsg(fast_rate_label)	"Fast Rate:"
set dgrpMsg(fast_delay_label)	"Fast Delay:"
set dgrpMsg(slow_rate_label)	"Slow Rate:"
set dgrpMsg(slow_delay_label)	"Slow Delay:"
set dgrpMsg(header_size_label)	"Header Size:"

#============================================================================== 

# menu options

set dgrpMsg(config_mo)	"Configure / Initialize"
set dgrpMsg(add_mo)	"Add New"
set dgrpMsg(delete_mo)	"Delete / Uninitialize"
set dgrpMsg(about_mo)	"About RealPort Manager"

set dgrpMsg(tty_mo)	"Show tty's"
set dgrpMsg(cu_mo)	"Show cu's"
set dgrpMsg(pr_mo)	"Show pr's"

set dgrpMsg(daemon_stat_mo)	"Check daemon status"
set dgrpMsg(daemon_stop_mo)	"Stop Daemon"
set dgrpMsg(daemon_start_mo)	"Start Daemon"
set dgrpMsg(daemon_all_mo)	"View all running daemons"

#============================================================================== 

# file errors

set dgrpMsg(file_dne_error) { 
The file "%1$s" does not exist!
} 

set dgrpMsg(file_cannot_create_error) { 
Error creating file "%1$s"
}

set dgrpMsg(file_cannot_open_error) { 
Cannot open file file "%1$s"
}

set dgrpMsg(dir_dne_error) {The directory "%1$s" does not exist! \
Please make sure that the
RealPort package is installed on the system, and the path variables 
declared in dgrp_gui are correct. 
} 

set dgrpMsg(cannot_cd_error) { 
You do not have permission to cd to "%1$s"
}

set dgrpMsg(no_execute_perm_error) { 
You do not have execute permission for the program "%1$s"
}

#============================================================================== 

# error messages

set dgrpMsg(no_ps_selected) {No selection has been made! \
Please choose a unit from the list. }

set dgrpMsg(no_port_selected) {No selection has been made! \
Please choose a port from the list. }

set dgrpMsg(linkspeed_over) {The value for "%1$s" is out of bounds. \
Must be less than or equal to %2$s.  See Help -> Valid Ranges for details. }

set dgrpMsg(linkspeed_under) {The value for "%1$s" is out of bounds.\
Must be greater than or equal to %2$s.  See Help -> Valid Ranges for details. }

set dgrpMsg(invalid_IP) {The IP address "%1$s" is not valid.}

set dgrpMsg(invalid_id) {The RealPort id "%1$s" is not valid.}

set dgrpMsg(id_in_use) {The RealPort id "%1$s" is in use already.}

set dgrpMsg(invalid_nports) {The number of ports must be between 0 and 64.}

set dgrpMsg(invalid_mode) {The file mode "%1$s" is not valid.  Expecting one to four octal digits (0-7).}

set dgrpMsg(invalid_owner) {The owner id "%1$s" is not valid.  Expecting positive integer.}

set dgrpMsg(invalid_group) {The group id "%1$s" is not valid.  Expecting positive integer.}

#============================================================================== 

set dgrpMsg(linkspeed_help_label)	"About Link Speed" 
set dgrpMsg(linkspeed_help) {

Link Speed

This is an optional parameter useful on slow speed WAN links.\
It is a formatted string containing  1  to  5 decimal numbers,\
separated by commas or periods.  The numbers are:

   (a) Maximum link speed in bits/second
   (b) Highest delay for maximum speed in milliseconds
   (c) Minimum link speed in bits/second
   (d) Lowest delay for minimum speed in milliseconds
   (e) Estimated packet header size

This parameter allows  the  user  to  configure  a  balance\
between  latency  (eg keystroke  delay)  and  throughput.\
When the instantaneous delay between the UNIX system and the\
RealPort client is below (b), the UNIX system transmits data at\
up to  rate (a).  However when the delay exceeds (d), data is send\
only at speed (c).  When the delay is between (b) and (d),  data\
is  sent  at  a  proportional  value between (a) and (c).

In  each  of  these  computations  we  assume a packet header size\
of (e) for each packet sent across the link.  A value of 46 bytes\
is  assumed  by  default;  this value  is  appropriate  for\
Encapsulated Ethernet over Frame Relay.  A value of 8 should be\
used for PPP links with Van Jacobsen header compression.

Most WAN users need specify only parameter (a) and let parameters\
(b) (c) (d)  (e) take default values.  For example most 56K Frame\
Relay links with a combination of terminals and printers can use\
a simple speed string of just 56000.  }

#============================================================================== 

set dgrpMsg(linkspeed_ranges_label)	"Valid Ranges"
set dgrpMsg(linkspeed_ranges) {

Link Speed Valid Ranges

These are the valid ranges:

FAST RATE:  greater than or equal to the maximum of 2400 bps and the\
value specified by slow rate, and less than 10,000,000.

SLOW RATE:  greater than or equal to 600 bps, and less than the\
value specified by fast rate. (The fast rate value must be given.)

	600 <= max(slow rate, 2400) <= fast rate < 10 000 000

FAST DELAY: greater than or equal to 0 bps,  and less than the minimum\
of 2400 bps and the value specified by slow delay.

SLOW DELAY: greater than or equal to the fast delay,\
and less than or equal to 10,000 bps. (The fast delay must be given.)

	0 <= fast delay <= min(slow delay, 2400) <= 10 000

HEADER SIZE:  greater than or equal to 2 bytes, and less than or equal\
to 128 bytes.

	2 <= header size <= 128

}

#============================================================================== 
#1234567890123456789012345678901234567890123456789012345678901234567890123456789

set dgrpMsg(about_info) {

Digi RealPort Configuration Manager
	Version: %1$s,  Date: %2$s

Digi RealPort Driver
	%3$s 

This program allows you to easily install and configure new RealPort-enabled \
products, or \
update the configuration for those already installed. \
For detailed help, consult the man page dgrp_gui(8). 

COPYRIGHT INFORMATION

Copyright 2000 Digi International (www.digi.com)

This program is free software; you can redistribute it and/or modify it under \
the terms of the GNU General Public License as published by the Free Software \
Foundation; either version 2, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY \
WARRANTY, EXPRESS OR IMPLIED; without even the implied warranty of MERCHANTABILITY \
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License \
for more details. 

You should have received a copy of the GNU General Public License along with \
this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, \
Cambridge, MA 02139, USA. 

AUTHORS
	Ann-Marie Westgate
	James Puzzo  <jamesp@digi.com>
	Scott H Kilau <scottk at digi dot com>

}

#============================================================================== 

set dgrpMsg(port_closed_info) {

This port is currently closed and there no processes waiting to\
open it.  The modem signals are stale and will therefore show all\
zeros until an open is attempted.

This message will not be shown again.
}


