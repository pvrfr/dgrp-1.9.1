#
# Common definitions go here.
#

#
# TRUE_VERSION is the version string used in the driver build,
# it is intended to be in the form:
#
#   1.0-3
#
# A string noting a particular special modification could be
# used as well.  This string will be reported when the driver
# is loaded, and will be exposed by its /proc/dgrp/info
# interface.
#
TRUE_VERSION="1.9-36Beta6"

#
# DRP_PART_NUM is the part number string for the driver package.
# It should be in the form:
#
#   40002086_A
#
DRP_PART_NUM=40002086_X6P

#
# DRP_REL_NOTE is the part number string for the driver release
# notes.  It should be in the form:
#
#   93000359_A
#
DRP_REL_NOTE=93000359_X6P

#
# DRP_USER_DOC is the part number string for the software manual.
# It should be in the form:
#
#   92000582_A
#
DRP_USER_DOC=92000582_A

#
# DRP_PKG_VER is the "version" number string included in the
# various documentation and packaging files.  It should be
# in the form:
#
#   1.0
#
DRP_PKG_VER=1.9

#
# DRP_PKG_REV is the "revision" of this version.  Together,
# a linux module revision is built with:
#
#   ${DRP_PKG_VER}-${DRP_PKG_REV}
#
DRP_PKG_REV=36

#
# DRP_PKG_DATE is the "date" string included in (for now) the
# release notes.  It should be in the form:
#
#   05/25/2000
#
DRP_PKG_DATE=02/08/2016

#
#	Miscelaneous path macro's
#
#  ***  take the MAN paths with a grain of salt,
# 	the filenames have the chapter hard coded throughout
# 	they are included here more for reference...


SBINDIR=	/usr/sbin
# SRCDIR=		/usr/src/dg/dgrp

MANDIR=		/usr/man

#
#	Paths used for install
#
DGRPBIN=	$(RPM_BUILD_ROOT)/usr/bin/dgrp
DRPDDIR=	$(RPM_BUILD_ROOT)/usr/bin/dgrp/daemon
DGRPCONFIG=	$(RPM_BUILD_ROOT)/usr/bin/dgrp/config
DGRPMAN=	$(RPM_BUILD_ROOT)/usr/man/man8

INIT_DIR= $(shell \
	if [ -d /etc/rc.d/init.d ]; \
	then echo "$(RPM_BUILD_ROOT)/etc/rc.d/init.d"; \
	else echo "$(RPM_BUILD_ROOT)/etc/init.d"; fi)

#
# 	Ditty paths
#
DITTYNAME=	ditty-rp
DITTYBIN=	$(RPM_BUILD_ROOT)/usr/bin
DITTYMAN=	$(RPM_BUILD_ROOT)/usr/man/man1

#
# 	DGIPServ paths
#
IPSERVNAME=	dgipserv
IPSERVBIN=	$(RPM_BUILD_ROOT)/usr/sbin
IPSERVMAN=	$(RPM_BUILD_ROOT)/usr/man/man1

#
# 	DGELReset paths
#
ELRESETNAME=	dgelreset
ELRESETBIN=	$(RPM_BUILD_ROOT)/usr/sbin
ELRESETMAN=	$(RPM_BUILD_ROOT)/usr/man/man1

