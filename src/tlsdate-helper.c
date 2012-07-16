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
 * \file tlsdate-helper.c
 * \brief Helper program that does the actual work of setting the system clock.
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

#include "../config/tlsdate-config.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <arpa/inet.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

/** Name of user that we feel safe to run SSL handshake with. */
#ifndef UNPRIV_USER
#define UNPRIV_USER "nobody"
#endif
#ifndef UNPRIV_GROUP
#define UNPRIV_GROUP "nogroup"
#endif

// We should never accept a time before we were compiled
// We measure in seconds since the epoch - eg: echo `date '+%s'`
// We set this manually to ensure others can reproduce a build;
// automation of this will make every build different!
#ifndef RECENT_COMPILE_DATE
#define RECENT_COMPILE_DATE (uint32_t) 1342323666
#endif

#ifndef MAX_REASONABLE_TIME
#define MAX_REASONABLE_TIME (uint32_t) 1999991337
#endif

// After the duration of the TLS handshake exceeds this threshold
// (in msec), a warning is printed.
#define TLS_RTT_THRESHOLD      2000

static int verbose;

static int ca_racket;

static const char *host;

static const char *port;

static const char *protocol;

static const char *certdir;

/** helper function to print message and die */
static void
die(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(1);
}


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


/**
 * Run SSL handshake and store the resulting time value in the
 * 'time_map'.
 *
 * @param time_map where to store the current time
 */
static void
run_ssl (uint32_t *time_map, int time_is_an_illusion)
{
  BIO *s_bio;
  SSL_CTX *ctx;
  SSL *ssl;
  uint32_t compiled_time = RECENT_COMPILE_DATE;
  uint32_t max_reasonable_time = MAX_REASONABLE_TIME;

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
    if (1 != SSL_CTX_load_verify_locations(ctx, NULL, certdir))
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

  if (NULL == BIO_new_fp(stdout, BIO_NOCLOSE))
    die ("BIO_new_fp returned error, possibly: %s", strerror(errno));

  // This should run in seccomp
  // eg:     prctl(PR_SET_SECCOMP, 1);
  if (1 != BIO_do_connect(s_bio)) // XXX TODO: BIO_should_retry() later?
    die ("SSL connection failed\n");
  if (1 != BIO_do_handshake(s_bio))
    die ("SSL handshake failed\n");
  // Verify the peer certificate against the CA certs on the local system
  if (ca_racket) {
    long ssl_verify_result;

    if (NULL == SSL_get_peer_certificate(ssl))
    {
      die ("Getting SSL certificate failed\n");
    }
    if (time_is_an_illusion)
    {
      // XXX TODO: If we want to trust the remote system for time,
      // can we just read that time out of the remote system and if the
      // cert verifies, decide that the time is reasonable?
      // Such a process seems to indicate that a once valid cert would be
      // forever valid - we stopgap that by ensuring it isn't less than
      // the latest compiled_time and isn't above max_reasonable_time...
      // XXX TODO: Solve eternal question about the Chicken and the Egg...
      verb("V: freezing time for x509 verification\n");
      memcpy(time_map, ssl->s3->server_random, sizeof (uint32_t));
      if (compiled_time < ntohl( * time_map)
          &&
          ntohl(*time_map) < max_reasonable_time)
      {
        verb("V: remote peer provided: %d, prefered over compile time: %d\n",
              ntohl( *time_map), compiled_time);
        // In theory, we instruct verify to check if it _would be valid_ if the
        // verification happened at ((time_t) ntohl(*time_map))
        X509_VERIFY_PARAM_set_time(ctx->param, (time_t) ntohl(*time_map));
      } else {
        die("V: the remote server is a false ticker! server: %d compile: %d\n",
             ntohl(*time_map), compiled_time);
      }
    }
    // In theory, we verify that the cert is valid
    ssl_verify_result = SSL_get_verify_result(ssl);
    switch (ssl_verify_result)
    {
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      die ("SSL certificate is self signed\n");
    case X509_V_OK:
      verb ("V: SSL certificate verification passed\n");
      break;
    default:
      die ("SSL certification verification error: %ld\n",
     ssl_verify_result);
    }
  } else {
    verb ("V: Certificate verification skipped!\n");
  }
  // from /usr/include/openssl/ssl3.h
  //  ssl->s3->server_random is an unsigned char of 32 bits
  memcpy(time_map, ssl->s3->server_random, sizeof (uint32_t));
}


