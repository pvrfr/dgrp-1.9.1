/*****************************************************************************
 *
 * Copyright 1999 Digi International (www.digi.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED; without even the 
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
 * PURPOSE.  See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

/****************************************************************************
 *
 *  Filename:
 *
 *     $Id: dgrp_mk_spec.c,v 1.7 2005/03/16 21:30:57 scottk Exp $
 *
 *  Description:
 *
 *     This program will create the three "tty" devices associated
 *     with a particular port.  One refers to the port by the current
 *     major number, the PortServer ID, and port number.
 *
 *     It will test to see if the device exists, and if so, it
 *     changes it to the new major number and preserves the ownership
 *     permissions.
 *
 *  Author:
 *
 *     James A. Puzzo
 *
 *  History:
 *
 *     10/27/99 (JAP) -- Initial implementation
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#if DO_WE_NEED_THIS_ANYMORE
# include <linux/fs.h>
#endif
#include <linux/kdev_t.h>		/* For MKDEV in userspace */
#include <errno.h>

#define MAXCHAN 64

#define TTY_BASE 0x00
#define CU_BASE  0x40
#define PR_BASE  0x80

#define TTY_PREFIX "/dev/tty"
#define CU_PREFIX  "/dev/cu"
#define PR_PREFIX  "/dev/pr"

#define DEFAULT_MODE  0666
#define DEFAULT_OWNER 0
#define DEFAULT_GROUP 0

char *Progname;


/*
 *  Print usage string
 */

