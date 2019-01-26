/*
 *  dgipserv - a utility program with several functions:
 *    1. an alternative to DHCP or BOOTP for assigning IP addresses to
 *    Etherlite units.
 *    2. Reset Etherlites which have become "stuck".
 *    3. Download firmware to Etherlite units.
 *
 *  Authored by Emilio Millan, February 1998, but with this caveat:
 *    Much of the code below was in some way derived from the
 *    source of the CMU bootpd, Berkeley's TFTP server, Mark
 *    Schank's TFTP server, and cdelsreset.
 *  Also, I couldn'ta done it with help from Mark Schank.
 * Broadcast bootp stuff added by Bob rubendunst at Mark's repeated suggestion.
 * 
 * Copyright (c) 1990-2001 Digi International Inc., All Rights Reserved
 *
 * This software contains proprietary and confidential information of Digi
 * International Inc.  By accepting transfer of this copy, Recipient agrees
 * to retain this software in confidence, to prevent disclosure to others,
 * and to make no use of this software other than that for which it was
 * delivered.  This is an unpublished copyrighted work of Digi International
 * Inc.  Except as permitted by federal law, 17 USC 117, copying is strictly
 * prohibited.
 *
 * Restricted Rights Legend
 *
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in sub-paragraph (c)(1)(ii) of The Rights in
 * Technical Data and Computer Software clause at DFARS 252.227-7031 or
 * subparagraphs (c)(1) and (2) of the Commercial Computer Software -
 * Restricted Rights at 48 CFR 52.227-19, as applicable.
 *
 * Digi International Inc. 11001 Bren Road East, Minnetonka, MN 55343
 *
 *****************************************************************************/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			/* for malloc() */
#include <string.h>
#include <errno.h>          /* for errno */
#include <time.h>           /* for time(), difftime() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>		/* for socket() */
#include <netdb.h>			/* for getservbyname() */
#include <sys/time.h>		/* for select() */
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_ntoa() */
#include <arpa/tftp.h>		/* for struct tftphdr and various constants */
#include <sys/param.h>		/* for MAXHOSTNAMELEN on Irix */
#include "fas.h"

#if defined(AIX4)
#include <net/if.h>		/* for struct ifnet */
#include <sys/select.h>		/* for struct fd_set */
#endif

/* For Solaris X86, duh... */
/*
#if !defined(AIX4) && !defined(HPUX) && !defined(Linux)
*/
#if defined(Solaris)
extern int gethostname(char *name, int namelen);
#endif

/* the 6th arg of recvfrom  is variant across O/Ses */
/* avoid using long as its size changes from 32 to 64 bits on HPUX */
#if defined(SCO)
#define bootp_int32         int
#else
#define bootp_int32         unsigned int
#endif

typedef unsigned char HWADDR[6];
typedef bootp_int32 IPADDR;

/* this is the standard bootp request/reply. Since the 1st bootp message
   comes from the Etherlite, many of these fields are not initialized by
   the program, but by the Etherlite. 

   The Etherlite encapsulates its vendor specific commands using the
   op code 43 (see rfc1533). The vendor specific commands are:
   1 4  auth host address
   2 8  auth host/netmask dual quad
   3 1 subcommand
   3 1 1 store the IP
   3 1 2 erase the IP

   As of Firmware version 7.9, the "gateway" and subnet mask are also stored.
   These are regular BOOTP vendor-specific options, not 43 encoded really
   vendor specific opcodes as above.

   1 4 quad netmask
   3 4 quad router (we call this gateway)
*/

/* Vendor Specific options defined by us. */
#define VS_HOST_IP           1
#define VS_HOST_MASK         2
#define VS_SAVE              3
#define VS_STORED_IP         4
#define VS_PROD_ID           5
#define VS_VP_NUM_PORTS      6
#define VS_VP_TIMEOUT        7
#define VS_OPTIMIZE          8

#define VS_SAVE_STORE        1
#define VS_SAVE_ERASE        2

#define VP_MAX_NUM_PORTS     16
#define VP_MAX_TIMEOUT_DECISEC  600

#define OPTIMIZE_THROUGHPUT  0
#define OPTIMIZE_LATENCY     1

#define BP_CHADDR_LEN       16
#define BP_SNAME_LEN        64
#define BP_FILE_LEN        128
#define BP_VEND_LEN         64
#define BP_MINPKTSZ        300
struct bootp {
  unsigned char    bp_op;                     /* packet opcode type */
  unsigned char    bp_htype;                  /* hardware addr type 1=enet*/
  unsigned char    bp_hlen;                   /* hardware addr length 6 */
  unsigned char    bp_hops;                   /* gateway hops */
  bootp_int32   bp_xid;                    /* transaction ID */
  unsigned short   bp_secs;                   /* seconds since boot began */
  unsigned short   bp_flags;                  /* RFC1532 broadcast, etc. */
  struct in_addr   bp_ciaddr;                 /* client IP address */
  struct in_addr   bp_yiaddr;                 /* 'your' IP address */
  struct in_addr   bp_siaddr;                 /* server IP address */
  struct in_addr   bp_giaddr;                 /* gateway IP address */
  unsigned char    bp_chaddr[BP_CHADDR_LEN];  /* client hardware address */
  char             bp_sname[BP_SNAME_LEN];    /* server host name */
  char             bp_file[BP_FILE_LEN];      /* 128 byteboot file name */
  unsigned char    bp_vend[BP_VEND_LEN];      /* 64 byte vendor-specific area */
  /* note that bp_vend can be longer, extending to end of packet. */
};
/* struct bp_op values */
#define BOOTREPLY		     2
#define BOOTREQUEST		     1

#define IPPORT_BOOTPS 67  /* port to which bootp binds receive requests */
#define IPPORT_BOOTPC 68  /* the port at which responses are received */
#define IPPORT_TFTP   69  /* trivial FTP, (for firmware update) */

#define MAX_MSG_SIZE    (3*512) /* Maximum packet size */
#define MAXHADDRLEN          6  /* not really, but for our purposes it'll do */
#define MAX_EL_REQUEST_GAP  60  /* seconds */

#define RP_TCP_PORT     771
#define ELS_TCP_PORT    10001
#define FASCMDSZ        8


typedef enum { ALL, FAS, NETCX } RESET_TYPE;

RESET_TYPE reset_type = ALL;


#define MAX_AUTH_HOSTS  10
#define NEW_BP_VEND_LEN (MAX_AUTH_HOSTS*10+15+1)
#define GEN_OPTIONS_MAX 12

/* Central Data base ethernet address */
HWADDR CD_base_addr = { 0x00, 0xA0, 0xE7, 0x00, 0x00, 0x00 };

/*-------------------------------------------------------------------*/
/* function prototypes */

int str2hwaddr(char *s, HWADDR h);
char *hwaddr2str(HWADDR h);
char *inet_ultoa(bootp_int32 trintaeduas);
char hwaddrs_equal(HWADDR h1, HWADDR h2);
void setarp(int s, struct in_addr *ia, u_char *ha, int len);
void dumpdata(FILE *fp, unsigned char *data, int len);
int reset_elmodule(char *ipaddr, int rp_port);
int reset_rp_elmodule(char *ipaddr, int rp_port);
void usage_and_exit(char **argv);
int exeunt(int code);

/*-------------------------------------------------------------------*/
void perror_stdout(char *s) {
  printf("%s: %s\n", s, strerror(errno));
}

#define perror perror_stdout

char *AIX_str_ip = NULL;   /* IP address used in setarp() for AIX */
char *pktbuf = NULL;       /* receive packet buffer */

