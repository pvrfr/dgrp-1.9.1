/*
 *	DINC_HW.C
 *
 *	Hardware dependencies for SunOS/Solaris/HP/Irix.  More to come soon.
 */

#include "dinc.h"


static char dinc_hw_sccs[] =
	"@(#) $Id: dinc_hw.c,v 1.17 2010/08/04 15:26:18 sethb Exp $ Copyright (C) 1995-1998 Digi International Inc., All Rights Reserved";


extern int tty_fd;
extern int mdmsigs;


#if defined(SOLARIS) || defined(SUNOS) || defined(LINUX) || defined(OPENBSD) || defined(SCO) || defined(SCO6) || defined(TRU64) || defined (FREEBSD)

extern struct termios tty_termios;

#if defined(SCO)
# define CRTSCTS  (RTSFLOW | CTSFLOW)
#endif

#if defined(SOLARIS)
  int is_hwfc() { return(tty_termios.c_cflag&(CRTSXOFF|CRTSCTS)); }
  void set_hwfc() { tty_termios.c_cflag |=   (CRTSXOFF|CRTSCTS); }
  void clr_hwfc() { tty_termios.c_cflag &=  ~(CRTSXOFF|CRTSCTS); }

#elif defined(SCO6)

# include <sys/termiox.h>

struct termiox tx;

int is_hwfc() {
	int stat;
	
	/* fetch current state */
	stat = ioctl(tty_fd,TCGETX, &tx);
	if(stat < 0) {
		perror("TCGETX failed");
		bail(1);
	}
	return(tx.x_hflag & (RTSXOFF | CTSXON));
}


/*
 *	set_hwfc()
 *
 *	Makes HP-proprietary ioctl to set hardware flow control.
 */
void set_hwfc() {
  int stat;
  
  /* fetch current state */
  stat = ioctl(tty_fd,TCGETX, &tx);
  if(stat < 0) {
    perror("TCGETX failed");
    bail(1);
  }
  
  tx.x_hflag |= (RTSXOFF | CTSXON);
  
  /* set new state */
  stat = ioctl(tty_fd,TCSETX, &tx);
  if(stat < 0) {
    perror("TCSETX failed");
		bail(1);
  }
}

/*
 *	clr_hwfc()
 *	
 *	Makes HP-proprietary ioctl to set hardware flow control.
 */
void clr_hwfc() {
  int stat;
  
  /* fetch current state */
  stat = ioctl(tty_fd,TCGETX, &tx);
  if(stat < 0) {
    perror("TCGETX failed");
    bail(1);
  }
  
  tx.x_hflag &= ~(RTSXOFF | CTSXON);
  
  /* set new state */
  stat = ioctl(tty_fd,TCSETX, &tx);
  if(stat < 0) {
    perror("TCSETX failed");
    bail(1);
  }
}
#else

  int is_hwfc() { return(tty_termios.c_cflag &CRTSCTS); }
  void set_hwfc() { tty_termios.c_cflag |= CRTSCTS; }
  void clr_hwfc() { tty_termios.c_cflag &= ~CRTSCTS; }
#endif



/*
 *	set_baud_exten()
 *
 *	Returns OK if settable, !OK if not.
 */
int set_baud_exten(int exten) {

#if !defined(USE_STS_EXT_BAUDS)
  return(!OK);
#else
  static stsextctl xc;
  int stat;
  
  /* get current state */
  stat = ioctl(tty_fd, STSGETEXTCTL, &xc);
  if(stat < 0) {
    return(!OK);
  }
  
  xc.x_flags &= ~(XC_BAUDEXT | XC_LOOP);
  xc.x_flags |= (exten << XC_BAUDSHIFT);
  
  /* set new state */
  stat = ioctl(tty_fd, STSSETEXTCTL, &xc);
  if(stat < 0) {
    perror("STSSETEXTCTL");
    return(!OK);
  }
  return(OK);
#endif	/* USE_STS_EXT_BAUDS */
}

/*
 *	set_modem()
 *	
 *	Sets modem control state based on 'mdmsigs' global var.
 *	We bail if we fail.
 */
void set_modem() {
  int mdm;

  if(ioctl(tty_fd, TIOCMGET, &mdm) < 0)
    bail(1);
  mdm &= ~(TIOCM_DTR|TIOCM_RTS);
  if(mdmsigs & DINC_DTR)
    mdm |= TIOCM_DTR;
  if(mdmsigs & DINC_RTS)
    mdm |= TIOCM_RTS;
  if(ioctl(tty_fd, TIOCMSET, &mdm) < 0)
    bail(1);
}


/*
 *	get_modem()
 *
 *	Hardware-specific call to fetch modem status.  Translates to
 *	DINC_xxx modem indications.
 *	We bail if we fail.
 */
