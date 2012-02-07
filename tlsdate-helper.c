/* Copyright (c) 2012, Jacob Appelbaum.
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */
/*
                    This file contains the license for tlsdate,
        a free software project to set your system clock securely.

        It also lists the licenses for other components used by tlsdate.

      For more information about tlsdate, see https://github.com/ioerror/tlsdate

             If you got this file as a part of a larger bundle,
        there may be other license terms that you should be aware of.

===============================================================================
tlsdate is distributed under this license:

Copyright (c) 2011-2012, Jacob Appelbaum <jacob@appelbaum.net>
Copyright (c) 2011-2012, The Tor Project, Inc.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.

    * Neither the names of the copyright owners nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
===============================================================================
If you got tlsdate as a static binary with OpenSSL included, then you should
know:

 "This product includes software developed by the OpenSSL Project for use in
  the OpenSSL Toolkit (http://www.openssl.org/)"

===============================================================================
*/

/**
 * \file tlsdate.c
 * \brief The main program to assist in setting the system clock.
 **/

/*
 * tlsdate is a tool for setting the system clock by hand or by communication
 * with the network. It does not set the RTC. It is designed to be as secure as
 * TLS (RFC 2246) but of course the security of TLS is often reduced to
 * whichever CA racket you believe is trustworthy. By default, tlsdate trusts
 * your local CA root store - so any of these companies could assist in a MITM
 * attack against you and you'd be screwed.

 * This tool is designed to be run by hand or as a system daemon. It must be
 * run as root or otherwise have the proper caps; it will not be able to set
 * the system time without running as root or another privileged user.
 */

#include "tlsdate-config.h"

#include <stdarg.h>
#include <stdint.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <arpa/inet.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif
#ifdef HAVE_LINUX_CAPABILITY_H
#include <linux/capability.h>
#endif


#define UNPRIV_USER "nobody"

// We should never accept a time before we were compiled
// We measure in seconds since the epoch - eg: echo `date '+%s'`
// We set this manually to ensure others can reproduce a build;
// automation of this will make every build different!
#define RECENT_COMPILE_DATE (uint32_t) 1328610583
#define MAX_REASONABLE_TIME (uint32_t) 1999991337


static void
die(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(1);
}


#define NUM_OF(x) (sizeof (x) / sizeof *(x))

/** Drop all caps except CAP_SYS_TIME */
static void drop_caps(void)
{
  cap_t caps;
  cap_value_t needed_caps[] = {CAP_SYS_TIME};

  if (NULL == (caps = cap_init()))
    die("cap_init: %s\n", strerror(errno));
  if (0 != cap_set_flag(caps, CAP_EFFECTIVE, NUM_OF (needed_caps), needed_caps, CAP_SET))
    die("cap_set_flag() failed\n");
  if (0 != cap_set_flag(caps, CAP_PERMITTED, NUM_OF (needed_caps), needed_caps, CAP_SET))
    die("cap_set_flag: %s\n", strerror(errno));
  if (0 != cap_set_proc(caps))
    die("cap_set_proc: %s\n", strerror(errno));
  if (0 != cap_free(caps))
    die("cap_free: %s\n", strerror(errno));
}

/** Switch to a different uid and gid */
static void switch_uid(const struct passwd *pw)
{
  if (0 != setgid(pw->pw_gid))
    die("setgid(%d): %s\n", (int)pw->pw_gid, strerror(errno));
  if (0 != initgroups(UNPRIV_USER, pw->pw_gid))
    die("initgroups: %s\n", strerror(errno));
  if (0 != setuid(pw->pw_uid))
    die("setuid(%d): %s\n", (int)pw->pw_uid, strerror(errno));
}

/** create a temp directory, chroot into it, and chdir to the new root. */
static void chroot_tmp(void)
{
  char jaildir[] = "/tmp/tlsdate_XXXXXX";
  pid_t cpid;

  if (NULL == mkdtemp(jaildir))
    die("mkdtemp(%s): %s\n", jaildir, strerror(errno));
  cpid = fork ();
  if (-1 == cpid)
    die("fork failed: %s\n", strerror (errno));
  if (0 != cpid)
  {
    int status;
    int ret;

    while ( (cpid != (ret = waitpid (cpid, &status, 0))) &&
	    (EAGAIN == errno) ) ;
    if (ret != cpid)
      die ("waitpid failed: %s\n", strerror (errno));
    if (0 != rmdir (jaildir))
      fprintf (stderr, "Failed to remove jail directory `%s': %s\n",
	       jaildir,
	       strerror (errno));
    if (WIFEXITED (status))
      exit (WEXITSTATUS (status));
    exit (1);
  }
  if (0 != chroot(jaildir))
    die("chroot(%s): %s\n", jaildir, strerror(errno));
  if (0 != chdir("/"))
    die("chdir: %s\n", strerror(errno));
}

/** This is inspired by conversations with stealth. */
static void drop_privs(void)
{
  struct passwd *pw;

  if (NULL == (pw = getpwnam(UNPRIV_USER)))
    die("getpwnam(%s): %s\n", UNPRIV_USER, strerror(errno));
  if (0 != prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0))
    die("prctl(PR_SET_KEEPCAPS): %s\n", strerror(errno));
  chroot_tmp();
  switch_uid(pw);
  drop_caps();
}


static int verbose;

/** helper function for 'verbose' output */
static void
verb (const char *fmt, ...)
{
  va_list ap;

  if (! verbose) return;
  va_start(ap, fmt);
  // FIXME: stdout or stderr for verbose messages?
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}


