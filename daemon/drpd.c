/************************************************************************
 * Realport daemon for linux.
 *
 *  Originally written for HP-UX by Gene Olson.
 *
 *  Ported to linux by James Puzzo, 8/99
 *  SSL layer added by Scott H Kilau, 8/03
 ************************************************************************/


/* Export the SSL license as the first searchable text */
#include "ssl_license.h"

char ssl_license[] = SSL_LICENSE;

char copyright[] =
    "Copyright (c) 1998 Digi International.  All Rights Reserved.";

char ident[] = "$Id: drpd.c,v 1.18 2007/09/25 20:30:50 scottk Exp $";

char version[] = DIGI_VERSION;

#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <unistd.h>

#include <syslog.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <strings.h>

#include <sys/ioctl.h>

#include "digirp.h"

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/objects.h>
#include <openssl/pem.h>

/* We need Anonymous Diffie Hellman for encrypt level */
#define CIPHER_LIST_WITH_ANON "ALL:!LOW:!EXP:!MD5:@STRENGTH"
#define CIPHER_LIST "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

#define CAFILE "/etc/drp/drp.pem"
#define CERTFILE "/etc/drp/client.pem"

#define SECURE_NONE 0
#define SECURE_ENCRYPT 1
#define SECURE_SERVER 2
#define SECURE_CLIENT 3

#define DEFAULT_SECURE_PORT 1027

/************************************************************************
 * Command Parameters.
 ************************************************************************/

char *nodeName;				/* Realport node device name */

char* serverName;			/* Realport server name or
					   IP address */

char resolvedName[1024];

struct sockaddr_in serverSin;

int serverPort;				/* Realport server IP Port */

int secureserverPort;			/* Realport secure server IP Port */

int serverTimeout;			/* Realport server timeout value */

time_t serverlastreadtime;		/* time() value of last read from server */

int debug;				/* Debug level */

int nondaemon = 0;			/* Don't daemonize if "nondaemon" is <> 0 */

int secure;				/* SSL or not */

link_t lk;

char progName[257];			/* Program name for error messages */

typedef struct speed_struct digi_speed_t;

struct speed_struct
{
    int		spd_minDelay;		/* Minimum Delay in milliseconds */
    int		spd_maxSpeed;		/* Maximum speed in kilobits */
    int		spd_maxDelay;		/* Maximum Delay in milliseconds */
    int		spd_minSpeed;		/* Minimum speed in kilobits */
};

digi_speed_t speed;				/* Speed structure */

int ipv6;				/* Should the connection be ipv6 enabled? */


#define LOGPRI (LOG_INFO | LOG_DAEMON)


/************************************************************************
 * Other data.
 ************************************************************************/

int serverFD;				/* Socket file descriptor */

int deviceFD;				/* Network device file descriptor */

int maxFD;				/* Maximum of two above */

/************************************************************************
 * Support for the assert() macro.
 ************************************************************************/

#define assert(x) if (!(x)) *(volatile long *)-1 = 0


/************************************************************************
 * Gets a temp string for use with printfs and such.
 *
 * The routine copies a string to its internal circular buffer,
 * and returns a pointer to it.  The strings in the buffer are
 * eventually re-used, and so should not have a lifetime longer
 * than a few printfs.
 ************************************************************************/

char* tempString(char* str)
{
    int size;
    static char buf[2000];
    static int ptr;

    size = strlen(str) + 1;

    if (ptr < size)
	ptr = sizeof(buf);

    ptr -= size;

    assert(ptr >= 0);

    strcpy(buf + ptr, str);

    return buf + ptr;
}


/************************************************************************
 * Gets the error string associated with Errno.
 ************************************************************************/

char* errorString()
{
    static char buf[30];

    char* s = strerror(errno);

    if (s == 0)
    {
	sprintf(buf, "Error %d", errno);

	s = tempString(buf);
    }

    return s;
}


/************************************************************************
 *  This is just here for debugging, so you have somewhere to put
 *  a breakpoint before the program exits.
 ************************************************************************/

void realExit(int status)
{
    exit(status);
}


/************************************************************************
 * Prints a daemon exit message and terminates.
 ************************************************************************/

void daemonExit(int status)
{
    syslog(LOGPRI, "%s Abnormal Daemon Exit\n", progName);

    realExit(status);
}


/************************************************************************
 * Catch the signal.
 ************************************************************************/

void catch(int number)
{
    syslog(LOGPRI, "%s Exit on interrupt %d\n", progName, number);

    realExit(1);
}

/************************************************************************
 * Catch the alarm signal.
 ************************************************************************/

void catch_alarm(int number)
{
	/* Do Nothing */
}


/************************************************************************
 * Decode line speed parameter.
 ************************************************************************/