int get_modem() {
  int mdm;
  static int raw_mdm;
  
  if(ioctl(tty_fd, TIOCMGET, &raw_mdm) < 0)
    bail(1);
  
  mdm = 0;
  if(raw_mdm & TIOCM_CAR) mdm |= DINC_CAR;
  if(raw_mdm & TIOCM_DTR) mdm |= DINC_DTR;
  if(raw_mdm & TIOCM_RTS) mdm |= DINC_RTS;
  if(raw_mdm & TIOCM_CTS) mdm |= DINC_CTS;
  if(raw_mdm & TIOCM_DSR) mdm |= DINC_DSR;
  return(mdm);
}
#endif /* SUNOS || SOLARIS */



#if defined(HP)
#include <sys/termiox.h>

struct termiox tx;



int is_hwfc() {
	int stat;
	
	/* fetch current state */
	stat = ioctl(tty_fd,TCGETX, &tx);
	if(stat < 0) {
		perror("TCGETX failed");
		bail(1);
	}
	return(tx.x_hflag & (RTSXOFF | CTSXON));
}


/*
 *	set_hwfc()
 *
 *	Makes HP-proprietary ioctl to set hardware flow control.
 */
void set_hwfc() {
  int stat;
  
  /* fetch current state */
  stat = ioctl(tty_fd,TCGETX, &tx);
  if(stat < 0) {
    perror("TCGETX failed");
    bail(1);
  }
  
  tx.x_hflag |= (RTSXOFF | CTSXON);
  
  /* set new state */
  stat = ioctl(tty_fd,TCSETX, &tx);
  if(stat < 0) {
    perror("TCSETX failed");
		bail(1);
  }
}

/*
 *	clr_hwfc()
 *	
 *	Makes HP-proprietary ioctl to set hardware flow control.
 */
void clr_hwfc() {
  int stat;
  
  /* fetch current state */
  stat = ioctl(tty_fd,TCGETX, &tx);
  if(stat < 0) {
    perror("TCGETX failed");
    bail(1);
  }
  
  tx.x_hflag &= ~(RTSXOFF | CTSXON);
  
  /* set new state */
  stat = ioctl(tty_fd,TCSETX, &tx);
  if(stat < 0) {
    perror("TCSETX failed");
    bail(1);
  }
}


/*
 *	set_baud_exten()
 *
 *	Returns OK if settable, !OK if not.
 */
int set_baud_exten(int exten) {
  static stsextctl xc;
  int stat;
  
  /* attempt to fetch current extended params */
  stat = ioctl(tty_fd,STSGETEXTCTL,&xc);
  if(stat < 0)
    return(!OK);			/* must be another serial driver */
  
  xc.x_flags &= ~XC_BAUDEXT;
  xc.x_flags |= (exten << XC_BAUDSHIFT);
  
  /* set new params */
  stat = ioctl(tty_fd,STSSETEXTCTL,&xc);
  if(stat < 0)
    return(!OK);
  
  return(OK);
}

/*
 *	set_modem()
 *	
 *	Sets modem control state based on 'mdmsigs' global var.
 */
void set_modem() {
  int mdm = 0;
  int stat;
  
  if(mdmsigs & DINC_DTR)
    mdm |= MDTR;
  if(mdmsigs & DINC_RTS)
    mdm |= MRTS;
  stat = ioctl(tty_fd, MCSETA, &mdm);
  if(stat < 0) {
    perror("set_modem: MCSETA failed");
    bail(1);
  }
}


/*
 *	get_modem
 *
 *	Hardware-specific call to fetch modem status.  Translates to
 *	DINC_xxx modem indications.
 */
int get_modem() {
  int stat;
  int raw_mdm,mdm;
  
  stat = ioctl(tty_fd, MCGETA, &raw_mdm);
  if(stat < 0) {
    perror("show_modem: MCGETA failed");
    bail(1);
  }
  mdm = 0;
  if(raw_mdm & MDCD) mdm |= DINC_CAR;
  if(raw_mdm & MDTR) mdm |= DINC_DTR;
  if(raw_mdm & MRTS) mdm |= DINC_RTS;
  if(raw_mdm & MCTS) mdm |= DINC_CTS;
  if(raw_mdm & MDSR) mdm |= DINC_DSR;
  return(mdm);
}
#endif /* HP */


#if defined(IRIX5)

extern struct termios tty_termios;


int is_hwfc() {	return(tty_termios.c_cflag & CNEW_RTSCTS); }

/*
 *	set_hwfc()
 */
void set_hwfc() {
	tty_termios.c_cflag |= CNEW_RTSCTS;
}

/*
 *	clr_hwfc()
 */
void clr_hwfc() {
	tty_termios.c_cflag &= ~CNEW_RTSCTS;
}


#if !defined(USE_STS_EXT_BAUDS)
int set_baud_exten(int exten) {
    return(!OK);
}
#else
/*
 *	set_baud_exten()
 *
 *	Returns OK if settable, !OK if not.
 */