/*-------------------------------------------------------------------*/
int main(int argc, char *argv[]) 
{
  int sock_bootp;                /* our socket for BOOTP */
  int sock_tftp;                 /* our socket for TFTP */
  struct servent *servp;         /* used to get values for the next three
				    variables */
  int bootps_port;               /* the port for BOOTP requests */
  int bootpc_port;               /* the port for BOOTP responses */
  int tftp_port;                 /* the port for TFTP requests */
  int rp_port = RP_TCP_PORT;     /* the port for RealPort requests */
  struct sockaddr_in bind_addr;  /* used to bind to the BOOTPS port */
  struct sockaddr_in recv_addr;  /* the address from which we received
				    the BOOTP request */
  struct sockaddr_in tftp_addr;
  int n;                         /* size of the packet we transact */
  struct bootp *bp;              /* pktbuf, casted */
  int pktlen;                    /* size of the packet we send back */
  char *str_hwaddr=NULL;         /* hardware address from the command line */
  char *str_ipaddr=NULL;         /* IP address from the command line */
  HWADDR hwaddr;                 /* str_hwaddr, parsed */
  HWADDR reply_hwaddr;           /* hw addr to reply to */
  IPADDR ipaddr;                 /* str_ipaddr, parsed */
  IPADDR siaddr;                 /* tftp server ip address */
  int vpports, vptimeout;        /* number of virtual ports, and virtual port timeout */
  int optimize;                  /* optimize flag, latency==1, throughput==0 */
  unsigned char vendor[NEW_BP_VEND_LEN];
  /* vendor-specific options */
  int vendor_len=0;              /* length of the above */
  unsigned char  gen_options[GEN_OPTIONS_MAX];
  int  gen_len = 0;			/* length of not 43 options */
  int storing=0, erasing=0, updating=0, resetting=0, broadcasting=0;
  int got_tftp=0, got_netmask=0, got_gateway=0;
  int got_vpports=0, got_vptimeout=0, got_optimize=0;
  /* optional program behaviors */
  int num_auth_hosts=0;          /* number of times the -host
				    flag is used */
  int j;                         /* general-purpose integer :-) */
  char *fw_filename;             /* name of firmware file to be uploaded
				    to the module */
  time_t start, now;             /* stuff used to decide when to give
				    a "don't give up" message and when
				    to give a "give up" message */
  int gave_dontgiveup=0, gave_giveup=0;
  int opt_base = 0;	
			/* array idx for options merge */
  /*
   * Try to grok the command-line arguments.
   */
  for (j=1; j<argc; j++) {
    if (j==(argc-2)) {str_hwaddr=argv[j]; continue;}
    if (j==(argc-1)) {str_ipaddr=argv[j]; AIX_str_ip=argv[j]; continue;}
    
    /*
     * -reset 
     */
    if (strncmp("-r", argv[j], 2)==0) {
      resetting=1;
#if defined(PRINT_ARG_PARSE)
      printf("Will reset the module.\n");
#endif
      continue;
    }

    /*
     * -store
     */
    if (strncmp("-s", argv[j], 2)==0) {
      if (erasing || broadcasting || storing) {
	printf("Choose only one: broadcast, store, or erase.\n\n");
	usage_and_exit(argv);
      }
      
      vendor[vendor_len++]=VS_SAVE;
      vendor[vendor_len++]=1;
      vendor[vendor_len++]=VS_SAVE_STORE;
      storing=1;
#if defined(PRINT_ARG_PARSE)
      printf("Will store the IP address.\n");
#endif
      continue;
    }
    
    /*
     * -broadcast
     */
    if (strncmp("-b", argv[j], 2)==0) {
      if (erasing || broadcasting || storing) {
	printf("Choose only one: broadcast, store, or erase.\n\n");
	usage_and_exit(argv);
      }
      
      vendor[vendor_len++]=VS_SAVE;
      vendor[vendor_len++]=1;
      vendor[vendor_len++]=VS_SAVE_STORE;
      broadcasting=1;
#if defined(PRINT_ARG_PARSE)
      printf("Will store the IP address using a bootp broadcast.\n");
#endif
      continue;
    }
    /*
     * -erase
     */
    if (strncmp("-e", argv[j], 2)==0) {
      if (erasing || broadcasting || storing) {
	printf("Choose only one: broadcast, store, or erase.\n\n");
	usage_and_exit(argv);
      }
      
      vendor[vendor_len++]=VS_SAVE;
      vendor[vendor_len++]=1;
      vendor[vendor_len++]=VS_SAVE_ERASE;
      erasing=1;
#if defined(PRINT_ARG_PARSE)
      printf("Will erase the stored IP address.\n");
#endif
      continue;
    }

    /*
     * -tftp tftp (really the "server" option of bootp.)
     */
    if (strncmp("-t", argv[j], 2)==0) {
      
      if (got_tftp) {
	printf("You can only specify one tftp server address.\n\n");
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	/* Bad use of -tftp. */
	printf("-tftp requires an IP address\n\n");
	usage_and_exit(argv);
      }

      siaddr=inet_addr(argv[j+1]);

      if (siaddr==(-1)) {
	printf("Bad tftp server IP address: \"%s\"\n\n", argv[j+1]);
	usage_and_exit(argv);
      }

      got_tftp = 1;

      j++;
#if defined(PRINT_ARG_PARSE)
      printf("Adding tftp server address %s.\n", inet_ultoa(siaddr));
#endif
      continue;

    }

    /*
     * -gateway gateway (really the "router" option of bootp.)
     */
    if (strncmp("-g", argv[j], 2)==0) {
      
      if (got_gateway) {
	printf("You can only specify one gateway.\n\n");
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	/* Bad use of -gateway. */
	printf("-gateway requires an IP address\n\n");
	usage_and_exit(argv);
      }
      
      
      {		
	IPADDR unit_router;
	
	unit_router=inet_addr(argv[j+1]);
	if (unit_router==(-1)) {
	  printf("Bad netmask: \"%s\"\n\n", argv[j+1]);
	  usage_and_exit(argv);
	}
	
	if ((gen_len+6) > GEN_OPTIONS_MAX) {
	  printf("too many -g and -n options: \"%s\"\n\n", argv[j+1]);
	  usage_and_exit(argv);
	}
	
	got_gateway = 1;
	
	unit_router=htonl(unit_router);
	
	gen_options[gen_len++]=3;
	gen_options[gen_len++]=4;
	gen_options[gen_len++]=(unit_router&0xFF000000) >> 24;
	gen_options[gen_len++]=(unit_router&0x00FF0000) >> 16;
	gen_options[gen_len++]=(unit_router&0x0000FF00) >>  8;
	gen_options[gen_len++]=(unit_router&0x000000FF) >>  0;
	
	j++;
#if defined(PRINT_ARG_PARSE)
	printf("Adding gateway(router) address %s.\n",
	       inet_ultoa(unit_router));
#endif
	continue;
	
      }
    }
    
    /*
     * -netmask netmask
     */
    if (strncmp("-n", argv[j], 2)==0) {
      
      if (got_netmask) {
	printf("You can only specify one netmask.\n\n");
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	/* Bad use of -netmask. */
	printf("-netmask requires a netmask\n\n");
	usage_and_exit(argv);
      }
      
      {		
	IPADDR unit_netmask;
	
	unit_netmask=inet_addr(argv[j+1]);
	if (unit_netmask==(-1)) {
	  printf("Bad netmask: \"%s\"\n\n", argv[j+1]);
	  usage_and_exit(argv);
	}
	
	if ((gen_len+6) > GEN_OPTIONS_MAX) {
	  printf("too many -g and -n options: \"%s\"\n\n", argv[j+1]);
	  usage_and_exit(argv);
	}
	
	
	got_netmask=1;
	unit_netmask=htonl(unit_netmask);
	
	gen_options[gen_len++]=1;
	gen_options[gen_len++]=4;
	gen_options[gen_len++]=(unit_netmask&0xFF000000) >> 24;
	gen_options[gen_len++]=(unit_netmask&0x00FF0000) >> 16;
	gen_options[gen_len++]=(unit_netmask&0x0000FF00) >>  8;
	gen_options[gen_len++]=(unit_netmask&0x000000FF) >>  0;
	
	j++;
#if defined(PRINT_ARG_PARSE)
	printf("Adding netmask address %s.\n",
	       inet_ultoa(unit_netmask));
#endif
	continue;
	
      }
    }
    
    /*
     * -host ip_addr [ netmask ]
     */
    if (strncmp("-h", argv[j], 2)==0) {
      if (num_auth_hosts==MAX_AUTH_HOSTS) {
	printf("Only %d hosts may be declared authorized.\n", MAX_AUTH_HOSTS);
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	/* Bad use of -hosts. */
	printf("-hosts requires an IP address (and optional bitmask)\n\n");
	usage_and_exit(argv);
      }
      if (j==(argc-4) || argv[j+2][0]=='-') {
	/* This is a one-arg use of -hosts. */
	
	IPADDR auth_ip;
	
	auth_ip=inet_addr(argv[j+1]);
	if (auth_ip==(-1)) {
	  printf("Bad IP address: \"%s\"\n\n", argv[j+1]);
	  usage_and_exit(argv);
	}
	
	auth_ip=htonl(auth_ip);
	
	vendor[vendor_len++]=VS_HOST_IP;
	vendor[vendor_len++]=4;
	vendor[vendor_len++]=(auth_ip&0xFF000000) >> 24;
	vendor[vendor_len++]=(auth_ip&0x00FF0000) >> 16;
	vendor[vendor_len++]=(auth_ip&0x0000FF00) >>  8;
	vendor[vendor_len++]=(auth_ip&0x000000FF) >>  0;
	
	j++;
	num_auth_hosts++;
#if defined(PRINT_ARG_PARSE)
	printf("Adding IP address %s to the authorized host list.\n",
	       inet_ultoa(auth_ip));
#endif
	continue;
	
      } else {
	/* This is a two-arg use of -hosts. */
	
	IPADDR auth_ip, auth_netmask;
	
	auth_ip=inet_addr(argv[j+1]);
	if (auth_ip==(-1)) {
          printf("Bad IP address: \"%s\"\n\n", argv[j+1]);
          usage_and_exit(argv);
	}
	
	auth_ip=htonl(auth_ip);
	
	auth_netmask=inet_addr(argv[j+2]);
	if (auth_netmask==(-1)) {
          printf("Bad netmask: \"%s\"\n\n", argv[j+2]);
          usage_and_exit(argv);
	}
	
	auth_netmask=htonl(auth_netmask);
	
	vendor[vendor_len++]=VS_HOST_MASK;
	vendor[vendor_len++]=8;
	vendor[vendor_len++]=(auth_netmask&0xFF000000) >> 24;
	vendor[vendor_len++]=(auth_netmask&0x00FF0000) >> 16;
	vendor[vendor_len++]=(auth_netmask&0x0000FF00) >>  8;
	vendor[vendor_len++]=(auth_netmask&0x000000FF) >>  0;
	vendor[vendor_len++]=(auth_ip&0xFF000000) >> 24;
	vendor[vendor_len++]=(auth_ip&0x00FF0000) >> 16;
	vendor[vendor_len++]=(auth_ip&0x0000FF00) >>  8;
	vendor[vendor_len++]=(auth_ip&0x000000FF) >>  0;
	
#if defined(DEBUG)
	dumpdata(stdout, vendor, vendor_len);
#endif
	
	j+=2;
	num_auth_hosts++;
	
#if defined(PRINT_ARG_PARSE)
	printf("Adding IP address %s/", inet_ultoa(auth_ip));
	printf("netmask %s to the authorized host list.\n",
	       inet_ultoa(auth_netmask));
#endif
	continue;
      }
    } /* -hosts */
    
    /*
     * -file (or -firmware)
     */
    if (strncmp("-f", argv[j], 2)==0) {
      struct stat statbuf;
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	printf("-file (-firmware) requires the name of "
	       "the firmware file to be uploaded\n\n");
	usage_and_exit(argv);
      }
      if (updating) {
	printf("Only one file may be uploaded.\n\n");
	usage_and_exit(argv);
      }
      
      if (strlen(argv[j+1]) >= (BP_FILE_LEN-1)) {
	printf("Filename is too long: \"%s\"."
	       "Must be %d characters or fewer.\n\n",
	       argv[j+1], BP_FILE_LEN-1);
	usage_and_exit(argv);
      }
      
      if (stat(argv[j+1], &statbuf)<0) {
	printf("Can't open file \"%s\".\n", argv[j+1]);
	perror("stat");
	printf("\n");
	usage_and_exit(argv);
      }
      
      updating=1;
      fw_filename=argv[j+1];
      j++;
      
#if defined(PRINT_ARG_PARSE)
      printf("Will send file \"%s\" to the module.\n", fw_filename);
#endif
      continue;
    }

    /*
     * -port 
     */
    if (strncmp("-p", argv[j], 2)==0) {
      j++;
      rp_port=(int)strtol(argv[j],NULL,0);
      continue;
    }
    
    /*
     * -vpports N
     */
    if (strncmp("-vpp", argv[j], 4)==0) {
      
      if (got_vpports) {
	printf("You can only specify the number of virtual ports once.\n\n");
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	/* Bad use of -vpports. */
	printf("-vpports requires a number\n\n");
	usage_and_exit(argv);
      }

      vpports=atoi(argv[j+1]);

      if (vpports<0 || vpports>VP_MAX_NUM_PORTS) {
	printf("Number of virtual ports must be from 0 to %d\n\n", VP_MAX_NUM_PORTS);
	usage_and_exit(argv);
      }

      vendor[vendor_len++]=VS_VP_NUM_PORTS;
      vendor[vendor_len++]=2;
      vendor[vendor_len++]=(vpports&0xFF00) >> 8;
      vendor[vendor_len++]=(vpports&0x00FF);

      got_vpports = 1;

      j++;
#if defined(PRINT_ARG_PARSE)
      printf("Adding vpports %d.\n", vpports);
#endif
      continue;

    }

    /*
     * -vptimeout N
     */
    if (strncmp("-vpt", argv[j], 4)==0) {
      
      if (got_vptimeout) {
	printf("You can only specify one virtual port timeout.\n\n");
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-') {
	/* Bad use of -vptimeout. */
	printf("-vptimeout requires a number\n\n");
	usage_and_exit(argv);
      }

      vptimeout=atoi(argv[j+1]);

      if (vptimeout<0 || vptimeout>VP_MAX_TIMEOUT_DECISEC) {
	printf("Virtual port timeout must be from 0 to %d decisec\n\n",
	  VP_MAX_TIMEOUT_DECISEC);
	usage_and_exit(argv);
      }

      vendor[vendor_len++]=VS_VP_TIMEOUT;
      vendor[vendor_len++]=2;
      vendor[vendor_len++]=(vptimeout&0xFF00) >> 8;
      vendor[vendor_len++]=(vptimeout&0x00FF);

      got_vptimeout = 1;

      j++;
#if defined(PRINT_ARG_PARSE)
      printf("Adding vptimeout %d.\n", vptimeout);
#endif
      continue;

    }

    /*
     * -optimize latency/throughput
     */
    if (strncmp("-o", argv[j], 2)==0) {
      
      if (got_optimize) {
	printf("You can only set optimize once.\n\n");
	usage_and_exit(argv);
      }
      
      if (j==(argc-3) || argv[j+1][0]=='-'
      || (strncmp(argv[j+1],"latency",strlen(argv[j+1])) != 0
          && strncmp(argv[j+1],"throughput",strlen(argv[j+1])) != 0)) {
	/* Bad use of -optimize. */
	printf("-optimize requires 'latency' or 'throughput'\n\n");
	usage_and_exit(argv);
      }

      optimize
	= strncmp(argv[j+1],"latency",strlen(argv[j+1])) == 0 ? OPTIMIZE_LATENCY : OPTIMIZE_THROUGHPUT;

      vendor[vendor_len++]=VS_OPTIMIZE;
      vendor[vendor_len++]=2;
      vendor[vendor_len++]=(optimize&0xFF00) >> 8;
      vendor[vendor_len++]=(optimize&0x00FF);

      got_optimize = 1;

      j++;
#if defined(PRINT_ARG_PARSE)
      printf("Adding optimize %s.\n", optimize == OPTIMIZE_LATENCY ? "latency" : "throughtput");
#endif
      continue;

    }

    printf("Unrecognized argument: \"%s\"\n\n", argv[j]);
    usage_and_exit(argv);
  } /* looping through argv[] */
  
  /*
   * Got enough args?
   */
  if (str_hwaddr==NULL || str_ipaddr==NULL) {
    printf("Too few arguments.\n\n");
    usage_and_exit(argv);
  }
  
  /*
   * Make sure the HW address is kosher.
   */
  /* Replace dashes with colons--otherwise sscanf() pukes. */
  for (j=0; j<strlen(str_hwaddr); j++)
    if (str_hwaddr[j]=='-')
      str_hwaddr[j]=':';
  if (str2hwaddr(str_hwaddr, hwaddr)<0) {
    printf("Bad hardware address: \"%s\"\n", str_hwaddr);
    printf("A hardware address should be a six-byte value "
	   "(e.g., 00-A0-E7-00-03-63).\n\n");
    usage_and_exit(argv);
  }
  
  /*
   * Make sure the IP address is kosher.
   */
  ipaddr=inet_addr(str_ipaddr);
  if (ipaddr==(-1)) {
    printf("Bad IP address: \"%s\"\n", str_ipaddr);
    printf("An IP address should be a four-byte value "
	   "(e.g., 192.9.200.10).\n\n");
    usage_and_exit(argv);
  }
  
