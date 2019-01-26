
#================================================================================
#
# FILE:  dgrp_nodes.tcl
#
# This file contains all the code to create and update the window with
# all the PortServer nodes displayed.  This is a list of all the procedures
# contained in this file:
#
#  PS_Config_Window { f default args } 
#  create_display_all_portservers_menubar { w } 
#  create_display_all_portservers { w } 
#  dismiss_display_all_portservers { w } 
#  check_linkspeed { } 
#  set_linkspeed { } 
#  setup_ps_settings { ID } 
#  is_integer { string } 
#  isvalid_ps_ipaddress { IP } 
#  isvalid_ps_id { ID } 
#  isvalid_ps_nports { nports } 
#  isvalid_ps_mode { mode } 
#  setup_new_ps_settings { ID } 
#  save_ps_settings { ID } 
#  refresh_node_listbox { {old_selection 0} } 
#  delete_node_listbox { } 
#  fill_node_listbox { } 
#  setup_node_arrays { } 
#  delete_node_arrays { } 
#  commit_portserver_config { ID } 
#  config_portserver { } 
#  add_portserver { } 
#  remove_portserver { } 
#  start_daemon { } 
#  stop_daemon { } 
#  check_daemon { } 
#  show_device_nodes { prefix } 
#
#================================================================================

#--------------------------------------------------------------------------------
#
# PS_Config_Window
#
# Description:
#
# 	this is the port server configuration window consisting of a 
# 	listbox, title, bottom logo and buttons
# 	the args field determines the button text
#
proc PS_Config_Window { f default args } {
	global this_node 
	global dgrpMsg

	set text_fields {id ip ports major ipport mode owner group} 
	
	# Create the top frame
	set w [toplevel $f]
	wm title $w $dgrpMsg(config_title)

	frame $w.top 
	label $w.top.toptext -text [format $dgrpMsg(config_toptext) $this_node(id)]
	pack $w.top.toptext -side left

	# Create the middle frame
	frame $w.mid 

	# text entry fields
	foreach field $text_fields {
		entry $w.mid.e$field -textvariable this_node($field)\
		   -relief sunken 
	}
	set linkspeed [Radio_Pulldown_Menu $w.mid.elinkspeed this_node(linkspeed) \
		 auto 38400 57600 115200 ]

	set encrypt [Radio_Pulldown_Menu $w.mid.eencrypt this_node(encrypt)  never always ]

	# Add the text field for the encrypt ip port
	entry $w.mid.eencrypt_ipport -textvariable this_node(encrypt_ipport)\
	   -relief sunken 

	# add the custom option
	$linkspeed.mbutton.menu add separator
	$linkspeed.mbutton.menu add checkbutton -label "Custom..." \
	  -variable custom_linkspeed -command set_linkspeed 
	
	# labels
	label $w.mid.lid 	-text $dgrpMsg(id_label) -anchor w
	label $w.mid.lip	-text $dgrpMsg(ip_label) -anchor w
	label $w.mid.lports 	-text $dgrpMsg(ports_label) -anchor w
	label $w.mid.llinkspeed -text $dgrpMsg(speed_label) -anchor w
	label $w.mid.lmajor 	-text $dgrpMsg(major_label) -anchor w
	label $w.mid.lipport 	-text $dgrpMsg(ipport_label) -anchor w
	label $w.mid.lmode 	-text $dgrpMsg(mode_label) -anchor w
	label $w.mid.lowner 	-text $dgrpMsg(owner_label) -anchor w
	label $w.mid.lgroup 	-text $dgrpMsg(group_label) -anchor w
	label $w.mid.lencrypt 	-text $dgrpMsg(encrypt_label) -anchor w
	label $w.mid.lencrypt_ipport 	-text $dgrpMsg(encrypt_ipport_label) -anchor w

	foreach field $text_fields {
		$w.mid.e$field 	delete 0 end
		grid $w.mid.l$field $w.mid.e$field -sticky news
	}

	# Display the encrypt option.
	grid $w.mid.lencrypt $w.mid.eencrypt -sticky news

	# Now display the encrypt port number option.
	$w.mid.eencrypt_ipport delete 0 end
	grid $w.mid.lencrypt_ipport $w.mid.eencrypt_ipport -sticky news

	grid $w.mid.llinkspeed $w.mid.elinkspeed -sticky news

	# Create the bottom frame
	frame $w.bottom -borderwidth 10

	# Create the command buttons
	set i 0
	foreach but $args {
		button $w.bottom.button$i -text $but -command \
			"set button $i"
		bind $w.bottom.button$i <Return> "set button $i"
		if {$i == $default} {
			frame $w.bottom.default -relief sunken -bd 1
			raise $w.bottom.button $i
			pack $w.bottom.default -side left -expand 1\
			    -padx 3m -pady 2m
			pack $w.bottom.button$i -in $w.bot.default \
			    -side left -padx 2m -pady 2m \
			    -ipadx 2m -ipady 1m	
		} else {
			pack $w.bottom.button$i -side left -expand 1\
			    -padx 3m -pady 3m -ipadx 2m -ipady 1m
		}
		incr i
	}

	# Now pack it all in together
	pack $w.top -side top -expand 1 -fill x
	pack $w.mid 
	pack $w.bottom -side bottom -fill x
	return $w
}

