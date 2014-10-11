/*
 * tlsdated.c - invoke tlsdate when necessary.
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * We invoke tlsdate once at system startup, then we start trying to invoke
 * tlsdate when a new network route appears. We try a few times after each route
 * comes up. As soon as we get a successful tlsdate run, we save that timestamp
 * to disk, then linger to wait for system shutdown. At system shutdown
 * (indicated by us getting SIGTERM), we save our timestamp to disk.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <grp.h> /* setgroups */
#include <fcntl.h>
#include <limits.h>
#include <linux/rtc.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#include <event2/event.h>

#include "src/conf.h"
#include "src/routeup.h"
#include "src/util.h"
#include "src/tlsdate.h"
#include "src/dbus.h"
#include "src/platform.h"

const char *kCacheDir = DEFAULT_DAEMON_CACHEDIR;

int
is_sane_time (time_t ts)
{
  return ts > RECENT_COMPILE_DATE && ts < TLSDATED_MAX_DATE;
}

/*
 * Load a time value out of the file named by path. Returns 0 if successful,
 * -1 if not. The file contains the time in seconds since epoch in host byte
 * order.
 */
int
load_disk_timestamp (const char *path, time_t * t)
{
  int fd = platform->file_open (path, 0 /* RDONLY */, 1 /* CLOEXEC */);
  time_t tmpt = 0;
  if (fd < 0)
    {
      perror ("Can't open %s for reading", path);
      return -1;
    }
  if (platform->file_read(fd, &tmpt, sizeof(tmpt)))
    {
      perror ("Can't read seconds from %s", path);
      platform->file_close (fd);
      return -1;
    }
  platform->file_close (fd);
  if (!is_sane_time (tmpt))
    {
      error ("Disk timestamp is not sane: %ld", tmpt);
      return -1;
    }
  *t = tmpt;
  return 0;
}


void
usage (const char *progn)
{
  printf ("Usage: %s [flags...] [--] [tlsdate command...]\n", progn);
  printf ("  -w        don't set hwclock\n");
  printf ("  -p        dry run (don't really set time)\n");
  printf ("  -r        use stdin instead of netlink for routes\n");
  printf ("  -t <n>    try n times to synchronize the time\n");
  printf ("  -d <n>    delay n seconds between tries\n");
  printf ("  -T <n>    give subprocess n chances to exit\n");
  printf ("  -D <n>    delay n seconds between wait attempts\n");
  printf ("  -c <path> set the cache directory\n");
  printf ("  -a <n>    run at most every n seconds in steady state\n");
  printf ("  -m <n>    run at most once every n seconds in steady state\n");
  printf ("  -j <n>    add up to n seconds jitter to steady state checks\n");
  printf ("  -l        don't load disk timestamps\n");
  printf ("  -s        don't save disk timestamps\n");
  printf ("  -U        don't use DBus if supported\n");
  printf ("  -u <user> user to change to\n");
  printf ("  -g <grp>  group to change to\n");
  printf ("  -v        be verbose\n");
  printf ("  -b        use verbose debugging\n");
  printf ("  -x <h>    set proxy for subprocs to h\n");
  printf ("  -h        this\n");
}

void
set_conf_defaults (struct opts *opts)
{
  static char *kDefaultArgv[] =
  {
    (char *) DEFAULT_TLSDATE, (char *) "-H", (char *) DEFAULT_HOST, NULL
  };
  opts->user = UNPRIV_USER;
  opts->group = UNPRIV_GROUP;
  opts->max_tries = MAX_TRIES;
  opts->min_steady_state_interval = STEADY_STATE_INTERVAL;
  opts->wait_between_tries = WAIT_BETWEEN_TRIES;
  opts->subprocess_tries = SUBPROCESS_TRIES;
  opts->subprocess_wait_between_tries = SUBPROCESS_WAIT_BETWEEN_TRIES;
  opts->steady_state_interval = STEADY_STATE_INTERVAL;
  opts->continuity_interval = CONTINUITY_INTERVAL;
  opts->base_path = kCacheDir;
  opts->base_argv = kDefaultArgv;
  opts->argv = NULL;
  opts->should_dbus = 1;
  opts->should_sync_hwclock = DEFAULT_SYNC_HWCLOCK;
  opts->should_load_disk = DEFAULT_LOAD_FROM_DISK;
  opts->should_save_disk = DEFAULT_SAVE_TO_DISK;
  opts->should_netlink = DEFAULT_USE_NETLINK;
  opts->dry_run = DEFAULT_DRY_RUN;
  opts->jitter = 0;
  opts->conf_file = NULL;
  opts->sources = NULL;
  opts->cur_source = NULL;
  opts->proxy = NULL;
  opts->leap = 0;
}