/** drop root rights and become 'nobody' */
static void
become_nobody ()
{
  uid_t uid;
  gid_t gid;
  struct passwd *pw;
  struct group  *gr;

  if (0 != getuid ())
    return; /* not running as root to begin with; should (!) be harmless to continue
         without dropping to 'nobody' (setting time will fail in the end) */
  pw = getpwnam(UNPRIV_USER);
  gr = getgrnam(UNPRIV_GROUP);
  if (NULL == pw)
    die ("Failed to obtain UID for `%s'\n", UNPRIV_USER);
  if (NULL == gr)
    die ("Failed to obtain GID for `%s'\n", UNPRIV_GROUP);
  uid = pw->pw_uid;
  if (0 == uid)
    die ("UID for `%s' is 0, refusing to run SSL\n", UNPRIV_USER);
  gid = pw->pw_gid;
  if (0 == gid || 0 == gr->gr_gid)
    die ("GID for `%s' is 0, refusing to run SSL\n", UNPRIV_USER);
  if (pw->pw_gid != gr->gr_gid)
    die ("GID for `%s' is not `%s' as expected, refusing to run SSL\n",
          UNPRIV_USER, UNPRIV_GROUP);

  if (0 != initgroups((const char *)UNPRIV_USER, gr->gr_gid))
    die ("Unable to initgroups for `%s' in group `%s' as expected\n",
          UNPRIV_USER, UNPRIV_GROUP);

#ifdef HAVE_SETRESGID
  if (0 != setresgid (gid, gid, gid))
    die ("Failed to setresgid: %s\n", strerror (errno));
#else
  if (0 != (setgid (gid) | setegid (gid)))
    die ("Failed to setgid: %s\n", strerror (errno));
#endif
#ifdef HAVE_SETRESUID
  if (0 != setresuid (uid, uid, uid))
    die ("Failed to setresuid: %s\n", strerror (errno));
#else
  if (0 != (setuid (uid) | seteuid (uid)))
    die ("Failed to setuid: %s\n", strerror (errno));
#endif
}