#----------------------------------------------------------------------
#
proc create_display_all_portservers_menubar { w } {
	global dgrpFVals 
	global dgrpMsg

	# Create the menu bar
	menu $w.menubar
	$w config -menu $w.menubar

	# Create menu bar entries
	foreach m {RealPort Daemon View} {
		set $m [menu $w.menubar.m$m]
		$w.menubar add cascade -label $m -menu $w.menubar.m$m
	}

	# 
	# these are the pull down menu options for "RealPort"
	#
	$RealPort add command -label "Refresh List" \
		-command { 
			refresh_node_listbox
		}
	$RealPort add separator
	$RealPort add command -label $dgrpMsg(config_mo) \
		-command config_portserver
	$RealPort add command -label $dgrpMsg(add_mo) \
		-command add_portserver
	$RealPort add command -label $dgrpMsg(delete_mo) \
		-command remove_portserver
	$RealPort add separator
	$RealPort add command -label $dgrpMsg(about_mo) \
		-command { get_driver_ver; \
			info_dialog $dgrpMsg(about_info) $dgrpMsg(gui_ver) \
				$dgrpMsg(gui_date) $dgrpMsg(driver_ver) }
	$RealPort add separator
	$RealPort add command -label "Quit" -command exit

	# 
	# these are the pull down menu options for "View"
	#
	$View add command -label Ports \
		-command view_configured_ports
	$View add separator
	$View add command -label $dgrpMsg(tty_mo) -command { show_device_nodes tty }
	$View add command -label $dgrpMsg(cu_mo) -command { show_device_nodes cu }

	#
	# these are all the command for the "Daemon"
	#
	$Daemon add command -label $dgrpMsg(daemon_stat_mo) -command check_daemon
	$Daemon add command -label $dgrpMsg(daemon_stop_mo) -command stop_daemon
	$Daemon add command -label $dgrpMsg(daemon_start_mo) -command start_daemon
	$Daemon add separator
	$Daemon add command -label $dgrpMsg(daemon_all_mo) \
		-command {
			set command "ps ax | grep drpd | grep -v grep"
			Run $command
		}

	return $w.menubar
}

#----------------------------------------------------------------------
# 
proc create_display_all_portservers { w } {
	global env
	global ps_list
	global node
	global dgrpVals
	global dgrpFVals
	global dgrpMsg

	toplevel $w
	wm title $w $dgrpMsg(main_title)

	#
	# set the value of some initial variables
	#

	# custom_linkspeed is true until set otherwise in set_linkspeed
	set custom_linkspeed 1  
	set dgrpVals(dying) 0

        # Bind the X-event "Destroy" to manage the dying flag as well
        bind $w <Destroy> {
                if {!$dgrpVals(dying)} {
                        dismiss_display_all_portservers $w
                        exit 0
                }
        }

	# set up the top menu bar
	create_display_all_portservers_menubar $w

	# Create the top frame
	frame $w.top 

	if {[info exists env(HOSTNAME)]} {
		set HOSTNAME $env(HOSTNAME)
	} else {
		set HOSTNAME "localhost"
	}

	label $w.top.toptext -text [format $dgrpMsg(main_toptext) $HOSTNAME] 
	pack $w.top.toptext -side left

	#
        # Add the refresh button to update the display
        #
	button $w.top.bRefresh -text Refresh 
        pack $w.top.bRefresh -padx 5 -pady 5 -side right

	# Create the middle frame
	frame $w.mid -borderwidth 10

	# Create the node list box, read in the portserver settings,
	# then add them all to the listbox
	set ps_list [Scrolled_Listbox $w.mid.node_listbox -width 80 -height 10]
	setup_node_arrays
	fill_node_listbox

	$ps_list selection clear 0 end
	$ps_list selection set 0
	$w.top.bRefresh configure -command refresh_node_listbox

	# add the headings for the listbox
	frame $w.mid.toptext
	label $w.mid.toptext.headings -text $dgrpMsg(ps_headings) -font {system 12}
	pack $w.mid.toptext.headings -side left
	pack $w.mid.toptext $w.mid.node_listbox -side top -fill x  -expand true

	# Create the bottom frame
	frame $w.bottom -borderwidth 10 

	label $w.bottom.ldigi_gif -image $dgrpFVals(logo)
	button $w.bottom.bConfigure -text Configure -command config_portserver
	button $w.bottom.bAdd -text Add -command add_portserver
	button $w.bottom.bDelete -text Delete -command remove_portserver 
	button $w.bottom.bQuit -text Quit -command exit

	bind $w.bottom.bConfigure <Return> { config_portserver }
	bind $w.bottom.bAdd	<Return> { add_portserver }
	bind $w.bottom.bDelete	<Return> { remove_portserver }
	bind $w.bottom.bQuit	<Return> exit


	pack $w.bottom.ldigi_gif -side left
	pack $w.bottom.bConfigure $w.bottom.bAdd \
		 $w.bottom.bDelete $w.bottom.bQuit \
		 -side left -padx 5 -expand 1
	
	# Now pack it all in together

	pack $w.top -side top -expand 1 -fill x
	pack $w.mid
	pack $w.bottom -side bottom -expand 1 -fill x

	# bind the default action on double clicking a line
	bind $ps_list <Double-1> { config_portserver }
	bind $ps_list <Return>	 { config_portserver }

	dbg_gui "create_display_all_portservers: nboards = $dgrpVals(nboards)"

	return $w

}

