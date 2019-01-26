/*
 *	DINC.C
 *
 *	Direct INstant Connect
 *
 *	Opens a serial device and let's you immediately dial out
 *	or do things with it.  No files to edit.  Also supports
 *	extended baud rates.
 *
 *	Copyright (c) 1995-2000 Digi International Inc., All Rights Reserved
 *	Distribute freely but give credit where credit is due.
 *
 *	Original Author: Dennis Cronin
 *	Maintained by: Jeff Randall (randall@cd.com)
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "dinc.h"

static char dinc_sccs[] = "@(#) $Id: dinc.c,v 1.29 2010/08/06 13:50:34 sethb Exp $ Copyright (C) 1995-1998 Digi International Inc., All Rights Reserved";


#if !defined(MAX)
# define MAX(x,y)		((x) > (y) ? (x) : (y))
#endif


/* io_buf */
struct io_buf {
	int		 cnt;
	int		 index;
	char	 d[BSZ];
};

/*
 *	This program freely uses global vars - I don't intend for 
 *	this utility to get very big so it shouldn't be too tricky
 *	to maintain, globals and all.
 */
char		 	*progname;
int				 tty_fd;
char			 tty_name[80];
int				 baudrate = 9600;
int				 baudexten = 0;
int				 parity = 0;
int				 csize = 8;
int				 cstopb = 0;
int				 mdmsigs = DINC_DTR | DINC_RTS;
int				 hwfc = FALSE;
int				 swfc = TRUE;
int				 baud_exten_supported = FALSE;
struct termios	 tty_termios,user_save_termios,user_termios;
int				 user_setup_saved = FALSE;
int				 init_port = TRUE;
int				 local_echo = FALSE;
int				 add_lf_in = FALSE;
int				 add_lf_out = FALSE;
int				 add_cr_in = FALSE;
int				 add_cr_out = FALSE;
int				 strip_cr_in = FALSE;
int				 strip_cr_out = FALSE;
int				 strip_lf_in = FALSE;
int				 strip_lf_out = FALSE;
struct io_buf	 t_in, t_out, u_in, u_out;
char			 lock_file[40];
char			 tmp_file[40];
char       *log_file_name;
int        logfd = -1;


#if defined(SOLARIS) && defined(SOLARIS25)
#define FAST_TERMIOS
#endif

#if defined(LINUX) || defined(FREEBSD)
#define FAST_TERMIOS
#define MAX_BAUD_RATE 460800
#endif

#if defined(SCO6)
#define B57600		0x00A0000F
#define B76800		0x00C0000F
#define B115200		0x00E0000F
#define B230400		0x0200000F
#define B460800		0x0400000F
#define B921600		0x0600000F
#define MAX_BAUD_RATE	460800
#endif

#if defined(TRU64)
#define FAST_TERMIOS
#define MAX_BAUD_RATE 460800
#endif

/* 
 * Unix baudrate codes and associated information
 */
struct {
    int          rate;
    int          code;
    int          ext;
    char        *name;
} static        baudtab[] = {


#ifdef FAST_TERMIOS

# ifndef B76800
#  define B76800 76800
# endif

# ifndef B153600
#  define B153600 153600
# endif

# ifndef B230400
#  define B230400 230400
# endif

# ifndef B307200
#  define B307200 307200
# endif

# ifndef B460800
#  define B460800 460800
# endif

# define MAX_BAUD_RATE 460800

/*
 * termios directly supports faster bit-rates without having to
 * use proprietary "baud extensions" and the like.  Currently,
 * Solaris 2.5 and Irix 6.3/6.4 support this.
 */
/*  rate        code        ext     name */
    50,         B50,        0,      "50",
    75,         B75,        0,      "75",
    110,        B110,       0,      "110",
    134,        B134,       0,      "134.5",
    150,        B150,       0,      "150",
    200,        B200,       0,      "200",
    300,        B300,       0,      "300",
    600,        B600,       0,      "600",
    1200,       B1200,      0,      "1200",
    1800,       B1800,      0,      "1800",
    2400,       B2400,      0,      "2400",
    4800,       B4800,      0,      "4800",
    9600,       B9600,      0,      "9600",
    19200,      B19200,     0,      "19200",
    38400,      B38400,     0,      "38400",
    57600,      B57600,     0,      "57600",
    76800,      B76800,     0,      "76800",
    115200,     B115200,    0,      "115200",
    153600,     B153600,    0,      "153600",
    230400,     B230400,    0,      "230400",
    307200,     B307200,    0,      "307200",
    460800,     B460800,    0,      "460800",
		 0, 	0,			0,		"0"
# define NBAUDS  21

#else

# define MAX_BAUD_RATE 57600

    50,         B50,        0,      "50",
    75,         B75,        0,      "75",
    110,        B110,       0,      "110",
    134,        B134,       0,      "134.5",
    150,        B150,       0,      "150",
    200,        B200,       0,      "200",
    300,        B300,       0,      "300",
    600,        B600,       0,      "600",
# if defined(HP)
    900,        _B900,      0,      "900",
# endif
    1200,       B1200,      0,      "1200",
    1800,       B1800,      0,      "1800",
    2400,       B2400,      0,      "2400",
# if defined(HP)
    3600,       _B3600,     0,      "3600",
# endif
    4800,       B4800,      0,      "4800",
# if defined(HP)
    7200,       _B7200,     0,      "7200",
# endif
    9600,       B9600,      0,      "9600",
    19200,      B19200,     0,      "19200",
    38400,      B38400,     0,      "38400",
    57600,      B38400,     1,      "57600",
    76800,      B38400,     2,      "76800",
    115200,     B38400,     3,      "115200",
    153600,     B38400,     4,      "153600",
    230400,     B38400,     5,      "230400",
    307200,     B38400,     6,      "307200",
    460800,     B38400,     7,      "460800",
		 0, 	0,			0,		"0"
# if defined(HP)
#  define NBAUDS  24
# else
#  define NBAUDS  21
# endif
#endif
};


