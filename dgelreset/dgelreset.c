/*****************************************************************************
 *
 *  dgelreset - a utility program with one function:
 *    1. Reboot an Etherlites unit with either FAS or RealPort firmware
 *
 *  Authored by James Puzzo, October 2001, though much is borrowed
 *  directly from "cdipserv" and "cdelsreset" in the Central Data
 *  EtherLte software tool kit.
 * 
 * Copyright (c) 2001-2002 Digi International Inc., All Rights Reserved
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
#include <netdb.h>			/* for gethostbyname() */
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


#define MAX_MSG_SIZE    (3*512) /* Maximum packet size */
#define MAX_EL_REQUEST_GAP  60  /* seconds */

#define RP_TCP_PORT     771
#define ELS_TCP_PORT    10001


typedef enum { ALL, FAS, NETCX } RESET_TYPE;

RESET_TYPE reset_type = ALL;


/*-------------------------------------------------------------------*/
/* function prototypes */

int reset_elmodule   (struct hostent *ipinfo, int rp_port);
int reset_rp_elmodule(struct hostent *ipinfo, int rp_port);

void usage_and_exit(char **argv);


/*-------------------------------------------------------------------*/
int main(int argc, char *argv[]) 
{
  struct hostent *ipinfo;        /* IP info about remote host */
  int rp_port = RP_TCP_PORT;     /* the port for RealPort requests */

  char *str_ipaddr=NULL;         /* IP address from the command line */

  int j;                         /* general-purpose integer :-) */

  /*
   * Try to grok the command-line arguments.
   */
  for (j=1; j<argc; j++) {
    if (j==(argc-1)) {str_ipaddr=argv[j]; continue;}
    
    /*
     * -port 
     */
    if (strncmp("-p", argv[j], 2)==0) {
      j++;
      rp_port=(int)strtol(argv[j],NULL,0);
      continue;
    }

    /*
     * -rp 
     */
    if (strncmp("-r", argv[j], 2)==0) {
      if (reset_type != ALL && reset_type != NETCX) {
        printf("Only one of the following options may be used: -rp, -sts\n\n");
        usage_and_exit(argv);
      }
      reset_type = NETCX;
      continue;
    }

    /*
     * -sts 
     */
    if (strncmp("-s", argv[j], 2)==0) {
      if (reset_type != ALL && reset_type != FAS) {
        printf("Only one of the following options may be used: -rp, -sts\n\n");
        usage_and_exit(argv);
      }
      reset_type = FAS;
      continue;
    }

    printf("Unrecognized argument: \"%s\"\n\n", argv[j]);
    usage_and_exit(argv);
  } /* looping through argv[] */
 
 
  /*
   * Got enough args?
   */
  if (str_ipaddr==NULL) {
    printf("Too few arguments.\n\n");
    usage_and_exit(argv);
  }
  
  
  /*
   * Make sure the IP address is kosher.
   */
  ipinfo=gethostbyname(str_ipaddr);
  if (ipinfo==NULL) {
    printf("Bad IP name or address: \"%s\"\n", str_ipaddr);
    herror(argv[0]);
    usage_and_exit(argv);
  }
 
 
  printf("Resetting the EtherLite module...\n");
  if (reset_elmodule(ipinfo,rp_port)<0)
    printf("Remote reset failed.  You will have to manually power cycle "
           "the module.\n");
  
  return(0);  /* That's it! */
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


/*---------------------------------------------------------------*/

int sock_read(int s, caddr_t buf, int wanted);
void bitmash(unsigned char *challenge, unsigned char *key);


int reset_elmodule(struct hostent *ipinfo, int rp_port) 
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
    rc = reset_rp_elmodule(ipinfo,rp_port);
    return(rc);
  }
  
  /*
   * Create the socket and the IP connect to the module.
   */
  if ((s=socket(AF_INET, SOCK_STREAM, 0))<0) {
    perror("socket");
    return(-1);
  }
  
  memset((char *)&el_addr, 0, sizeof(el_addr));
  memcpy((char *)&el_addr.sin_addr, ipinfo->h_addr, ipinfo->h_length);

  el_addr.sin_family = AF_INET;
  el_addr.sin_port = htons(ELS_TCP_PORT);
  
  if (connect(s, (struct sockaddr *)&el_addr, sizeof(el_addr)) < 0) {
    close(s);

    /*
     *  If we can't connect to the FAS port, it might be a RealPort
     *  based unit.
     */
    rc = reset_rp_elmodule(ipinfo,rp_port);
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
  
  bzero((char *)&reset_rsp, sizeof(rsp_global));
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

int reset_rp_elmodule(struct hostent *ipinfo, int rp_port) 
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

  memset((char *)&el_addr, 0, sizeof(el_addr));
  memcpy((char *)&el_addr.sin_addr, ipinfo->h_addr, ipinfo->h_length);

  el_addr.sin_family = AF_INET;
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
  if (sock_read(s, pktbuf, 12) < 0) {
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
  printf("usage: %s [options] ip_addr\n", argv[0]);
  printf("  options can include:\n"
	 "    -port n   change the RealPort TCP port number to use\n"
    "              for EtherLite units with RealPort-based\n"
    "              firmware (default value is 771)\n"
    "    -rp       only attempt to reset by using the protocol\n"
    "              compatible with RealPort device drivers\n"
    "              (may not be used in conjunction with -sts)\n"
    "    -sts      only attempt to reset by using the protocol\n"
    "              compatible with STS device drivers\n"
    "              (may not be used in conjunction with -rp)\n"
	 "\n"	
	 "  ip_addr is the IP name or address of the unit to reset.\n"
	 "\n"	
	 );
  exit(1);
}