#----------------------------------------------------------------------
proc dismiss_display_all_portservers { w } {
	global dgrpVals

	set dgrpVals(dying) 1
	destroy_window $w
}

#----------------------------------------------------------------------
#
# check_linkspeed
#
# Description:
# 
# 	check to make sure that the linkspeed associalted with ID
# 	is within the valid ranges.  The linkspeed is obtained from
# 	this_node(linkspeed)

# Returns:  the link speed string
#
# this is the valid ranges:
#
# 	      2400 <= fast_rate  < 10 000 000
#    	         0 <= fast_delay < 2400
#  	       600 <= slow_rate  <= fast_rate
# 	fast_delay <= slow_delay <= 10 000
# 	         2 <= header_size <= 128

proc check_linkspeed { } {
	global dgrpMsg
	global this_node
	global node

	dbg_gui "check_linkspeed this_node(linkspeed):  $this_node(linkspeed)"

	set line [split $this_node(linkspeed) ","]

	set rates(fast_rate,given)	[lindex $line 0]
	set rates(fast_rate,min)	2400
	set rates(fast_rate,max)	10000000
	set rates(fast_rate,name)	"fast rate"

	set rates(fast_delay,given)	[lindex $line 1]
	set rates(fast_delay,min)	0
	set rates(fast_delay,max)	2400
	set rates(fast_delay,name)	"fast delay"

	set rates(slow_rate,given)	[lindex $line 2]
	set rates(slow_rate,min)	600	
	set rates(slow_rate,max)	$rates(fast_rate,given)
	set rates(slow_rate,name)	"slow rate"

	set rates(slow_delay,given)	[lindex $line 3]
	set rates(slow_delay,min)	$rates(fast_delay,given)
	set rates(slow_delay,max)	10000
	set rates(slow_delay,name)	"slow delay"

	set rates(header_size,given)	[lindex $line 4]
	set rates(header_size,min)	2
	set rates(header_size,max)	128
	set rates(header_size,name)	"header size"
	
	set ret	 {}

	if { [string compare $ret $node(linkspeed,default)] == 0 } {
		return $ret
	}

	foreach rr {fast_rate fast_delay slow_rate slow_delay header_size} {
		if { [string compare $rates($rr,given) ""] == 0 } break;
		if { [expr $rates($rr,min) > $rates($rr,given)] } {
	   	   error_dialog $dgrpMsg(linkspeed_under) $rates($rr,name) $rates($rr,min) 
	   	   return -1
		} elseif { [expr $rates($rr,given) > $rates($rr,max)] } {
	   	   error_dialog $dgrpMsg(linkspeed_over) $rates($rr,name) $rates($rr,max) 
	   	   return -1
		} else { lappend ret $rates($rr,given) }
	}

	return [join $ret ,]
}