void
parse_argv (struct opts *opts, int argc, char *argv[])
{
  int opt;
  while ((opt = getopt (argc, argv, "hwrpt:d:T:D:c:a:lsvbm:j:f:x:Uu:g:")) != -1)
    {
      switch (opt)
        {
        case 'w':
          opts->should_sync_hwclock = 0;
          break;
        case 'r':
          opts->should_netlink = 0;
          break;
        case 'U':
          opts->should_dbus = 0;
          break;
        case 'p':
          opts->dry_run = 1;
          break;
        case 't':
          opts->max_tries = atoi (optarg);
          break;
        case 'd':
          opts->wait_between_tries = atoi (optarg);
          break;
        case 'T':
          opts->subprocess_tries = atoi (optarg);
          break;
        case 'D':
          opts->subprocess_wait_between_tries = atoi (optarg);
          break;
        case 'c':
          opts->base_path = optarg;
          break;
        case 'a':
          opts->steady_state_interval = atoi (optarg);
          break;
        case 'l':
          opts->should_load_disk = 0;
          break;
        case 's':
          opts->should_save_disk = 0;
          break;
        case 'v':
          verbose = 1;
          break;
        case 'b':
          verbose_debug = 1;
          break;
        case 'm':
          opts->min_steady_state_interval = atoi (optarg);
          break;
        case 'j':
          opts->jitter = atoi (optarg);
          break;
        case 'f':
          opts->conf_file = optarg;
          break;
        case 'x':
          opts->proxy = optarg;
          break;
        case 'u':
          opts->user = optarg;
          break;
        case 'g':
          opts->group = optarg;
          break;
        case 'h':
        default:
          usage (argv[0]);
          exit (1);
        }
    }
  if (optind < argc)
    opts->base_argv = argv + optind;
  /* Validate arguments */
}

static
void add_source_to_conf (struct opts *opts, char *host, char *port, char *proxy)
{
  struct source *s;
  struct source *source = (struct source *) calloc (1, sizeof *source);
  if (!source)
    fatal ("out of memory for source");
  source->host = strdup (host);
  if (!source->host)
    fatal ("out of memory for host");
  source->port = strdup (port);
  if (!source->port)
    fatal ("out of memory for port");
  if (proxy)
    {
      source->proxy = strdup (proxy);
      if (!source->proxy)
        fatal ("out of memory for proxy");
    }
  if (!opts->sources)
    {
      opts->sources = source;
      source->id = 0;
    }
  else
    {
      for (s = opts->sources; s->next; s = s->next)
        ;
      source->id = s->id + 1;
      s->next = source;
    }
}

static struct conf_entry *
parse_source (struct opts *opts, struct conf_entry *conf)
{
  char *host = NULL;
  char *port = NULL;
  char *proxy = NULL;
  /* a source entry:
   * source
   *   host <host>
   *   port <port>
   *   [proxy <proxy>]
   * end
   */
  assert (!strcmp (conf->key, "source"));
  conf = conf->next;
  while (conf && strcmp (conf->key, "end"))
    {
      if (!strcmp (conf->key, "host"))
        host = conf->value;
      else if (!strcmp (conf->key, "port"))
        port = conf->value;
      else if (!strcmp (conf->key, "proxy"))
        proxy = conf->value;
      else
        fatal ("malformed config: '%s' in source stanza", conf->key);
      conf = conf->next;
    }
  if (!conf)
    fatal ("unclosed source stanza");
  if (!host || !port)
    fatal ("incomplete source stanza (needs host, port)");
  add_source_to_conf (opts, host, port, proxy);
  return conf;
}