#if defined(PRINT_ARG_PARSE)
  printf("hardware address: %s\n", hwaddr2str(hwaddr));
  printf("IP address:       %s\n", inet_ultoa(ipaddr));
  printf("\n");
#endif
  /*
   * Get space for receiving packets and composing replies.
   */
  pktbuf = malloc(MAX_MSG_SIZE);
  if (!pktbuf) {
    perror("malloc");
    return(1);
  }
  bp = (struct bootp *) pktbuf;
  
  /*
   * Open up a socket.
   */
  if ((sock_bootp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exeunt(1);
  }
  
  /*
   * Get server's listening port number.
   */
  servp = getservbyname("bootps", "udp");
  if (servp) {
    bootps_port = ntohs((u_short) servp->s_port);
  } else {
    bootps_port = (u_short) IPPORT_BOOTPS;
    printf("(udp/bootps: unknown service--assuming port %d)\n",	bootps_port);
  }
  
  /*
   * Bind socket to BOOTPS port.
   */
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  bind_addr.sin_port = htons(bootps_port);
  if (bind(sock_bootp, (struct sockaddr *) &bind_addr, sizeof(bind_addr)) < 0) {
    perror("bind");
    if (errno==EADDRINUSE) {
      printf("Most likely there is a BOOTP server already running on "
	     "this system.\n");
    }
    if (errno==EACCES) {
      printf("You will need root access "
	     "to run this command on this system.\n");
    }
    exeunt(1);
  }
  
  /*
   * Get destination port number so we can reply to client.
   */
  servp = getservbyname("bootpc", "udp");
  if (servp) {
    bootpc_port = ntohs(servp->s_port);
  } else {
    bootpc_port = (u_short) IPPORT_BOOTPC;
    printf("(udp/bootpc: unknown service--assuming port %d)\n", bootpc_port);
  }
  
  if (updating) {
    /*
     * Get TFTP port.
     */
    servp = getservbyname("tftp", "udp");
    if (servp) {
      tftp_port = ntohs(servp->s_port);
    } else {
      tftp_port = (u_short) IPPORT_TFTP;
      printf("(udp/tftp: unknown service--assuming port %d)\n", tftp_port);
    }
    
    if ((sock_tftp=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("socket");
      exeunt(1);
    }
    
    tftp_addr.sin_family = AF_INET;
    tftp_addr.sin_addr.s_addr = INADDR_ANY;
    tftp_addr.sin_port = htons(tftp_port);
    if (bind(sock_tftp, (struct sockaddr *) &tftp_addr,
	     sizeof(struct sockaddr_in)) < 0) {
      perror("bind");
      if (errno==EADDRINUSE) {
	printf("Most likely there is a TFTP server already running on"
	       "this system.\n");
      }
      if (errno==EACCES) {
	printf("You will need root access "
	       "to run this command on this system.\n");
      }
      exeunt(1);
    }
  } /* if (updating) */
  
  
  if (resetting) {
    printf("Resetting the EtherLite module...\n");
    if (reset_elmodule(str_ipaddr,rp_port)<0)
      printf("Remote reset failed.  You will have to manually power cycle "
	     "the module.\n");
  }
  
  /*
   * Process incoming requests.
   */
  printf("Awaiting a BOOTP request from HW address %s...\n",
	 hwaddr2str(hwaddr));
  printf("(This may take up to a minute.)\n");
  
  start=time(NULL);
  for (;;) {
    fd_set readset;
    struct timeval timeout={5,0}; /* every 5 seconds */
#if defined(AIX4) && defined(socklen_t)
    socklen_t ra_len;
#elif defined(HPUX)
    int ra_len;
#else
    bootp_int32 ra_len;
#endif
    
    FD_ZERO(&readset);
    FD_SET(sock_bootp, &readset);
    
    /*
     * Time to emit some words of wisdom?
     */
    now=time(NULL);
    if (!gave_giveup && difftime(now, start) > 
	MAX_EL_REQUEST_GAP+timeout.tv_sec) {
      printf("If you are trying to serve an EtherLite module, \n"
	     "a BOOTP request should have been received by now.\n"
	     "A problem may exist.\n");
      gave_giveup=1;
    }
    
    if (!gave_dontgiveup && difftime(now, start)
	>= (MAX_EL_REQUEST_GAP/2)) {
      printf("Don't give up hope!\n");
      gave_dontgiveup=1;
    }
    
    /*
     * Wait for something to come in...
     */
    if (select(sock_bootp+1, &readset, NULL, NULL, &timeout)!=1) {
      printf("None yet...\n");
      continue;
    }
    
    /*
     * Something came in, so get it.
     */
    ra_len = sizeof(recv_addr);
    n = recvfrom(sock_bootp, pktbuf, MAX_MSG_SIZE, 0,
		 (struct sockaddr *)&recv_addr, &ra_len);
    
    /*
     * Check for bad things.
     */
    if (n <= 0) {
      /* Uh-oh. */
      perror("recvfrom");
      continue;
    }
    /* note PS 3 sends out runt-sized dhcp instead of bootp.
       Use (n < (sizeof(struct bootp)-BP_VEND_LEN to include PS 3 */
    if (n < sizeof(struct bootp)) {
      printf("(Ignoring a short packet.)");
      continue;
    }

    pktlen=n;
    
    if (bp->bp_op!=BOOTREQUEST) {
      /* printf("Not a BOOTREQUEST.\n"); */
      continue;
    }
    
    if (bp->bp_hlen!=6) {
      printf("Bad hardware-address length (%d)\n", bp->bp_hlen);
      continue;
    }
    
    /*
     * All looks good so far.  Make sure the request is for us.
     */
    if (!hwaddrs_equal(hwaddr, bp->bp_chaddr)) {
      /* check for an unassigned Ethernet address on an EtherLite */
      if( hwaddr[0]==bp->bp_chaddr[0] && hwaddr[1]==bp->bp_chaddr[1]
      && hwaddr[2]==bp->bp_chaddr[2]
      && hwaddrs_equal( bp->bp_chaddr, CD_base_addr ) ) {
	printf("(Setting EtherLite Ethernet address to %s.)\n",
	       hwaddr2str(hwaddr));
	memcpy(bp->bp_chaddr, hwaddr, sizeof(HWADDR));
	memcpy(reply_hwaddr, CD_base_addr, sizeof(HWADDR));
      }
      else {
	printf("(Ignoring a BOOTP request from %s.)\n",
	       hwaddr2str(bp->bp_chaddr));
	continue;
      }
    }
    else {
      memcpy(reply_hwaddr, hwaddr, sizeof(HWADDR));
    }
    
    /*
     * OK--let's rock and roll!
     */
    printf("Got one--responding...\n");
    break;
  }
  
#if defined(DEBUG)
  printf("recvd pkt from IP addr %s\n", inet_ntoa(recv_addr.sin_addr));
  printf("bp_hlen=%d\n", bp->bp_hlen);
  printf("addr=%02X.%02X.%02X.%02X.%02X.%02X\n",
	 reply_hwaddr[0], reply_hwaddr[1], reply_hwaddr[2],
	 reply_hwaddr[3], reply_hwaddr[4], reply_hwaddr[5]);
  printf("client, your,  server, & gateway addrs\n");
  dumpdata(stdout, (unsigned char *) &bp->bp_ciaddr.s_addr, sizeof(struct in_addr)*4);
  printf("Vendor area\n");
  dumpdata(stdout, bp->bp_vend, BP_VEND_LEN);
#endif
  /*
   *  Let's reply!
   */
  {
    struct in_addr dst;
    u_short port;
    struct sockaddr_in send_addr;
    
    /*
     * Turn this into a reply.
     */
    bp->bp_op=BOOTREPLY;
    
    /*
     * This is the address we're handing back.
     */
    (bp->bp_yiaddr).s_addr=ipaddr;
    
    if (bp->bp_ciaddr.s_addr) {
      /* If the request came it with an IP address, send the
       * response to that IP address.  With EtherLites, this'll
       * never happen.
       */
      dst = bp->bp_ciaddr;
      port = bootpc_port;
    } else if (bp->bp_giaddr.s_addr) {
      /*
       * Otherwise, if it came in via a gateway, send the response
       * to the gateway.
       */
      printf("(Responding via the gateway at %s...)\n",
	     inet_ntoa(bp->bp_giaddr));
      dst = bp->bp_giaddr;
      port = bootps_port;
    } else {
      unsigned char *ha;
      int len;
      
      /*
       * Otherwise, send it back using the hardware address we got
       * as the address by doing so ARP trickery.
       */
      dst = bp->bp_yiaddr;
      if (broadcasting)
	dst.s_addr = -1;			
      port = bootpc_port;
      
      ha = reply_hwaddr;
      len = bp->bp_hlen;
      if (len > MAXHADDRLEN)
	len = MAXHADDRLEN;
      setarp(sock_bootp, &dst, ha, len);
    }
    
    /* Set up socket address for send. */
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(port);
    send_addr.sin_addr = dst;
    
#if defined(DEBUG)
    printf("sending to %s dst=%s\n", hwaddr2str(reply_hwaddr),inet_ntoa(send_addr.sin_addr));
#endif
    if (connect(sock_bootp,
		(struct sockaddr *)&send_addr, sizeof(send_addr))<0) {
      perror("connect");
    }
    
    /*
     * Tack on the vendor-specific stuff. The unit
     generates the magic cookie of 99.130.83.99.

    PS 3 would require supplying DHCP options in its order, and would require
    another two packets to be exchanged.
    */
    opt_base = 4;
#if defined(DEBUG)
    printf("generic options len %d vendor_len %d\n", gen_len, vendor_len);
#endif
    if (vendor_len==0 && gen_len==0) {
      bp->bp_vend[opt_base++]=0xFF;
    } else {
      /* It would appear that Etherlite v7.9 firmware needs the T43 stuff
	 first. Otherwise, it ignores it. */ 
      /* Tack on the needed bytes after the magic cookie. */
      if (vendor_len) {
	bp->bp_vend[opt_base++]=43;	       /* vendor specific */
	bp->bp_vend[opt_base++]=vendor_len;   /* bytes that follow */
	memcpy(&bp->bp_vend[opt_base], vendor, vendor_len);
	opt_base += vendor_len;
      }
      if (gen_len) {
	memcpy(&bp->bp_vend[opt_base], gen_options, gen_len);
	opt_base += gen_len;
      }
      bp->bp_vend[opt_base++]=0xFF;
      pktlen=sizeof(struct bootp) /* struct size */
	- BP_VEND_LEN  /* less vendor area */
	+ opt_base;				/* plus what we used */
    }
    
#if defined(DEBUG)
    {
      printf("bootp dump\n");
      dumpdata(stdout, &bp->bp_op, 44); /* top thru client hw addr */
      dumpdata(stdout, bp->bp_vend, 64); /* top thru client hw addr */
      
    }
    
    printf("sizeof(struct bootp)=%d pktlen=%d\n",
	   sizeof(struct bootp),
	   pktlen);
#endif
    
    /*
     * Figure out our IP address.
     */
    if( got_tftp ) {
      (bp->bp_siaddr).s_addr=siaddr;
    } else
#define USE_HOSTNAME_TO_GET_IPADDR
#if defined(USE_HOSTNAME_TO_GET_IPADDR)
    {
      char hostname[MAXHOSTNAMELEN+1];
      struct hostent *he;
      struct in_addr my_ip_addr;
      
      if (gethostname(hostname, MAXHOSTNAMELEN)==0) {
	if ((he=gethostbyname(hostname))!=NULL) {
	  memcpy(&my_ip_addr, he->h_addr, sizeof(my_ip_addr));
	  
	  /* This is us. */
	  bp->bp_siaddr=my_ip_addr;
	}
      }
    }
#else
    {
      struct sockaddr_in sa;
      int sa_len;
      
      /* This *should* work, but... */
      
      sa_len=sizeof(sa);
      if (getsockname(sock_bootp, (struct sockaddr *)&sa, &sa_len)<0)
	perror("getsockname");
      else
	bp->bp_siaddr=sa.sin_addr;
      printf("%s\n", inet_ntoa(sa.sin_addr));
    }
#endif
    
    if (updating) {
      memcpy(bp->bp_file, fw_filename, strlen(fw_filename)+1);
    }
    
    /* Send reply with same size packet as request used. */
#if defined (DEBUG)
    dumpdata(stdout, (unsigned char *) pktbuf, 44); /* top thru client hw addr */
    dumpdata(stdout, (unsigned char *) pktbuf+300-64, 64); /* top thru client hw addr */
#endif
    if (broadcasting) {
      broadcasting=1;
      setsockopt(sock_bootp, SOL_SOCKET, SO_BROADCAST, &broadcasting, sizeof(int));
    }
    
    if ((n=send(sock_bootp, pktbuf, pktlen, 0)) != pktlen) {
      if (n < 0)
	perror("send");
      else
	printf("send only sent %d out of %d bytes.\n", n, pktlen);
      exeunt(1);
    }
    close(sock_bootp);
  }
  
  /*
   * Print bombastic warnings as necessary.
   */
  
  if (erasing) {
    printf(
	   "  +------------------------------------------------------------------+\n"
	   "  | If the EtherLite module did NOT have a stored IP address, the    |\n"
	   "  | \"on\" indicator will soon be steadily lit, and the module will    |\n"
	   "  | then be ready for use.                                           |\n"
	   "  |                                                                  |\n"
	   "  | If the EtherLite module DID have a stored IP address, then it    |\n"
	   "  | will now reprogram itself to erase that IP address.  This will   |\n"
	   "  | take about 10 seconds.  DO NOT DISRUPT POWER TO THE MODULE UNTIL |\n"
	   "  | THE \"ON\" INDICATOR HAS RESUMED RAPID FLASHING FOR A FULL 5     |\n"
	   "  | SECONDS.  Failure to do so could render the EtherLite module     |\n"
	   "  | inoperable.                                                      |\n"
	   "  |      Once reprogramming has finished, the EtherLite module will  |\n"
	   "  | await delivery of a new IP address.  You may use this program    |\n"
	   "  | to deliver this address.                                         |\n"
	   "  +------------------------------------------------------------------+\n");
  } else {
    if (storing || broadcasting) {
      printf(
	     "  +---------------------------------------------------------------------+\n"
	     "  | If the EtherLite module did not have a stored IP address, or if the |\n"
	     "  | address to be stored is different than the one currently stored,    |\n"
	     "  | the EtherLite module is now reprogramming itself.  This will take   |\n"
	     "  | about 10 seconds.  DO NOT DISRUPT POWER TO THE MODULE UNTIL THE     |\n"
	     "  | \"ON\" INDICATOR HAS REMAINED STEADILY LIT FOR A FULL 5 SECONDS.      |\n"
	     "  | Failure to do so could render the EtherLite module inoperable.      |\n"
	     "  | Once reprogramming has finished, the EtherLite module will be ready.|\n"
	     "  | (If store doesn't work, try using -broadcast.)                      |\n"
	     "  +---------------------------------------------------------------------+\n");
    }
    printf("Address successfully served.\n");
  }
  
  if (updating) {
    char pktbuf[sizeof(struct tftphdr)+SEGSIZE];
    int pktlen=sizeof(struct tftphdr)+SEGSIZE;
    struct tftphdr *t=(struct tftphdr *)pktbuf;
    int recv_cnt;
    struct sockaddr_in from;
#if defined(AIX4) && defined(socklen_t)
    socklen_t addrlen=sizeof(struct sockaddr_in);
#elif defined(HPUX)
    int addrlen=sizeof(struct sockaddr_in);
#else
    bootp_int32 addrlen=sizeof(struct sockaddr_in);
#endif
    int fd;
    int last_nread=999;
    
    printf("Awaiting a TFTP request...\n");
    
    for (;;) {
      fd_set readset;
      struct timeval timeout={17,0}; /* wait up to 17 seconds was 5, store takes 12 sec */
      
      FD_ZERO(&readset);
      FD_SET(sock_tftp, &readset);
      
      if (select(sock_tftp+1, &readset, NULL, NULL, &timeout) != 1) {
	printf("The module has been quiescent for %d seconds.\n"
	       "Assuming something is wrong...\n", (int)timeout.tv_sec);
	goto bail;
      }
      
      if ((recv_cnt=recvfrom(sock_tftp, pktbuf, pktlen, 0,
			     (struct sockaddr *)&from, &addrlen))<0) {
	perror("recvfrom");
	continue;
      }
      
      switch (ntohs(t->th_opcode)) {
      case RRQ:
	/*
	 *  This is a read request--the module is asking for its firmware.
	 */
	
#if defined(DEBUG)
	printf("The module wants file %s.\n", (char *)&t->th_block);
#endif
	
	/*
	 * Open up the file for sending.
	 */
	fd=open((char *)&t->th_block, O_RDONLY);
	if (fd<0) {
	  fprintf( stderr, "Can't open %s\n", (char *)&t->th_block );
	  perror("open");
	  goto bail;
	}
	
	/*
	 * Ready the first data packet to go out.
	 */
	t->th_opcode=htons(DATA);
	t->th_block=htons(1);       /* Yep, first one's numbered 1, not 0.
				       Guess who made that mistake. */
	if (read(fd, &t->th_data, SEGSIZE)<0) {
	  perror("read");
	  goto bail;
	}
	
	/* We use the same socket for receives as for sends. */
	if (connect(sock_tftp, (struct sockaddr *)&from, sizeof(from)) < 0) {
	  perror("connect");
	  exeunt(1);
	}
	
	printf("Starting to upload the firmware...\n");
	
	/* Send her off! */
	if (send(sock_tftp, pktbuf, SEGSIZE+4, 0) != SEGSIZE+4) {
	  perror("send");
	  goto bail;
	}
	/* Await the module's response. */
	break;
	
      case ACK: {
	/*
	 * The module firmware has acknowledged the data block we sent.
	 */
	int nread;
	
	/* Get the next hunk of data. */
	if (lseek(fd, ntohs(t->th_block)*SEGSIZE, SEEK_SET)<0)
	  goto bail;
	
	/* Read the packet to go. */
	t->th_opcode=htons(DATA);
        t->th_block=htons(ntohs(t->th_block)+1);
	nread=read(fd, &t->th_data, SEGSIZE);
	
        if (nread<0) {
          perror("read");
          goto bail;
        }
	
	/* Hold on--are we done? */
	if (nread==0 && last_nread==0) {
	  /* Yes--hooray! */
	  printf("Firmware successfully uploaded.\n");
	  goto bail;
	}
	last_nread=nread;
	
	/* Nope--packet away! */
        if (send(sock_tftp, pktbuf, nread+4, 0) != nread+4) {
          perror("send");
          goto bail;
        }
	break;
      }
      
      case ERROR:
	printf("The module reported a TFTP error.\n");
	goto bail;
	
      case WRQ:
      case DATA:
      default:
	printf("Received an unexpected TFTP request (%d).\n",
	       ntohs(t->th_opcode));
	goto bail;
	
      } /* switch (opcode) */
    } /* for loop */
  bail:
    close(sock_tftp);
  } /* if (updating) */
  
  
  return(0);  /* That's it! */
}

/*-------------------------------------------------------------------*/
int str2hwaddr(char *s, HWADDR h) 
{
  unsigned int ch[6];
  int i;
  
  if (sscanf(s, "%2x%2x%2x%2x%2x%2x",
	     &ch[0], &ch[1], &ch[2], &ch[3], &ch[4], &ch[5])==6)
    ;
  else if (sscanf(s, "%2x.%2x.%2x.%2x.%2x.%2x",
		  &ch[0], &ch[1], &ch[2], &ch[3], &ch[4], &ch[5])==6)
    ;
  else if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x",
		  &ch[0], &ch[1], &ch[2], &ch[3], &ch[4], &ch[5])==6)
    ;
  else
    return(-1);
  
  for (i=0; i<6; i++) {
    ((char *)h)[i]=ch[i];
  }
  