/* protos */
#if defined(__STDC__)
void	 loop(void);
int move(char *dst, char *src, int n, int add_cr, int add_lf, int strip_cr, int strip_lf);
void	 proc_user_input(void);
void	 proc_user_char(int c);
int		 proc_tilde_cmd(int c);
void	 tout_putc(int c);
void	 open_tty(void);
void	 tty_open_timeout(int arg);
void	 lock_tty(void);
void	 unlock_tty(void);
int		 read_lock_proc_id(char *name);
void	 create_tmp_proc_id();
int		 set_tty_params(void);
void	 show_tty_params(void);
void	 underscore(int c);
void	 save_user_params(void);
void	 set_user_params(void);
void	 restore_user_params(void);
void	 show_modem(void);
void	 show_info(void);
void	 sign_on(void);
void	 write_string(int fd, char *s);
void	 bail(int code);
void	 usage(void);
#else
void	 loop();
int	 move();
void	 proc_user_input();
void	 proc_user_char();
int		 proc_tilde_cmd();
void	 tout_putc();
void	 open_tty();
void	 tty_open_timeout();
void	 lock_tty();
void	 unlock_tty();
int		 read_lock_proc_id();
void	 create_tmp_proc_id();
int		 set_tty_params();
void	 show_tty_params();
void	 underscore();
void	 save_user_params();
void	 set_user_params();
void	 restore_user_params();
void	 show_modem();
void	 show_info();
void	 sign_on();
void	 write_string();
void	 bail();
void	 usage();
#endif


/*
 *	main
 */
int main(int argc, char **argv) {
	extern int	 optind;
	extern char	*optarg;
	int			 c;
	int			 stat;
	int			 i;
	int			 speed;
  
	progname = "dinc";
  
	if(argc < 2)
		usage();
  
	/* process options */
	while((c = getopt(argc,argv,"hs125678ENOiecrlfCRLFo:")) != EOF) {
		switch(c) {
		case 'h':				/* hardware flow control */
			hwfc = TRUE;
			break;
			
		case 's':				/* software flow control _off_ */
			swfc = FALSE;
			break;
			
		case '1':				/* one stop bit */
			cstopb = FALSE;
			break;
			
		case '2':				/* two stop bits */
			cstopb = TRUE;
			break;
			
		case '5':				/* 5-bit chars */
			csize = 5;
			break;

		case '6':				/* 6-bit chars */
			csize = 6;
			break;
				
		case '7':				/* 7 bit chararacters */
			csize = 7;
			break;
			
		case '8':				/* 8 bit characters */
			break;
			
		case 'E':				/* even parity */
			parity = 2;
			break;
			
		case 'N':				/* no parity */
			parity = 0;	
			break;
			
		case 'O':				/* off parity */
			parity = 1;
			break;
			
		case 'i':				/* don't init the port */
			init_port = FALSE;
			break;
		case 'e':
			local_echo = !local_echo;
			break;
		case 'l':   /* Add LF to CR on incoming. */
			add_lf_in = !add_lf_in;
			break;
		case 'L':   /* Add LF to CR on outgoing. */
			add_lf_out = !add_lf_out;
			break;
		case 'c':   /* Add CR to LF on incoming. */
			add_cr_in = !add_cr_in;
			break;
		case 'C':   /* Add CR to LF on outgoing. */
			add_cr_out = !add_cr_out;
			break;
		case 'r':   /* Strip CR on CRLF on incoming. */
			strip_cr_in = !strip_cr_in;
			break;
		case 'R':   /* Strip CR on CRLF on outgoing. */
			strip_cr_out = !strip_cr_out;
			break;
		case 'f':   /* Strip LF on CRLF on incoming. */
			strip_lf_in = !strip_lf_in;
			break;
		case 'F':   /* Strip LF on CRLF on outgoing. */
			strip_lf_out = !strip_lf_out;
			break;
		case 'o':  /* Set a log file for IO */
      log_file_name = optarg;
			break;
			
		case '?':
			usage();
		}
	}
	
	/* examine rest of args expecting port and possibly baud rate */
	for( ; optind != argc; optind++) {
		/* if first char is numeric, assume it's a baud rate */
		if(isdigit(argv[optind][0]))
			baudrate = atoi(argv[optind]);
		/* otherwise assume it's the port */
		else
			strcpy(tty_name, argv[optind]);
	}
	
	/* make sure we got a port spec */
	if(tty_name[0] == 0) {
		printf("No port specified.\n");
		usage();
	}

  /* Open the log file */
  if (log_file_name) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    logfd = open(log_file_name, O_APPEND|O_CREAT|O_RDWR, mode);
    if (logfd == -1) {
      printf("Error trying to open log file %s", log_file_name);
      perror(strerror(errno));
      exit(1);
    }
  }

	/* open the TTY port */
	open_tty();
	
	/* see if we can gain ownership */
	lock_tty();

	if(set_baud_exten(0) == OK)
		baud_exten_supported = TRUE;
	
	if(init_port == TRUE) {
		/* setup  line characteristics at tty port */
		if(set_tty_params() != OK) 
			usage();
		
		/* make sure modem signals are active */
		set_modem();
		
	}
	else {
		stat = tcgetattr(tty_fd,&tty_termios);		/* find current speed */
		if(stat < 0) {
			perror("set_tty_params: tcgetattr");
			bail(1);
		}

		baudrate=0;
		speed=cfgetospeed(&tty_termios);
		for(i=0; baudtab[i].code != 0; i++) {
			if(baudtab[i].code == speed) {
				baudrate = baudtab[i].rate;
				break;
			}
		}
		
		if(tty_termios.c_iflag & (IXON | IXOFF)) 
			swfc=TRUE;
		else
			swfc=FALSE;
		
		if(is_hwfc()) 
			hwfc=TRUE;
		else
			hwfc=FALSE;
		
	}
	
	/* setup line characteristics at user port */
	save_user_params();
	set_user_params();
	
	/* sign on */
	sign_on();
	
	/* start main loop */
	loop();
	/* never returns */
	return(0);			/* prevent -Wall from complaining 'tho */
}