int
main(int argc, char **argv)
{
  uint32_t *time_map;
  struct timeval start_timeval;
  struct timeval end_timeval;
  struct timeval warp_time;
  int status;
  pid_t ssl_child;
  long long rt_time_ms;
  uint32_t server_time_s;
  int setclock;
  int showtime;
  int timewarp;
  int leap;

  if (argc != 11)
    return 1;
  host = argv[1];
  port = argv[2];
  protocol = argv[3];
  certdir = argv[6];
  ca_racket = (0 != strcmp ("unchecked", argv[4]));
  verbose = (0 != strcmp ("quiet", argv[5]));
  setclock = (0 == strcmp ("setclock", argv[7]));
  showtime = (0 == strcmp ("showtime", argv[8]));
  timewarp = (0 == strcmp ("timewarp", argv[9]));
  leap = (0 == strcmp ("leapaway", argv[10]));

  if (timewarp)
  {
    warp_time.tv_sec = RECENT_COMPILE_DATE;
    warp_time.tv_usec = 0;
    verb ("V: RECENT_COMPILE_DATE is %lu.%06lu\n",
         (unsigned long)warp_time.tv_sec,
         (unsigned long)warp_time.tv_usec);
  }

  /* We are not going to set the clock, thus no need to stay root */
  if (0 == setclock && 0 == timewarp)
  {
    become_nobody ();
  }

  time_map = mmap (NULL, sizeof (uint32_t),
       PROT_READ | PROT_WRITE,
       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == time_map)
  {
    fprintf (stderr, "mmap failed: %s\n",
             strerror (errno));
    return 1;
  }

  /* Get the current time from the system clock. */
  if (0 != gettimeofday(&start_timeval, NULL))
  {
    die ("Failed to read current time of day: %s\n", strerror (errno));
  }

  verb ("V: time is currently %lu.%06lu\n",
       (unsigned long)start_timeval.tv_sec,
       (unsigned long)start_timeval.tv_usec);

  if (((unsigned long)start_timeval.tv_sec) < ((unsigned long)warp_time.tv_sec))
  {
    verb ("V: local clock time is less than RECENT_COMPILE_DATE\n");
    if (timewarp)
    {
      verb ("V: Attempting to warp local clock into the future\n");
      if (0 != settimeofday(&warp_time, NULL))
      {
        die ("setting time failed: %s (Attempted to set clock to %lu.%06lu)\n",
        strerror (errno),
        (unsigned long)warp_time.tv_sec,
        (unsigned long)warp_time.tv_usec);
      }
      if (0 != gettimeofday(&start_timeval, NULL))
      {
        die ("Failed to read current time of day: %s\n", strerror (errno));
      }
      verb ("V: time is currently %lu.%06lu\n",
           (unsigned long)start_timeval.tv_sec,
           (unsigned long)start_timeval.tv_usec);
      verb ("V: It's just a step to the left...\n");
    }
  } else {
    verb ("V: time is greater than RECENT_COMPILE_DATE\n");
  }

  /* initialize to bogus value, just to be on the safe side */
  *time_map = 0;

  /* Run SSL interaction in separate process (and not as 'root') */
  ssl_child = fork ();
  if (-1 == ssl_child)
    die ("fork failed: %s\n", strerror (errno));
  if (0 == ssl_child)
  {
    become_nobody ();
    run_ssl (time_map, leap);
    (void) munmap (time_map, sizeof (uint32_t));
    _exit (0);
  } 
  if (ssl_child != waitpid (ssl_child, &status, 0))
    die ("waitpid failed: %s\n", strerror (errno));
  if (! (WIFEXITED (status) && (0 == WEXITSTATUS (status)) ))
    die ("child process failed in SSL handshake\n");

  if (0 != gettimeofday(&end_timeval, NULL))
    die ("Failed to read current time of day: %s\n", strerror (errno));
  
  /* calculate RTT */
  rt_time_ms = (end_timeval.tv_sec - start_timeval.tv_sec) * 1000 + (end_timeval.tv_usec - start_timeval.tv_usec) / 1000;
  if (rt_time_ms < 0)
    rt_time_ms = 0; /* non-linear time... */
  server_time_s = ntohl (*time_map);
  munmap (time_map, sizeof (uint32_t));

  verb ("V: server time %u (difference is about %d s) was fetched in %lld ms\n",
  (unsigned int) server_time_s,
  start_timeval.tv_sec - server_time_s,
  rt_time_ms);

  /* warning if the handshake took too long */
  if (rt_time_ms > TLS_RTT_THRESHOLD) {
    verb ("V: the TLS handshake took more than %d msecs - consider using a different " \
      "server or run it again\n", TLS_RTT_THRESHOLD);
  }

  if (showtime)
  {
     struct tm  ltm;
     time_t tim = server_time_s;
     char       buf[256];

     localtime_r(&tim, &ltm);
     (void) strftime(buf, sizeof buf, "%a %b %e %H:%M:%S %Z %Y", &ltm);
     fprintf(stdout, "%s\n", buf);
  }

  /* finally, actually set the time */
  if (setclock)
  {
    struct timeval server_time;

    /* correct server time by half of RTT */
    server_time.tv_sec = server_time_s + (rt_time_ms / 2 / 1000);
    server_time.tv_usec = (rt_time_ms / 2) % 1000;

    // We should never receive a time that is before the time we were last
    // compiled; we subscribe to the linear theory of time for this program
    // and this program alone!
    if (server_time.tv_sec >= MAX_REASONABLE_TIME)
      die("remote server is a false ticker from the future!\n");
    if (server_time.tv_sec <= RECENT_COMPILE_DATE)
      die ("remote server is a false ticker!\n");
    if (0 != settimeofday(&server_time, NULL))
      die ("setting time failed: %s (Difference from server is about %d)\n",
     strerror (errno),
     start_timeval.tv_sec - server_time_s);
    verb ("V: setting time succeeded\n");
  }
  return 0;
}
