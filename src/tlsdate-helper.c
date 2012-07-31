/* Copyright (c) 2012, Jacob Appelbaum.
 * Copyright (c) 2012, The Tor Project, Inc.
 * Copyright (c) 2012, Christian Grothoff. */
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
#include "tlsdate-helper.h"


/** helper function to print message and die */
static void
die (const char *fmt, ...)
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
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}


void
openssl_time_callback (const SSL* ssl, int where, int ret)
{
  if (where == SSL_CB_CONNECT_LOOP && ssl->state == SSL3_ST_CR_CERT_A)
  {
    // XXX TODO: If we want to trust the remote system for time,
    // can we just read that time out of the remote system and if the
    // cert verifies, decide that the time is reasonable?
    // Such a process seems to indicate that a once valid cert would be
    // forever valid - we stopgap that by ensuring it isn't less than
    // the latest compiled_time and isn't above max_reasonable_time...
    // XXX TODO: Solve eternal question about the Chicken and the Egg...
    uint32_t compiled_time = RECENT_COMPILE_DATE;
    uint32_t max_reasonable_time = MAX_REASONABLE_TIME;
    uint32_t server_time;
    verb("V: freezing time for x509 verification\n");
    memcpy(&server_time, ssl->s3->server_random, sizeof(uint32_t));
    if (compiled_time < ntohl(server_time)
        &&
        ntohl(server_time) < max_reasonable_time)
    {
      verb("V: remote peer provided: %d, prefered over compile time: %d\n",
            ntohl(server_time), compiled_time);
      verb("V: freezing time with X509_VERIFY_PARAM_set_time\n");
      X509_VERIFY_PARAM_set_time(ssl->ctx->param, (time_t) ntohl(server_time));
    } else {
      die("V: the remote server is a false ticker! server: %d compile: %d\n",
           ntohl(server_time), compiled_time);
    }
  }
}

uint32_t
get_certificate_keybits (EVP_PKEY *public_key)
{
  /*
    In theory, we could use check_bitlen_dsa() and check_bitlen_rsa()
   */
  uint32_t key_bits;
  switch (public_key->type)
  {
    case EVP_PKEY_RSA:
      verb("V: key type: EVP_PKEY_RSA\n");
      key_bits = BN_num_bits(public_key->pkey.rsa->n);
      break;
    case EVP_PKEY_RSA2:
      verb("V: key type: EVP_PKEY_RSA2\n");
      key_bits = BN_num_bits(public_key->pkey.rsa->n);
      break;
    case EVP_PKEY_DSA:
      verb("V: key type: EVP_PKEY_DSA\n");
      key_bits = BN_num_bits(public_key->pkey.dsa->p);
      break;
    case EVP_PKEY_DSA1:
      verb("V: key type: EVP_PKEY_DSA1\n");
      key_bits = BN_num_bits(public_key->pkey.dsa->p);
      break;
    case EVP_PKEY_DSA2:
      verb("V: key type: EVP_PKEY_DSA2\n");
      key_bits = BN_num_bits(public_key->pkey.dsa->p);
      break;
    case EVP_PKEY_DSA3:
      verb("V: key type: EVP_PKEY_DSA3\n");
      key_bits = BN_num_bits(public_key->pkey.dsa->p);
      break;
    case EVP_PKEY_DSA4:
      verb("V: key type: EVP_PKEY_DSA4\n");
      key_bits = BN_num_bits(public_key->pkey.dsa->p);
      break;
    case EVP_PKEY_DH:
      verb("V: key type: EVP_PKEY_DH\n");
      key_bits = BN_num_bits(public_key->pkey.dh->pub_key);
      break;
    case EVP_PKEY_EC:
      verb("V: key type: EVP_PKEY_EC\n");
      key_bits = EVP_PKEY_bits(public_key);
      break;
    // Should we also care about EVP_PKEY_HMAC and EVP_PKEY_CMAC?
    default:
      key_bits = 0;
      die ("unknown public key type\n");
      break;
  }
  verb ("V: keybits: %d\n", key_bits);
  return key_bits;
}

/**
 This extracts the first commonName and checks it against hostname.
*/
uint32_t
check_cn (SSL *ssl, const char *hostname)
{
  uint32_t ret;
  char *cn_buf;
  X509 *certificate;
  X509_NAME *xname;
  cn_buf = malloc(HOST_NAME_MAX + 1);

  if (NULL == cn_buf)
  {
    die ("Unable to allocate memory for cn_buf\n");
  }

  certificate = SSL_get_peer_certificate(ssl);
  if (NULL == certificate)
  {
    die ("Unable to extract certificate\n");
  }

  memset(cn_buf, '\0', (HOST_NAME_MAX + 1));
  xname = X509_get_subject_name(certificate);
  ret = X509_NAME_get_text_by_NID(xname, NID_commonName,
                                  cn_buf, HOST_NAME_MAX);

  if (-1 == ret && ret != strlen(hostname))
  {
    die ("Unable to extract commonName\n");
  }
  if (strcasecmp(cn_buf, hostname))
  {
    verb ("V: commonName mismatch! Expected: %s - received: %s\n",
          hostname, cn_buf);
  } else {
    verb ("V: commonName matched: %s\n", cn_buf);
    return 1;
  }
  X509_NAME_free(xname);
  X509_free(certificate);
  free(cn_buf);
  return 0;
}