/*
 *	loop()
 */
void loop() {
	struct timeval	 tv;
	fd_set			 rfdv, wfdv;
	int				 stat;
	int				 max_fd;
	
	/* catch obvious signals */
	signal(SIGINT, bail);
	signal(SIGHUP, bail);
	signal(SIGTERM, bail);
	signal(SIGALRM, bail);

	max_fd = MAX(STDIN_FILENO, 
				 MAX(STDOUT_FILENO,
					 MAX(STDERR_FILENO, tty_fd))) + 1;
	
	/* main loop */
	while(1) {
		/* see what we're doing this time */
		FD_ZERO(&wfdv);						/* clear */
		FD_ZERO(&rfdv);						/* clear */
		FD_SET(STDIN_FILENO,&rfdv);			/* always read stdin */
		if(t_in.cnt == 0)
			FD_SET(tty_fd,&rfdv);			/* read TTY */
		if(u_out.cnt != 0)
			FD_SET(STDOUT_FILENO,&wfdv);	/* write stdout */
		if(t_out.cnt != 0)
			FD_SET(tty_fd,&wfdv);			/* write TTY */
		
		tv.tv_sec = 1;						/* one sec poll for life */
		tv.tv_usec = 0;						/* reset each time for Linux */
  
		/* watch for activity */
		stat = select(max_fd,&rfdv,&wfdv,NULL, &tv);
		if(stat < 0) {
			perror("select failed");
			bail(1);
		}
		
		/* see if we just timed out */
		if(stat == 0) {
			/* poll modem status just as a way to see if we're still online */
			(void) get_modem();
			continue;
		}
		
		/* check user input */
		if(FD_ISSET(STDIN_FILENO,&rfdv)) {
			stat = read(STDIN_FILENO,u_in.d,ISZ);
			if(stat < 0) {
				perror("read from user");
				bail(1);
			}
			/* show we have user chars */
			u_in.cnt = stat;
			u_in.index = 0;
		}
		
		/* check user output */
		if(FD_ISSET(STDOUT_FILENO,&wfdv)) {
			stat = write(STDOUT_FILENO, &u_out.d[u_out.index], u_out.cnt - u_out.index);
      if (logfd > -1) {
        write(logfd,  &u_out.d[u_out.index], u_out.cnt - u_out.index);
      }
			if(stat < 0) {
				perror("write to user output failed");
				bail(1);
			}
			
			/* update buf for amount written */
			u_out.index += stat;
			
			/* see if we emptied the buf */
			if(u_out.index == u_out.cnt) 
				u_out.index = u_out.cnt = 0;
		}
		
		/* check TTY input */
		if(FD_ISSET(tty_fd,&rfdv)) {
			stat = read(tty_fd, t_in.d, ISZ);
			if(stat < 0) 
				bail(1);
			/*
			 * if we read 0 in spite of select saying we got chars,
			 * we're probably offline.
			 */
			if(stat == 0) 
				bail(1);
			
			/* show we have chars to process */
			t_in.cnt = stat;
			t_in.index = 0;
		}
		
		/* check TTY out */
		if(FD_ISSET(tty_fd, &wfdv)) {
			stat = write(tty_fd, &t_out.d[t_out.index],
						 t_out.cnt - t_out.index);
			
			/* an error usually would mean the line hung up */
			if(stat < 0) 
				bail(1);
			
			/* show we moved some chars */
			t_out.index += stat;
			if(t_out.index == t_out.cnt)
				t_out.index = t_out.cnt = 0;
		}
		
		/* try to move data between in and out buf pairs */
		if(t_in.cnt != 0 && u_out.cnt == 0) {
			u_out.cnt = move(u_out.d, t_in.d, t_in.cnt, add_cr_in, add_lf_in, strip_cr_in, strip_lf_in);
			t_in.cnt = 0;
		}
		proc_user_input();
	}
}

