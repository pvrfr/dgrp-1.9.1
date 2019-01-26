/*
 *	dinc.h 
 *
 */

static char dinchSccsId[] = "@(#) $Id: dinc.h,v 1.14 2010/08/04 15:26:18 sethb Exp $ Copyright (C) 1995-1998 Digi International Inc., All Rights Reserved";

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
 
#if defined(HP)
#include <sys/modem.h>
#endif
#if !defined(HP) && !defined(OPENBSD) && !defined(TRU64) && !defined(FREEBSD)
#include <termio.h>
#endif
 
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
 
#if defined(SOLARIS)
#include <sys/mkdev.h>
#endif
 
#if !defined(HP) && !defined(LINUX) && !defined(OPENBSD) && !defined(FREEBSD)
#include <sys/stream.h>
#endif

#if !defined(HP) && !defined(AIX4) && !defined(LINUX) && !defined(SCO) && !defined(TRU64) && !defined(SCO6)
#include <sys/ioccom.h>
#endif
 
#if !defined(HP) && !defined(LINUX) && !defined(OPENBSD) && !defined(FREEBSD)
#include <sys/stropts.h>
#endif
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef OPENBSD
# include <i386/signal.h>
# include <i386/types.h>
# include <rpc/types.h>
# include <sys/errno.h>
# include <sys/fcntl.h>
# include <sys/ioccom.h>
# include <sys/select.h>
# include <sys/termios.h>
# include <sys/unistd.h>
# include <termios.h>
# include <time.h>
#endif
 
#ifdef FREEBSD
# include <signal.h>
# include <sys/types.h>
# include <rpc/types.h>
# include <sys/errno.h>
# include <sys/fcntl.h>
# include <sys/ioccom.h>
# include <sys/select.h>
# include <sys/termios.h>
# include <sys/unistd.h>
# include <termios.h>
# include <time.h>
#endif

#if defined(AIX4)
#include <sys/select.h>
#endif
 
/* include CD-specific header file */
#ifdef  USE_STS_EXT_BAUDS
# include "cd_ext.h"
#endif


#ifndef TRUE
# define        TRUE    (1)
# define        FALSE   (0)
#endif

#ifndef UNKNOWN
# define		UNKNOWN (2)
#endif
 
#ifndef OK
#define OK                      0
#endif
 
#define BSZ                     512
#define ISZ                     64
#define UC_IDLE                 0
#define UC_CR                   1
#define UC_TILDE                2
#define DINC_CAR                0x10
#define DINC_DTR                0x08
#define DINC_RTS                0x04
#define DINC_CTS                0x02
#define DINC_DSR                0x01

/* functions from dinc_hw.c */
void set_modem();
int get_modem();
int set_baud_exten(int exten);
int is_hwfc();
void set_hwfc();
void clr_swfc();
void clr_hwfc();
void bail(int code);