#if defined(DEBUG)
  printf("Address parsed as %02X:%02X:%02X:%02X:%02X:%02X\n",
	 ch[0], ch[1], ch[2], ch[3], ch[4], ch[5]);
#endif
  return(0);
}

/*-------------------------------------------------------------------*/
char *hwaddr2str(HWADDR h) 
{
  static char s[64];
  unsigned char *ch=h;
  
  sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X",
	  ch[0], ch[1], ch[2], ch[3], ch[4], ch[5]);
  return(s);
}

/*-------------------------------------------------------------------*/
char hwaddrs_equal(HWADDR h1, HWADDR h2) 
{
  unsigned char *ch1=h1, *ch2=h2;
  
  return(ch1[0]==ch2[0] && ch1[1]==ch2[1] &&
	 ch1[2]==ch2[2] && ch1[3]==ch2[3] &&
	 ch1[4]==ch2[4] && ch1[5]==ch2[5]);
}

/*-------------------------------------------------------------------*/
char *inet_ultoa(bootp_int32 ul) 
{
  struct in_addr a;
  
  a.s_addr=ul;
  return(inet_ntoa(a));
}

/*-------------------------------------------------------------------*/
#if defined(AIX4) || defined(HPUX) || defined(SCO) || defined(UW) || defined(Linux) || defined(TRU64)
# include <sys/ioctl.h>
#else
# include <sys/sockio.h>
#endif
#include <net/if_arp.h>
#define bcopy(s,d,l)    memcpy(d,s,l)
#define bzero(p,l)      memset(p,0,l)