/*
 *	move()
 *	
 *	Saves having to keep track of bcopy vs. memcpy.
 *	Also handles CRLF translations.
 */
int move(char *dst, char *src, int n, int add_cr, int add_lf, int strip_cr, int strip_lf) {
    int found_cr = FALSE;
    int found_lf = FALSE;
    int num = 0;
	while(n--)
	{
	    found_cr = (*src == '\r');
	    found_lf = (*src == '\n');
	    if (strip_cr && found_cr)
	    {
		/* Strip all CRs */
		src++;
		continue;
	    }
	    if (strip_lf && found_lf)
	    {
		/* Strip all LFs */
		src++;
		continue;
	    }
	    if (found_lf && add_cr)
	    {
		/* Add CR before LF. */
		*dst++ = '\r';
		++num;
	    }
	    *dst++ = *src++;
	    ++num;
	    if (found_cr && add_lf)
	    {
		/* Add LF after CR. */
		*dst++ = '\n';
		++num;
	    }
	}
	return num;
}

/*
 *	proc_user_input()
 *
 *	Mostly just echo thru to t_out buf, but watch for special
 *	command sequences.
 */
void proc_user_input() {
	int c;
	
	/* make sure we have something to do */
	if(u_in.cnt == 0)
		return;
	
	/* copy to TTY out buf checking for special commands */
	while(u_in.index != u_in.cnt) {
		c = u_in.d[u_in.index++];
		proc_user_char(c);
	}
	u_in.index = u_in.cnt = 0;
}

/*
 *	proc_user_char()
 *	
 *	Handle tilde commands.  Regular chars just get moved to out buf.
 */
void proc_user_char(int c) {
	static int	 state = UC_CR;
	
	switch(state) {
	case UC_IDLE:							/* no special state */
		/* see what char we got */
		switch(c) {
		case '\r':
		case '\n':
			state = UC_CR;
			tout_putc(c);
			break;
		default:
			tout_putc(c);
			break;
		}
		break;
	case UC_CR:								/* we'e gotten a CR or NL */
		/* see what char we got */
		switch(c) {
		case '\r':
		case '\n':
			state = UC_CR;
			tout_putc(c);
			break;
		case '~':
			state = UC_TILDE;
			break;							/* don't echo it yet */
		default:
			state = UC_IDLE;
			tout_putc(c);
			break;
		}
		break;
	case UC_TILDE:			    			/* we're looking for special cmd */
		/* see what char we got */
		switch(c) {
		case '\r':
		case '\n':
			state = UC_CR;
			tout_putc('~');
			tout_putc(c);
			break;
		case '~':
			tout_putc('~');
			/* note: stay in tilde state */
			break;
		default:
			proc_tilde_cmd(c);
			state = UC_CR;
			break;
		}
		break;
	}
}

/*
 *	proc_tilde_cmd()
 *
 *	Handle tilde commands.  Returns OK if valid command handled.
 *	!OK if bad command.
 */
int proc_tilde_cmd(int c) {
	int stat;
	
	static char *cmds = "\r\nDINC tilde cmds:\n\r (+-)Baud Csize Dtr Framing Hwfc Info breaK Modem Oflush Parity Rts Swfc eXit\n\r";
	
	switch(tolower(c)) {
	case '.':							/* exit */
	case 'q':
	case 'x':
		bail(0);
		break;
	case 'p':							/* select parity mode */
		parity = (parity + 1) % 3;
		set_tty_params();
		show_tty_params();
		underscore(c);
		break;
	case 'c':							/* select character size */
		csize++;						/* rotates 5-6-7-8 */
		if (csize > 8)
			csize = 5;
		set_tty_params();
		show_tty_params();
		underscore(c);
		break;
	case 'b':							/* select next baud rate */
	case '+':
	case '-':
	case '_':
	case '=':
		while(next_baud(c == '-' || c == '_' ? -1 : 1) != OK)
			;
		show_tty_params();
		underscore(c);
		break;
	case 'f':							/* select framing */
		cstopb = !cstopb;
		set_tty_params();
		show_tty_params();
		underscore(c);
		break;
	case 'h':						     /* toggle hardware flow control */
		hwfc = !hwfc;
		set_tty_params();
		show_tty_params();
		underscore(c);
		break;
	case 's':
		swfc = !swfc;
		set_tty_params();
		show_tty_params();
		underscore(c);
		break;
	case 'd':							/* toggle DTR */
		mdmsigs ^= DINC_DTR;
		set_modem();
		show_modem();
		break;
	case 'r':							/* toggle RTS */
		mdmsigs ^= DINC_RTS;
		set_modem();
		show_modem();
		break;
	case 'm':							/* show current modem state */
		show_modem();
		break;
	case 'i':							/* general info */
		show_info();
		break;
	case 'k':							/* send break */
#if defined(OPENBSD) || defined(FREEBSD)
		stat = ioctl(tty_fd,TIOCSBRK,0);
		if(stat < 0) 
			perror("TIOCSBRK");
#else
		stat=tcsendbreak(tty_fd, 0);
		if (stat<0)
		  perror("tcsendbreak");
#endif
		break;
		
	case 'o':							/* output flush */
		/* clear our own little output buffer */
		t_out.index = t_out.cnt = 0;
		
		/* clear out down thru line discipline, etc. */
		stat = tcflush(tty_fd,TCOFLUSH);
		if(stat < 0)
			perror("TCOFLUSH failed");
		else {
			t_out.cnt = t_out.index = 0;
			write_string(STDOUT_FILENO,"OUTPUT FLUSHED\n\r");
		}
		break;
	default:
		write_string(STDOUT_FILENO,cmds);
		return(!OK);
	}
	return(OK);
}