int decodeSpeed(char* string)
{
    int n;
    char extra[10];

    n = sscanf(string,
	       "%d%1[^0-9 ]%d%1[^0-9 ]%d%1[^0-9 ]%d%1[^0-9 ]%d%1s",
	       &lk.lk_fast_rate,   extra,
	       &lk.lk_fast_delay,  extra,
	       &lk.lk_slow_rate,   extra,
	       &lk.lk_slow_delay,  extra,
	       &lk.lk_header_size, extra);

    if ((n & 1) == 0)
    {
	fprintf(stderr,
		"%s Speed format is 1-5 numbers separated by "
		"non-digits\n",
		progName);
	return 1;
    }

    if (lk.lk_fast_rate < 2400 || lk.lk_fast_rate >= 10000000)
    {
	fprintf(stderr,
		"%s speed parameter 1 (FAST RATE) out of range\n",
		progName);
	return 1;
    }

    if (n < 3)
    {
	lk.lk_fast_delay = 60;
    }
    else if (lk.lk_fast_delay < 0 || lk.lk_fast_delay >= 2400)
    {
	fprintf(stderr,
		"%s speed parameter 2 (FAST DELAY) out of range\n",
		progName);
	return 1;
    }

    if (n < 5)
    {
	lk.lk_slow_rate = lk.lk_fast_rate / 4;
    }
    else if (lk.lk_slow_rate < 600 ||
	     lk.lk_slow_rate > lk.lk_fast_rate)
    {
	fprintf(stderr,
		"%s speed parameter 3 (SLOW RATE) out of range\n",
		progName);
	return 1;
    }

    if (n < 7)
    {
	lk.lk_slow_delay = lk.lk_fast_delay + 300;
    }
    else if (lk.lk_slow_delay > 10000 ||
	     lk.lk_slow_delay < lk.lk_fast_delay)
    {
	fprintf(stderr,
		"%s speed parameter 4 (SLOW DELAY) out of range\n",
		progName);
	return 1;
    }

    if (n < 9)
    {
	lk.lk_header_size = 46;
    }
    else if (lk.lk_header_size < 2 || lk.lk_header_size > 128)
    {
	fprintf(stderr,
		"%s speed parameter 5 (HEADER SIZE) out of range\n",
		progName);
	return 1;
    }

    return 0;
}


/************************************************************************
 * Decode Command line arguments.
 ************************************************************************/

void decodeCommandLine(int argc, char **argv)
{
    int c;
    int ac;
    char extra[10];
    char secureopt[100];

    serverPort = 771;
    secureserverPort = 1027;
    serverTimeout = -1;

    while ((c = getopt(argc, argv, "?h6dnp:q:s:e:t:V")) != -1)
    {
	switch (c)
	{
	    /*
	     * Increment debug level.
	     */


	case 'd':
	    debug++;
	    break;

            /*
             * Should the connection be done using ipv6?
             */

        case '6':
            ipv6++;
            break;

	    /*
	     * Should we force the daemon to not daemonize?
	     */

	case 'n':
	    nondaemon = 1;
	    break;

	    /*
	     * Get Realport server port number.
	     */

	case 'p':
	    if (sscanf(optarg, "%d%c", &serverPort, extra) != 1 ||
		serverPort < 20 ||
		serverPort >= 65536)
	    {
		fprintf(stderr, "%s Invalid server port number\n", progName);
		goto usage;
	    }
	    break;

	    /*
	     * Get Encrypted Realport server port number.
	     */

	case 'q':
	    if (sscanf(optarg, "%d%c", &secureserverPort, extra) != 1 ||
		serverPort < 20 ||
		serverPort >= 65536)
	    {
		fprintf(stderr, "%s Invalid secure server port number\n", progName);
		goto usage;
	    }
	    break;

	    /*
	     * Get Server timeout value.
	     */

	case 't':
	    if (sscanf(optarg, "%d%c", &serverTimeout, extra) != 1)
	    {
		fprintf(stderr, "%s Invalid server timeout value\n", progName);
		goto usage;
	    }
	    break;

	    /*
	     * Should we do an SSL connection?
	     */

	case 'e':
	    if (sscanf(optarg, "%20s%c", secureopt, extra) != 1)
	    {
		fprintf(stderr, "%s Invalid secure method\n", progName);
		goto usage;
	    }
	    if (strncmp(secureopt, "never", strlen("never")) == 0) {
		secure = 0;
	    }
	    if (strncmp(secureopt, "always", strlen("always")) == 0) {
		secure = 1;
	    }
#if 0
	    if (strncmp(secureopt, "secure", strlen("secure")) == 0) {
		secure = 2;
	    }
#endif
	    break;

	    /*
	     * Check validity of line speed parameter and exit.
	     */

	case 's':
	    realExit(decodeSpeed(optarg));

	    /*
	     * Display Version and exit.
	     */

	case 'V':
	    fprintf(stderr, "%s version: %s\n", progName, version);
	    realExit(0);

	default:
	    goto usage;
	}
    }
	
    /*
     * There must be either 2 or 3 positional arguments.
     */

    ac = argc - optind;

    if (ac < 2 || ac > 3)
	goto usage;

    /*
     * Get Realport network device number.
     */

    nodeName = argv[optind];

    /*
     * Get Realport node name.
     */

    serverName = argv[optind + 1];

    /*
     * Get speed information.
     */

    if (ac >= 3 && decodeSpeed(argv[optind + 2]))
	goto usage;
	
    return;

    /*
     * Print usage message.
     */

 usage:
    fprintf(stderr,
	    "usage: %s [-?hVx] [-p serverPort] "
	    "nodeName nodeAddress [speed]\n", argv[0]);
    realExit(2);
}



/*
 * Borrowed code from openssl/apps/s_cb.c
 */
