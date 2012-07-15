/* Copyright (c) 2012, Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tor-time.h
  * \brief The main header for our clock helper.
  **/

#ifndef _TLSDATE_H
#define _TLSDATE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

/** The current version of tlsdate. */
#define tlsdate_version "0.1"

/** This is where we store parsed commandline options. */
typedef struct {
  int verbose;
  int ca_racket;
  int help;
  int showtime;
  int setclock;
  time_t manual_time;
  char *host;
  char *port;
  char *protocol;
} tlsdate_options_t;

#endif
