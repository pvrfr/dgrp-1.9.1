
#================================================================================
#
# FILE:  dgrp_ttys.tcl
#
# This file contains all the code to create and update the window with
# all the ports for a single PS displayed.  This is a list of all the procedures
# contained in this file:
#
#  create_status_toplevel { }
#  dismiss_status_toplevel { w } 
#  status_event_loop { w nn } 
#  create_view_configured_ports_menubar { w } 
#  refresh_port_listbox { }
#  setup_port_arrays { id } 
#  do_update { w nn } 		- update the modem status toplevel window
#  delete_port_arrays { }	- deletes all the ports arrays
#  delete_port_listbox { }	- delete the ports listbox
#  fill_port_listbox { ID } 
#  view_configured_ports { }
#
# These are all the global arrays for this file:
# (for a port ttyIDnn, cuIDnn, or prIDnn, all arrays are indexed by nn)
# global port_list	- a list of all ports associated with this node
# global port(type,)	- tty | cu | pr
# global port(id,)	- ID
# global port(tty_open_cnt,) - number of tty and cu devices open
# global port(pr_open_cnt,)  - number of printer ports open
# global port(tot_wait_cnt,) - total number of process waiting on open
# global port(mstat,)	- modem status flags
# global port(iflag,) 	- the iflags given by the PS
# global port(oflag,) 	- the oflags given by the PS
# global port(cflag,) 	- the cflags given by the PS
# global port(bps,) 	- the baud rate set on the PS
# global port(digiflags,) - the Digi specific flags
# global port(description,) - serial, modem, etc
# global port(status,)	- open, closed
#
# These are some of the global variables:
# dgrpVals(refresh_update_delay) 	- delay for the modem status window 
# dgrpVals(status_toplevel_dying) 	- set to 1 if dying, so don't try to do updates
# dgrpVals(current_node_id) 		- this is the PortServer id we are looking at
#
#================================================================================

#--------------------------------------------------------------------------------
#
proc create_status_toplevel { } {
	global dgrpVals
	global dgrpMsg
	global mflag
	global plistbox

	set portnumber [$plistbox curselection] 
	if { $portnumber == "" } { error_dialog $dgrpMsg(no_port_selected); return }

	set dgrpVals(status_toplevel_dying) 0
	foreach index {DTR RTS CTS DSR DCD RI OPEN} { set mflag($index) 0 }

	set w [toplevel .dgrp_status]
	set oldFocus [focus]
	catch {tkwait visibility $w}
	catch {grab set $w}

	wm title $w $dgrpMsg(port_status_title)

	# Bind the X-event "Destroy" to manage the status_toplevel_dying flag as well
        bind $w <Destroy> {
                if {!$dgrpVals(status_toplevel_dying)} {
                        dismiss_status_toplevel .dgrp_status
                }
        }

	#
	# Create the top text
	# 
	frame $w.top
	label $w.top.toptext -text $dgrpMsg(port_status_toptext)
        button $w.top.bDismiss -text "Dismiss" \
               -command { if {!$dgrpVals(status_toplevel_dying)} {
				dismiss_status_toplevel .dgrp_status; \
				refresh_port_listbox } }

        bind $w.top.bDismiss <Return> { if {!$dgrpVals(status_toplevel_dying)} {
				dismiss_status_toplevel .dgrp_status; \
				refresh_port_listbox } }

	pack $w.top.toptext -side left
	pack $w.top.bDismiss -padx 5 -pady 5 -side left

        #
        # Create the radio buttons
        #
	frame $w.mid -borderwidth 5
	set column 0
	foreach modemsignal {DTR RTS CTS DSR DCD RI OPEN} { 
		label $w.mid.l$modemsignal -text $modemsignal
		checkbutton $w.mid.b$modemsignal -variable mflag($modemsignal)
		grid $w.mid.l$modemsignal -row 0 -column $column -sticky ns
		grid $w.mid.b$modemsignal -row 1 -column $column -sticky ns
		incr column
	}

        #
        # Create the bottom frame
        #
        frame $w.bottom

	# 
	# Add the delay scale
	# 
	dbg_gui "update_delay is ($dgrpVals(refresh_update_delay))"
	scale $w.bottom.scale -from 0 -to 10 -length 200 -variable dgrpVals(refresh_update_delay) \
		-orient horizontal -label "Update Delay in Seconds" \
		-tickinterval 2 -showvalue true
	pack $w.bottom.scale

	pack $w.top -side top -expand 1 -fill x
	pack $w.mid -fill x -pady 2m
        pack $w.bottom -side bottom -fill x

	# start with focus on the dismiss button
	focus $w.top.bDismiss

        # !!!
        #     Enter the Main Event Loop
        # !!!

        status_event_loop $w $portnumber

}

#----------------------------------------------------------------------
#
proc dismiss_status_toplevel { w } {
        global dgrpVals

        set dgrpVals(status_toplevel_dying) 1
	destroy_window $w
}