int set_baud_exten(int exten) {
  static struct strioctl strreq;
  static stsextctl xc;
  
  /* get current state */
  strreq.ic_cmd    = STSGETEXTCTL;
  strreq.ic_timout = 0;
  strreq.ic_len    = sizeof(xc);
  strreq.ic_dp     = (char *) &xc;
  if(ioctl(tty_fd, I_STR, &strreq) < 0) {
    printf("GETEXTCTL failed\n");
    return(!OK);
  }
  
  xc.x_flags &= ~XC_BAUDEXT;
  xc.x_flags |= (exten << XC_BAUDSHIFT);
  
  /* set new state */
  strreq.ic_cmd    = STSSETEXTCTL;
  strreq.ic_timout = 0;
  strreq.ic_len    = sizeof(xc);
  strreq.ic_dp     = (char *) &xc;
  if(ioctl(tty_fd, I_STR, &strreq) < 0) {
    perror("STSSETEXTCTL");
    printf("STSSETEXTCTL failed\n");
    return(!OK);
  }
  return(OK);
}
#endif

/*
 *	set_modem()
 *
 *	Sets modem control state based on 'mdmsigs' global var.
 *  Internal ports on SGI don't seem to like TIOCMSET for some
 *  reason (?), so instead of bailing, we'll just continue on
 *  here.  I tried TIOMBIS/MBIC, not setting the flags, passing
 *  by reference rather than by value -- none of these worked.
 *  Interestingly enough, TIOCMGET works just fine.
 *
 */
void set_modem() {
  int mdm;
  int stat;
  
  /* same story as Linux? -rpr */
  if(ioctl(tty_fd, TIOCMGET, &mdm) < 0)
    bail(1);
  mdm &= ~(TIOCM_DTR|TIOCM_RTS);
 
  /** the bits to set **/

  if(mdmsigs & DINC_DTR)
    mdm |= TIOCM_DTR;
  if(mdmsigs & DINC_RTS)
    mdm |= TIOCM_RTS;

  stat = ioctl(tty_fd, TIOCMSET, mdm);
  if(stat < 0) {
    perror("set_modem: TIOCMSET failed");
    write_string(STDERR_FILENO,
		   "\nContinuing...\n\r");
  }
  return;
}


/*
 *	get_modem()
 *
 *	Hardware-specific call to fetch modem status.  Translates to
 *	DINC_xxx modem indications.
 */
int get_modem() {
  int stat,mdm;
  static int raw_mdm;
  
  stat = ioctl(tty_fd, TIOCMGET, &raw_mdm);
  if(stat < 0) {
    perror("get_modem failed");
    bail(1);
  }
  mdm = 0;
  if(raw_mdm & TIOCM_CAR) mdm |= DINC_CAR;
  if(raw_mdm & TIOCM_DTR) mdm |= DINC_DTR;
  if(raw_mdm & TIOCM_RTS) mdm |= DINC_RTS;
  if(raw_mdm & TIOCM_CTS) mdm |= DINC_CTS;
  if(raw_mdm & TIOCM_DSR) mdm |= DINC_DSR;
  return(mdm);
}
#endif /* IRIX5 */



#if defined(AIX4)

int is_hwfc() { return(FALSE); }

/*
 *	set_hwfc()
 *
 *	Makes proprietary ioctl to set hardware flow control.
 */
void set_hwfc() {  }

/*
 *	clr_hwfc()
 *	
 *	Makes proprietary ioctl to set hardware flow control.
 */
void clr_hwfc() {  }


/*
 *	set_baud_exten()
 *
 *	Returns OK if settable, !OK if not.
 */
int set_baud_exten(int exten) {
  return(!OK);
}

/*
 *	set_modem()
 *	
 *	Sets modem control state based on 'mdmsigs' global var.
 *      We now get the current bitset and modify it for Linux's native ports.
 */
void set_modem() {
  int stat;
  int mdm;

  if(ioctl(tty_fd, TIOCMGET, &mdm) < 0)
    bail(1);
  mdm &= ~(TIOCM_DTR|TIOCM_RTS);
  if(mdmsigs & DINC_DTR)
    mdm |= TIOCM_DTR;
  if(mdmsigs & DINC_RTS)
    mdm |= TIOCM_RTS;
  stat = ioctl(tty_fd, TIOCMSET, &mdm);
  if(stat < 0) {
    perror("set_modem: TIOCMSET failed");
  }
}


/*
 *	get_modem()
 *
 *	Hardware-specific call to fetch modem status.  Translates to
 *	DINC_xxx modem indications.
 */
int get_modem() {
  int stat;
  int raw_mdm,mdm;
  
  stat = ioctl(tty_fd, TIOCMGET, &raw_mdm);
  if(stat < 0) {
    perror("show_modem: TIOCMGET failed");
    bail(1);
  }
  mdm = 0;
  if(raw_mdm & TIOCM_CAR) mdm |= DINC_CAR;
  if(raw_mdm & TIOCM_DTR) mdm |= DINC_DTR;
  if(raw_mdm & TIOCM_RTS) mdm |= DINC_RTS;
  if(raw_mdm & TIOCM_CTS) mdm |= DINC_CTS;
  if(raw_mdm & TIOCM_DSR) mdm |= DINC_DSR;
  return(mdm);
}
#endif /* AIX4 */