#----------------------------------------------------------------------
#
# set_linkspeed
#
# Description:
#
# 	Window to set the link speed for a specified node
#
proc set_linkspeed { } {
	global dgrpMsg
	global custom_linkspeed
	global this_node

	# 1. Create the top-lovel window and divide it into top 
	# and bottom parts.

	# Create the top frame
	set w [toplevel .set_link]
	wm title $w $dgrpMsg(linkspeed_title)
	frame $w.top 
	label $w.top.toptext -text "Link-Speed Settings for Device"
	pack $w.top.toptext -side left
	pack $w.top -side top -fill x

	# add a menubar
	menu $w.menubar
	$w config -menu $w.menubar 
       	set help [menu $w.menubar.mhelp]
       	$w.menubar add cascade -label Help -menu $help
	$help add command -label $dgrpMsg(linkspeed_help_label) \
		-command { info_dialog $dgrpMsg(linkspeed_help) }
	$help add command -label $dgrpMsg(linkspeed_ranges_label) \
		-command { info_dialog $dgrpMsg(linkspeed_ranges) }

	# 2. Fill the middle part with the 4 options
	frame $w.mid 
	foreach field {fast_rate fast_delay slow_rate slow_delay header_size} {
		entry $w.mid.e$field -textvariable $field\
		   -relief sunken 
		$w.mid.e$field 	delete 0 end
	}
	label $w.mid.lfast_rate  -text $dgrpMsg(fast_rate_label)  -anchor w
	label $w.mid.lfast_delay -text $dgrpMsg(fast_delay_label) -anchor w
	label $w.mid.lslow_rate  -text $dgrpMsg(slow_rate_label)  -anchor w
	label $w.mid.lslow_delay -text $dgrpMsg(slow_delay_label) -anchor w
	label $w.mid.lheader_size -text $dgrpMsg(header_size_label) -anchor w
	foreach field {fast_rate fast_delay slow_rate slow_delay header_size} {
		grid $w.mid.l$field $w.mid.e$field -sticky news
	}

	set line [split $this_node(linkspeed) ","]
	set i 0
	foreach j {fast_rate fast_delay slow_rate slow_delay header_size} {
		set line_item [lindex $line $i]
		dbg_gui $line_item; 
		if { $line_item != "{}" } { $w.mid.e$j insert 0 $line_item }
		set i [expr $i + 1]
	}

	pack $w.mid

	# 3. Create a row of buttons at the bottom 

	frame $w.bottom 
	pack $w.bottom -side bottom -fill both
	foreach but {Commit Cancel} {
		button $w.bottom.b$but -text $but 
		pack $w.bottom.b$but -side left -expand 1\
			    -padx 3m -pady 3m -ipadx 2m -ipady 1m
	}
	$w.bottom.bCommit configure -command {\
		set custom_linkspeed 1
		set this_node(linkspeed) \
		   "$fast_rate,$fast_delay,$slow_rate,$slow_delay,$header_size"
		set this_node(linkspeed) [check_linkspeed]
		if { $this_node(linkspeed) != -1 } { destroy_window .set_link }
		}
	$w.bottom.bCancel configure -command "destroy_window .set_link"


	# 4.  # set a grab, and claim the focus too.
	set oldFocus [focus]
	catch {tkwait visibility $w}
	catch {grab set $w}
	focus $w.bottom.bCancel
}


#----------------------------------------------------------------------
# 
# setup_ps_settings
#
# Description:
#
# 	copy the node(field,) enties to this_node(field)
#
proc setup_ps_settings { ID } {
	global dgrpVals
	global node
        global this_node

	foreach name $dgrpVals(node_arraynames) {
		set this_node($name) $node($name,$ID)
	}

}


#----------------------------------------------------------------------
#
# is_integer
#
# Return:	0 if not an integer greater than 0
#		1 if string is an integer greater than 0
# 
proc is_integer { string } {
	set result ""
	if { [scan $string %d result] != 0 } {
		dbg_gui "Entering is_integer with `$string' and got `$result'"
		if { [string compare $string $result] == 0 } {
			# ok, we read in an integer
			if { $result < 0 } { return 0 } else { return 1 }
		}
	} else {
		return 0
	}
}

