#================================================================================
#
# FILE:  dgrp_misc.tcl
#
# A collection of miscellaneous commands not directly related to PortServers.
# This is a list of all the procedures in this file:
#
#  dbg_gui { text } 
#  destroy_window { w } 
#  info_dialog {text args} 
#  error_dialog {text args} 
#  warning_dialog {text default b0 b1} 
#  dialog {w title text bitmap default args}
#  Scroll_Set {scrollbar geoCmd offset size} 
#  Scrolled_Listbox { f args } 
#  Radio_Pulldown_Menu {f newvar args} 
#
#================================================================================

#--------------------------------------------------------------------------------
# 
# dbg_gui
#
# Description:
#
#	if the value "gui_debug" is set, write to stdout

proc dbg_gui { text } {
	global dgrpVals

	if $dgrpVals(gui_debug) { puts "dgrp_gui: $text" }

}

#--------------------------------------------------------------------------------
# 
# destroy_window
#

proc destroy_window { w } {

	catch {grab release $w }
	destroy $w
}

#--------------------------------------------------------------------------------

proc info_dialog {text args} {

	if { "$args" == "" } { 
		dialog .dinfo {} "$text" info -1 OK
	} else { 
		dialog .dinfo {} [eval {format $text} $args] info -1 OK 
	}
	
}

proc error_dialog {text args} {

	if { "$args" == "" } { 
		dialog .dinfo {} "$text" info -1 OK
	} else { 
		dialog .dinfo {} [eval {format $text} $args] info -1 OK 
	}
}

proc warning_dialog {text default b0 b1} {
	dialog .dwarn {Warning} "Warning: $text" warning $default $b0 $b1
}

#--------------------------------------------------------------------------------
# 
# dialog
# 
# Description:
#
#	prepares a top level dialog box for warnings, errors and messages

proc dialog {w title text bitmap default args} {
	global button

	# 1. Create the top-lovel window and divide it into top 
	# and bottom parts.

	toplevel $w -class Dialog
	wm title $w $title
	wm iconname $w Dialog
	frame $w.top -relief raised -bd 1
	pack $w.top -side top -fill both
	frame $w.bot -relief raised -bd 1
	pack $w.bot -side bottom -fill both

	# 2. Fill the top part with the bitmap and message.

	message $w.top.msg -text $text 
	pack $w.top.msg -side right -expand 1 -fill both \
		-padx 3m -pady 3m
	if {$bitmap != ""} {
		label $w.top.bitmap -bitmap $bitmap
		pack $w.top.bitmap -side left -padx 3m -pady 3m
	}
	
	# 3. Create a row of buttons at the bottom of the dialog.

	set i 0
	foreach but $args {
		button $w.bot.button$i -text $but -command \
			"set button $i"
		if {$i == $default} {
			frame $w.bot.default -relief sunken -bd 1
			raise $w.bot.button$i
			pack $w.bot.default -side left -expand 1\
			    -padx 3m -pady 2m
			pack $w.bot.button$i -in $w.bot.default \
			    -side left -padx 2m -pady 2m \
			    -ipadx 2m -ipady 1m	
		} else {
			pack $w.bot.button$i -side left -expand 1\
			    -padx 3m -pady 3m -ipadx 2m -ipady 1m
		}
		incr i
	}

	# 4. Set up a binding for <Return>, if there's a default,
	# set a grab, and claim the focus too.

	if {$default >= 0} { 
		bind $w <Return> "$w.bot.button$default flash; \
			set button $default"
	}
	set oldFocus [focus]
	dbg_gui "oldFocus is $oldFocus"
	catch {tkwait visibility $w}
	catch {grab set $w}
	focus $w

	# 5. Wait for the user to respond, then restore the focus
	# and return the index of the selected button.

	tkwait variable button
	destroy_window $w
	focus $oldFocus
	return $button
}

#--------------------------------------------------------------------------------
#
# Scroll_Set 
#
# Description:
#
# 	manages optional scrollbars.

proc Scroll_Set {scrollbar geoCmd offset size} {
	if {$offset != 0.0 || $size != 1.0} {
		eval $geoCmd;	# Make sure it is visible
		$scrollbar set $offset $size
	} else {
		set manager [lindex $geoCmd 0]
		$manager forget $scrollbar;  # hide it
	}
}

#--------------------------------------------------------------------------------
#

proc Scrolled_Listbox { f args } {
	frame $f
	listbox $f.list \
		-xscrollcommand [list Scroll_Set $f.xscroll \
			[list grid $f.xscroll -row 1 -column 0 -sticky we]] \
		-yscrollcommand [list Scroll_Set $f.yscroll \
			[list grid $f.yscroll -row 0 -column 1 -sticky ns]] \
		-font {system 12}
	eval {$f.list configure} $args
	scrollbar $f.xscroll -orient horizontal \
		-command [list $f.list xview]
	scrollbar $f.yscroll -orient vertical \
		-command [list $f.list yview]
	grid $f.list $f.yscroll -sticky news
	grid $f.xscroll -sticky news
	grid rowconfigure $f 0 -weight 1
	grid columnconfigure $f 0 -weight 1
	return $f.list
}

#--------------------------------------------------------------------------------
#

proc Radio_Pulldown_Menu {f newvar args} {
	
	frame $f
	menubutton $f.mbutton -textvariable $newvar -menu $f.mbutton.menu \
		-relief raised 
	menu $f.mbutton.menu
	foreach item $args {
		$f.mbutton.menu add radiobutton  -label $item -variable $newvar \
			-value $item 
	}
	pack $f.mbutton -fill x
	return $f

}
