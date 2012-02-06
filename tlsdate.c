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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>

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

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "tlsdate.h"
#define tlsdate_version "0.1"
#define UNPRIV_USER "nobody"
#define DEFAULT_HOST "www.torproject.org"
#define DEFAULT_PORT "443"
#define DEFAULT_PROTOCOL "tlsv1"

// We should never accept a time before we were compiled
// We measure in seconds since the epoch - eg: echo `date '+%s'`
// We set this manually to ensure others can reproduce a build;
// automation of this will make every build different!
#define RECENT_COMPILE_DATE (uint32_t) 451328143528

static void
die(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(1);
}

/** Return the proper commandline switches when the user needs information. */
static void
usage(void)
{
  fprintf(stderr, "tlsdate usage:\n"
          " [-h|--help]\n"
          " [-s|--skip-verification]\n"
          " [-H|--host] [hostname|ip]\n"
          " [-p|--port] [port number]\n"
          " [-P]--protocol] [sslv23|sslv3|tlsv1]\n"
          " [-v|--verbose]\n");
}

/** Set the system clock to the value stored in <b>now</b>. */
static int set_absolute_time(const struct timeval *now)
{
  return settimeofday(now, NULL);
}

static int set_adj_time(const struct timeval *delta, struct timeval *olddelta)
{
  return adjtime(delta, olddelta);
}

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

// Drop all caps except CAP_SYS_TIME
static void drop_caps(void)
{
  int r;
  cap_t caps;
  cap_value_t needed_caps[] = {CAP_SYS_TIME};

  caps = cap_init();
  if (caps == NULL)
    die("cap_init: %s\n", strerror(errno));
  r = cap_set_flag(caps, CAP_EFFECTIVE, NUM_OF (needed_caps), needed_caps, CAP_SET);
  if (0 != r)
    die("cap_set_flag() failed\n");
  r = cap_set_flag(caps, CAP_PERMITTED, NUM_OF (needed_caps), needed_caps, CAP_SET);
  if (0 != r)
    die("cap_set_flag: %s\n", strerror(errno));
  r = cap_set_proc(caps);
  if (0 != r)
    die("cap_set_proc: %s\n", strerror(errno));
  r = cap_free(caps);
  if (0 != r)
    die("cap_free: %s\n", strerror(errno));
}

/** Switch to a different uid and gid. */
static void switch_uid(const struct passwd *pw)
{
  int r;

  r = setgid(pw->pw_gid);
  if (0 != r)
    die("setgid(%d): %s\n", (int)pw->pw_gid, strerror(errno));
  r = initgroups(UNPRIV_USER, pw->pw_gid);
  if (0 != r)
    die("initgroups: %s\n", strerror(errno));
  r = setuid(pw->pw_uid);
  if (0 != r)
    die("setuid(%d): %s\n", (int)pw->pw_uid, strerror(errno));
}

/** create a temp directory, chroot into it, and chdir to the new root. */
void chroot_tmp(void)
{
  // XXX TODO: this file is left behind - we should unlink it somehow
  char template[] = "/tmp/tlsdate_XXXXXX";
  char *tmp_dir;
  int r = 0;
  tmp_dir = mkdtemp(template);
  if (tmp_dir == NULL) // bad order
    die("mkdtemp(%s): %s\n", template, strerror(errno));
  r = chroot(tmp_dir);
  if (r != 0)
    die("chroot(%s): %s\n", tmp_dir, strerror(errno));
  r = chdir("/");
  if (r != 0)
    die("chdir: %s\n", strerror(errno));
  return; // bad style
}

/** This is inspired by conversations with stealth. */
int drop_privs(void)
{
  struct passwd *pw;
  int r = 0;
  pw = getpwnam(UNPRIV_USER);
  if (pw == NULL)
    die("getpwnam(%s): %s\n", UNPRIV_USER, strerror(errno));
  r = prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
  if (r != 0)
    die("prctl(PR_SET_KEEPCAPS): %s\n", strerror(errno));

  // These all fail closed with die() - no need to check return values
  chroot_tmp();
  switch_uid(pw);
  drop_caps();

  return r;
}