#----------------------------------------------------------------------
#
# isvalid_ps_ipaddress 
#
# Description:
#
# 	if the name is one word beginning with [a-zA-Z], it is assumed valid
# 	if the name begins with a number, it is split into
# 	4 integers, n.n.n.n, each 0 =< n <= 255
#
proc isvalid_ps_ipaddress { IP } {
	global dgrpMsg

	# check that this is a valid IP or a name
	set line $IP
	dbg_gui "Entering isvalid_ps_ipaddress with `$IP'"

	# 
	# check to see if this is a named address
	#
	if { [string match {[a-zA-Z]*} $line] > 0 } {
		# treat this as an IP name, check it is one word
		scan $IP {%s} result
		if { [string compare $result $IP] != 0 } {
		  error_dialog $dgrpMsg(invalid_IP) $IP
	 	  return 0 
		}
	} else {
	#
	# see if this is a valid IP address nnn.nnn.nnn.nnn
	#
		set line [split $line "."]
		if { [ llength $line ] != 4 } {
		  	error_dialog $dgrpMsg(invalid_IP) $IP
	 		return 0 
		} else {
			foreach i $line {
			   if { $i < 0 || $i >255 } { 
		  		error_dialog $dgrpMsg(invalid_IP) $IP
	 	  		return 0 
	 		   }
			}
		}
	}
	return 1
}

#----------------------------------------------------------------------
# 
# isvalid_ps_id 
#
# Description:
#
# 	checks that the node id is not already in use, and is valid
#
proc isvalid_ps_id { ID } {
	global dgrpVals
	global dgrpMsg

	dbg_gui "Entering isvalid_ps_id with `$ID'"
	if { ![string match {[a-zA-Z0-9][a-zA-Z0-9]} $ID] &&
	     ![string match {[a-zA-Z0-9]} $ID] } {
		error_dialog $dgrpMsg(invalid_id) $ID 
		return 0
	}

	# check that the id is not in use
	foreach i $dgrpVals(master_node_list) {
		if {[string compare $ID $i] == 0} {
			error_dialog $dgrpMsg(id_in_use) $ID 
			return 0
		}
	}
	return 1
}

#----------------------------------------------------------------------
# 
# isvalid_ps_nports
#
# Description:
#
# 	check that the number of ports is valid
#
proc isvalid_ps_nports { nports } {
	global dgrpMsg

	dbg_gui "Entering isvalid_ps_nports with `$nports'"
	# check that the number of ports is valid
	if { $nports < 0 || $nports > 64 } { 
		error_dialog $dgrpMsg(invalid_nports)
		return 0
	} else { 
		return 1
	}
}
	
#----------------------------------------------------------------------
# 
# isvalid_ps_mode
#
# Description:
#
# 	check that the file mode is valid
#
proc isvalid_ps_mode { mode } {
	global dgrpMsg

	if { [string compare $mode default] == 0 } { return 1 }

	set result "";  # just so we don't get an error 
	scan $mode {%[0-7]} result
	dbg_gui "Entering isvalid_ps_mode with `$mode'and got result `$result'"

	if { ( [string compare $mode $result ] != 0 ) || \
		 ($result > 7777) || ($result < 0) } {
		error_dialog $dgrpMsg(invalid_mode) $mode
		return 0
	} else { 
		return 1
	}
}
#----------------------------------------------------------------------
#
# setup_new_ps_settings
#
# Description:
#
# 	Set some reasonable initial values for a new node
# 
proc setup_new_ps_settings { ID } {
	global node
	global dgrpVals
	
	dbg_gui "in proc setup_new_ps_settings id is $ID"

	foreach name $dgrpVals(node_arraynames) {
		set node($name,$ID) $node($name,default)
	}
}

#----------------------------------------------------------------------
#
# save_ps_settings 
#
# Description:
#
# 	save the values from this_node(field) to node(field,ID)
# 	for a specified node
# 	Doesn't do any error checks
# 
proc save_ps_settings { ID } {
	global node
        global this_node
	global dgrpVals
	
	dbg_gui "in proc save_ps_settings id is $ID"

	foreach name $dgrpVals(node_arraynames) {
		set node($name,$ID) $this_node($name)
	}
}

#----------------------------------------------------------------------
proc refresh_node_listbox { {old_selection -99} } {
	global ps_list 

	dbg_gui "Now in refresh_node_listbox"

	# it is possible for old_selection to be -1
	if { $old_selection < 0 } {
		set old_selection [$ps_list curselection]
	}

	delete_node_arrays
	delete_node_listbox
	setup_node_arrays
	fill_node_listbox

	$ps_list selection clear 0 end
	if { [string compare $old_selection ""] != 0 } {
		$ps_list selection set $old_selection
	}
}

#----------------------------------------------------------------------
# 
# delete_node_listbox
#
# Description:
#
# 	Delete the old listbox, and set all the indices to -1
#
proc delete_node_listbox { } {
	global ps_list 
	global node
	global dgrpVals

	dbg_gui "end is "
	dbg_gui [$ps_list index end]

	# delete the current listbox
	$ps_list delete 0 end
	foreach id $dgrpVals(master_node_list) { set node(index,$id) -1 }
}