/*
 *	tout_putc()
 *
 *	Put a character in the output buffer for TTY.
 */
void tout_putc(int c) {
	/* Add LF overrides if both set. */
	if (add_cr_out && !add_lf_out && c == '\n')
	    tout_putc('\r');
	if(t_out.cnt < BSZ)
		t_out.d[t_out.cnt++] = c;
	if (local_echo)
	{
	    char ch = c;
	    write(STDOUT_FILENO, &ch, 1);
	}
	if (add_lf_out && c == '\r')
	    tout_putc('\n');
}

/*
 *	open_tty()
 *	
 *	Opens the TTY port non-blocking style.  Runs a watchdog timer
 *	in case another process is blocking for carrier preventing us
 *	from proceeding with open.
 *
 *	Exits on failure.
 */
void open_tty() {
	/* set signal handler for open watchdog */
	signal(SIGALRM, tty_open_timeout);
	
	/* set watchdog timer */
	alarm(10);
	
	/* open the tty port */
	tty_fd = open(tty_name,O_RDWR | O_NDELAY);
	if(tty_fd < 0) {
		perror(tty_name);
        alarm(0);			/* be pendantic about clearing the timer */
		exit(1);
	}
	
	/* clear the timer */
	alarm(0);
	
	/* go back to default alarm handler */
	signal(SIGALRM, SIG_DFL);
	
	/* restore normal blocking operation on port */
	fcntl(tty_fd, F_SETFL, 0);
}

/*
 *	tty_open_timeout()
 *
 *	Just prints a message saying the device is busy.
 */
void tty_open_timeout(int arg) {
	/* note: only passing 'arg' to printif to shut up compiler */
	printf("Timed out: port busy or another process is blocked in open.\n",
		   arg);
	exit(1);
}

/*
 *	lock_tty()
 *	
 *	Does uucp-style locking protocol to keep multiple dialouts
 *	from accessing the same port.  Simply exits if errors encountered
 *	or if port is busy.
 */
void lock_tty() {
#if defined(NO_UULOCKING)
	return;
#else
	
	int		 pid;
	char 	*p;
	
	/* build appropriate name for lockfile */
	
#if defined(SOLARIS)
	/* do SVR4 flavor */
	struct stat s;
	
	/* fetch info on device */
	if(stat(tty_name, &s) < 0) {
		perror(tty_name);
		fprintf(stderr,"Couldn't stat device node.\n");
		exit(1);
	}
	
	/* form full file spec */
	sprintf(lock_file,"%s/LK.%3.3lu.%3.3lu.%3.3lu",
			LOCK_DIR,
			major(s.st_dev),
			major(s.st_rdev),
			minor(s.st_rdev));
#else
	/* do BSD */
	strcpy(lock_file,LOCK_DIR);
	strcat(lock_file,"/LCK..");
	
	/* strip basename off */
	if((p = strrchr(tty_name,'/')) == NULL)
		strcpy(lock_file,tty_name);
	else 
		strcat(lock_file, p + 1);	
#endif
	
	/*
	 * now create a temp file and attempt to atomically link the
     * lock name to our temp file.
     */
	while(create_tmp_proc_id(), link(tmp_file, lock_file) < 0) {
		
		/* clean up the temp file */
		unlink(tmp_file);
		
		/* see if lockfile already in existance */
		if(errno != EEXIST) {
			perror(lock_file);
			fprintf(stderr,"Couldn't link lockfile name.\n");
			exit(1);
		}
		
		/* lockfile already exists, find which process owns it */
		pid = read_lock_proc_id(lock_file);
		
		/* check for suddenly ceased to exist */
		if(pid < 0) 
			continue;
		
		/* check for reasonable process ID */
		if(pid < 5) {
			fprintf(stderr,"Line locked with dubious format lockfile '%s'.\n",
					lock_file);
			exit(1);
		}
		
		/* see if live process exists w/ that ID */
		if(kill(pid, 0) == 0) {
			fprintf(stderr,"%s: already in use.\n", tty_name);
			exit(1);
		}
		
		/*
		 *  either process _just_ finished or we have orphan lock file.
		 *  see if lockfile still exists w/ same pid in it.
		 */
		if(read_lock_proc_id(lock_file) != pid) 
			continue;
		
		/*
		 *  we assume now it's an orphan and we remove it.  there are
		 *  all sorts of possible race conditions here, but we'll
		 *  assume this is an infrequent event and take the easy
		 *  way.  plus, there's no clearly defined protocol that
		 *  uucp uses, so why bother being clever?
		 */
		if(unlink(lock_file) < 0) {
			perror(lock_file);
			fprintf(stderr,"Couldn't remove possibly stagnant lockfile '%s'.\n",
					lock_file);
			exit(1);
		}
		
		/* now we try again */
	}
	
	/* made it to here, we have a lockfile with proper name */
	
	/* clean temp and we're done */
	unlink(tmp_file);
#endif	/* !NO_UULOCKING */
}