void
usage( void )
{
	fprintf( stderr, "\n" );
	fprintf( stderr, "USAGE: %s [options] <ID> <major> <n>\n", Progname );
	fprintf( stderr, "\n" );
	fprintf( stderr, "       ID    - device ID\n" );
	fprintf( stderr, "       major - device' major number\n" );
	fprintf( stderr, "       n     - Port number being configured\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "       options:\n" );
	fprintf( stderr, "\n" );
	fprintf( stderr, "           -m <mode>     "
	                 "Set the file protection mode (default is 0%o)\n",
	                 DEFAULT_MODE );
	fprintf( stderr, "           -o <owner>    "
	                 "Set the user ID of the file owner, default %d.\n",
	                 DEFAULT_OWNER );
	fprintf( stderr, "                         "
	                 "Value must be an integer.\n" );
	fprintf( stderr, "           -g <group>    "
	                 "Set the group ID of the file owner, default %d.\n",
	                 DEFAULT_GROUP );
	fprintf( stderr, "                         "
	                 "Value must be an integer.\n" );
	fprintf( stderr, "\n" );
}


/*
 *  Double check the validity of the PortServer ID
 */

int
test_ID( char *id )
{
	int len;

	len = strlen( id );

	if( ( len < 1 ) || ( len > 2 ) )
		return 1;

	for( ; len > 0 ; len-- ) {
		if( !isalnum( id[len - 1] ) && ( id[len - 1] != '_' ) )
			return 1;
	}

	return 0;
}



/*****************************************************************************
*
* Function:
*
*    create_device
*
* Author:
*
*    James A. Puzzo
*
* Parameters:
*
*    char  *prefix    --  typically  /dev/tty, /dev/cu or /dev/pr
*    char  *id        --  PortServer ID
*    int    port      --  port number (zero based)
*    ulong  major     --  desired major number
*    ulong  minor     --  desired minor number (port number + offset)
*    ulong  mode      --  default mode (if device doesn't exist now)
*    ulong  uid       --  default user ID (if device doesn't exist now)
*    ulong  gid       --  default group ID (if device doesn't exist now)
*
* Return Values:
*
*    0 -- Success
*    1 -- Failure
*
* Description:
*
*    Tests to see if the current device exists.  If it does, it
*    remembers the current permissions and ownership and deletes it.
*    It then creates a new version with the requested major, and either
*    the default or saved permission and ownership.
*
*    If any error occurs, including finding the file already exists and
*    is NOT a character special file, an error message is sent to "stderr"
*    and the procedure returns.
*
******************************************************************************/

int
create_device( char *prefix,        char *id,            int port,
               unsigned long major, unsigned long minor,
               unsigned long mode,  unsigned long uid,
               unsigned long gid )
{
	char        devfile[30];
	int         rc;
	struct stat filestats;

	sprintf( devfile, "%s%s%02d", prefix, id, port );

	rc = stat( devfile, &filestats );

	if( !rc ) {
		/*
		 *  We have a valid file status structure
		 */

		if( !S_ISCHR( filestats.st_mode ) ) {
			fprintf( stderr, "ERROR: %s: non-character device file"
			                 " named %s already exists.\n",
			                 Progname, devfile );
			return( 1 );
		}

		/*
		 *  We have all of the info. we need to "recreate" the
		 *  file, so unlink the existing one.
		 */

		rc = unlink( devfile );
		if( rc ) {
			fprintf( stderr, "ERROR: %s: couldn't unlink %s\n",
			                 Progname, devfile );
			return( 1 );
		}

		/*
		 *  Override the mode, uid, and gid we were supplied
		 *  with the ones associated with the original.
		 */

		mode = (unsigned long) filestats.st_mode;
		uid  = (unsigned long) filestats.st_uid;
		gid  = (unsigned long) filestats.st_gid;
	}

	/*
	 *  At this point, we assume that no collision will
	 *  occur, because either the "stat" failed because
	 *  the file didn't exist, the "stat" failed for some
	 *  other reason, or we have just removed the file.
	 *  In any case, we attempt to create the file now.
	 *  If this fails, return the error.
	 */
	rc = mknod( devfile, S_IFCHR | mode, MKDEV( major, minor ) );
	if( rc ) {
		fprintf( stderr, "ERROR: %s: couldn't create %s (err: %d)\n",
		                 Progname, devfile, errno );
		fprintf( stderr, "       %s: mode: 0%o,  dev_t: 0x%x\n",
		                 Progname, S_IFCHR | mode,
		                 MKDEV( major, minor ) );
		return( 1 );
	}

	rc = chmod( devfile, S_IFCHR | mode );
	if( rc ) {
		fprintf( stderr, "ERROR: %s: couldn't update file mode of %s "
		                 "(err: %d)\n", Progname, devfile, errno );
		fprintf( stderr, "       %s: mode: 0%o,  dev_t: 0x%x\n",
		                 Progname, S_IFCHR | mode,
		                 MKDEV( major, minor ) );
		return( 1 );
	}

	rc = chown( devfile, (uid_t) uid, (gid_t) gid );
	if( rc ) {
		fprintf( stderr, "ERROR: %s: couldn't update owners of %s "
		                 "(err: %d)\n", Progname, devfile, errno );
		fprintf( stderr, "       %s: uid: 0x%x,  gid: 0x%x\n",
		                 Progname, uid, gid );
		return( 1 );
	}

	return( 0 );
}



/*****************************************************************************
*
* Function:
*
*    main
*
* Author:
*
*    James A. Puzzo
*
* Description:
*
*    USAGE: ./dgrp_mk_spec [options] <ID> <major> <n>
*
*           ID    - PortServer ID
*           major - PortServer's major number
*           n     - Port number being configured
*
*           options:
*
*               -m <mode>     Set the file protection mode (default is 0644)
*               -o <owner>    Set the user ID of the file owner, default 0.
*                             Value must be an integer.
*               -g <group>    Set the group ID of the file owner, default 0.
*                             Value must be an integer.
*
******************************************************************************/

int
main( int argc, char **argv )
{
	char          *id;
	unsigned long  major, port, ttyminor, cuminor, prminor;
	unsigned long  mode = DEFAULT_MODE;
	unsigned long  uid  = DEFAULT_OWNER;
	unsigned long  gid  = DEFAULT_GROUP;
	int            rc;

	extern char   *optarg;
	extern int     optind;
	int            c;

	Progname = argv[0];

	while( ( c = getopt( argc, argv, "m:o:g:" ) ) != EOF ) {
		switch( c ) {
		case 'm':

			/*
			 * The old implementation assumed the user knew
			 * that they should always pass in an octal value
			 * by adding a "0" in front of the number, or that
			 * they convert the number to octal before they send it in.
			 *
			 * I believe this assumption is too generous.
			 *
			 * I think we should try a bit harder to determine
			 * what the user intended to send to us.
			 * So we do a bit more work here than is probably
			 * needed.
			 * And this all could be fooled by adding spaces
			 * in, but this should capture most users.
			 */

			if (optarg && *optarg && *optarg != '0')
				/* Force base to octal */
				mode = strtoul( optarg, NULL, 8 );
			else
				mode = strtoul( optarg, NULL, 0 );
			break;

		case 'o':
			uid = strtoul( optarg, NULL, 0 );
			break;

		case 'g':
			gid = strtoul( optarg, NULL, 0 );
			break;

		default:
			usage();
			exit(1);
		}
	}

	if( ( argc - optind ) != 3 ) {
		usage();	
		exit( 1 );
	}

	id = argv[optind];
	major = strtoul( argv[optind + 1], NULL, 0 );
	port  = strtoul( argv[optind + 2], NULL, 0 );

	if( test_ID( id ) ) {
		fprintf( stderr, "ERROR: %s: invalid ID number: \"%s\"\n",
		                 Progname, id );
		exit( 1 );
	}

	if( port >= MAXCHAN ) {
		fprintf( stderr, "ERROR: %s: port number: %d\n",
		                 Progname, port );
		exit( 1 );
	}

	ttyminor = port | TTY_BASE;
	cuminor  = port | CU_BASE;
	prminor  = port | PR_BASE;

	rc = create_device( TTY_PREFIX, id, port, major, ttyminor,
	                    mode, uid, gid );
	if( rc ) {
		fprintf( stderr, "ERROR: %s: couldn't create TTY "
		                 "device: %d,%d (%x)\n",
		                 Progname, major, ttyminor, rc );
		exit( 1 );
	}

	rc = create_device( CU_PREFIX, id, port, major, cuminor,
	                    mode, uid, gid );
	if( rc ) {
		fprintf( stderr, "ERROR: %s: couldn't create CU "
		                 "device: %d,%d (%x)\n",
		                 Progname, major, cuminor, rc );
		exit( 1 );
	}
	rc = create_device( PR_PREFIX, id, port, major, prminor,
	                    mode, uid, gid );
	if( rc ) {
		fprintf( stderr, "ERROR: %s: couldn't create PR "
		                 "device: %d,%d (%x)\n",
		                 Progname, major, prminor, rc );
		exit( 1 );
	}
	
	return 0;
}
