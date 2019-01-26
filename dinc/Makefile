#
# Makefile for DINC - Direct INstant Connect utility
#
# @(#) $Id: Makefile,v 1.19 2012/10/16 12:18:32 sethb Exp $ Copyright (C) Digi International Inc., All Rights Reserved
# Authored by Dennis Cronin, Maintained by Jeff Randall
#

#
# Tools
#
# cc recommended for SunOS (gcc has problems with certain ioctls)
# either cc or gcc for Solaris, HP, and Irix
#
#CC = cc
CC = gcc

#
# Binary name
DINC=dinc

#
# define directory you want DINC to live in
#
INSTALLDIR = /usr/local/bin

#
# and directory for the man page
#
MANDIR= /usr/local/man/man1
MANEXT=1
#MANDIR= /usr/local/man/man1m
#MANEXT=1m

#
# Lock directory must be specified
#
# Linux typically uses '/var/lock'
# OpenBSD uses '/var/spool/locks'
#
#LOCK=/var/spool/locks
#LOCK=/usr/spool/uucp
#LOCK=/var/lock

# version DEFS
#
# -DSUNOS                    build for SunOS 4.1.x
# -DSOLARIS                  build for Solaris 2.x
# -DIRIX5                    build for Irix 5.x and 6.x
# -DHP                       build for a HP-UX 9.0x and 10.0x
# -DLINUX                    build for a Linux system
# -DOPENBSD                  build for OpenBSD 2.6+ (and maybe other BSDs)
# -DSCO                      build for SCO
# -DSCO6                     build for SCO6
# -DNO_UULOCKING             disable uucp lock files
# -DUSE_STS_EXT_BAUDS        enable STS/ELS/PCI high baud rate support
#

#DEFS=-DSUNOS -DUSE_STS_EXT_BAUDS
#DEFS=-DSOLARIS -DUSE_STS_EXT_BAUDS
#DEFS=-DIRIX5 -DUSE_STS_EXT_BAUDS
#DEFS=-DIRIX5 -DFAST_TERMIOS
#DEFS=-DHP -DUSE_STS_EXT_BAUDS
#DEFS=-DHP -DUSE_STS_EXT_BAUDS
#DEFS=-DLINUX
#DEFS=-DOPENBSD -DUSE_STS_EXT_BAUDS
#DEFS=-DFREEBSD -DUSE_STS_EXT_BAUDS
#DEFS=-DSCO -DUSE_STS_EXT_BAUDS -D_SVID3
#DEFS=-DSCO6 -DUSE_STS_EXT_BAUDS -D_SVID3

# end of configurables

RELFILES = COPYRIGHT README Makefile dinc.c dinc.h dinc_hw.c cd_ext.h dinc.1

all: dinc

.c.o: 
	$(CC) -c $(DEFS) -DLOCK_DIR="\"$(LOCK)\"" $<

dinc: dinc.o dinc_hw.o 
	$(CC) -o ${DINC} dinc.o dinc_hw.o

install: dinc
	mkdir -p $(INSTALLDIR)
	mkdir -p $(MANDIR)
	cp dinc     $(INSTALLDIR)/dinc
	chown $(BIN_OWN) $(INSTALLDIR)/dinc
	chgrp $(BIN_GRP) $(INSTALLDIR)/dinc
	chmod 4111  $(INSTALLDIR)/dinc
	cp dinc.1   $(MANDIR)/dinc.$(MAN_EXT)
	chown $(MAN_OWN) $(MANDIR)/dinc.$(MAN_EXT)
	chgrp $(MAN_GRP) $(MANDIR)/dinc.$(MAN_EXT)
	chmod 644   $(MANDIR)/dinc.$(MAN_EXT)

shar:
	chmod a+rw $(RELFILES)
	shar $(RELFILES) > dinc.shar

clean:
	rm -f *.o dinc

####################################################################
#
# makes for specific operating systems to override defaults settings
#

LINUX:
	@make all DEFS="-DLINUX" \
		LOCK=/var/lock 

SUNOS:
	@make all DEFS="-DSUNOS -DUSE_STS_EXT_BAUDS" \
		LOCK=/usr/spool/uucp 

SOLARIS:
	@make all DEFS="-DSOLARIS -DUSE_STS_EXT_BAUDS" CC=cc \
		LOCK=/var/spool/locks 

SOLARIS_X86:
	@make all DEFS="-DSOLARIS -DUSE_STS_EXT_BAUDS" CC=gcc \
		LOCK=/var/spool/locks 

TRU64:
	@make all DEFS="-DTRU64" CC=cc \
		LOCK=/var/spool/locks 

IRIX:
	@make all DEFS="-DIRIX5 -DUSE_STS_EXT_BAUDS" CC=cc \
		LOCK=/var/spool/locks

IRIXNEW:
	@make all DEFS="-DIRIX5 -DFAST_TERMIOS" CC=cc \
		LOCK=/var/spool/locks 

HP9:
	@make all DEFS="-DHP -DUSE_STS_EXT_BAUDS" \
		LOCK=/usr/spool/uucp 

HP10:
	@make all DEFS="-DHP -DUSE_STS_EXT_BAUDS" CC=cc \
		LOCK=/var/spool/locks 

OPENBSD:
	@make all DEFS="-DOPENBSD -DUSE_STS_EXT_BAUDS" \
		LOCK=/var/spool/lock

OSX:
	@make all DEFS="-DFREEBSD" LOCK=/tmp

FREEBSD:
	@make all DEFS="-DFREEBSD" LOCK=/var/spool/lock

SCO:
	@make all DEFS="-DSCO6 -DFAST_TERMIOS -D_SVID3" CC=cc \
		LOCK=/var/spool/locks

SCO6:
	@make all DEFS="-DSCO6 -DFAST_TERMIOS -D_SVID3" CC=cc \
		LOCK=/var/spool/locks

#finis