int
main(int argc, char **argv)
{
  int ca_racket;
  const char *host;
  const char *port;
  const char *protocol;

  struct timeval start_timeval;
  struct timeval end_timeval;

  BIO *s_bio;
  BIO *c_bio;
  SSL_CTX *ctx;
  SSL *ssl;

  if (argc != 6)
    return 1;
  host = argv[1];
  port = argv[2];
  protocol = argv[3];
  ca_racket = (0 != strcmp ("unchecked", argv[4]));
  verbose = (0 != strcmp ("quiet", argv[5]));

  SSL_load_error_strings();
  SSL_library_init();

  ctx = NULL;
  if (0 == strcmp("sslv23", protocol))
  {
    verb ("V: using SSLv23_client_method()\n");
    ctx = SSL_CTX_new(SSLv23_client_method());
  } else if (0 == strcmp("sslv3", protocol))
  {
    verb ("V: using SSLv3_client_method()\n");
    ctx = SSL_CTX_new(SSLv3_client_method());
  } else if (0 == strcmp("tlsv1", protocol))
  {
    verb ("V: using TLSv1_client_method()\n");
    ctx = SSL_CTX_new(TLSv1_client_method());
  } else
    die("Unsupported protocol `%s'\n", protocol);

  if (ctx == NULL)
    die("OpenSSL failed to support protocol `%s'\n", protocol);

  if (ca_racket)
  {
    // For google specifically:
    // SSL_CTX_load_verify_locations(ctx, "/etc/ssl/certs/Equifax_Secure_CA.pem", NULL);
    if (1 != SSL_CTX_load_verify_locations(ctx, NULL, "/etc/ssl/certs/"))
      fprintf(stderr, "SSL_CTX_load_verify_locations failed\n");
  }

  if (NULL == (s_bio = BIO_new_ssl_connect(ctx)))
    die ("SSL BIO setup failed\n");
  BIO_get_ssl(s_bio, &ssl);
  if (NULL == ssl)
    die ("SSL setup failed\n");
  SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
  if ( (1 != BIO_set_conn_hostname(s_bio, host)) ||
       (1 != BIO_set_conn_port(s_bio, port)) )
    die ("Failed to initialize connection to `%s:%s'\n", host, port);

  if (NULL == (c_bio = BIO_new_fp(stdout, BIO_NOCLOSE)))
    die ("FIXME: error message");

  // This should run in seccomp
  // eg:     prctl(PR_SET_SECCOMP, 1);
  if (1 != BIO_do_connect(s_bio)) // XXX TODO: BIO_should_retry() later?
    die ("SSL connection failed\n");    
  
  drop_privs();

  /* Get the current time from the system clock. */
  if (0 != gettimeofday(&start_timeval, NULL))
    die ("Failed to read current time of day: %s\n", strerror (errno));
  verb ("V: time is currently %lu.%06lu\n",
	(unsigned long)start_timeval.tv_sec, 
	(unsigned long)start_timeval.tv_usec);  

  if (1 != BIO_do_handshake(s_bio))
    die ("SSL handshake failed\n");

  if (0 != gettimeofday(&end_timeval, NULL))
    die ("Failed to read current time of day: %s\n", strerror (errno));

  // Verify the peer certificate against the CA certs on the local system
  if (ca_racket) {
    X509 *x509;
    long ssl_verify_result;

    verb ("V: Attempting to verify certificate\n");
    if (NULL == (x509 = SSL_get_peer_certificate(ssl)) )
      die ("Getting SSL certificate failed\n");

    // In theory, we verify that the cert is valid
    ssl_verify_result = SSL_get_verify_result(ssl);
    switch (ssl_verify_result)
    {
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      fprintf (stderr, "E: self signed cert\n");
      break;
    case X509_V_OK:
      verb ("V: verification OK: %ld\n", ssl_verify_result);
      break;
    default:
      fprintf(stderr, "E: verification error: %ld\n", ssl_verify_result);
      break;
    }
    if (ssl_verify_result != X509_V_OK)
      die("certificate verification failed!\n");
  } else {
    verb ("V: Certificate verification skipped!\n");
  }


  if (verbose)
  {
    uint32_t rt_time;

    /* FIXME: report in ms instead... */
    /* FIXME: abs!? */
    rt_time = abs(end_timeval.tv_sec - start_timeval.tv_sec);
    verb ("V: server_random fetched in %i sec\n", rt_time);
  }
  // from /usr/include/openssl/ssl3.h
  //  ssl->s3->server_random is an unsigned char of 32 bytes
  {
    struct timeval server_time;
    uint32_t server_time_tmp;

    memcpy(&server_time_tmp, ssl->s3->server_random, sizeof (uint32_t));
    server_time.tv_sec = ntohl(server_time_tmp);
    server_time.tv_usec = 0;
    verb ("V: server_random with ntohl is: %lu.0\n",
	  (unsigned long)server_time.tv_sec);
    // We should never receive a time that is before the time we were last
    // compiled; we subscribe to the linear theory of time for this program
    // and this program alone!
    if (server_time.tv_sec >= MAX_REASONABLE_TIME)
      die("remote server is a false ticker from the future!");
    if (server_time.tv_sec <= RECENT_COMPILE_DATE)
      die ("remote server is a false ticker!");

    // FIXME: correct by RTT?
    if (0 != settimeofday(&server_time, NULL))
      die ("V: setting time failed: %s\n", strerror (errno));
  }
  verb ("V: setting time succeeded\n");
  return 0;
}