void msg_cb(int write_p, int version, int content_type, const void *buf, size_t len, SSL *ssl, void *arg)
	{
	const char *str_write_p, *str_version, *str_content_type = "", *str_details1 = "", *str_details2= "";
	
	str_write_p = write_p ? ">>>" : "<<<";

	switch (version)
		{
	case SSL2_VERSION:
		str_version = "SSL 2.0";
		break;
	case SSL3_VERSION:
		str_version = "SSL 3.0 ";
		break;
	case TLS1_VERSION:
		str_version = "TLS 1.0 ";
		break;
	default:
		str_version = "???";
		}

	if (version == SSL2_VERSION)
		{
		str_details1 = "???";

		if (len > 0)
			{
			switch (((unsigned char*)buf)[0])
				{
				case 0:
					str_details1 = ", ERROR:";
					str_details2 = " ???";
					if (len >= 3)
						{
						unsigned err = (((unsigned char*)buf)[1]<<8) + ((unsigned char*)buf)[2];
						
						switch (err)
							{
						case 0x0001:
							str_details2 = " NO-CIPHER-ERROR";
							break;
						case 0x0002:
							str_details2 = " NO-CERTIFICATE-ERROR";
							break;
						case 0x0004:
							str_details2 = " BAD-CERTIFICATE-ERROR";
							break;
						case 0x0006:
							str_details2 = " UNSUPPORTED-CERTIFICATE-TYPE-ERROR";
							break;
							}
						}

					break;
				case 1:
					str_details1 = ", CLIENT-HELLO";
					break;
				case 2:
					str_details1 = ", CLIENT-MASTER-KEY";
					break;
				case 3:
					str_details1 = ", CLIENT-FINISHED";
					break;
				case 4:
					str_details1 = ", SERVER-HELLO";
					break;
				case 5:
					str_details1 = ", SERVER-VERIFY";
					break;
				case 6:
					str_details1 = ", SERVER-FINISHED";
					break;
				case 7:
					str_details1 = ", REQUEST-CERTIFICATE";
					break;
				case 8:
					str_details1 = ", CLIENT-CERTIFICATE";
					break;
				}
			}
		}

	if (version == SSL3_VERSION || version == TLS1_VERSION)
		{
		switch (content_type)
			{
		case 20:
			str_content_type = "ChangeCipherSpec";
			break;
		case 21:
			str_content_type = "Alert";
			break;
		case 22:
			str_content_type = "Handshake";
			break;
			}

		if (content_type == 21) /* Alert */
			{
			str_details1 = ", ???";
			
			if (len == 2)
				{
				switch (((unsigned char*)buf)[0])
					{
				case 1:
					str_details1 = ", warning";
					break;
				case 2:
					str_details1 = ", fatal";
					break;
					}

				str_details2 = " ???";
				switch (((unsigned char*)buf)[1])
					{
				case 0:
					str_details2 = " close_notify";
					break;
				case 10:
					str_details2 = " unexpected_message";
					break;
				case 20:
					str_details2 = " bad_record_mac";
					break;
				case 21:
					str_details2 = " decryption_failed";
					break;
				case 22:
					str_details2 = " record_overflow";
					break;
				case 30:
					str_details2 = " decompression_failure";
					break;
				case 40:
					str_details2 = " handshake_failure";
					break;
				case 42:
					str_details2 = " bad_certificate";
					break;
				case 43:
					str_details2 = " unsupported_certificate";
					break;
				case 44:
					str_details2 = " certificate_revoked";
					break;
				case 45:
					str_details2 = " certificate_expired";
					break;
				case 46:
					str_details2 = " certificate_unknown";
					break;
				case 47:
					str_details2 = " illegal_parameter";
					break;
				case 48:
					str_details2 = " unknown_ca";
					break;
				case 49:
					str_details2 = " access_denied";
					break;
				case 50:
					str_details2 = " decode_error";
					break;
				case 51:
					str_details2 = " decrypt_error";
					break;
				case 60:
					str_details2 = " export_restriction";
					break;
				case 70:
					str_details2 = " protocol_version";
					break;
				case 71:
					str_details2 = " insufficient_security";
					break;
				case 80:
					str_details2 = " internal_error";
					break;
				case 90:
					str_details2 = " user_canceled";
					break;
				case 100:
					str_details2 = " no_renegotiation";
					break;
					}
				}
			}
		
		if (content_type == 22) /* Handshake */
			{
			str_details1 = "???";

			if (len > 0)
				{
				switch (((unsigned char*)buf)[0])
					{
				case 0:
					str_details1 = ", HelloRequest";
					break;
				case 1:
					str_details1 = ", ClientHello";
					break;
				case 2:
					str_details1 = ", ServerHello";
					break;
				case 11:
					str_details1 = ", Certificate";
					break;
				case 12:
					str_details1 = ", ServerKeyExchange";
					break;
				case 13:
					str_details1 = ", CertificateRequest";
					break;
				case 14:
					str_details1 = ", ServerHelloDone";
					break;
				case 15:
					str_details1 = ", CertificateVerify";
					break;
				case 16:
					str_details1 = ", ClientKeyExchange";
					break;
				case 20:
					str_details1 = ", Finished";
					break;
					}
				}
			}
		}

	fprintf(stderr, "%s %s%s [length %04lx]%s%s\n", str_write_p, str_version,
		str_content_type, (unsigned long)len, str_details1, str_details2);

	if (len > 0)
		{
		size_t num, i;
		
		fprintf(stderr, "   ");
		num = len;

		for (i = 0; i < num; i++)
			{
			if (i % 16 == 0 && i > 0)
				fprintf(stderr, "\n   ");
			fprintf(stderr, " %02x", ((unsigned char*)buf)[i]);
			}
		if (i < len)
			fprintf(stderr, " ...");
		fprintf(stderr, "\n");
		}
	}


