/* Copyright (c) 2012, Jacob Appelbaum.
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file tlsdate.c
 * \brief The main program to assist in setting the system clock.
 **/

/*
 * tlsdate is a tool for setting the system clock by hand or by communication
 * with the network. It does not set the RTC. It is designed to be as secure as
 * TLS (RFC 2246) and additionally as secure as the Tor protocol itself. This
 * tool is designed to be run by hand or as a system daemon. It may be run by
 * Tor at a later date but unless tlsdate has the proper caps, it will not be
 * able to set the system time without running as root or another privileged
 * user.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
//#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
//#endif
//#ifdef HAVE_TIME_H
#include <time.h>
//#endif
#include <string.h>

#include <arpa/inet.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

//#ifdef HAVE_PRCTL
#include <sys/prctl.h>
//#endif
//#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
//#endif

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "tlsdate.h"
#define tlsdate_version "0.1"
#define UNPRIV_USER "nobody"

/** Return the proper commandline switches when the user needs information. */
static void
usage(void)
{
  fprintf(stderr, "tlsdate usage:\n"
          " [-h|--help]\n"
          " [-v|--verbose]\n");
}

/*
XXX: TODO

Functions to implement:

  Verification of remote certificate for Tor nodes
  Pin SSL certs for racket mode

*/

/** Set the system clock to the value stored in <b>now</b>. */
int set_absolute_time(const struct timeval *now)
{
  return settimeofday(now, NULL);
}

int set_adj_time(const struct timeval *delta, struct timeval *olddelta)
{
  return adjtime(delta, olddelta);
}

// Drop all caps except CAP_SYS_TIME
int drop_caps(void)
{
  int r = 0;
  cap_t caps;

  cap_value_t needed_caps[1] = {CAP_SYS_TIME};
  caps = cap_init();
  if (caps == NULL)
  {
    fprintf(stderr, "cap_init() failed\n");
    exit(23);
  }
  r = cap_set_flag(caps, CAP_EFFECTIVE, 1, needed_caps, CAP_SET);
  if (r != 0)
  {
    fprintf(stderr, "cap_set_flag() failed\n");
    exit(23);
  }
  r = cap_set_flag(caps, CAP_PERMITTED, 1, needed_caps, CAP_SET);
  if (r != 0)
  {
    fprintf(stderr, "cap_set_flag() failed\n");
    exit(23);
  }
  r = cap_set_proc(caps);
  if (r != 0)
  {
    fprintf(stderr, "cap_set_proc() failed\n");
    exit(23);
  }
  r = cap_free(caps);
  if (r != 0)
  {
    fprintf(stderr, "cap_free() failed\n");
    exit(23);
  }
  return r;
}

int switch_uid(struct passwd *pw)
{
  int r = 0;
  r = setgid(pw->pw_gid);
  r = initgroups(UNPRIV_USER, pw->pw_gid);
  r = setuid(pw->pw_uid);
  return r;
}

int chroot_tmp(void)
{
  int r = 0;
  r = chroot("/tmp/");
  r = chdir("/");
  return r;
}

/* This is inspired by conversations with stealth */
int drop_privs(void)
{
  struct passwd *pw;
  int r = 0;
  pw = getpwnam(UNPRIV_USER);
  if (pw == NULL)
  {
    fprintf(stderr, "getpwnam() failed\n");
    exit(23);
  }
  // check these to ensure they're always 0, less is bad
  r = prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
  if (r != 0)
  {
    fprintf(stderr, "prctl() failed\n");
    exit(23);
  }

  r = chroot_tmp();
  if (r != 0)
  {
    fprintf(stderr, "chroot_tmp() failed\n");
    exit(23);
  }

  r = switch_uid(pw);
  if (r != 0)
  {
    fprintf(stderr, "switch_uid() failed\n");
    exit(23);
  }

  r = drop_caps();
  if (r != 0)
  {
    fprintf(stderr, "drop_caps() failed\n");
    exit(23);
  }

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
  uint32_t ca_racket = 1;
  long ssl_verify_result = 0;

  BIO *s_bio, *c_bio;
  SSL_CTX *ctx;
  SSL *ssl;
  X509 *x509;

  SSL_load_error_strings();
  SSL_library_init();

  memset(&tlsdate_options, 0, sizeof(tlsdate_options));

  while (1) {
    int option_index = 0;
    static struct option long_options[] =
      {
        {"verbose", 0, 0, 'v'},
        {"help", 0, 0, 'h'},
        {0, 0, 0, 0}
      };

    c = getopt_long(argc, argv, "vh",
                    long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'v': tlsdate_options.verbose = 1; break;
      case 'h': tlsdate_options.help = 1; usage(); exit(1); break;
      case '?': break;
      default : fprintf(stderr, "Unknown option!\n"); usage(); exit(1);
    }
  }

  if (tlsdate_options.verbose) {
    fprintf(stderr, "V: tlsdate version %s\n"
            "V: We were called with the following arguments:\n"
            "V: verbose = %d, help = %d\n",
            tlsdate_version, tlsdate_options.verbose, tlsdate_options.help);
  }

  /* Get the current time from the system clock. */
  gettimeofday(&timeval, NULL);
  if (tlsdate_options.verbose)
    fprintf(stderr, "V: time is currently %lu sec and %lu usec\n", timeval.tv_sec,
                                                                   timeval.tv_usec);

  ctx = SSL_CTX_new(SSLv23_client_method());
  if (ctx == NULL)
    exit(1);

  if (ca_racket)
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

  // XXX TODO: Don't hardcode this garbage...
  if (ca_racket)
  {
    // Tor Project HTTPS server
    BIO_set_conn_hostname(s_bio, "www.torproject.org:443");
  } else {
    // Tor Directory Authority OR port
    BIO_set_conn_hostname(s_bio, "rgnx.net:80");
  }

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
  {
    fprintf(stderr, "drop_privs() failed\n");
    exit(23);
  }

  gettimeofday(&start_timeval, NULL);
  r = BIO_do_handshake(s_bio);
  if (r != 1)
    exit(6);
  gettimeofday(&end_timeval, NULL);
  // Verify the peer certificate against the CA certs on the local system
  if (ca_racket) {
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
      fprintf(stderr, "self signed cert\n");
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
  }

  rt_time = abs(end_timeval.tv_sec - start_timeval.tv_sec);
  if (tlsdate_options.verbose)
    fprintf(stdout, "V: server_random fetched in %i sec\n", rt_time);

  // from /usr/include/openssl/ssl3.h
  //  ssl->s3->server_random is an unsigned char of 32 bytes
  memcpy(&server_time_tmp, ssl->s3->server_random, 4);
  server_time.tv_sec = ntohl(server_time_tmp);
  if (tlsdate_options.verbose)
    fprintf(stdout, "V: server_random with ntohl is: %lu and 0 usec\n", server_time.tv_sec);

  // Set the time absolutely...
  // r = set_absolute_time(&server_time);
  // Ensure that we only increase time...
  //r = set_adj_time(NULL, &server_time);
  r = set_absolute_time(&server_time);

  if (tlsdate_options.verbose)
  {
    fprintf(stderr, "V: setting time returned: %i\n", r);
    if (r == 0) {
      fprintf(stdout, "tlsdate: SUCCESS\n");
    } else {
      fprintf(stderr, "tlsdate: FAILURE\n");
    }
  }
  exit(r);
}