/*
 *	unlock_tty()
 *
 *	Simply removes our lock file if we've made one (and not already
 *	removed it).
 */
void unlock_tty() {
	if(lock_file[0] != 0) {
		unlink(lock_file);
		lock_file[0] = 0;
	}
}

/*
 *	read_lock_proc_id()
 *	
 *	Attempts to read a 10 digit process ID from the spec'd lockfile.
 *
 *	Returns:
 *		>= 0 valid process ID
 *		-1   general error accessing file
 *	
 *	Exits if we don't have permission to read the file.
 */
int read_lock_proc_id(char *name) {
	int		 lfd,ret;
	char	 proc_id[12];
	
	/* try to open the lockfile */
	lfd = open(name, O_RDONLY);
	if(lfd < 0) {
		if(errno == EACCES || errno == EPERM) {
			fprintf(stderr,"No read access to existing lockfile '%s'.\n",
					name);
			exit(1);
		}
		return(-1);
	}
	
	/* try to read a process ID */
	ret = read(lfd, proc_id, sizeof(proc_id) - 1);
	if(ret < 0)
		return(-1);
	close(lfd);
	
	/* null terminate proc ID string */
	proc_id[ret] = 0;
	
	/* and return the numberic value */
	return(atoi(proc_id));
}


#if !defined(NO_UULOCKING)
/*
 *	create_tmp_proc_id()
 *
 *	Attempts to create a temp file with our process ID in it.
 *	Exits if error encountered.
 */
void create_tmp_proc_id() {
	char	 proc_id[12];
	int		 tmp_fd;
	
	/* now create a temp file */
	sprintf(tmp_file,"%s/DINCTMP%d", LOCK_DIR, getpid());
	tmp_fd = creat(tmp_file, 0444);
	if(tmp_fd < 0) {
		perror(tmp_file);
		fprintf(stderr,"Couldn't create temp file.\n");
		exit(1);
	}
	
	/* write our process ID to the file */
	sprintf(proc_id,"%10d\n",getpid());
	if(write(tmp_fd, proc_id, strlen(proc_id)) < (int) strlen(proc_id)) {
		perror(tmp_file);
		fprintf(stderr,"Write failed on temp file.\n");
		unlink(tmp_file);
		exit(1);
	}
	close(tmp_fd);
}
#endif /* !NO_UULOCKING */

/*
 *	set_tty_params()
 *
 *	Set TTY line params based on values in global vars.  If we fail
 *	due to lack of baud extension support, just return !OK.  If
 *	we fail doing normal setup, print error and exit.  If all goes
 *	well, return OK.
 */
int set_tty_params() {
	int		 stat,i;
	int		 code,ext;
	
	/* lookup baud code and baud extension factor */
	for(i = 0; baudtab[i].rate; i++) {
		if(baudrate == baudtab[i].rate) {
			code = baudtab[i].code;
			ext = baudtab[i].ext;
			break;
		}
	}
	
	/* make sure we found a match */
	if(baudtab[i].rate == 0) {
		printf("Can't set baudrate %d\n", baudrate);
		return(!OK);
	}
	
	/* see if extended baud rates supported */
	if(baud_exten_supported == TRUE) {
		stat = set_baud_exten(ext);
		if(stat != OK) 
			return(!OK);
	}
	
	/* get the regular part of the attributes */
	stat = tcgetattr(tty_fd,&tty_termios);
	if(stat < 0) {
		perror("set_tty_params: tcgetattr");
		bail(1);
	}
	
	/* set line up for raw mode operation */
	tty_termios.c_lflag = 0;
	tty_termios.c_oflag &= ~OPOST;
#ifdef ONLCR
	tty_termios.c_oflag &= ~ONLCR;
#endif
#if !defined(FREEBSD)
	tty_termios.c_iflag &= 
		~(INPCK | PARMRK | BRKINT | INLCR | ICRNL | IUCLC | IXANY );
#else
	tty_termios.c_iflag &= 
		~(INPCK | PARMRK | BRKINT | INLCR | ICRNL | IXANY );
#endif
	tty_termios.c_iflag |= IGNBRK;
	tty_termios.c_cflag &= ~(CSIZE | PARODD | PARENB | CSTOPB);
	tty_termios.c_cflag |= (CREAD | CLOCAL | HUPCL);
	
	/* set character size */
	switch (csize) {
	case 8:
		tty_termios.c_cflag |= CS8;
		break;
	case 7:
		tty_termios.c_cflag |= CS7;
		break;
	case 6:
		tty_termios.c_cflag |= CS6;
		break;
	case 5:
		tty_termios.c_cflag |= CS5;
		break;
	}
	
	/* set parity mode */
	if(parity == 1) {
		tty_termios.c_cflag |= (PARODD | PARENB);
		tty_termios.c_iflag |= INPCK;
	}
	else if(parity == 2) {
		tty_termios.c_cflag |= PARENB;
		tty_termios.c_iflag |= INPCK;
	}
	
	/* set software flow control mode */
	if(swfc)
		tty_termios.c_iflag |= (IXON | IXOFF);
	else
		tty_termios.c_iflag &= ~(IXON | IXOFF);
	
	/* set hardware flow control mode */
	if(hwfc)
		set_hwfc();
	else
		clr_hwfc();
	
	/* set framing */
	if(cstopb)
		tty_termios.c_cflag |= CSTOPB;
	
	/* set baud rate */
#ifdef SCO6
	if (baudrate < 57600) {
		cfsetospeed(&tty_termios, code);
		cfsetispeed(&tty_termios, code);
	}
	else {
		tty_termios.c_cflag |= code;
	}
#else
	cfsetospeed(&tty_termios,code);
	cfsetispeed(&tty_termios,code);
#endif
	/* set for non-blocking behavior */
	tty_termios.c_cc[VMIN] = 
		tty_termios.c_cc[VTIME] = 1;
	
	/* set the new attributes */
	stat = tcsetattr(tty_fd, TCSANOW, &tty_termios);
	if(stat < 0) {
		if(errno == EINVAL)
			return(!OK);
		else {
			perror("tcsetattr");
			bail(1);
		}
	}
	return(OK);
}