#----------------------------------------------------------------------
#
proc status_event_loop { w nn } {
	global dgrpVals

	set timer1 0
	while {1} {
		if { [expr $timer1 < 1 ] } {
			set timer1 [ clock seconds ]
		}
		update
		set timer2 [ clock seconds ]
		if { [expr ($timer2 - $timer1) >= $dgrpVals(refresh_update_delay) ]
			&& !$dgrpVals(status_toplevel_dying) } {
			do_update $w $nn
			set timer1 0
		}
	}
}

#----------------------------------------------------------------------
#
proc do_update { w nn } {
        global port
	global mflag
	global dgrpVals
	global dgrpMsg
	
	# read in the most current settings
	delete_port_arrays
        setup_port_arrays $dgrpVals(current_node_id)

	set DM_FLAGS $port(mstat,$nn)
	set mflag(DTR) [expr $DM_FLAGS & 0x01]
	set mflag(RTS) [expr ($DM_FLAGS & 0x02)?1:0]
	set mflag(CTS) [expr ($DM_FLAGS & 0x10)?1:0]
	set mflag(DSR) [expr ($DM_FLAGS & 0x20)?1:0]
	set mflag(RI)  [expr ($DM_FLAGS & 0x40)?1:0]
	set mflag(DCD) [expr ($DM_FLAGS & 0x80)?1:0]

	# If the port is closed, then the status
	# does not make sense, so we want to 
	# give a warning, BUT we only want to
	# give it once, and we do want to 
	# continue checking for fresh stats...

	set mflag(OPEN) [expr ( $port(tot_wait_cnt,$nn) + $port(tty_open_cnt,$nn))] 
	set totcnt [expr ( $mflag(OPEN) + $port(pr_open_cnt,$nn) ) ]
	if { $mflag(OPEN) > 0 } { set mflag(OPEN) 1 }
	if { $totcnt == 0  && $dgrpVals(warned_about_closed) == 0 } {
		info_dialog $dgrpMsg(port_closed_info)
		incr dgrpVals(warned_about_closed)
	} 

}

#----------------------------------------------------------------------
#
proc create_view_configured_ports_menubar { w } {
	global dgrpVals

	# Create the menu bar
        menu $w.menubar
        $w config -menu $w.menubar

        foreach m {Ports} {
                set $m [menu $w.menubar.m$m]
                $w.menubar add cascade -label $m -menu $w.menubar.m$m
        }

      	$Ports add command -label "Refresh List" \
                -command refresh_port_listbox
    	$Ports add separator
	$Ports add command -label "Modem Status" -command create_status_toplevel 
}

#----------------------------------------------------------------------
# refresh_port_listbox
# 
proc refresh_port_listbox { } {
        global plistbox 
        global dgrpVals
	
        dbg_gui "Now in refresh_port_listbox"
	set old_selection [$plistbox curselection]

        delete_port_arrays 
        delete_port_listbox
        setup_port_arrays $dgrpVals(current_node_id)
        fill_port_listbox $dgrpVals(current_node_id)

        $plistbox selection clear 0 end
        if { [string compare $old_selection ""] != 0 } {
                $plistbox selection set $old_selection
        } 
}

#----------------------------------------------------------------------
#
# go grab all the port settings 
#

proc delete_port_arrays { } {
        global 	port_list 
        global 	port
	global  dgrpVals

        dbg_gui "deleting all port arrays"

	if { [llength port_list] == 0 } { return; }

        foreach num $port_list {
	   foreach name $dgrpVals(port_arraynames) {	
                if {[info exists port($name,$num)]} {
					unset port($name,$num)
				}
	   }
        }

	set port_list {} 

}

#----------------------------------------------------------------------
#
# go grab all the port settings 
#

proc setup_port_arrays {id} {
        global 	port_list 
        global 	port
        global dgrpFVals
        global dgrpMsg
	global node

        #
        # cat /proc/dgrp/ports/<$id> for settings
        #
	set filename "$dgrpFVals(proc_ports_dir)/${id}"
        dbg_gui "setup_port_arrays: filename is $filename" 
        dbg_gui "setup_port_arrays: TMP_FILE is  $dgrpFVals(tmp_file)" 

	file delete -force $dgrpFVals(tmp_file)

	set port_list {}

	# the following relies on the fact that we count from 0
	set max_ports [expr $node(ports,$id) - 1]

        if [file exists $filename] {
                set errorno [exec cat $filename > $dgrpFVals(tmp_file)]
                dbg_gui "setup_port_arrays: errorno is $errorno" 

                if [catch {open $dgrpFVals(tmp_file) r} fp1] { 
                        puts "cannot open file $dgrpFVals(tmp_file)"
                        error_dialog "$fp1"; 
                        exit; 
                }
                while {[gets $fp1 line] >=0} {
        	   dbg_gui "read line $line" 
                # process the config file line by line
                # format is 
                # port_num tty_open_cnt pr_open_cnt tot_wait_cnt mstat iflag oflag cflag bps digiflags
                   if { [string match {[0-9]*} $line] != 0 } {
                        set scanresult [scan $line "%d %d %d %d 0x%x 0x%x 0x%x 0x%x %d 0x%x" f1 f2 f3 f4 f5 f6 f7 f8 f9 f10]
                        if { $scanresult != 10 } {
                             puts "invalid line read ($scanresult): $line"
                             continue;
                        }
        		dbg_gui "portnumber is $f1" 
			# only show ports that are configured
			if { $f1 > $max_ports } { break; }
                        lappend port_list 	$f1
                        set port(tty_open_cnt,$f1) $f2
                        set port(pr_open_cnt,$f1) $f3
                        set port(tot_wait_cnt,$f1) $f4
                        set port(mstat,$f1)	$f5
                        set port(iflag,$f1)	$f6
                        set port(oflag,$f1)	$f7
                        set port(cflag,$f1)	$f8
                        set port(bps,$f1)	$f9
                        set port(digiflags,$f1)	$f10

			# compute the status now
			if { [expr ($port(tty_open_cnt,$f1) + $port(pr_open_cnt,$f1))] > 0 } {
                        	set port(status,$f1)	OPEN
			} elseif { $port(tot_wait_cnt,$f1) > 0 } {
                        	set port(status,$f1)	WAITING
			} else {
                        	set port(status,$f1)	CLOSED
			}
                        set port(description,$f1)	NA
                   }
                } 
                close $fp1
       } else {
		error_dialog $dgrpMsg(file_dne_error) $filename
	}

	file delete -force $dgrpFVals(tmp_file)

} 