#----------------------------------------------------------------------
# 
# fill_node_listbox
#
# Description:
#
# 	Fill the node listbox with the value in the node_field arrays
# 	Set the node(index,) array to hold the listbox index for each id
#
proc fill_node_listbox { } {
	global dgrpVals
	global node
	global ps_list

	# insert all the nodes

	set errorno 0

	foreach id $dgrpVals(master_node_list) {

		set node(index,$id) [$ps_list index end]

		foreach name {id ports major ip status linkspeed encrypt} {
			if { ![info exists node($name,$id)] } {
			   dbg_gui "the value $name for $id does not exist"
			   set $name "error" 
			   set errorno 1
			} else { 
			   set $name $node($name,$id) 
			   dbg_gui "set $name to $node($name,$id)"
			}
		}

		$ps_list insert end [format "%2s    %2s   %3s  %15s  %6s  %20s        %6s" \
			$id $ports $major $ip $status $linkspeed $encrypt]

	}
	set dgrpVals(nboards) [$ps_list index end] 
}

#----------------------------------------------------------------------
#
proc delete_node_arrays { } {
	global node
	global dgrpVals

	dbg_gui "deleting all arrays"
	foreach id $dgrpVals(master_node_list) {
	   foreach name $dgrpVals(node_arraynames) {
		if {[info exists node($name,$id)]} { unset node($name,$id) }
	   }
	}

}

#----------------------------------------------------------------------
#
proc setup_node_arrays { } {
	global node
	global dgrpVals
	global dgrpFVals
	global dgrpMsg

	set dgrpVals(master_node_list) { }

	#
	# cat /proc/dgrp/config for settings
	#
	file delete -force $dgrpFVals(tmp_file)
	if [file exists $dgrpFVals(proc_config)] {
		set errorno [exec cat $dgrpFVals(proc_config) > $dgrpFVals(tmp_file)]
		dbg_gui "setup_ps_settings: errorno is $errorno" 

		if [catch {open $dgrpFVals(tmp_file) r} fp1] { 
			puts $dgrpMsg(file_cannot_open_error) $dgrpFVals(tmp_file)
			error_dialog "$fp1"; 
			exit; 
		}
		while {[gets $fp1 line] >=0} {
   		# process the config file line by line
		# format is 
		# id major status nports (status_index)
		#                      vv these spaces are important!!
		   if { [string match {  [a-zA-Z0-9]*} $line] != 0 } {
   			scan $line "%s %d %s %d %s" f1 f2 f3 f4 f5
			lappend dgrpVals(master_node_list) $f1
   			set node(id,$f1) $f1
   			set node(major,$f1) $f2
   			set node(status,$f1) $f3
   			set node(ports,$f1) $f4
		   }
		} 
		close $fp1
	}

	#
	# cat /proc/dgrp/config for settings
	#
	if [file exists $dgrpFVals(dgrp_store)] { 

		if [catch {open $dgrpFVals(dgrp_store) r} fp1] { 
			puts $dgrpMsg(file_cannot_open_error) $dgrpFVals(dgrp_store)
			error_dialog "$fp1"; 
			exit; 
		}
		while {[gets $fp1 line] >=0} {
   		# process the store file line by line
		# format is
		# ID IP PortCount SpeedString IPPort Mode Owner Group Encrypt Encrypt_IPPort
		dbg_gui "line: $line"
		   if { [string match {[a-zA-Z0-9]*} $line] != 0 } {
   			scan $line "%s %s %s %s %s %s %s %s %s %s" \
			    	f1 f2 f3 f4 f5 f6 f7 f8 f9 f10
			# these next 2 should ALREADY be set!
			set node(id,$f1) $f1
   			set node(ports,$f1) $f3 
			# if there is a PS setup in /etc/dgrp.backing.store,
			# that is not initialized, include it in the master list
			dbg_gui "Searching for $f1 in $dgrpVals(master_node_list)"
			if { [lsearch -exact $dgrpVals(master_node_list) $f1] == -1 } {
				lappend dgrpVals(master_node_list) $f1
				dbg_gui "Not found!  Appending..."
			}
			# these are new
   			set node(ip,$f1) $f2
			set node(linkspeed,$f1) $f4
			set node(ipport,$f1) $f5
			set node(mode,$f1) $f6
			set node(owner,$f1) $f7
			set node(group,$f1) $f8
			set node(encrypt,$f1) $f9
			set node(encrypt_ipport,$f1) $f10
		   }	
		# end while
		}
		close $fp1
	}
	
	# ok, now make sure nothing goes unset!
	foreach label $dgrpVals(node_arraynames) {
		foreach id $dgrpVals(master_node_list) {
   	   		if {![info exists node(${label},$id)]} {
			   set node(${label},$id) $node(${label},default)
			}
   	  	} 
	} 
	set dgrpVals(master_node_list) [ lsort -dictionary $dgrpVals(master_node_list) ]
	dbg_gui "master list: $dgrpVals(master_node_list)"
	file delete -force $dgrpFVals(tmp_file)

}