int verify_callback(int ok, X509_STORE_CTX *store)
{
	char data[256];


	if (!ok) {

		X509 *cert = X509_STORE_CTX_get_current_cert(store);
		int depth = X509_STORE_CTX_get_error_depth(store);
		int err = X509_STORE_CTX_get_error(store);

		fprintf(stderr, "-Error with certificate at depth: %i\n", depth);
		X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
		fprintf(stderr, "    issuer    = %s\n", data);
		X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
		fprintf(stderr, "    subject    = %s\n", data);
		fprintf(stderr, "    err %i:%s\n", err, X509_verify_cert_error_string(err));
	}

	return ok;
}

long post_connection_check(SSL *ssl, char *host, char *resolvedhost)
{
	X509 *cert = NULL;
	X509_NAME *subj;
	char data[256];
	int extcount;
	int ok = 0;
	struct sockaddr_in sin;
	struct hostent *hp;


	if (secure != SECURE_ENCRYPT) {
	    if (!(cert = SSL_get_peer_certificate(ssl))) {
		fprintf(stderr, "No Server Cert passed, and not in ENCRYPT mode, failing...\n");
		goto err_occured;
	    }
	}

	if (!host || !resolvedhost) {
	    fprintf(stderr, "No host or resolvedhost!\n");
	    goto err_occured;
	}

	if (debug)
		fprintf(stderr, "%s <---> %s\n", host, resolvedhost);

	if ((extcount = X509_get_ext_count(cert)) > 0) {
	    int i;

	    for (i = 0; i < extcount; i++) {
		const char *extstr;
		X509_EXTENSION *ext;

		ext = X509_get_ext(cert, 1);
		extstr = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

		if (!strcmp(extstr, "subjectAltName")) {
		    int j;
		    unsigned char *data;
		    STACK_OF(CONF_VALUE) *val;
		    CONF_VALUE *nval;
		    X509V3_EXT_METHOD *meth;

		    if (!(meth = X509V3_EXT_get(ext)))
			break;
		    data = ext->value->data;

		    val = meth->i2v(meth,
			meth->d2i(NULL, &data, ext->value->length), NULL);

		    for (j = 0; j < sk_CONF_VALUE_num(val); j++) {
			nval = sk_CONF_VALUE_value(val, j);
			if (!strcmp(nval->name, "DNS") && !strcmp(nval->value, host)) {
			    ok = 1;
			    break;
			}
		    }
		}
		if (ok)
		    break;

	    }
	}

	if (!ok && (subj = X509_get_subject_name(cert)) &&
	    X509_NAME_get_text_by_NID(subj, NID_commonName, data, 256) > 0) {
	    data[255] = 0;


	    bzero((char *)&sin, sizeof(sin));
	    sin.sin_addr.s_addr = inet_addr(data);

	    if (sin.sin_addr.s_addr == INADDR_NONE) {
		hp = gethostbyname(data);

		if (hp == 0) {
			/* */
		}

		bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
	    }

	    if (memcmp(&serverSin.sin_addr.s_addr, &sin.sin_addr.s_addr,
		sizeof(sin.sin_addr.s_addr))) {
		fprintf(stderr, "sin mismatch in Certificate! (Cert: %x Host: %x)\n",
		    serverSin.sin_addr.s_addr, sin.sin_addr.s_addr);
		goto err_occured;
	    }
	}

	X509_free(cert);
	return SSL_get_verify_result(ssl);

err_occured:
	if (cert)
	    X509_free(cert);

	return X509_V_ERR_APPLICATION_VERIFICATION;
}



int seed_prng(int bytes)
{

	if (debug > 1)
		fprintf(stderr, "%s Before RAND_load_file.\n", progName);

	if (RAND_load_file("/dev/random", bytes)) {
		if (debug > 1)
			fprintf(stderr, "%s seeded PRNG with /dev/random.\n", progName);
		return 1;
	}

	if (debug > 1)
		fprintf(stderr, "%s Unable to seed PRNG with /dev/random.\n", progName);

	return 0;
}

	

/************************************************************************
 * Copies data from the Network Device to the TCP connection and data
 * from the TCP connection to the Network Device.
 ************************************************************************/