/*
 * Setup the arp cache so that IP address 'ia' will be temporarily
 * bound to hardware address 'ha' of length 'len'.
 */
void setarp(int s, struct in_addr *ia, u_char *ha, int len) 
{
  
  struct arpreq arpreq;       /* Arp request ioctl block */
  struct sockaddr_in *si;
  
#if defined(DEBUG)
  printf("IP address is %s\n", inet_ntoa(*ia));
  /* For debugging ,consider adding the |ATF_PERM|ATF_PUBL flags below so the
     arp cache entry is permanent. See if it exists after dgipserv with
     arp -an. On HPUX, this yielded scrambled hw addressses because HPUX 11
     has a arp_hw_addr_len field, which other O/Ses lack. Setting it to
     6 gets the hw addresses working. 2/2001 rpr */
#endif
  
  bzero((caddr_t) & arpreq, sizeof(arpreq));
  arpreq.arp_flags = ATF_COM;
#if defined(HP11)
  arpreq.arp_hw_addr_len = 6;	/* HP needs this set or arp entry is toast */
#endif
  
  /* Set up the protocol address. */
  arpreq.arp_pa.sa_family = AF_INET;
  si = (struct sockaddr_in *) &arpreq.arp_pa;
  si->sin_addr = *ia;
  
  /* Set up the hardware address. */
  bcopy(ha, arpreq.arp_ha.sa_data, len);
#if defined(HPUX)
  arpreq.arp_ha.sa_family = AF_UNSPEC; /* HP arp(7p) man page demands this */
#else
  arpreq.arp_ha.sa_family = ARPHRD_ETHER;
#endif
  
#if defined(AIX4) || defined(SCO) || defined(UW)
  /* use command line service to set arp table, since ioctl(SIOCSARP) fails */
  { int rc;
  char sstring[256];
#if defined(SCO)
  char prefix[]="/etc";
  char target[]=" ";
  char suffix[] = " temp";
#endif
#if defined(UW)
  char prefix[]="/usr/sbin";
  char target[]=" ";
  char suffix[] = " temp";
#endif
#if defined(AIX)
  char prefix[]="/usr/sbin";
  char target[]="ether ";
  char suffix[]=" temp";
#endif
  
  printf("HW = %s\n", hwaddr2str(ha));
  printf("IP = %s \n", AIX_str_ip);
#if defined(SCO) || defined(UW)
  sprintf(sstring, "%s/arp -s %s%s %s%s", prefix, AIX_str_ip, target,
	  hwaddr2str(ha), suffix);
#else
  sprintf(sstring, "%s/arp -s %s%s %s%s", prefix, target, AIX_str_ip,
	  hwaddr2str(ha), suffix);
#endif
  
  rc = system(sstring);
  
#if defined(SCO) || defined(UW)
  if (rc == 0x100)
    rc = 0;					/* SCO system gives wacko results */
#endif
  
  if (rc) {
    perror("system(arp)");
    printf("cmd=%s rc = %#02x\n", sstring, rc);
    printf("update arp table failed\n");
  }
  /*
    arpreq.arp_pa.sa_len = 4;
    bcopy(ia, arpreq.arp_pa.sa_data, 4);
    
    arpreq.at_length = len;
  */ 
  }
#else /* AIX4 */
  
  /*
   * On SunOS, the ioctl sometimes returns ENXIO, and it
   * appears to happen when the ARP cache entry you tried
   * to add is already in the cache.  (Sigh...)
   * XXX - Should this error simply be ignored? -gwr
   */
  if (ioctl(s, SIOCSARP, (caddr_t) &arpreq) < 0) {
    perror("ioctl(SIOCSARP)");
    printf("NOTE: The operating system reported a problem when dgipserv\n"
	   "attempted to prepare the ARP table for sending the BOOTP\n"
	   "response.  As such, the response may or may not reach the\n"
	   "module.  After dgipserv claims to have sent off its response,\n"
	   "you should check to see if the module received its IP address.\n"
	   "If not, reboot this host and try again.\n");
  }
#endif /* AIX4 */
}