/*
 *	show_tty_params()
 */
void show_tty_params() {
	static char		*parnames[3] = {"NONE","ODD ","EVEN"};
	char			 tbuf[80];
	
	sprintf(tbuf,"\r%6d BAUD %d %s %s SWFC=%s HWFC=%s           \r\n",
			baudrate,
			csize,
			parnames[parity],
			cstopb ? "2" : "1",
			swfc ? "ON " : "OFF",
			hwfc ? "ON " : "OFF");
	write_string(STDOUT_FILENO,tbuf);
}

/*
 *	underscore()
 *	
 *	Used to underscores most recently changed TTY param.
 */
void underscore(int c) {
	int		 len,i,b;
	int		 lead,mark;
	char	 tbuf[80];
	
	switch(tolower(c)) {
	case 'b':					/* underscore the baud rate */
	case '-':
	case '+':
	case '_':
	case '=':
		/* find length of baud number */
		for(b = baudrate, len = 0; b != 0; b /= 10, len++)
			;
		lead = 6 - len;
		mark = len + 5;
		break;
		
	case 'c':					/* underscore the char size */
		lead = 11;
		mark = 3;
		break;
		
	case 'f':					/* underscore framing */
		lead = 18;
		mark = 3;
		break;
		
	case 'p':					/* underscore parity */
		lead = 14;
		mark = parity == 1 ? 3 : 4;
		break;
		
	case 's':
		lead = 21;
		mark = swfc ? 7 : 8;
		break;
		
	case 'h':
		lead = 30;
		mark = hwfc ? 7 : 8;
		break;
		
	default:
		printf("oops in underscore\n");
		bail(1);
		break;
	}
	
	for(i = 0; i < lead; i++)
		tbuf[i] = ' ';
	for(b = 0; b < mark; b++, i++)
		tbuf[i] = '^';
	tbuf[i++] = '\n';
	tbuf[i++] = '\r';
	write(STDOUT_FILENO,tbuf,i);
}

/*
 *	next_baud()
 *
 *	Steps to next baud rate. Arg is either 1 or -1 to denote
 *	which way we're stepping.
 *	
 *	Returns OK if able to set, else
 *	returns !OK.
 */
int next_baud(int dir) {
	int i;
	
	for(i = 0; baudtab[i].rate; i++)
		if(baudrate == baudtab[i].rate)
			break;
	i += dir;
	if(i < 0) {
		i = NBAUDS;
	}
	if(baudtab[i].rate == 0)
		i = 0;

	if(baudtab[i].rate == MAX_BAUD_RATE && !baud_exten_supported)
		i = 0;
	
	baudrate = baudtab[i].rate;
	return(set_tty_params());
}

/*
 *	save_user_params()
 *	
 *	Fetch current user terminal params and stash 'em away.
 */
void save_user_params() {
	int stat;
	
	stat = tcgetattr(STDOUT_FILENO,&user_save_termios);
	if(stat < 0) {
		perror("save_user_params: tcgetattr");
		bail(1);
	}
	user_setup_saved = TRUE;
}

/*
 *	set_user_params()
 *
 *	Set raw mode at user terminal.
 */
void set_user_params() {
	int stat;
	
	stat = tcgetattr(STDOUT_FILENO,&user_termios);
	if(stat < 0) {
		perror("set_user_params: tcgetattr");
		bail(1);
	}
	
	/* set line up for raw mode operation */
	user_termios.c_lflag = 0;
	user_termios.c_oflag &= ~OPOST;
	user_termios.c_iflag &= ~(ICRNL | INLCR);
	user_termios.c_cc[VMIN] = 
		user_termios.c_cc[VTIME] = 1;
	
	/* set the new attributes */
	stat = tcsetattr(STDOUT_FILENO, TCSANOW, &user_termios);
	if(stat < 0) {
		perror("set_user_params: tcsetattr");
		bail(1);
	}
}

/*
 *	restore_user_params()
 *
 *	Put original user terminal params back in effect.
 */
void restore_user_params() {
	int stat;
	
	stat = tcsetattr(STDOUT_FILENO, TCSANOW, &user_save_termios);
	if(stat < 0) {
		perror("restore_user_params: tcsetattr");
		user_setup_saved = FALSE;
		bail(1);
	}
}

/*
 *	show_modem()
 *	
 *	Fetches and prints state of modem signals.
 */
