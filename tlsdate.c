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
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#define UNPRIV_USER "nobody"
#define DEFAULT_HOST "www.torproject.org"
#define DEFAULT_PORT "443"
#define DEFAULT_PROTOCOL "tlsv1"

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


int
main(int argc, char **argv)
{
  int verbose;
  int ca_racket;
  const char *host;
  const char *port;
  const char *protocol;

  host = DEFAULT_HOST;
  port = DEFAULT_PORT;
  protocol = DEFAULT_PROTOCOL;
  verbose = 0;
  ca_racket = 1;

  while (1) {
    int option_index = 0;
    int c;

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
      case 'v': verbose = 1; break;
      case 's': ca_racket = 0; break;
      case 'h': usage(); exit(1); break;
      case 'H': host = optarg; break;
      case 'p': port = optarg; break;
      case 'P': protocol = optarg; break;
      case '?': break;
      default : fprintf(stderr, "Unknown option!\n"); usage(); exit(1);
    }
  }

  if (verbose) {
    fprintf(stderr, 
	    "V: tlsdate version %s\n"
            "V: We were called with the following arguments:\n"
            "V: %s host = %s:%s\n",
            PACKAGE_VERSION,
	    ca_racket ? "validate SSL certificates" : "disable SSL certificate check",
            host, port);
    if (0 == ca_racket)    
      fprintf(stderr, "WARNING: Skipping certificate verification!\n");    
  }

  execlp ("tlsdate-helper",
	  "tlsdate", 
	  host,
	  port,
	  protocol,
	  (ca_racket ? "racket" : "unchecked"),
	  (verbose ? "verbose" : "quiet"),
	  NULL);
  fprintf (stderr,
	   "Failed to run tlsdate-helper\n");
  return 1;
}

