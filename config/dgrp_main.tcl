#================================================================================
#
# FILE:  dgrp_main.tcl
#
# This file contains all the initialization code and the function "main".  
# Here is a list of all the procedures contained in this file:
#
#  sanity_check {} 
#  get_driver_ver { } 
#  main { } 
#
#================================================================================

#--------------------------------------------------------------------------------
# 
# sanity_check
#
# Description:
#
#	determines whether the necessary files are available to run the gui
# 	without crashing

proc sanity_check {} {
	global dgrpFVals
	global dgrpMsg

	if { ![file exists $dgrpFVals(dgrp_root)] ||
	     ![file isdirectory $dgrpFVals(dgrp_root)] } {
		puts [format $dgrpMsg(dir_dne_error) $dgrpFVals(dgrp_root)]
		exit 0
	}
	
	if { ![file executable $dgrpFVals(dgrp_root)] } {
		puts [format $dgrpMsg(cannot_cd_error) $dgrpFVals(dgrp_root)]
		exit 0
	}

	if { ![file exists $dgrpFVals(dgrp_cfg_node)] } {
		puts [format $dgrpMsg(file_dne_error) $dgrpFVals(dgrp_cfg_node)]
		exit 0
	}

	exec echo > $dgrpFVals(tmp_file)

	if { ![file exists $dgrpFVals(tmp_file)] } {
		puts [format $dgrpMsg(file_cannot_create_error) $dgrpFVals(tmp_file)]
		exit 0
	}

	
	if { ![file executable $dgrpFVals(dgrp_cfg_node)] } {
		puts [format $dgrpMsg(no_execute_perm_error) $dgrpFVals(dgrp_cfg_node)]
		exit 0
	}
	
}

#--------------------------------------------------------------------------------
# 
# get_driver_ver
#
# Description:
#
# 	read the /proc/dgrp/info file for the current driver version 
# 	and store the value in dgrpMsg(driver_ver)
#
# Returns:
#	 0 on successful completion
#	-1 file does not exist
#	-2 failed open
#	-3 version line not found

proc get_driver_ver { } {
	global dgrpFVals
	global dgrpMsg

	set found 0
	if [file exists $dgrpFVals(proc_info)] {
		if [catch {open $dgrpFVals(proc_info) r} fp1] { 
                        puts $dgrpMsg(file_cannot_open_error) $dgrpFVals(proc_info)
                        error_dialog "$fp1";
			set dgrpMsg(driver_ver) $dgrpMsg(driver_ver_error)
                        return -2;
                }
		while {[gets $fp1 line] >=0} {
                	# process the file line by line
                   	if { [string match {version*} $line] != 0 } {
                        	scan $line "%s %s" f1 f2
				dbg_gui "field1 = '$f1', field2 = '$dgrpMsg(driver_ver)'"
				set dgrpMsg(driver_ver) [format $dgrpMsg(driver_ver_ok) $f2]
				set found 1
                   	}
                }
                close $fp1
	} else {
		set dgrpMsg(driver_ver) $dgrpMsg(driver_ver_dne) 
		return -1
	}

	if { $found == 0 } { 
		set dgrpMsg(driver_ver) $dgrpMsg(driver_ver_error) 
		return -3
	} else { return 0 }

}

#--------------------------------------------------------------------------------
# 
# main
#

proc main { } {
	global dgrpFVals

	# check for files and execute permissions
	sanity_check

	# change directory to the config directory
	if [catch {cd $dgrpFVals(dgrp_root)} err] {
		puts stderr $err
		exit 0
	}

	lappend auto_path $dgrpFVals(dgrp_root)/

	# kill any previous definitions of "."
	wm withdraw .
	set w [ create_display_all_portservers .dgrp_manager ]
	catch {tkwait visibility $w}
	catch {grab set $w}
	focus $w.bottom.bConfigure

	#
	# 
	# Works better than binding <Destroy>
	#
	wm protocol $w WM_DELETE_WINDOW { exit 0 }

}

lappend auto_path .
main 