#----------------------------------------------------------------------
#
# commit_portserver_config 
#
# Description:
#
# 	do the necessary error checking and run the 
#	config command to make it happen
#
proc commit_portserver_config { ID } {
	global dgrpFVals
	global dgrpMsg
	global node
        global this_node

	# setup the command line
	set command "$dgrpFVals(dgrp_cfg_node) init "
	dbg_gui "command in commit_portserver_config is $command"
	set options $dgrpFVals(dgrp_cfg_node_options)

	if { ! ( [isvalid_ps_ipaddress $this_node(ip)] \
		&& [isvalid_ps_nports $this_node(ports)] \
		&& [isvalid_ps_mode $this_node(mode)] ) } {
	   return 1
	}

	if { ( [string compare $this_node(owner) default] != 0 ) \
		&& !( [is_integer $this_node(owner)] ) } {
	   error_dialog $dgrpMsg(invalid_owner) $this_node(owner)
	   return 1
	}

	if { ( [string compare $this_node(group) default] != 0 ) \
		&& !( [is_integer $this_node(group)] ) } {
	   error_dialog $dgrpMsg(invalid_group) $this_node(group)
	   return 1
	}

	if { ([string compare $this_node(ipport) default] != 0) } {
		if { !([isvalid_ps_ipaddress $this_node(ip)] ) } {
			return 1
		}
	}

	if { ([string compare $this_node(encrypt_ipport) default] != 0) } {
		if { !([isvalid_ps_ipaddress $this_node(ip)] ) } {
			return 1
		}
	}

	# check if the speed string has changed
	if { [string compare $this_node(linkspeed) $node(linkspeed,$ID)] != 0 } {
		append options " -s $this_node(linkspeed)"
	}

	# check if the portnumber string has changed
	if { [string compare $this_node(ipport) $node(ipport,$ID)] != 0 } {
		append options " -p $this_node(ipport)"
	}
	
	# We always need to send in the encrypt option.
	append options " -e $this_node(encrypt)"

	# check if the encrypt ip port string has changed
	if { [string compare $this_node(encrypt_ipport) $node(encrypt_ipport,$ID)] != 0 } {
		append options " -q $this_node(encrypt_ipport)"
	}

	# check if the mode has changed
	if { [string compare $this_node(mode) $node(mode,$ID)] != 0 } {
		append options " -m $this_node(mode)"
	}

	# check if the owner has changed
	if { [string compare $this_node(owner) $node(owner,$ID)] != 0 } {
		append options " -o $this_node(owner)"
	}

	# check if the group has changed
	if { [string compare $this_node(group) $node(group,$ID)] != 0 } {
		append options " -g $this_node(group)"
	}

	save_ps_settings $this_node(id) 
	append command $options
	append command " $this_node(id) $this_node(ip) $this_node(ports)"
	Run $command
	return 0
}

#----------------------------------------------------------------------
proc config_portserver { } {
	global dgrpVals
	global dgrpMsg
	global node
        global this_node
	global ps_list

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0 ]; 

	# store the current vales for this port server
	setup_ps_settings $ID
	set w [PS_Config_Window .ps_config -1 Revert Commit Cancel]


	foreach name {id ports ip major mode ipport owner group encrypt_ipport} {
		$w.mid.e${name} insert 0 $node($name,$ID)
	}

	# set some fields to be read only
	$w.mid.eid	config -state disabled -relief flat
	$w.mid.emajor	config -state disabled -relief flat

	# revert
	$w.bottom.button0 configure -command " setup_ps_settings $ID; "
	bind $w.bottom.button0 <Return> " setup_ps_settings $ID; "

	# commit
	$w.bottom.button1 configure -command { 
		if { [commit_portserver_config $this_node(id)] == 0 } {
			refresh_node_listbox
			destroy_window .ps_config
		}
	}
	bind $w.bottom.button1 <Return> {\
		if { [commit_portserver_config $this_node(id)] == 0 } { 
			refresh_node_listbox
			destroy_window .ps_config }}

	# cancel
	$w.bottom.button2 configure -command { destroy_window .ps_config; 
		refresh_node_listbox}
	bind $w.bottom.button2 <Return> { destroy_window .ps_config;
		refresh_node_listbox}

	# grab the focus
	set oldFocus [focus]
	catch {tkwait visibility $w}
	catch {grab set $w}
	focus $w.bottom.button2
}