/**
 Search for a hostname match in the SubjectAlternativeNames.
*/
uint32_t
check_san (SSL *ssl, const char *hostname)
{
  X509 *cert;
  int extcount, ok = 0;
  /* What an OpenSSL mess ... */
  if (NULL == (cert = SSL_get_peer_certificate(ssl)))
  {
    die ("Getting certificate failed\n");
  }

  if ((extcount = X509_get_ext_count(cert)) > 0)
  {
    int i;
    for (i = 0; i < extcount; ++i)
    {
      const char *extstr;
      X509_EXTENSION *ext;
      ext = X509_get_ext(cert, i);
      extstr = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

      if (!strcmp(extstr, "subjectAltName"))
      {

        int j;
        void *extvalstr;
        const unsigned char *tmp;

        STACK_OF(CONF_VALUE) *val;
        CONF_VALUE *nval;
        X509V3_EXT_METHOD *method;

        if (!(method = X509V3_EXT_get(ext)))
        {
          break;
        }

        tmp = ext->value->data;
        if (method->it)
        {
          extvalstr = ASN1_item_d2i(NULL, &tmp, ext->value->length,
                                    ASN1_ITEM_ptr(method->it));
        } else {
          extvalstr = method->d2i(NULL, &tmp, ext->value->length);
        }

        if (!extvalstr)
        {
          break;
        }

        if (method->i2v)
        {
          val = method->i2v(method, extvalstr, NULL);
          for (j = 0; j < sk_CONF_VALUE_num(val); ++j)
          {
            nval = sk_CONF_VALUE_value(val, j);
            if ((!strcasecmp(nval->name, "DNS") &&
                !strcasecmp(nval->value, host) ) ||
                (!strcasecmp(nval->name, "iPAddress") &&
                !strcasecmp(nval->value, host)))
            {
              verb ("V: subjectAltName matched: %s, type: %s\n", nval->value, nval->name); // We matched this; so it's safe to print
              ok = 1;
              break;
            }
              verb ("V: subjectAltName found but not matched: %s, type: %s\n", nval->value, nval->name); // XXX: Clean this string!
          }
        }
      } else {
        verb ("V: found non subjectAltName extension\n");
      }
      if (ok)
      {
        break;
      }
    }
  } else {
    verb ("V: no X509_EXTENSION field(s) found\n");
  }
  X509_free(cert);
  return ok;
}

uint32_t
check_name (SSL *ssl, const char *hostname)
{
  uint32_t ret;
  ret = 0;
  ret = check_cn(ssl, hostname);
  ret += check_san(ssl, hostname);
  if (0 != ret && 0 < ret)
  {
    verb ("V: hostname verification passed\n");
  } else {
    die ("hostname verification failed for host %s!\n", host);
  }
  return ret;
}

uint32_t
verify_signature (SSL *ssl, const char *hostname)
{
  long ssl_verify_result;
  X509 *certificate;

  certificate = SSL_get_peer_certificate(ssl);
  if (NULL == certificate)
  {
    die ("Getting certificate failed\n");
  }
  // In theory, we verify that the cert is valid
  ssl_verify_result = SSL_get_verify_result(ssl);
  switch (ssl_verify_result)
  {
  case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
  case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    die ("certificate is self signed\n");
  case X509_V_OK:
    verb ("V: certificate verification passed\n");
    break;
  default:
    die ("certification verification error: %ld\n",
         ssl_verify_result);
  }
 return 0;
}

void
check_key_length (SSL *ssl)
{
  uint32_t key_bits;
  X509 *certificate;
  EVP_PKEY *public_key;
  certificate = SSL_get_peer_certificate (ssl);
  public_key = X509_get_pubkey (certificate);
  if (NULL == public_key)
  {
    die ("public key extraction failure\n");
  } else {
    verb ("V: public key is ready for inspection\n");
  }

  key_bits = get_certificate_keybits (public_key);
  if (MIN_PUB_KEY_LEN >= key_bits && public_key->type != EVP_PKEY_EC)
  {
    die ("Unsafe public key size: %d bits\n", key_bits);
  } else {
     if (public_key->type == EVP_PKEY_EC)
       if(key_bits >= MIN_ECC_PUB_KEY_LEN
          && key_bits <= MAX_ECC_PUB_KEY_LEN)
       {
         verb ("V: ECC key length appears safe\n");
       } else {
         die ("Unsafe ECC key size: %d bits\n", key_bits);
     } else {
       verb ("V: key length appears safe\n");
     }
  }
  EVP_PKEY_free (public_key);
}

void
inspect_key (SSL *ssl, const char *hostname)
{

    verify_signature (ssl, hostname);
    check_name (ssl, hostname);
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

  if (time_is_an_illusion)
  {
    SSL_set_info_callback(ssl, openssl_time_callback);
  }

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
    inspect_key (ssl, host);
  } else {
    verb ("V: Certificate verification skipped!\n");
  }
  check_key_length(ssl);
  // from /usr/include/openssl/ssl3.h
  //  ssl->s3->server_random is an unsigned char of 32 bits
  memcpy(time_map, ssl->s3->server_random, sizeof (uint32_t));
  SSL_free(ssl);
  SSL_CTX_free(ctx);
}

/** drop root rights and become 'nobody' */
static void
become_nobody (void)
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