/*---------------------------------------------------------------*/
#include <ctype.h>

void dumpdata(FILE *fp, unsigned char *data, int len) 
{
  int i, j;
  char tmp[40];
  
  for (i=0; i<len; i+=16) {
    sprintf(tmp, "%5d/0x%X: ", i, i);
    fprintf(fp, "%12s", tmp);
    for (j=0; j<16; j++)
      if (i+j<len)
	fprintf(fp, "%02X ", data[i+j]);
      else
	fprintf(fp, "   ");
    fprintf(fp, " ");
    for (j=0; j<16; j++)
      if (i+j<len) {
	if (isprint((char)data[i+j]))
	  fprintf(fp, "%c", data[i+j]);
	else
	  fprintf(fp, ".");
      }
    fprintf(fp, "\n");
  }
}

/*---------------------------------------------------------------*/

int sock_read(int s, caddr_t buf, int wanted);
void bitmash(unsigned char *challenge, unsigned char *key);

int reset_elmodule(char *ipaddr, int rp_port) 
{
  int s;
  struct sockaddr_in el_addr;
  req_unit_unlock	unlock_req;
  rsp_unit_unlock	unlock_rsp;
  req_unit_inquiry	inquiry_req;
  rsp_unit_inquiry	inquiry_rsp;
  req_global		reset_req;
  rsp_global		reset_rsp;
  char              str_inquiry[64];

  int rc;
 

  if (reset_type != ALL && reset_type != FAS) {
    printf("Skipping attempt to use STS software in the module...\n");
    rc = reset_rp_elmodule(ipaddr,rp_port);
    return(rc);
  }
 
  /*
   * Create the socket and the IP connect to the module.
   */
  if ((s=socket(AF_INET, SOCK_STREAM, 0))<0) {
    perror("socket");
    return(-1);
  }
  
  el_addr.sin_family = AF_INET;
  el_addr.sin_addr.s_addr = inet_addr(ipaddr);
  el_addr.sin_port = htons(ELS_TCP_PORT);
  
  if (connect(s, (struct sockaddr *)&el_addr, sizeof(el_addr)) < 0) {
    close(s);

    /*
     *  If we can't connect to the FAS port, it might be a RealPort
     *  based unit.
     */
    rc = reset_rp_elmodule(ipaddr,rp_port);
    return(rc);
  }
  
  /*
   * Request a challenge from the ELS.
   */
  
  unlock_req.opcode = FAS_UNIT_UNLOCK;
  if (write(s, &unlock_req, FASCMDSZ) < 0) {
    perror("write");
    close(s);
    return(-1);
  }
  
  if (sock_read(s, (caddr_t)&unlock_rsp, FASCMDSZ) < 0) {
    perror("read");
    close(s);
    return(-1);
  }
  
  if (unlock_rsp.opcode != FAS_UNIT_UNLOCK) {
    printf("Bad unlock response received from EtherLite module.\n");
    close(s);
    return(-1);
  }
  
  /*
   * Do the math that'll prove to the module that we're kosher.
   */
  bitmash(unlock_rsp.challenge,inquiry_req.key);
  
  /*
   * Send back our response.
   */
  inquiry_req.opcode = FAS_UNIT_INQUIRY;
  if (write(s, &inquiry_req, FASCMDSZ) < 0) {
    perror("write");
    close(s);
    return (-1);
  }
  
  if (sock_read(s, (caddr_t)&inquiry_rsp, FASCMDSZ) < 0) {
    perror("read");
    close(s);
    return(-1);
  }
  
  if (inquiry_rsp.opcode != FAS_UNIT_INQUIRY) {
    printf("Bad inquiry response received from EtherLite module.\n");
    close(s);
    return(-1);
  }
  
  /* Consume the remainder of the FAS_UNIT_INQUIRY response. */
  if (sock_read(s, (caddr_t)str_inquiry,
		256*inquiry_rsp.cnt_hi+inquiry_rsp.cnt_lo) < 0) {
    perror("read");
    close(s);
    return(-1);
  }
  
  /*
   * Done with the challenge.  Now we can issue the reset command.
   */
  reset_req.opcode = FAS_UNIT_RESET;
  if (write(s, &reset_req, FASCMDSZ) < 0) {
    perror("write");
    close(s);
    return(-1);
  }
  
  bzero((char *)&reset_rsp, FASCMDSZ);
  if (sock_read(s, (caddr_t)&reset_rsp, FASCMDSZ) < 0) {
    perror("read");
    close(s);
    return(-1);
  }
  
  if (reset_rsp.opcode != FAS_UNIT_RESET) {
    printf("Bad reset response received from EtherLite module.\n"
	   "You will need to upgrade the firmware in the module to version 6.0\n"
	   "or later before you can reset it remotely.\n");
    close(s);
    return(-1);
  }

  printf("Reset complete.\n");  
  close(s);
  return(0);
}