#----------------------------------------------------------------------
proc add_portserver { } {
	global this_node
	global node
	global dgrpVals

	foreach label $dgrpVals(node_arraynames) { 
		set this_node($label) $node($label,default) 
	}

	set w [PS_Config_Window .ps_add -1 Commit Cancel]

	# we don't use all the node_arraynames
	foreach label {id ip ports major ipport mode owner group encrypt_ipport} { 
		$w.mid.e${label} insert 0 $node($label,default)
	}

	$w.mid.emajor	config -state disabled -relief flat

	# commit
	$w.bottom.button0 configure -command { \
		if { [isvalid_ps_id $this_node(id)] } {
			setup_new_ps_settings $this_node(id)
			if { [commit_portserver_config $this_node(id)] == 0 } {
				destroy_window .ps_add
			}
		}
	}
	bind $w.bottom.button0 <Return> { if { [isvalid_ps_id $this_node(id)] } {
			setup_new_ps_settings $this_node(id)
			if { [commit_portserver_config $this_node(id)] == 0 } {
				destroy_window .ps_add
			}
		}
	}


	# cancel
	$w.bottom.button1 configure -command { destroy_window .ps_add}
	bind $w.bottom.button1 <Return> { destroy_window .ps_add}

	# grab the focus
	set oldFocus [focus]
	catch {tkwait visibility $w}
	catch {grab set $w}
	focus $w.mid.eid
}

#----------------------------------------------------------------------
#
# remove_portserver { } {
#
# Description:
#
# 	calls dgrp config script to remove the selected portserver
#
proc remove_portserver { } {
	global node
        global this_node
	global dgrpFVals
	global dgrpMsg
	global ps_list

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0 ]; 

	setup_ps_settings $ID
	set w [PS_Config_Window .ps_remove -1 Remove Cancel]

	# we don't use all the node_arraynames
	foreach label {id ip ports major ipport mode owner group encrypt_ipport} { 
		$w.mid.e${label} insert 0 $node($label,$ID)
		$w.mid.e${label} config -state disabled -relief flat
	}

	$w.mid.eencrypt.mbutton	config -state disabled;
	$w.mid.elinkspeed.mbutton	config -state disabled;

	#
	# button: remove
	#
	$w.bottom.button0 configure -command {
		destroy_window .ps_remove; 
		set command "$dgrpFVals(dgrp_cfg_node) -v -v "
		append command "uninit $this_node(id) $this_node(ip) $this_node(ports)" 
		Run $command 
		refresh_node_listbox [expr [$ps_list curselection] - 1]
	}
	bind $w.bottom.button0 <Return> { destroy_window .ps_remove; 
		set command "$dgrpFVals(dgrp_cfg_node) -v -v "
		append command "uninit $this_node(id) $this_node(ip) $this_node(ports)" 
		Run $command 
		refresh_node_listbox [expr [$ps_list curselection] - 1]
	}

	#
	# button: cancel
	#
	$w.bottom.button1 configure -command {destroy_window .ps_remove}
	bind $w.bottom.button1 <Return> {destroy_window .ps_remove}

	# grab the focus
	set oldFocus [focus]
	catch {tkwait visibility $w}
	catch {grab set $w}
	focus $w.bottom.button1
}


#----------------------------------------------------------------------
proc start_daemon { } {
	global ps_list
	global dgrpFVals
	global dgrpMsg

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0 ]; 
	set IP [lindex [$ps_list get $linenumber] 3 ]; 

	set command "$dgrpFVals(dgrp_cfg_node) $dgrpFVals(dgrp_cfg_node_options) start"
	append command " $ID $IP"
	Run $command
	refresh_node_listbox
}

#----------------------------------------------------------------------
proc stop_daemon { } {
	global ps_list
	global dgrpFVals
	global dgrpMsg

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0 ]

	set command "$dgrpFVals(dgrp_cfg_node) $dgrpFVals(dgrp_cfg_node_options) stop"
	append command " $ID "
	Run $command
	refresh_node_listbox
}

#----------------------------------------------------------------------
proc check_daemon { } {
	global ps_list
	global dgrpMsg

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0 ]

	set command "ps ax | grep \"drpd $ID\" | grep -v grep"
	Run $command
}

#----------------------------------------------------------------------
proc show_device_nodes { prefix } {
	global ps_list
	global dgrpMsg

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0 ]

	set command "ls -l /dev | grep $prefix$ID.."
	Run $command
}