void show_modem() {
	int			 m;
	char		 tbuf[160];
	static int	 initd;
	static int	 last_mdm;
	int			 mdm_delta;
	
	mdmsigs = get_modem();
	
	sprintf(tbuf,"\rCAR=%s DTR=%s RTS=%s CTS=%s DSR=%s         \r\n",
			mdmsigs & DINC_CAR ? "ON " : "OFF",
			mdmsigs & DINC_DTR ? "ON " : "OFF",
			mdmsigs & DINC_RTS ? "ON " : "OFF",
			mdmsigs & DINC_CTS ? "ON " : "OFF",
			mdmsigs & DINC_DSR ? "ON " : "OFF");
	
	/* if first call, don't show delta info */
	if(!initd) {
		initd = TRUE;
		last_mdm = mdmsigs;
	}
	/* hilight signals that have changed */
	else {
		mdm_delta = mdmsigs ^ last_mdm;
		last_mdm = mdmsigs;
		/* underscore changed signals */
		for(m = DINC_CAR; m != 0; m >>= 1) {
			if(mdm_delta & m) {				/* changed? */
				if(mdmsigs & m)
					strcat(tbuf,"^^^^^^  ");			/* underscore "ON" */
				else
					strcat(tbuf,"^^^^^^^ ");			/* underscore "OFF" */
			}
			else
				strcat(tbuf,"        ");			/* no underscore */
		}
		strcat(tbuf,"        \n\r");
	}
	
	/* write finished modem status display */
	write_string(STDOUT_FILENO,tbuf);
}

/*
 *	show_info()
 *
 *	Also the sign_on banner.
 */
void show_info() {
	char	 tbuf[80];
	char 	*dashes = "----------------------------------------";
	int		 dcnt;
	
	write_string(STDOUT_FILENO,"\n\r");
	sprintf(tbuf," DINC --- port=%s ",tty_name);
	if((int) strlen(tbuf) < 40) {
		dcnt = (40 - (int) strlen(tbuf)) / 2;
		write(STDOUT_FILENO,dashes,dcnt);
		write_string(STDOUT_FILENO,tbuf);
		write(STDOUT_FILENO,dashes,dcnt);
		write(STDOUT_FILENO,"\007",1);
	}
	else
		write_string(STDOUT_FILENO,tbuf);
	write_string(STDOUT_FILENO,"\n\r");
	
	show_tty_params();
	show_modem();
}

/*
 *	sign_on()
 */
void sign_on()
{
	show_info();
	write_string(STDOUT_FILENO,"Type ~? for help.\n\r");
}

/*
 *	write_string()
 *	
 *	Just a call to 'write' but expects a string instead buf buf/leng.
 */
void write_string(int fd, char *s) {
	write(fd,s,strlen(s));
  if (logfd > -1) {
  	write(logfd,s,strlen(s));
  }
}

/*
 *	bail()
 *
 *	Exit after error.  Attempt to restore user terminal params first.
 */
void bail(int code) {
	static int	 called;
	
	if(called)					/* prevent recursive fails */
		exit(1);
	
	called = TRUE;
  if (logfd != -1) {
    close(logfd);
  }
	
	if(init_port == TRUE) {
		/* clear baud extension factor if set */
		if(baud_exten_supported == TRUE) {
			set_baud_exten(0);
		}
	}
	
	/* if we locked the TTY, release it */
	unlock_tty();
	
	/* if we changed user terminal params, restore */
	if(user_setup_saved)  {
		restore_user_params();
    char *closing = "DINC\007 closing...\n";
		printf(closing);
    if (logfd > -1) {
      write(logfd, closing, strlen(closing));
    }
	}
	
	if(init_port == TRUE) { 
		/* drop modem signals */
		if(tty_fd > 0) {
			tcflush(tty_fd, TCOFLUSH);
			mdmsigs &= ~(DINC_DTR | DINC_RTS);
			set_modem();
		}
	}
	
	exit(code);
}


/*
 *	usage()
 */
void usage() {
	printf("usage: dinc [[-125678ENOhsiecrlfCRLF] [[baudrate]] [port]\n");
	printf("  -1           one stop bit\n");
	printf("  -2           two stop bits\n");
	printf("  -5           5 bit characters\n");
	printf("  -6           6 bit characters\n");
	printf("  -7           7 bit characters\n");
	printf("  -8           8 bit characters\n");
	printf("  -E           even parity\n");
	printf("  -N           no parity\n");
	printf("  -O           odd parity\n");
	printf("  -h           enable hardware flow control mode\n");
	printf("  -s           disable software flow control\n");
	printf("  -i           do not init port on start or reset port on exit\n");
	printf("  -e           toggle local echo of characters.\n");
	printf("  -c           toggle add CR on LF on incoming chars.\n");
	printf("  -r           toggle strip CR on incoming chars.\n");
	printf("  -l           toggle add LF on CR on incoming chars.\n");
	printf("  -f           toggle strip LF on incoming chars.\n");
	printf("  -C           toggle add CR on LF on outgoing chars.\n");
	printf("  -R           toggle strip CR on outgoing chars.\n");
	printf("  -L           toggle add LF on CR on outgoing chars.\n");
	printf("  -F           toggle strip LF on outgoing chars.\n");
	printf("  -o           Log all IO to log file.\n");
	bail(1);
}