int reset_rp_elmodule(char *ipaddr, int rp_port) 
{
  int s;
  struct sockaddr_in el_addr;
  unsigned char pktbuf[1024];

#if defined(DEBUG) 
  int rlen;
  int i;
  char nums[80];
  char chrs[80];
#endif

  if (reset_type != ALL && reset_type != NETCX) {
    printf("Skipping attempt to use RealPort software in the module...\n");
    return(-1);
  }

  printf("Communicating with the RealPort software in the module...\n");

  /*
   * Create the socket and the IP connect to the module.
   */
  if ((s=socket(AF_INET, SOCK_STREAM, 0))<0) {
    perror("socket");
    return(-1);
  }

  el_addr.sin_family = AF_INET;
  el_addr.sin_addr.s_addr = inet_addr(ipaddr);
  el_addr.sin_port = htons(rp_port);
  
  if (connect(s, (struct sockaddr *)&el_addr, sizeof(el_addr)) < 0) {
    perror("connect");
    close(s);
    return(-1);
  }


  /*
   *  Build the packet which executes a unit reboot in the NetC/X protocol
   */
  pktbuf[0] = 0xfd;   /* KME packet   */
  pktbuf[1] = 0x00;   /* Module ID    */
  pktbuf[2] = 0x04;   /* Request type */
  pktbuf[3] = 0x00;   /* Board number */
  pktbuf[4] = 0x00;   /* Conc number  */
  pktbuf[5] = 0x00;   /* Unused       */
  pktbuf[6] = 0x00;   /* Addr 0       */
  pktbuf[7] = 0x00;   /* Addr 1       */
  pktbuf[8] = 0x00;   /* Addr 2       */
  pktbuf[9] = 0x00;   /* Addr 3       */
  pktbuf[10]= 0x00;   /* Size, MSB 0  */
  pktbuf[11]= 0x06;   /* Size, LSB 6  */
  pktbuf[12]= 'N';
  pktbuf[13]= 'O';
  pktbuf[14]= 'T';
  pktbuf[15]= 'F';
  pktbuf[16]= 'A';
  pktbuf[17]= 'S';

  if (write(s, pktbuf, 18) < 0) {
    perror("write");
    close(s);
    return(-1);
  }

  bzero(pktbuf, 1024);
  if (sock_read(s, (caddr_t)pktbuf, 12) < 0) {
    perror("read");
    close(s);
    return(-1);
  }

  if ( pktbuf[0] != 0xfd ||
       pktbuf[2] != 0x04 ) {
    printf("Bad reset response received from EtherLite module.\n");
    close(s);
    return(-1);
  }

  if ( pktbuf[10]!= 0x00 ||
       pktbuf[11]!= 0x00 ) {
    printf("Bad reset response length %d received from EtherLite module.\n",
           pktbuf[10] * 256 + pktbuf[11]);
    close(s);
    return(-1);
  }

#if defined(DEBUG)
  rlen=12;
  nums[0]=chrs[0]=0;
  for (i=0;i<rlen;i++)
  { char dm[4];
    if( i && !(i % 16) )
    {
      printf("%-48.48s  %s\n", nums, chrs);
      nums[0]=chrs[0]=0;
    }

    sprintf(dm,"%02X ", (unsigned char)pktbuf[i]); strcat(nums, dm);

    if( isgraph( pktbuf[i] ) )
    {
      sprintf( dm, "%c", (unsigned char)pktbuf[i] );
      strcat(chrs,dm);
    }
    else strcat( chrs, "." );
  }

  if( i && (i % 16) )
  {
    printf("%-48.48s  %s\n", nums, chrs);
    nums[0]=chrs[0]=0;
  }
#endif /* DEBUG */

  printf("Reset complete.\n");
  close(s);
  return(0);
}