#----------------------------------------------------------------------
#
# Delete the old listbox, and set all the indices to -1
#
proc delete_port_listbox { } {
        global plistbox 
	global port
	global port_list

        dbg_gui "end is "
        dbg_gui [$plistbox index end]

        # delete the current listbox
        $plistbox delete 0 end
        foreach num $port_list {
		set port_index($num) -1
	}
}

#----------------------------------------------------------------------
#
# Fill the port listbox with the value in the node_field arrays
# Set the port(index,) array to hold the listbox index for each id
#
proc fill_port_listbox { ID } {
	global plistbox
	global port
	global port_list

	#
	# insert each port into the list in numeric order
	#

	if { ![info exists port(status,0) ] }  {
		return; }

        foreach nn $port_list {
                set port(index,$nn) [$plistbox index end]
		$plistbox insert end [format " %#02d  %8s  %6d  %10s" \
			$nn $port(status,$nn) $port(bps,$nn) \
			$port(description,$nn)]
        }
}

#----------------------------------------------------------------------
#
proc view_configured_ports { } {
	global ps_list
	global plistbox
	global dgrpVals
	global dgrpFVals
	global dgrpMsg
	global port
        global port_list 

	set linenumber [$ps_list curselection];
	if { $linenumber == "" } { error_dialog $dgrpMsg(no_ps_selected); return }
	set ID [lindex [$ps_list get $linenumber] 0];

	set w [toplevel .port_config]
	set dgrpVals(current_node_id) $ID
	

	# Let's make this window the focus
	set oldFocus [focus]
	catch {tkwait visibility $w}
	catch {grab set $w}

	wm title $w $dgrpMsg(port_title)

	#
	# add a menubar
	#
	create_view_configured_ports_menubar $w

	#
	# Create the top frame
	#
	frame $w.top
	label $w.top.toptext -text [format $dgrpMsg(port_toptext) $ID]
	pack $w.top.toptext -side left

        #
        # Add the refresh button to update the display
        #
        button $w.top.bRefresh -text Refresh
        pack $w.top.bRefresh -padx 5 -pady 5 -side right

	# Create the middle frame
	frame $w.mid -borderwidth 10
	# add the list box
	set plistbox [Scrolled_Listbox $w.mid.port_listbox -width 80 -height 10]

	# bind the default action on double clicking a line
	bind $plistbox <Return> create_status_toplevel
	bind $plistbox <Double-1> create_status_toplevel

	setup_port_arrays $ID
	fill_port_listbox $ID

        $plistbox selection clear 0 end
        $plistbox selection set 0

	$w.top.bRefresh configure -command refresh_port_listbox
        bind $w.top.bRefresh <Return> { refresh_port_listbox }

	#
	# add the headings for the listbox
	#
	frame $w.mid.toptext
	label $w.mid.toptext.headings -text $dgrpMsg(port_headings) \
		-font {system 12}

	pack $w.mid.toptext.headings -side left
	pack $w.mid.toptext $w.mid.port_listbox -side top -fill x  -expand true

	#
	# Create the bottom frame and buttons
	#
	frame $w.bottom -borderwidth 10

	button $w.bottom.bDismiss -text Dismiss \
		-command { destroy_window .port_config }
	bind $w.bottom.bDismiss <Return> "destroy_window .port_config" 
	button $w.bottom.bStatus -text "Modem Status" \
		-command create_status_toplevel
	bind $w.bottom.bStatus <Return> create_status_toplevel
		
	label $w.bottom.ldigi_gif -image $dgrpFVals(logo)
	pack $w.bottom.ldigi_gif -side left
	pack $w.bottom.bDismiss $w.bottom.bStatus -side left -padx 5 -expand 1

	pack $w.top -side top -fill x
	pack $w.mid
	pack $w.bottom -side bottom -fill x

	focus $w.bottom.bDismiss

}

