/* Copyright (c) 2013, Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tlsdate.h
  * \brief The main header for our clock helper.
  **/

#ifndef TLSDATE_H
#define TLSDATE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

#define UNPRIV_USER "nobody"
#define DEFAULT_HOST "www.ptb.de"
#define DEFAULT_PORT "443"
#define DEFAULT_PROTOCOL "tlsv1"
#define DEFAULT_CERTDIR "/etc/ssl/certs"
#define DEFAULT_DAEMON_CACHEDIR "/var/cache/tlsdated"
#define DEFAULT_DAEMON_TMPSUFFIX ".new"
#define DEFAULT_TLSDATE "/usr/sbin/tlsdate"

/* tlsdated magic numbers */
#define MAX_TRIES 10
#define WAIT_BETWEEN_TRIES 10
#define SUBPROCESS_TRIES 10
#define SUBPROCESS_WAIT_BETWEEN_TRIES 3
#define STEADY_STATE_INTERVAL 86400
#define DEFAULT_SYNC_HWCLOCK 1
#define DEFAULT_LOAD_FROM_DISK 1
#define DEFAULT_SAVE_TO_DISK 1
#define DEFAULT_USE_NETLINK 1
#define DEFAULT_DRY_RUN 0

#ifndef TLSDATED_MAX_DATE
#define TLSDATED_MAX_DATE 1999991337 /* this'll be a great bug some day */
#endif


/** The current version of tlsdate. */
#define tlsdate_version "0.1"

int is_sane_time (time_t ts);
int load_disk_timestamp (const char *path, time_t * t);
void save_disk_timestamp (const char *path, time_t t);
int tlsdate (char *argv[], char *envp[], int tries, int wait_between_tries);

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

#endif /* TLSDATE_H */