int
main(int argc, char **argv)
{
  int r = 0;
  int c = 0;
  struct timeval start_timeval;
  struct timeval end_timeval;
  struct timeval timeval;
  tlsdate_options_t tlsdate_options;
  struct timeval server_time;
  uint32_t server_time_tmp;
  server_time.tv_sec = 0;
  server_time.tv_usec = 0;
  uint32_t rt_time = 0;
  long ssl_verify_result = 0;

  BIO *s_bio, *c_bio;
  SSL_CTX *ctx;
  SSL *ssl;
  X509 *x509;

  SSL_load_error_strings();
  SSL_library_init();

  memset(&tlsdate_options, 0, sizeof(tlsdate_options));

  // By default, we're buying into the CA racket
  tlsdate_options.ca_racket = 1;
  tlsdate_options.host = malloc(strlen(DEFAULT_HOST));
  if (tlsdate_options.host == NULL)
    die("malloc() failed: %s\n", strerror(errno));
  tlsdate_options.port = malloc(strlen(DEFAULT_PORT));
  if (tlsdate_options.port == NULL)
    die("malloc() failed: %s\n", strerror(errno));
  tlsdate_options.protocol = malloc(strlen(DEFAULT_PROTOCOL));
  if (tlsdate_options.protocol == NULL)
    die("malloc() failed: %s\n", strerror(errno));
  strncpy(tlsdate_options.host, DEFAULT_HOST, strlen(DEFAULT_HOST));
  strncpy(tlsdate_options.port, DEFAULT_PORT, strlen(DEFAULT_PORT));
  strncpy(tlsdate_options.protocol, DEFAULT_PROTOCOL, strlen(DEFAULT_PROTOCOL));
  /* By default, we don't have a context. */
  ctx = NULL;

  while (1) {
    int option_index = 0;
    static struct option long_options[] =
      {
        {"verbose", 0, 0, 'v'},
        {"skip-verification", 0, 0, 's'},
        {"help", 0, 0, 'h'},
        {"host", 0, 0, 'H'},
        {"port", 0, 0, 'p'},
        {"protocol", 0, 0, 'P'},
        {0, 0, 0, 0}
      };

    c = getopt_long(argc, argv, "vshH:p:P:",
                    long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'v': tlsdate_options.verbose = 1; break;
      case 's': tlsdate_options.ca_racket = 0; break;
      case 'h': tlsdate_options.help = 1; usage(); exit(1); break;
      case 'H': tlsdate_options.host = optarg; break;
      case 'p': tlsdate_options.port = optarg; break;
      /* We could verify things like so... do we care?
                check = realloc(tlsdate_options.port, strlen(optarg));
                if (tlsdate_options.port == NULL || check == NULL)
                  die("realloc() failed: %s\n", strerror(errno));
                check = memset(tlsdate_options.port, 0, strlen(optarg));
                if (tlsdate_options.port != check)
                  die("memset() failed: %s\n", strerror(errno));
                check = strncpy(tlsdate_options.port, optarg, strlen(optarg - 1));
                if (check == NULL)
                  die("strncpy() failed: %s\n", strerror(errno));
                // XXX TODO: ensure that tlsdate_options.port is > 0 && < 65536
                // if not, die("invalid port!\n");
                break;
      */
      case 'P': tlsdate_options.protocol = optarg; break;
      case '?': break;
      default : fprintf(stderr, "Unknown option!\n"); usage(); exit(1);
    }
  }

  if (tlsdate_options.verbose) {
    fprintf(stderr, "V: tlsdate version %s\n"
            "V: We were called with the following arguments:\n"
            "V: ca_racket = %d, verbose = %d, help = %d\n"
            "V: host = %s, port = %s\n",
            tlsdate_version, tlsdate_options.ca_racket,
            tlsdate_options.verbose, tlsdate_options.help,
            tlsdate_options.host, tlsdate_options.port);
    if (tlsdate_options.ca_racket == 0)
    {
      fprintf(stdout, "V: !!!!!!!!!!!!! WARNING !!!!!!!!!!!!\n");
      fprintf(stdout, "V: Skipping certificate verification!\n");
      fprintf(stdout, "V: !!!!!!!!!!!!! WARNING !!!!!!!!!!!!\n");
    }
  }

  /* Get the current time from the system clock. */
  gettimeofday(&timeval, NULL);
  if (tlsdate_options.verbose)
    fprintf(stderr, "V: time is currently %lu.%06lu\n",
            (unsigned long)timeval.tv_sec, (unsigned long)timeval.tv_usec);

  if (tlsdate_options.protocol == NULL)
    die("no protocol set; unable to proceed\n");

  if (strncmp("sslv23", tlsdate_options.protocol, 6) == 0)
  {
    fprintf(stdout, "V: using SSLv23_client_method()\n");
    ctx = SSL_CTX_new(SSLv23_client_method());
  }
  if (strncmp("sslv3", tlsdate_options.protocol, 5) == 0)
  {
    fprintf(stdout, "V: using SSLv3_client_method()\n");
    ctx = SSL_CTX_new(SSLv3_client_method());
  }
  if (strncmp("tlsv1", tlsdate_options.protocol, 5) == 0)
  {
    fprintf(stdout, "V: using TLSv1_client_method()\n");
    ctx = SSL_CTX_new(TLSv1_client_method());
  }

  if (ctx == NULL)
    die("unable to init context\n");

  if (tlsdate_options.ca_racket)
  {
    // For google specifically:
    // r = SSL_CTX_load_verify_locations(ctx, "/etc/ssl/certs/Equifax_Secure_CA.pem", NULL);
    r = SSL_CTX_load_verify_locations(ctx, NULL, "/etc/ssl/certs/");
    if (r == 0)
      fprintf(stdout, "V: SSL_CTX_load_verify_locations\n");
  }

  s_bio = BIO_new_ssl_connect(ctx);
  if (s_bio == NULL)
    exit(2);

  BIO_get_ssl(s_bio, &ssl);
  if (ssl == NULL)
    exit(3);

  SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

  BIO_set_conn_hostname(s_bio, tlsdate_options.host);
  BIO_set_conn_port(s_bio, tlsdate_options.port);

  c_bio = BIO_new_fp(stdout, BIO_NOCLOSE);
  if (c_bio == NULL)
    exit(4);

  // This should run in seccomp
  // eg:     prctl(PR_SET_SECCOMP, 1);
  r = BIO_do_connect(s_bio); // XXX TODO: BIO_should_retry() later?
  if (r <= 0)
    exit(5);

  // XXX TODO: this should happen way way before here...
  r = drop_privs();
  if (r != 0)
    die("drop_privs() failed: %s\n", strerror(errno));

  gettimeofday(&start_timeval, NULL);
  r = BIO_do_handshake(s_bio);
  if (r != 1)
    exit(6);
  gettimeofday(&end_timeval, NULL);
  // Verify the peer certificate against the CA certs on the local system
  if (tlsdate_options.ca_racket) {
    if (tlsdate_options.verbose)
      fprintf(stdout, "V: Attempting to verify certificate\n");
    x509 = SSL_get_peer_certificate(ssl);
    if (x509 == NULL)
      exit(7);
    if (tlsdate_options.verbose)
      fprintf(stdout, "V: SSL_get_peer_certificate returned\n");

    // In theory, we verify that the cert is valid
    ssl_verify_result = SSL_get_verify_result(ssl);
    if ( ssl_verify_result == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
         ssl_verify_result == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN )
    {
      fprintf(stderr, "E: self signed cert\n");
    } else {
      if (  ssl_verify_result == X509_V_OK )
      {
        if (tlsdate_options.verbose)
          fprintf(stderr, "V: verification OK: %ld\n", ssl_verify_result);
      } else {
          fprintf(stderr, "E: verification error: %ld\n", ssl_verify_result);
      }
    }

    if (tlsdate_options.verbose)
      fprintf(stdout, "V: ssl_verify_result returned %ld\n", ssl_verify_result);
  } else {
    if (tlsdate_options.verbose)
    {
      fprintf(stdout, "V: Certificate verification skipped!\n");
    }
  }

  if (ssl_verify_result != 0 && tlsdate_options.ca_racket != 1)
    die("certificate verification failed!\n");

  rt_time = abs(end_timeval.tv_sec - start_timeval.tv_sec);
  if (tlsdate_options.verbose)
    fprintf(stdout, "V: server_random fetched in %i sec\n", rt_time);

  // from /usr/include/openssl/ssl3.h
  //  ssl->s3->server_random is an unsigned char of 32 bytes
  memcpy(&server_time_tmp, ssl->s3->server_random, 4);
  server_time.tv_sec = ntohl(server_time_tmp);
  if (tlsdate_options.verbose)
    fprintf(stdout, "V: server_random with ntohl is: %lu.0\n",
            (unsigned long)server_time.tv_sec);

  // We should never receive a time that is before the time we were last
  // compiled; we subscribe to the linear theory of time for this program
  // and this program alone!
  if (server_time.tv_sec <= RECENT_COMPILE_DATE)
    die("remote server is a false ticker!");

  // Set the time absolutely...
  // r = set_absolute_time(&server_time);
  // Ensure that we only increase time...
  //r = set_adj_time(NULL, &server_time);
  r = set_absolute_time(&server_time);

  if (tlsdate_options.verbose)
  {
    fprintf(stderr, "V: setting time returned: %i\n", r);
    if (r == 0) {
      fprintf(stdout, "V: tlsdate: SUCCESS\n");
    } else {
      fprintf(stderr, "V: tlsdate: FAILURE\n");
    }
  }
  exit(r);
}