void copyData()
{
    int n;
    ssize_t rcount;
    ssize_t wcount;
    ssize_t sent;
    fd_set fdread;
    struct timeval timeout;
    SSL_METHOD *meth = NULL;
    SSL_CTX *ctx = NULL;
    SSL *con = NULL;
    X509*    server_cert;
    char*    str;

    u_char buf[8000];
    int err = 0;

    if (secure) {

	if (!SSL_library_init()) {
		syslog(LOGPRI, "%s SSL library init failed! - %m\n", progName);
		if (debug)
		    fprintf(stderr, "%s SSL library init failed! - %m\n", progName);
		return;
	}

	if (!seed_prng(1)) {
		syslog(LOGPRI, "%s SSL seed_prng failed! - %m\n", progName);
		if (debug)
		    fprintf(stderr, "%s SSL seed_prng failed! - %m\n", progName);
		return;
	}

        SSL_load_error_strings();

        OpenSSL_add_ssl_algorithms();

        meth = TLSv1_client_method();

        ctx = SSL_CTX_new(meth);

        if (ctx == NULL) {
	    syslog(LOGPRI, "%s SSL CTX new failed. - %m\n", progName);
	    return;
        }

	if (secure > 1) {

		if (SSL_CTX_load_verify_locations(ctx, CAFILE, NULL) != 1) {
			/* Do something here */
		}

		if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
			/* Do something here */
		}

/* Add this back in, when we do client certs -> server */
#if 0
		if (SSL_CTX_use_certificate_chain_file(ctx, CERTFILE) != 1)
		    syslog(LOGPRI, "%s Error loading certificate from file. - %m\n", progName);

		if (SSL_CTX_use_PrivateKey_file(ctx, CERTFILE, SSL_FILETYPE_PEM) != 1)
		    syslog(LOGPRI, "%s Error loading private key from file. - %m\n", progName);
#endif

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);

		SSL_CTX_set_verify_depth(ctx, 4);
	}

	/* Make sure that blocking mode *really* blocks */
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

	/* Load the ciphers the way we want */
	if (secure > SECURE_ENCRYPT) {
	    if (SSL_CTX_set_cipher_list(ctx, CIPHER_LIST) != 1) {
		syslog(LOGPRI, "%s SSL CTX set cipher list failed. - %m\n", progName);
		return;
	    }
	}
	else {
	    if (SSL_CTX_set_cipher_list(ctx, CIPHER_LIST_WITH_ANON) != 1) {
		syslog(LOGPRI, "%s SSL CTX set cipher list failed. - %m\n", progName);
		return;
	    }
	}

        con = SSL_new(ctx);

	if (debug > 1)
		SSL_set_msg_callback(con, msg_cb);

        SSL_set_connect_state(con);

        SSL_set_fd(con, serverFD);

	while ((err = SSL_connect(con)) <= 0) {
	    int err2 = SSL_get_error(con, err);
	    char *ptr;

	    switch (err2) {

	    case SSL_ERROR_SSL:
	    case SSL_ERROR_SYSCALL:
		ptr = ERR_error_string(ERR_get_error(), NULL);

		syslog(LOGPRI, "SSL_connect fatal error. (%d:%d) (%s)\nclosing socket.\n",
		    err, err2, ptr);
		if (debug >= 1)
                    fprintf(stderr, "SSL_connect fatal error. (%d:%d) (%s)\nclosing socket\n",
		        err, err2, ptr);
		shutdown(serverFD, 2);
		return;	

	    case SSL_ERROR_ZERO_RETURN:
		syslog(LOGPRI, "SSL_connect fatal error. Connection closed.  Sleeping 10 seconds and then will retry again\n");
		if (debug >= 1)
                    fprintf(stderr, "SSL_connect fatal error. Connection closed.   Sleeping 10 seconds and then will retry again\n");
		break;

	    case SSL_ERROR_WANT_READ:
	    case SSL_ERROR_WANT_WRITE:
		syslog(LOGPRI, "SSL_connect fatal error. READ/WRITE failed temporarily. Sleeping 10 seconds and then will retry again\n");
		if (debug >= 1)
                    fprintf(stderr, "SSL_connect fatal error. READ/WRITE failed temporarily. Sleeping 10 seconds and then will retry again\n");
		break;

	    case SSL_ERROR_WANT_CONNECT:
	    case SSL_ERROR_WANT_ACCEPT:
		syslog(LOGPRI, "SSL_connect fatal error. CONNECT/ACCEPT failed temporarily. Sleeping 10 seconds and then will retry again\n");
		if (debug >= 1)
                    fprintf(stderr, "SSL_connect fatal error. CONNECT/ACCEPT failed temporarily. Sleeping 10 seconds and then will retry again\n");
		break;

	    case SSL_ERROR_WANT_X509_LOOKUP:
		syslog(LOGPRI, "SSL_connect fatal error. X509 Lookup failed temporarily. Sleeping 10 seconds and then will retry again\n");
		if (debug >= 1)
                    fprintf(stderr, "SSL_connect fatal error. X509 Lookup failed temporarily. Sleeping 10 seconds and then will retry again\n");
		break;

	    }

	    sleep(10);
	}


	if (secure > SECURE_ENCRYPT) {
		if ((err = post_connection_check(con, serverName, resolvedName)) != X509_V_OK) {
			fprintf(stderr, "ERROR: Peer certificate: %p\n",
			    X509_verify_cert_error_string(err));
			shutdown(serverFD, 2);
			return;	
		}
	}

	/* If debugging is on, dig further to find out more about the connection */
	if (debug >= 1) {
	    fprintf(stderr, "SSL connection using %s\n", SSL_get_cipher(con));
	    server_cert = SSL_get_peer_certificate (con);
            fprintf(stderr, "Server certificate:\n");
	    str = X509_NAME_oneline (X509_get_subject_name (server_cert),0,0);
	    fprintf(stderr, "\t subject: %s\n", str);
	    free (str);
	}
    }

    maxFD = serverFD;
    if (maxFD < deviceFD)
	maxFD = deviceFD;

    serverlastreadtime = time(NULL);

    /*
     * Loop to copy data until an error or a disconnect.
     */

    for (;;)
    {

	if (secure && SSL_in_init(con) && !SSL_total_renegotiations(con)) {
	    if (debug >= 1)
		fprintf(stderr, "Sleeping for 5 seconds, waiting for SSL connection to complete...\n");
	    sleep(5);
	    serverlastreadtime = time(NULL);
	    continue;
	}


	/*
	 * Wait for read data to appear on either the net device
	 * or the socket.
	 */

	FD_ZERO(&fdread);

	if (secure)
	    FD_SET(SSL_get_fd(con), &fdread);
	else
            FD_SET(serverFD, &fdread);

	FD_SET(deviceFD, &fdread);

	/*
	 * If we care about server side timeouts, then
	 * do a bit of math to get the select to timeout
	 * at about the same time as our server timeout would expire.
	 */
	if (serverTimeout != -1)
	{
		timeout.tv_sec = serverTimeout - (time(NULL) - serverlastreadtime);
		if (timeout.tv_sec <= 0)
			timeout.tv_sec = 1;
		timeout.tv_usec = 0;
	}

	/* Sleep in select, waiting for something to come in */
	n = select(maxFD + 1, &fdread, (fd_set*)0, (fd_set*)0,
		(serverTimeout != -1) ? &timeout : 0);

	if (n <= 0)
	{

	    /* Handle select timeout case */
	    if (n == 0) {

		/*
		 * Check to see if we have hit the maximum timeout limit
		 * imposed on the remote Server.
		 */
		if ((serverTimeout != -1) && ((time(NULL) - serverlastreadtime) > serverTimeout))
		{
			/*
			 * Just shutdown the socket connection.
			 * The next time thru the select loop will
			 * automagically leave this routine, and do
			 * all the needed cleanup and reinit as well.
			 */
			shutdown(serverFD, 2);
			/* Reset read time, to avoid races on socket close */
			serverlastreadtime = time(NULL);
		}

		continue;
	    }

	    syslog(LOGPRI, "%s Select error - %m\n", progName);
	    if (debug >= 1)
		fprintf(stderr, "Select error - %s\n", errorString());

	    exit(2);
	}

	/*
	 * If server data has appeared, write it to the network
	 * device.
	 */

	if (FD_ISSET(serverFD, &fdread))
	{
	    if (secure) {
		rcount = SSL_read(con, buf, sizeof(buf));
	    }
	    else {
		if (serverTimeout != -1)
			alarm(serverTimeout);

		rcount = recv(serverFD, buf, sizeof(buf), 0);

		if (serverTimeout != -1)
			alarm(0);
	    }

	    if (debug >= 2)
		fprintf(stderr, "Read %ld bytes from the %s Server.\n",
		    (long int) rcount, (secure ? "Secure" : ""));

	    serverlastreadtime = time(NULL);

	    if (rcount <= 0)
	    {
		syslog(LOGPRI, "%s %s Server disconnect detected.\n",
		    progName, (secure ? "Secure" : ""));

		if (debug >= 1)
		{
		    if (rcount == 0)
		    {
			fprintf(stderr, "%s Server closed connection\n",
			    (secure ? "Secure" : ""));
		    }
		    else
		    {
			fprintf(stderr, "%s Server disconnect - %s\n",
			    (secure ? "Secure" : ""), errorString());
		    }
		}

		rcount = 0;
	    }

	    wcount = write(deviceFD, buf, rcount);

	    if (wcount != rcount)
	    {
		syslog(LOGPRI, "%s Network Device write error\n", progName);

		if (debug >= 1)
		{
		    if (wcount < 0)
		    {
			fprintf(stderr,
				"Error writing %ld bytes to device - %s\n",
				(long int) rcount, errorString());
		    }
		    else
		    {
			fprintf(stderr,
				"Incomplete write (%ld of %ld) to device\n",
				(long int) wcount, (long int) rcount);
		    }
		}

		rcount = 0;
	    }

	    if (rcount == 0)
		return;
	}

	/*
	 * When Realport data appears, write it to the network device.
	 */

	if (FD_ISSET(deviceFD, &fdread))
	{
	    if (serverTimeout != -1)
		alarm(serverTimeout);

	    rcount = read(deviceFD, buf, sizeof(buf));

	    if (serverTimeout != -1)
		alarm(0);

	    if (rcount == 0)
		continue;

	    if (debug >= 2)
		fprintf(stderr,
			"Read %ld bytes from network device\n",
			(long int) rcount);

	    if (rcount < 0)
	    {
		syslog(LOGPRI,
		       "%s Network device read error - %m\n",
		       progName);

		if (debug >= 1)
		{
		    fprintf(stderr,
			    "Network device read error - %s\n",
			    errorString());
		}
		return;
	    }

	    /*
	     * Loop to write data to the server connection until
	     * all of the data has been accepted.
	     */

	    sent = 0;

	    for (;;) {

		if (secure)
		    wcount = SSL_write(con, buf + sent, rcount - sent);
		else
		    wcount = send(serverFD, buf + sent, rcount - sent, 0);

		if (wcount <= 0)
		{
		    if (wcount < 0)
		    {
			if (errno == EAGAIN) {
			    if (debug >= 1)
				fprintf(stderr, "TCP link congestion\n");

			    continue;
			}

			if (debug >= 1)
			    fprintf(stderr, "TCP write error - %s\n", errorString());
		    }
		    else
		    {
			syslog(LOGPRI,
			       "%s %s Server disconnect (write)\n",
			       progName, (secure ? "Secure" : ""));

			if (debug >= 1)
			    fprintf(stderr, "TCP disconnect detected on write\n");

			wcount = write(deviceFD, buf, 0);

			if (wcount < 0 && debug >= 1)
			{
			    fprintf(stderr, "Error writing EOF to net device - %s\n", errorString());
			}
		    }

		    return;
		}

		sent += wcount;

		if (sent >= rcount)
		    break;

	    }

	    if (debug >= 2)
	        fprintf(stderr, "Write %ld bytes to %s Server\n",
		    (long int) wcount, (secure ? "Secure" : ""));


	    /*
	     * Detect a protocol error if the driver begins a packet
	     * with a RESET message.
	     */

	    if (buf[0] == 0xff)
	    {
		syslog(LOGPRI, "%s Driver shutdown connection\n", progName);

		if (debug >= 1)
		    fprintf(stderr, "Driver requested shutdown\n");

		shutdown(serverFD, 2);
		return;
	    }
	}

	/*
	 * Check to see if we have hit the maximum timeout limit
	 * imposed on the remote Server.
	 */
	if ((serverTimeout != -1) && ((time(NULL) - serverlastreadtime) > serverTimeout))
	{
		/*
		 * Just shutdown the socket connection.
		 * The next time thru the select loop will
		 * automagically leave this routine, and do
		 * all the needed cleanup and reinit as well.
		 */
		shutdown(serverFD, 2);
		/* Reset read time, to avoid races on socket close */
		serverlastreadtime = time(NULL);
	}
    }
}



