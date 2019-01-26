#
#	dgrp_gui Makefile for Digi RealPort driver for Linux
#

include ../Makefile.inc

INIT_LIB=\
	dgrp_daemon \
	dgrp_ditty \
	$(NULL)
#
# INIT_DIR=/etc/rc.d/init.d (moved to Makefile.inc)

# all install clean clobber web::
# all pkgIndex.tcl

all:	
	
install:  
	install -d $(INIT_DIR)
	install $(INIT_LIB) $(INIT_DIR)
	install -d $(RPM_BUILD_ROOT)/usr/bin
#	install -d $(RPM_BUILD_ROOT)/tmp
	install -d $(RPM_BUILD_ROOT)/tmp/dgrp
	install -m 755 dgrp_udev $(RPM_BUILD_ROOT)/usr/bin/dgrp_udev
#	install -m 644 10-dgrp.rules $(RPM_BUILD_ROOT)/tmp/10-dgrp.rules
	install -m 644 10-dgrp.rules $(RPM_BUILD_ROOT)/tmp/dgrp/10-dgrp.rules

uninstall:  
	-rm -f $(foreach FILE, $(INIT_LIB), $(INIT_DIR)/$(FILE))
	-rm -f $(RPM_BUILD_ROOT)/usr/bin/dgrp_udev
#	-rm -f $(RPM_BUILD_ROOT)/tmp/10-dgrp.rules
	-rm -f $(RPM_BUILD_ROOT)/tmp/dgrp/10-dgrp.rules
	-rm -f $(RPM_BUILD_ROOT)/tmp/dgrp/dgrp.backing.store.new
	-rmdir $(RPM_BUILD_ROOT)/tmp/dgrp

clean:
	-rm -f dgrp_udev

clobber:: clean

.PHONY: all install uninstall clean clobber