void
load_conf (struct opts *opts)
{
  FILE *f;
  struct conf_entry *conf, *e;
  char *conf_file = opts->conf_file;
  if (!opts->conf_file)
    conf_file = (char *) DEFAULT_CONF_FILE;
  f = fopen (conf_file, "r");
  if (!f)
    {
      if (opts->conf_file)
        {
          pfatal ("can't open conf file '%s'", opts->conf_file);
        }
      else
        {
          pinfo ("can't open conf file '%s'", conf_file);
          return;
        }
    }
  conf = conf_parse (f);
  if (!conf)
    pfatal ("can't parse config file");

  for (e = conf; e; e = e->next)
    {
      if (!strcmp (e->key, "max-tries") && e->value)
        {
          opts->max_tries = atoi (e->value);
        }
      else if (!strcmp (e->key, "min-steady-state-interval") && e->value)
        {
          opts->min_steady_state_interval = atoi (e->value);
        }
      else if (!strcmp (e->key, "wait-between-tries") && e->value)
        {
          opts->wait_between_tries = atoi (e->value);
        }
      else if (!strcmp (e->key, "subprocess-tries") && e->value)
        {
          opts->subprocess_tries = atoi (e->value);
        }
      else if (!strcmp (e->key, "subprocess-wait-between-tries") && e->value)
        {
          opts->subprocess_wait_between_tries = atoi (e->value);
        }
      else if (!strcmp (e->key, "steady-state-interval") && e->value)
        {
          opts->steady_state_interval = atoi (e->value);
        }
      else if (!strcmp (e->key, "base-path") && e->value)
        {
          opts->base_path = strdup (e->value);
          if (!opts->base_path)
            fatal ("out of memory for base path");
        }
      else if (!strcmp (e->key, "should-sync-hwclock"))
        {
          opts->should_sync_hwclock = e->value ? !strcmp (e->value, "yes") : 1;
        }
      else if (!strcmp (e->key, "should-load-disk"))
        {
          opts->should_load_disk = e->value ? !strcmp (e->value, "yes") : 1;
        }
      else if (!strcmp (e->key, "should-save-disk"))
        {
          opts->should_save_disk = e->value ? !strcmp (e->value, "yes") : 1;
        }
      else if (!strcmp (e->key, "should-netlink"))
        {
          opts->should_netlink = e->value ? !strcmp (e->value, "yes") : 1;
        }
      else if (!strcmp (e->key, "dry-run"))
        {
          opts->dry_run = e->value ? !strcmp (e->value, "yes") : 1;
        }
      else if (!strcmp (e->key, "jitter") && e->value)
        {
          opts->jitter = atoi (e->value);
        }
      else if (!strcmp (e->key, "verbose"))
        {
          verbose = e->value ? !strcmp (e->value, "yes") : 1;
        }
      else if (!strcmp (e->key, "source"))
        {
          e = parse_source (opts, e);
        }
     else if (!strcmp (e->key, "leap"))
        {
          opts->leap = e->value ? !strcmp (e->value, "yes") : 1;
        }
   }
}

void
check_conf (struct state *state)
{
  struct opts *opts = &state->opts;
  if (!opts->max_tries)
    fatal ("-t argument must be nonzero");
  if (!opts->wait_between_tries)
    fatal ("-d argument must be nonzero");
  if (!opts->steady_state_interval)
    fatal ("-a argument must be nonzero");
  if (snprintf (state->timestamp_path, sizeof (state->timestamp_path),
                "%s/timestamp", opts->base_path) >= sizeof (state->timestamp_path))
    fatal ("supplied base path is too long: '%s'", opts->base_path);
  if (opts->jitter >= opts->steady_state_interval)
    fatal ("jitter must be less than steady state interval (%d >= %d)",
           opts->jitter, opts->steady_state_interval);
}

int
cleanup_main (struct state *state)
{
  int i;
  for (i = 0; i < E_MAX; ++i)
    {
      struct event *e = state->events[i];
      if (e)
        {
          int fd = event_get_fd (e);
          if (fd >= 0 && ! (event_get_events (e) & EV_SIGNAL))
            close (fd);
          event_free (e);
        }
    }
  /* The other half was closed above. */
  platform->file_close (state->tlsdate_monitor_fd);
  if (state->tlsdate_pid)
    {
      platform->process_signal (state->tlsdate_pid, SIGKILL);
      platform->process_wait (state->tlsdate_pid, NULL, 0 /* !forever */);
    }
  /* Best effort to tear it down if it is still alive. */
  close(state->setter_notify_fd);
  close(state->setter_save_fd);
  if (state->setter_pid)
    {
      platform->process_signal (state->setter_pid, SIGKILL);
      platform->process_wait (state->setter_pid, NULL, 0 /* !forever */);
    }
  /* TODO(wad) Add dbus_cleanup() */
  if (state->base)
    event_base_free (state->base);
  memset(state, 0, sizeof(*state));
  info ("tlsdated clean up finished; exiting!");
  terminate_syslog ();
  return 0;
}