/*------------------------------------------------------------------*/
int sock_read(int s, caddr_t buf, int wanted) 
{
  int n;
  
  while (wanted)  {
    if ((n = read(s, buf, wanted)) <= 0) {
      return(-1);
    }
    buf += n;
    wanted -= n;
  }
  return(0);
}

/*------------------------------------------------------------------*/
/*
 *  bitmash function
 *
 *  Accepts an 6 byte challenge and returns a 3 byte key
 *  which has been bitmashed in a moderately inscrutable way.
 *  Certainly not enough security to foil a dedicated cryptographer
 *  but enough to discourage the casual cracker.
 *
 *  Those concerned with illicit access to the units should use the
 *  allowed hosts features implemented near the top of this file.
 *
 */
void bitmash(unsigned char *challenge, unsigned char *key) 
{
  int i, j, k, n, churns;
  
  /* mash the bits together */
  for( i=0, j=1, k=2, churns=0; churns < 31;
       i = (i + 1) % 6, j = (j + i) % 6, k = (k + j) % 6, churns++ ) {
    /* XOR a pair together */
    challenge[i] ^= challenge[j];
    /* rotate one byte each time */
    n = challenge[k] << 1;
    if( n & 0x100 )
      n |= 1;
    challenge[k] = (unsigned char) n;
  }
  /* build a key */
  key[0] = challenge[0] + challenge[4];
  key[1] = challenge[2] + challenge[1];
  key[2] = challenge[3] + challenge[5] + challenge[0];
  
  return;
}

/* give the user a clue & then exit */
void usage_and_exit(char **argv)
{
  printf("usage: %s [flags] hw_addr ip_addr\n", argv[0]);
  printf("  flags can include:\n"\
	 "    -reset    to reset the Etherlite before doing anything else\n"\
	 "    -store    to store the IP address in the module's EEPROM\n"\
	 "    -broadcast same as store but uses broadcast reply\n"\
	 "    -erase    to erase a stored IP from the module's EEPROM\n"\
	 "    -host auth_ip [ auth_netmask ]\n"				\
	 "              to add an IP address or list of IP addresses\n"	\
	 "              to the module's authorized host list\n"		\
	 "              (may be used up to ten times)\n"		\
	 "    -firmware filename\n"					\
	 "              to upload a firmware image to the EtherLite\n"	\
	 "              module once the IP address has been served\n"	\
	 "    -tftp tftp_server_ip\n"                                   \
	 "              to explicitly set the tftp server IP address\n" \
	 "    -vpports N\n"                                             \
	 "              to set the number of virtual ports\n"           \
	 "    -vptimeout N\n"                                           \
	 "              to set the virtual port timeout in decisecs\n"  \
	 "    -optimize latency/throughput\n"                           \
	 "              to optimize for latency or throughput\n"        \
	 " NOTE: the gateway and netmask need only be set for tftp firmware\n"\
	 " updates on complex networks. Not used for port operation.\n" \
	 "    -gateway unit_gateway\n"                                  \
	 "    -netmask unit_netmask\n"                                  \
    " NOTE: the port parameter is only needed for EtherLite modules\n" \
    " with RealPort firmware NOT using the default RealPort TCP port.\n" \
    "    -port tcp_port\n"
	 "\n"								\
	 "  hw_addr is the hardware (MAC) address of the device to\n"	\
	 "    be served its IP address (e.g. 00-a0-e7-00-00-00).\n"	\
	 "  ip_addr is the IP address to be served. (e.g. 192.168.0.1)\n"\
	 "\n"								\
	 );\
  exeunt(1);
}

/* a tidy exit that doesn't leak */
exeunt(int code)
{
  if (pktbuf) {
    free(pktbuf);
      pktbuf=NULL;
  }
exit(code);  
}
/* finis */
