/* Copyright (c) 2013, Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file tlsdate.h
  * \brief The main header for our clock helper.
  **/

#ifndef TLSDATE_H
#define TLSDATE_H

#include "src/configmake.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "src/rtc.h"

#define DEFAULT_HOST "google.com"
#define DEFAULT_PORT "443"
#define DEFAULT_PROXY "none"
#define DEFAULT_PROTOCOL "tlsv1"
#define DEFAULT_CERTDIR "/etc/ssl/certs"
#define DEFAULT_CERTFILE TLSDATE_CERTFILE
#define DEFAULT_DAEMON_CACHEDIR "/var/cache/tlsdated"
#define DEFAULT_DAEMON_TMPSUFFIX ".new"
#define DEFAULT_TLSDATE TLSDATE
#define DEFAULT_RTC_DEVICE "/dev/rtc"
#define DEFAULT_CONF_FILE TLSDATE_CONF_DIR "tlsdated.conf"

/* tlsdated magic numbers */
#define MAX_TRIES 10
#define WAIT_BETWEEN_TRIES 10
#define SUBPROCESS_TRIES 10
#define SUBPROCESS_WAIT_BETWEEN_TRIES 10
#define RESOLVER_TIMEOUT 30
/* Invalidate the network sync once per day. */
#define STEADY_STATE_INTERVAL (60*60*24)
/* Check if the clock has jumped every four hours. */
#define CONTINUITY_INTERVAL (60*60*4)
#define DEFAULT_SYNC_HWCLOCK 1
#define DEFAULT_LOAD_FROM_DISK 1
#define DEFAULT_SAVE_TO_DISK 1
#define DEFAULT_USE_NETLINK 1
#define DEFAULT_DRY_RUN 0
#define MAX_SANE_BACKOFF (10*60) /* exponential backoff should only go this far */

#ifndef TLSDATED_MAX_DATE
#define TLSDATED_MAX_DATE 1999991337L /* this'll be a great bug some day */
#endif

#define MAX_EVENT_PRIORITIES 2
#define PRI_SAVE 0
#define PRI_NET 1
#define PRI_WAKE 1
#define PRI_ANY 1

/* Sync sources in order of "reliability" */
#define SYNC_TYPE_NONE  (0)
#define SYNC_TYPE_BUILD  (1 << 0)
#define SYNC_TYPE_DISK  (1 << 1)
#define SYNC_TYPE_RTC  (1 << 2)
#define SYNC_TYPE_PLATFORM  (1 << 3)
#define SYNC_TYPE_NET  (1 << 4)

/* Simple time setter<>tlsdated protocol */
#define SETTER_EXIT 0
#define SETTER_BAD_TIME 1
#define SETTER_NO_SAVE 2
#define SETTER_READ_ERR 3
#define SETTER_TIME_SET 4
#define SETTER_SET_ERR 5
#define SETTER_NO_SBOX 6
#define SETTER_NO_RTC 7

#define TEST_HOST 'w', 'w', 'w', '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', \
                  'c', 'o', 'm'
#define TEST_HOST_SIZE 14
static const char kTestHost[] = { TEST_HOST, 0 };
#define TEST_PORT 80

/** The current version of tlsdate. */
#define tlsdate_version VERSION

/** GNU/Hurd support requires that we declare this ourselves: */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN PATH_MAX
#endif

struct source
{
	struct source *next;
	char *host;
	char *port;
	char *proxy;
	int id;
};

struct opts
{
  const char *user;
  const char *group;
  int max_tries;
  int min_steady_state_interval;
  int wait_between_tries;
  int subprocess_tries;
  int subprocess_wait_between_tries;
  int steady_state_interval;
  int continuity_interval;
  const char *base_path;
  char **base_argv;
  char **argv;
  int should_sync_hwclock;
  int should_load_disk;
  int should_save_disk;
  int should_netlink;
  int dry_run;
  int jitter;
  char *conf_file;
  struct source *sources;
  struct source *cur_source;
  char *proxy;
  int leap;
  int should_dbus;
};

#define MAX_FQDN_LEN 255
#define MAX_SCHEME_LEN 9
#define MAX_PORT_LEN 6  /* incl. : */
#define MAX_PROXY_URL (MAX_FQDN_LEN + MAX_SCHEME_LEN + MAX_PORT_LEN + 1)

enum event_id_t
{
  E_RESOLVER = 0,
  E_TLSDATE,
  E_TLSDATE_STATUS,
  E_TLSDATE_TIMEOUT,
  E_SAVE,
  E_SIGCHLD,
  E_SIGTERM,
  E_STEADYSTATE,
  E_ROUTEUP,
  E_MAX
};

struct event_base;

/* This struct is used for passing tlsdated runtime state between
 * events/ in its event loop.
 */
struct state
{
  struct opts opts;
  struct event_base *base;
  void *dbus;
  char **envp;

  time_t clock_delta;
  int last_sync_type;
  time_t last_time;

  char timestamp_path[PATH_MAX];
  struct rtc_handle hwclock;
  char dynamic_proxy[MAX_PROXY_URL];
  /* Event triggered events */

  struct event *events[E_MAX];
  int tlsdate_monitor_fd;
  pid_t tlsdate_pid;
  pid_t setter_pid;
  int setter_save_fd;
  int setter_notify_fd;
  uint32_t backoff;
  int tries;
  int resolving;
  int running;  /* tlsdate itself */
  int exitting;
};

char timestamp_path[PATH_MAX];

int is_sane_time (time_t ts);
int load_disk_timestamp (const char *path, time_t * t);
void save_disk_timestamp (const char *path, time_t t);
int add_jitter (int base, int jitter);
void time_setter_coprocess (int time_fd, int notify_fd, struct state *state);
int tlsdate (struct state *state);

int save_timestamp_to_fd (int fd, time_t t);
void set_conf_defaults (struct opts *opts);
int new_tlsdate_monitor_pipe (int fds[2]);
int read_tlsdate_response (int fd, time_t *t);

void invalidate_time (struct state *state);
int check_continuity (time_t *delta);

void action_check_continuity (int fd, short what, void *arg);
void action_kickoff_time_sync (int fd, short what, void *arg);
void action_invalidate_time (int fd, short what, void *arg);
void action_stdin_wakeup (int fd, short what, void *arg);
void action_netlink_ready (int fd, short what, void *arg);
void action_run_tlsdate (int fd, short what, void *arg);
void action_sigterm (int fd, short what, void *arg);
void action_sync_and_save (int fd, short what, void *arg);
void action_time_set (int fd, short what, void *arg);
void action_tlsdate_status (int fd, short what, void *arg);

int setup_event_timer_continuity (struct state *state);
int setup_event_timer_sync (struct state *state);
int setup_event_route_up (struct state *state);
int setup_time_setter (struct state *state);
int setup_tlsdate_status (struct state *state);
int setup_sigchld_event (struct state *state, int persist);

void report_setter_error (siginfo_t *info);

void sync_and_save (void *hwclock_handle, int should_save);

/** This is where we store parsed commandline options. */
typedef struct
{
  int verbose;
  int verbose_debug;
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