#ifdef TLSDATED_MAIN
int API
main (int argc, char *argv[], char *envp[])
{
  initalize_syslog ();
  struct state state;
  /* TODO(wad) EVENT_BASE_FLAG_PRECISE_TIMER | EVENT_BASE_FLAG_PRECISE_TIMER */
  struct event_base *base = event_base_new();
  if (!base)
    {
      fatal ("could not allocated new event base");
    }
  /* Add three priority levels:
   * 0 - time saving.  Must be done before any other events are handled.
   * 1 - network synchronization events
   * 2 - any other events (wake, platform, etc)
   */
  event_base_priority_init (base, MAX_EVENT_PRIORITIES);
  memset (&state, 0, sizeof (state));
  set_conf_defaults (&state.opts);
  parse_argv (&state.opts, argc, argv);
  check_conf (&state);
  load_conf (&state.opts);
  check_conf (&state);
  if (!state.opts.sources)
    add_source_to_conf (&state.opts, DEFAULT_HOST, DEFAULT_PORT, DEFAULT_PROXY);
  state.base = base;
  state.envp = envp;
  state.backoff = state.opts.wait_between_tries;
  /* TODO(wad) move this into setup_time_setter */
  /* grab a handle to /dev/rtc for time-setter. */
  if (state.opts.should_sync_hwclock &&
      platform->rtc_open(&state.hwclock))
    {
      pinfo ("can't open hwclock fd");
      state.opts.should_sync_hwclock = 0;
    }
  /* install the SIGCHLD handler for the setter and tlsdate */
  if (setup_sigchld_event (&state, 1))
    {
      error ("Failed to setup SIGCHLD event");
      goto out;
    }
  /* fork off the privileged helper */
  verb ("spawning time setting helper . . .");
  if (setup_time_setter (&state))
    {
      error ("could not fork privileged coprocess");
      goto out;
    }
  /* release the hwclock now that the time-setter is running. */
  if (state.opts.should_sync_hwclock)
    {
      platform->rtc_close (&state.hwclock);
    }
  /* drop privileges before touching any untrusted data */
  drop_privs_to (state.opts.user, state.opts.group);
  /* register a signal handler to save time at shutdown */
  if (state.opts.should_save_disk)
    {
      struct event *event = event_new (base, SIGTERM, EV_SIGNAL|EV_PERSIST,
                                       action_sigterm, &state);
      if (!event)
        fatal ("Failed to create SIGTERM event");
      event_priority_set (event, PRI_SAVE);
      event_add (event, NULL);
    }
  if (state.opts.should_dbus && init_dbus (&state))
    {
      error ("Failed to initialize DBus");
      goto out;
    }
  /* Register the tlsdate event before any listeners could show up. */
  state.events[E_TLSDATE] = event_new (base, -1, EV_TIMEOUT,
                                       action_run_tlsdate, &state);
  if (!state.events[E_TLSDATE])
    {
      error ("Failed to create tlsdate event");
      goto out;
    }
  event_priority_set (state.events[E_TLSDATE], PRI_NET);
  /* The timeout and fd will be filled in per-call. */
  if (setup_tlsdate_status (&state))
    {
      error ("Failed to create tlsdate status event");
      goto out;
    }
  /* TODO(wad) Could use a timeout on this to catch setter death? */
  /* EV_READ is for truncation/EPIPE notification */
  state.events[E_SAVE] = event_new (base, state.setter_save_fd,
                                    EV_READ|EV_WRITE, action_sync_and_save,
                                    &state);
  if (!state.events[E_SAVE])
    {
      error ("Failed to create sync & save event");
      goto out;
    }
  event_priority_set (state.events[E_SAVE], PRI_SAVE);
  /* Start by grabbing the system time. */
  state.last_sync_type = SYNC_TYPE_RTC;
  state.last_time = time (NULL);
  /* If possible, grab disk time and check the two. */
  if (state.opts.should_load_disk)
    {
      time_t disk_time = state.last_time;
      if (!load_disk_timestamp (state.timestamp_path, &disk_time))
        {
          verb ("disk timestamp available: yes");
          if (!is_sane_time (state.last_time) ||
              state.last_time < disk_time)
            {
              state.last_sync_type = SYNC_TYPE_DISK;
              state.last_time = disk_time;
            }
        }
      else
        {
          verb ("disk timestamp available: no");
        }
    }
  if (!is_sane_time (state.last_time))
    {
      state.last_sync_type = SYNC_TYPE_BUILD;
      state.last_time = RECENT_COMPILE_DATE + 1;
    }
  /* Save and announce the initial time source. */
  trigger_event (&state, E_SAVE, -1);
  verb ("tlsdated parasitic time synchronization initialized");
  info ("initial time sync type: %s", sync_type_str (state.last_sync_type));
  /* Initialize platform specific loop behavior */
  if (platform_init_cros (&state))
    {
      error ("Failed to initialize platform code");
      goto out;
    }
  if (setup_event_route_up (&state))
    {
      error ("Failed to setup route up monitoring");
      goto out;
    }
  if (setup_event_timer_sync (&state))
    {
      error ("Failed to setup a timer event");
      goto out;
    }
  if (setup_event_timer_continuity (&state))
    {
      error ("Failed to setup continuity timer");
      goto out;
    }
  /* Add a forced sync event to the event list. */
  action_kickoff_time_sync (-1, EV_TIMEOUT, &state);
  verb ("Entering dispatch . . .");
  event_base_dispatch (base);
  verb ("tlsdated event dispatch terminating gracefully");
out:
  return cleanup_main (&state);
}
#endif /* !TLSDATED_MAIN */
