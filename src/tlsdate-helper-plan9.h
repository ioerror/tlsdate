/* Copyright (c) 2012, Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tlsdate-helper.h
  * \brief The secondary header for our clock helper.
  **/

#ifndef TLSDATEHELPER_H
#define TLSDATEHELPER_H

#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <arpa/inet.h>
#include <ctype.h>

#ifndef USE_POLARSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#endif

int verbose;

#include "src/util.h"

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

#ifndef MIN_PUB_KEY_LEN
#define MIN_PUB_KEY_LEN (uint32_t) 1023
#endif

#ifndef MIN_ECC_PUB_KEY_LEN
#define MIN_ECC_PUB_KEY_LEN (uint32_t) 160
#endif

#ifndef MAX_ECC_PUB_KEY_LEN
#define MAX_ECC_PUB_KEY_LEN (uint32_t) 521
#endif
// After the duration of the TLS handshake exceeds this threshold
// (in msec), a warning is printed.
#define TLS_RTT_THRESHOLD      2000

// RFC 5280 says...
// ub-common-name-length INTEGER ::= 64
#define MAX_CN_NAME_LENGTH 64

// RFC 1034 and posix say...
#define TLSDATE_HOST_NAME_MAX 255

// To support our RFC 2595 wildcard verification
#define RFC2595_MIN_LABEL_COUNT 3

static int ca_racket;

static const char *host;

static const char *hostname_to_verify;

static const char *port;

static const char *protocol;

static char *proxy;

static const char *ca_cert_container;
#ifndef USE_POLARSSL
void openssl_time_callback (const SSL* ssl, int where, int ret);
uint32_t get_certificate_keybits (EVP_PKEY *public_key);
uint32_t check_cn (SSL *ssl, const char *hostname);
uint32_t check_san (SSL *ssl, const char *hostname);
long openssl_check_against_host_and_verify (SSL *ssl);
uint32_t check_name (SSL *ssl, const char *hostname);
uint32_t verify_signature (SSL *ssl, const char *hostname);
void check_key_length (SSL *ssl);
void inspect_key (SSL *ssl, const char *hostname);
void check_key_length (SSL *ssl);
void inspect_key (SSL *ssl, const char *hostname);
#endif
uint32_t dns_label_count (char *label, char *delim);
uint32_t check_wildcard_match_rfc2595 (const char *orig_hostname,
                                       const char *orig_cert_wild_card);
static void run_ssl (uint32_t *time_map, int time_is_an_illusion);

#endif