/************************************************************************
 * Connects to the remote server.
 ************************************************************************/

void mainLoop()
{
    int one = 1;
    struct hostent *hp;
    struct sockaddr_in sin;
    struct addrinfo *res0 = NULL;
    struct addrinfo *res = NULL;

    int message = 0;
    time_t msgtime;
    time_t now;
    int result = 0;

    /*
     * Loop to handle multiple server connections.
     */

    for (;;)
    {
	/*
	 * Enter duplicate error messages into the log file
	 * once per hour.
	 */

	if (message != 0)
	{
	    time(&now);

	    if ((u_long) (now - msgtime) >= 3600)
		message = 3;
	}

        if (ipv6) {
		struct addrinfo hints;
		char serverporttext[NI_MAXSERV];

		/* Zero out hints */
		memset(&hints, 0, sizeof(hints));

		/* Ensure we use an ipv6 connection */
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_STREAM;

		if (secure)
			snprintf(serverporttext, sizeof(serverporttext),
				"%d", secureserverPort);
		else
			snprintf(serverporttext, sizeof(serverporttext),
				"%d", serverPort);

		result = getaddrinfo(serverName, serverporttext, &hints, &res0);

		/* Non-zero return means error of some sort. */
		if (result) {
			syslog(LOGPRI, "%s getaddrinfo failed. - %s (%d)\n",
				progName, gai_strerror(result), result);

			if (debug >= 1) {
				fprintf(stderr, "getaddrinfo failed - %s (%d)\n",
					gai_strerror(result), result);
			}
			daemonExit(1);
		}

		serverFD = -1;

		/*
		 * Allocate a socket for the connection.
		 */
		for (res = res0; res; res = res->ai_next) {
			if ((serverFD = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) > 0)
				break;
		}

		if (serverFD < 0) {
			syslog(LOGPRI, "%s Could not obtain socket - %m\n", progName);
			if (debug >= 1)
				fprintf(stderr, "Socket() error - %s\n", errorString());

			daemonExit(1);
		}
	}
	else {

            /*      
             * Lookup server hostname or IP address.
             */
                 
            bzero((char *)&sin, sizeof(sin));

            sin.sin_addr.s_addr = inet_addr(serverName);
        
            if (sin.sin_addr.s_addr == INADDR_NONE) {
                hp = gethostbyname(serverName);
            
                if (hp == 0) {
                    if (message != 1) {
                        message = 1;
                        time(&msgtime);
                
                        syslog(LOGPRI, "%s Server host unknown - %s",
                            progName, errorString());
                    }
        
                    sleep(10);
                    continue;
                }

                bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
                sprintf(resolvedName, "%s", inet_ntoa(sin.sin_addr));
            
                if (debug >= 1) {
                    fprintf(stderr, "Resolved hostname: %s\n", serverName);
                }
            }
            else {
                sprintf(resolvedName, "%s", serverName);

                if (debug >= 1) {
                    fprintf(stderr, "Resolved ip: %s\n", serverName);
                }
            }   

            /*
	     * Form the complete server address.
	     */
             
	    sin.sin_family = AF_INET;

	    if (secure)
		sin.sin_port = htons((short) secureserverPort);
	    else
		sin.sin_port = htons((short) serverPort);

	    memcpy((char *) &serverSin, (char *) &sin, sizeof(struct sockaddr_in));

	    /*
	     * Allocate a socket for the connection.
	     */

	    serverFD = socket(AF_INET, SOCK_STREAM, 0);

	    if (serverFD < 0) {
		syslog(LOGPRI, "%s Could not obtain socket - %m\n", progName);

		if (debug >= 1) {
		    fprintf(stderr, "Socket() error - %s\n", errorString());
		}
		daemonExit(1);
	    }
	}

	/*
	 * Connect to the server.
	 */
	if (ipv6) {
	    result = connect(serverFD, res->ai_addr, res->ai_addrlen);
	    freeaddrinfo(res0);
	} else {
	    result = connect(serverFD, (struct sockaddr *) &sin, sizeof(sin));
	}

	if (result)
	{
	    if (message != 2)
	    {
		message = 2;
		time(&msgtime);

		if (secure)
		    syslog(LOGPRI,
		       "%s Cannot connect to SECURE server - %m\n",
		       progName);
		else
		    syslog(LOGPRI,
		       "%s Cannot connect to server - %m\n",
		       progName);
	    }
		
	    if (debug >= 1)
	    {
		if (secure)
		    fprintf(stderr,
			"Cannot connect to SECURE server %s:%d - %s\n",
			serverName,
			secureserverPort,
			errorString());
		else
		    fprintf(stderr,
			"Cannot connect to server %s:%d - %s\n",
			serverName,
			serverPort,
			errorString());
	    }
	    
	    close(serverFD);

	    if (secure) {
		if (debug)
			fprintf(stderr,
			   "Sleeping 10 seconds before trying connection to SECURE RealPort Device Server again\n");
	    }
	    else {
		if (debug)
			fprintf(stderr,
		 	  "Sleeping 10 seconds before trying connection to RealPort Device Server again\n");
	    }

	    sleep(10);
	    continue;
	}

	if (message != 0)
	{
	    syslog(LOGPRI,
		   "%s Connected to %s Server\n", progName, (secure ? "Secure" : ""));
	}
	
	if (debug >= 1)
	    fprintf(stderr, "Connected to %s Server\n", (secure ? "Secure" : ""));

	/*
	 * Set the TCP_NODELAY option to eliminate the
	 * default transmit delay.
	 */

	if (setsockopt(serverFD, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0)
	{
	    syslog(LOGPRI,
		   "%s Cannot set TCP_NDELAY - %m\n",
		   progName);

	    if (debug >= 1)
	    {
		fprintf(stderr,
			"Cannot set TCP_NODELAY: %s\n",
			errorString());
	    }
	}

	/*
	 * Copy data back and forth to the server.
	 */

	copyData();

	message = 3;

	if (debug >= 1)
	{
	    char buf[100];
	    fprintf(stdout, "Hit enter to reconnect: ");
	    fflush(stdout);
	    fgets(buf, sizeof(buf)-1, stdin);
	}
	else
	{
	    sleep(10);
	}

	/*
	 * Shut down the server connection.
	 */

	close(serverFD);
    }
}


/************************************************************************
 * Checks command parameters, opens the network device, and then
 * calls the mailLoop() procedure to do the rest.
 ************************************************************************/

int main(int argc, char **argv)
{
    char device[100];
    
    /*
     * Decode the command line.
     */

    sprintf(progName, "%s:", argv[0]);
    
    decodeCommandLine(argc, argv);

    sprintf(progName, "drpd(%s,%s)", nodeName, serverName);

    if (!secureserverPort)
	secureserverPort = DEFAULT_SECURE_PORT;

    /*
     * Open the Realport network device, which then remains open     * for the duration of the program.
     */

    sprintf(device, "/proc/dgrp/net/%s", nodeName);

    deviceFD = open(device, O_RDWR);

    if (deviceFD < 0)
    {
	syslog(LOGPRI,
	       "%s Cannot open %s - %m\n",
	       progName, device);
	       
	if (debug >= 1)
	{
	    fprintf(stderr,
		    "Cannot open %s - %s\n",
		    device, errorString());
	}

	daemonExit(1);
    }

    /*
     * If link parameters were specified, send them down to
     * the Realport device.
     */

    if (lk.lk_header_size != 0)
    {
	if (ioctl(deviceFD, DIGI_SETLINK, &lk) != 0)
	{
	    syslog(LOGPRI,
		   "%s Cannot set link parameters\n",
		   progName);

	    if (debug >= 1)
	    {
		fprintf(stderr,
			"Cannot set link parameters\n");
	    }

	    daemonExit(1);
	}
    }

    /*
     * Daemonize, unless debugging is turned on, or we have
     * explicitly requested nor to be a daemon.
     */

    if ((debug == 0) && (nondaemon == 0))
    {
	switch (fork())
	{
	case 0:
	    break;

	case -1:
	    syslog(LOGPRI, "%s Cannot fork - %m\n", progName);

	    fprintf(stderr, "Cannot fork - %s\n", errorString());
	    return 1;

	default:
	    return 0;
	}

	setsid();
	nice(-10);

        /*
         * The child no longer needs "stdin", "stdout", or "stderr",
         * and should not block processes waiting for them to close.
         *
         * Oddly, ipv6 needs stdin to remain open for some reason.
         */
	if (!ipv6)
            fclose(stdin);

        fclose(stdout);
        fclose(stderr);
    }

    /*
     * Arrange to catch terminate interrupts.
     */

    signal(SIGINT,  catch);
    signal(SIGTERM, catch);
    signal(SIGALRM, catch_alarm);

    /*
     * Run until interrupted.
     */

    mainLoop();

    return 0;
}
