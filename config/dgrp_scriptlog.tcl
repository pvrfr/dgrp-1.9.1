
#================================================================================
#
# FILE:  dgrp_scriptlog.tcl
#
# This is the package to provide logging of commands
# based on examples in "Practical Programming in Tcl/Tk"
# Chapter 19.
#
# Procedures included in this package:
#  Run {command}  	-- This is what is called from the outside
#  Runit { } 		-- This is what is called from the inside
#  Exec_Log { f args } 
#  Stop {} 
#  Log {} 
#
#================================================================================

#--------------------------------------------------------------------------------
#
proc Exec_Log { f args } {
# run a program with exec and log the output
	global run_command log input but frame
	global dgrpMsg

	set run_command [join $args]

	# Create the top frame for buttons and entry
	set oldFocus [focus]
	catch {set w [toplevel $f]} result 

	if { $result == $f } {

	catch {tkwait visibility $w}
	catch {grab set $w}
	set frame $f; 

	wm title $w $dgrpMsg(logger_title)

	frame $w.top -borderwidth 10

	# Create the command buttons.

	button $w.top.exit -text Exit -command {
		destroy_window $frame; \
		refresh_node_listbox; \
		return 0 \
	   }
	set but [button $w.top.run -text "Run it" -command Runit]

	# Create a labeled entry for the command

	label $w.top.l -text Command: -padx 0
	entry $w.top.cmd -relief sunken \
		-textvariable run_command
	pack $w.top.l -side left
	pack $w.top.cmd -side left -fill x -expand true
	pack $w.top.run $w.top.exit -side left 

	# Set up key binding equivalents to the buttons
	
	bind $w.top.exit <Return> { destroy_window $frame; \
		refresh_node_listbox; return 0 }
	bind $but <Return> Runit

	bind $w.top.cmd <Return> Runit
	bind $w.top.cmd <Control-c> Stop
	focus $but

	# Create a text widget to log the output

	frame $w.mid -borderwidth 10
	set log [Scrolled_Listbox $w.mid.log -width 80 -height 15]
	pack $w.mid.log -side left -fill x -expand true
	

	pack $w.top -side top -fill x
	pack $w.mid -side bottom -fill both -expand true

	} else { dbg_gui "OK $result" }

}

proc Run { given_command } {

	dbg_gui "in Run, the command is $given_command"
	set w [Exec_Log .frame $given_command]
}

proc Runit { } {
	global run_command input log but

	dbg_gui "in Runit, the run_command is $run_command"
	if [catch {open "|$run_command |& cat" } input] {
		$log insert end "> $input"
	} else {
		fileevent $input readable Log
		$log insert end "% $run_command"
		$but config -text Stop -command "Stop"
		bind $but <Return> Stop
	}
}

	
proc Stop {} {
	global input but frame

	dbg_gui "Call to Stop in scriptlog"
	catch {close $input}
	$but config -text "Run it" -command Runit
	bind $but <Return> Runit
}

proc Log {} {
	global input log
	if [eof $input] {
		Stop
	} else {
		flush stdout
		gets $input line
		$log insert end $line
		$log see end
	}
}

