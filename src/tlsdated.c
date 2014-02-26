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
#include <fcntl.h>
#include <limits.h>
#include <linux/rtc.h>
#include <stdarg.h>
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

#ifdef USE_POLARSSL
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#else
#include <openssl/rand.h>
#endif

#include "src/conf.h"
#include "src/event.h"
#include "src/routeup.h"
#include "src/util.h"
#include "src/tlsdate.h"

const char *kCacheDir = DEFAULT_DAEMON_CACHEDIR;

int
is_sane_time (time_t ts)
{
  return ts > RECENT_COMPILE_DATE && ts < TLSDATED_MAX_DATE;
}

void
build_argv(struct opts *opts)
{
  int argc;
  char **new_argv;

  assert(opts->sources);
  /* choose the next source in the list; if we're at the end, start over. */
  if (!opts->cur_source || !opts->cur_source->next)
    opts->cur_source = opts->sources;
  else
    opts->cur_source = opts->cur_source->next;

  if (opts->argv) {
    free(opts->argv);
    opts->argv = NULL;
  }

  for (argc = 0; opts->base_argv[argc]; argc++)
    ;
  argc++; /* uncounted null terminator */
  argc += 8;  /* -H host -p port -x proxy -v -l */
  new_argv = malloc (argc * sizeof(char *));
  if (!new_argv)
    fatal ("out of memory building argv");
  for (argc = 0; opts->base_argv[argc]; argc++)
    new_argv[argc] = opts->base_argv[argc];
  new_argv[argc++] = (char *) "-H";
  new_argv[argc++] = opts->cur_source->host;
  new_argv[argc++] = (char *) "-p";
  new_argv[argc++] = opts->cur_source->port;
  if (opts->cur_source->proxy || opts->proxy) {
    new_argv[argc++] = (char *) "-x";
    new_argv[argc++] = opts->proxy ? opts->proxy : opts->cur_source->proxy;
  }
  if (verbose)
    new_argv[argc++] = "-v";
  if (opts->leap)
    new_argv[argc++] = "-l";
  new_argv[argc++] = NULL;
  opts->argv = new_argv;
}

/*
 * Run tlsdate in a child process. We fork it off, then wait a specified time
 * for it to exit; if it hasn't exited by then, we kill it and log a baffled
 * message. We return tlsdate's exit code if tlsdate exits normally, and a
 * negative value if we can't launch it or it exits uncleanly, so this function
 * returns 0 for success and nonzero for failure, with >0 being a tlsdate exit
 * code and <0 being an exec/fork/wait error.
 */
int
tlsdate (struct opts *opts, char *envp[])
{
  pid_t pid;
  pid_t exited;
  int status;

  build_argv(opts);

  pid = fork();
  if (pid < 0)
  {
    pinfo("fork() failed");
    return -1;
  }
  else if (pid == 0)
  {
    execve(opts->argv[0], opts->argv, envp);
    pinfo("execve() failed");
    exit(1);
  }

  exited = wait_with_timeout(&status, opts->subprocess_timeout);
  info("child %d exited with %d", exited, status);

  if (exited == -ETIMEDOUT)
  {
    kill(pid, SIGKILL);
    wait(&status);
    return -1;
  }

  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/*
 * Load a time value out of the file named by path. Returns 0 if successful,
 * -1 if not. The file contains the time in seconds since epoch in host byte
 * order.
 */
int
load_disk_timestamp (const char *path, time_t * t)
{
  time_t tmpt;
  if (platform->file_read(path, &tmpt, sizeof(tmpt))) {
    info("can't load time file");
    return -1;
  }
  if (!is_sane_time(tmpt)) {
    info("time %lu not sane", tmpt);
    return -1;
  }
  *t = tmpt;
  return 0;
}

/* Save a time value to the file named by path. */
void
save_disk_timestamp (const char *path, time_t t)
{
  if (platform->file_write(path, &t, sizeof(t)))
    info("saving disk timestamp failed");
}

/*
 * Set the hardware clock referred to by fd (which should be a descriptor to
 * some device that implements the interface documented in rtc(4)) to the system
 * time. See hwclock(8) for details of why this is important. If we fail, we
 * just return - there's nothing the caller can really do about a failure of
 * this function except try later.
 */
void
sync_hwclock (void *rtc_handle)
{
  struct timeval tv;
  if (platform->time_get(&tv))
  {
    pinfo("gettimeofday() failed");
    return;
  }

  if (platform->rtc_write(rtc_handle, &tv))
    info("rtc_write() failed");
}

/*
 * Wait for a single event to happen. If should_netlink is true, we ask the
 * supplied routeup context to wait for an event; otherwise, we wait for a byte
 * of input on stdin. Semantics (for return value) are the same as for
 * routeup_once().
 */
int
wait_for_event (struct routeup *rtc, int should_netlink, int timeout)
{
  char buf[1];
  fd_set fds;
  struct timeval tv;
  int r;

  if (should_netlink)
    return routeup_once (rtc, timeout);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  FD_ZERO (&fds);
  FD_SET (STDIN_FILENO, &fds);
  r = select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
  if (r > 0)
    return read (STDIN_FILENO, buf, sizeof (buf)) != 1;
  return r == 0 ? 1 : -1;
}

char timestamp_path[PATH_MAX];

void
sync_and_save (void *hwclock_handle, int should_save)
{
  struct timeval tv;
  if (hwclock_handle)
    sync_hwclock (hwclock_handle);
  if (should_save)
    {
      if (platform->time_get(&tv))
  pfatal ("gettimeofday() failed");
      save_disk_timestamp (timestamp_path, tv.tv_sec);
    }
}

#ifdef USE_POLARSSL
static int random_init = 0;
static entropy_context entropy;
static ctr_drbg_context ctr_drbg;
static char *pers = "tlsdated";
#endif

int
add_jitter (int base, int jitter)
{
  int n = 0;
  if (!jitter)
    return base;
#ifdef USE_POLARSSL
  if (0 == random_init)
  {
    entropy_init(&entropy);
    if (0 > ctr_drbg_init(&ctr_drbg, entropy_func, &entropy,
                          (unsigned char *) pers, strlen(pers)))
    {
      pfatal ("Failed to initialize random source");
    }
    random_init = 1;
  }
  if (0 != ctr_drbg_random(&ctr_drbg, (unsigned char *)&n, sizeof(n)))
    fatal ("ctr_drbg_random() failed");
#else
  if (RAND_bytes((unsigned char *)&n, sizeof(n)) != 1)
    fatal ("RAND_bytes() failed");
#endif
  return base + (abs(n) % (2 * jitter)) - jitter;
}

int
calc_wait_time (const struct opts *opts)
{
  return add_jitter (opts->steady_state_interval, opts->jitter);
}

#ifdef TLSDATED_MAIN
#ifdef HAVE_DBUS
void
dbus_announce (void)
{
  char *argv[] = { TLSDATE_DBUS_ANNOUNCE, NULL };
  pid_t pid = fork();
  if (!pid) {
    drop_privs_to (DBUS_USER, DBUS_GROUP);
    pid = fork();
    if (!pid)
      exit(execve(argv[0], argv, NULL));
    else
      exit(pid < 0);
  } else if (pid > 0) {
    wait(NULL);
  }
}
#else
void dbus_announce(void)
{
}
#endif

void
sigterm_handler (int _unused)
{
  struct timeval tv;
  platform->pgrp_kill();
  if (gettimeofday (&tv, NULL))
    /* can't use stdio or syslog inside a sig handler */
    exit (2);
  save_disk_timestamp (timestamp_path, tv.tv_sec);
  exit (0);
}

void
sigterm_handler_nosave (int _unused)
{
  platform->pgrp_kill();
  exit(0);
}

void
usage (const char *progn)
{
  printf ("Usage: %s [flags...] [--] [tlsdate command...]\n", progn);
  printf ("  -w        don't set hwclock\n");
  printf ("  -p        dry run (don't really set time)\n");
  printf ("  -r        use stdin instead of netlink for routes\n");
  printf ("  -t <n>    try n times when a new route appears\n");
  printf ("  -d <n>    delay n seconds between tries\n");
  printf ("  -T <n>    give subprocess n chances to exit\n");
  printf ("  -D <n>    delay n seconds between wait attempts\n");
  printf ("  -c <path> set the cache directory\n");
  printf ("  -a <n>    run at most every n seconds in steady state\n");
  printf ("  -m <n>    run at most once every n seconds in steady state\n");
  printf ("  -j <n>    add up to n seconds jitter to steady state checks\n");
  printf ("  -l        don't load disk timestamps\n");
  printf ("  -s        don't save disk timestamps\n");
  printf ("  -v        be verbose\n");
  printf ("  -x <h>    set proxy for subprocs to h\n");
  printf ("  -h        this\n");
}

void
set_conf_defaults(struct opts *opts)
{
  static char *kDefaultArgv[] = {
    (char *) DEFAULT_TLSDATE, (char *) "-H", (char *) DEFAULT_HOST, NULL
  };
  opts->max_tries = MAX_TRIES;
  opts->min_steady_state_interval = STEADY_STATE_INTERVAL;
  opts->wait_between_tries = WAIT_BETWEEN_TRIES;
  opts->subprocess_timeout = SUBPROCESS_TIMEOUT;
  opts->steady_state_interval = STEADY_STATE_INTERVAL;
  opts->base_path = kCacheDir;
  opts->base_argv = kDefaultArgv;
  opts->argv = NULL;
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
parse_argv(struct opts *opts, int argc, char *argv[])
{
  int opt;

  while ((opt = getopt (argc, argv, "hwrpt:d:c:a:lsvm:j:f:x:")) != -1)
    {
      switch (opt)
  {
  case 'w':
    opts->should_sync_hwclock = 0;
    break;
  case 'r':
    opts->should_netlink = 0;
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
void add_source_to_conf(struct opts *opts, char *host, char *port, char *proxy)
{
  struct source *s;
  struct source *source = (struct source *) malloc (sizeof *source);
  if (!source)
    fatal ("out of memory for source");
  source->host = strdup (host);
  if (!source->host)
    fatal ("out of memory for host");
  source->port = strdup (port);
  if (!source->port)
    fatal ("out of memory for port");
  if (proxy) {
    source->proxy = strdup (proxy);
    if (!source->proxy)
      fatal ("out of memory for proxy");
  }
  if (!opts->sources) {
    opts->sources = source;
  } else {
    for (s = opts->sources; s->next; s = s->next)
      ;
    s->next = source;
  }
}

static struct conf_entry *
parse_source(struct opts *opts, struct conf_entry *conf)
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
  assert(!strcmp(conf->key, "source"));
  conf = conf->next;
  while (conf && strcmp(conf->key, "end")) {
    if (!strcmp(conf->key, "host"))
      host = conf->value;
    else if (!strcmp(conf->key, "port"))
      port = conf->value;
    else if (!strcmp(conf->key, "proxy"))
      proxy = conf->value;
    else
      fatal ("malformed config: '%s' in source stanza", conf->key);
    conf = conf->next;
  }
  if (!conf)
    fatal ("unclosed source stanza");
  if (!host || !port)
    fatal ("incomplete source stanza (needs host, port)");
  add_source_to_conf(opts, host, port, proxy);
  return conf;
}

void
load_conf(struct opts *opts)
{
  FILE *f;
  struct conf_entry *conf, *e;
  char *conf_file = opts->conf_file;
  if (!opts->conf_file)
    conf_file = (char *) DEFAULT_CONF_FILE;
  f = fopen (conf_file, "r");
  if (!f) {
    if (opts->conf_file) {
      pfatal ("can't open conf file '%s'", opts->conf_file);
    } else {
      pinfo ("can't open conf file '%s'", conf_file);
      return;
    }
  }
  conf = conf_parse (f);
  if (!conf)
    pfatal ("can't parse config file");

  for (e = conf; e; e = e->next) {
    if (!strcmp (e->key, "max-tries") && e->value) {
      opts->max_tries = atoi (e->value);
    } else if (!strcmp (e->key, "min-steady-state-interval") && e->value) {
      opts->min_steady_state_interval = atoi (e->value);
    } else if (!strcmp (e->key, "wait-between-tries") && e->value) {
      opts->wait_between_tries = atoi (e->value);
    } else if (!strcmp (e->key, "subprocess-timeout") && e->value) {
      opts->subprocess_timeout = atoi (e->value);
    } else if (!strcmp (e->key, "steady-state-interval") && e->value) {
      opts->steady_state_interval = atoi (e->value);
    } else if (!strcmp (e->key, "base-path") && e->value) {
      opts->base_path = strdup (e->value);
      if (!opts->base_path)
        fatal ("out of memory for base path");
    } else if (!strcmp (e->key, "should-sync-hwclock")) {
      opts->should_sync_hwclock = e->value ? !strcmp(e->value, "yes") : 1;
    } else if (!strcmp (e->key, "should-load-disk")) {
      opts->should_load_disk = e->value ? !strcmp(e->value, "yes") : 1;
    } else if (!strcmp (e->key, "should-save-disk")) {
      opts->should_save_disk = e->value ? !strcmp(e->value, "yes") : 1;
    } else if (!strcmp (e->key, "should-netlink")) {
      opts->should_netlink = e->value ? !strcmp(e->value, "yes") : 1;
    } else if (!strcmp (e->key, "dry-run")) {
      opts->dry_run = e->value ? !strcmp(e->value, "yes") : 1;
    } else if (!strcmp (e->key, "jitter") && e->value) {
      opts->jitter = atoi (e->value);
    } else if (!strcmp (e->key, "verbose")) {
      verbose = e->value ? !strcmp(e->value, "yes") : 1;
    } else if (!strcmp (e->key, "source")) {
      e = parse_source(opts, e);
    } else if (!strcmp (e->key, "leap")) {
      opts->leap = e->value ? !strcmp(e->value, "yes") : 1;
    }
  }
}

void
check_conf(struct opts *opts)
{
  if (!opts->max_tries)
    fatal ("-t argument must be nonzero");
  if (!opts->wait_between_tries)
    fatal ("-d argument must be nonzero");
  if (!opts->subprocess_timeout)
    fatal ("subprocess timeout must be nonzero");
  if (!opts->steady_state_interval)
    fatal ("-a argument must be nonzero");
  if (snprintf (timestamp_path, sizeof (timestamp_path), "%s/timestamp",
    opts->base_path) >= sizeof (timestamp_path))
    fatal ("supplied base path is too long: '%s'", opts->base_path);
  if (strlen (timestamp_path) + strlen (kTempSuffix) >= PATH_MAX)
    fatal ("supplied base path is too long: '%s'", opts->base_path);
  if (opts->jitter >= opts->steady_state_interval)
    fatal ("jitter must be less than steady state interval (%d >= %d)",
           opts->jitter, opts->steady_state_interval);
}

struct tlsdated_state
{
  struct routeup rtc;
  time_t last_success;
  struct opts *opts;
  char *envp[];
};

static time_t now(void)
{
  struct timeval tv;
  if (!platform->time_get(&tv))
    return 0;
  return tv.tv_sec;
}

int tlsdate_retry(struct opts *opts, char *envp[])
{
  int backoff = opts->wait_between_tries;
  int i;

  for (i = 0; i < opts->max_tries; i++)
  {
    if (!tlsdate(opts, envp))
      return 0;
    if (backoff < 1)
      fatal("backoff too small? %d", backoff);
    sleep(backoff);
    if (backoff < MAX_SANE_BACKOFF)
      backoff *= 2;
  }
  return 1;
}

int API
main (int argc, char *argv[], char *envp[])
{
  void *hwclock_handle = NULL;
  time_t last_success = 0;
  struct opts opts;
  struct timeval tv = { 0, 0 };
  int r;

  struct event *routeup = NULL;
  struct event *suspend = NULL;
  struct event *periodic = NULL; /* XXX */
  struct event *composite = event_composite();

  if (platform->pgrp_enter())
   pfatal("pgrp_enter() failed");

  suspend = event_suspend();

  event_composite_add(composite, suspend);

  set_conf_defaults(&opts);
  parse_argv(&opts, argc, argv);
  check_conf(&opts);
  load_conf(&opts);
  check_conf(&opts);

  periodic = event_every(opts.steady_state_interval);
  event_composite_add(composite, periodic);

  info ("started up, loaded config file");

  if (!opts.should_load_disk || load_disk_timestamp (timestamp_path, &tv.tv_sec))
    info ("sysclock %lu, no cached time", time(NULL));
  else
    info ("sysclock %lu, cached time %lu", time(NULL), tv.tv_sec);

  if (!opts.sources)
    add_source_to_conf(&opts, (char *) DEFAULT_HOST, (char *) DEFAULT_PORT,
                              (char *) DEFAULT_PROXY);

  /* grab a handle to /dev/rtc for sync_hwclock() */
  if (opts.should_sync_hwclock && !(hwclock_handle = platform->rtc_open()))
    pinfo ("can't open hwclock fd");

  if (opts.should_netlink)
    routeup = event_routeup();
  else
    routeup = event_fdread(STDIN_FILENO);
  event_composite_add(composite, routeup);

  if (!routeup)
    pfatal ("Can't open netlink socket");

  if (!is_sane_time (time (NULL)))
  {
    /*
     * If the time is before the build timestamp, we're probably on
     * a system with a broken rtc. Try loading the timestamp off
     * disk.
     */
    tv.tv_sec = RECENT_COMPILE_DATE;
    if (opts.should_load_disk
        && load_disk_timestamp (timestamp_path, &tv.tv_sec))
      pinfo ("can't load disk timestamp");
    if (!opts.dry_run && settimeofday (&tv, NULL))
      pfatal ("settimeofday() failed");
    dbus_announce();
    /*
     * don't save here - we either just loaded this time or used the
     * default time, and neither of those are good to save
     */
    sync_and_save (hwclock_handle, 0);
  }

  if (opts.should_save_disk)
    signal (SIGTERM, sigterm_handler);
  else
    signal (SIGTERM, sigterm_handler_nosave);

  /*
   * Try once right away. If we fail, wait for a route to appear, then try
   * for a while; repeat whenever another route appears. Try until we
   * succeed.
   */
  if (!tlsdate (&opts, envp))
  {
    last_success = time (NULL);
    sync_and_save (hwclock_handle, opts.should_save_disk);
    dbus_announce();
  }

  /*
   * Loop until we catch a fatal signal or routeup_once() fails. We run
   * tlsdate at least once a day, but possibly as often as routes come up;
   * this should handle cases like a VPN being brought up and down
   * periodically.
   */

  /*
   * The event loop.
   * When we start up, we may have no time fix at all; we'll call this "active
   * mode", where we are actively looking for opportunities to get a time fix.
   * Once we get a time fix, we go into "passive mode", where we're looking to
   * prevent clock drift or rtc corruption. The difference between these two
   * modes is whether we're aggressive about checking for time after network
   * routes come up.
   *
   * To do this, we create some event sources:
   *   e0 = event_routeup()
   *   e1 = event_suspend()
   *   e2 = event_every(interval)
   * Then we create a couple of composite events:
   *   active = event_anyof(3, e0, e1, e2)
   * Whenever our wait for an event returns, we start checking (i.e., running
   * tlsdate with exponential backoff until it succeeds). If checking succeeds
   * and we were in active state, we go to passive state; otherwise we return to
   * our previous state.
   */

  while ((r = event_wait(composite)))
  {
    if (r < 0)
    {
      info("event_wait() failed: %d", r);
      continue;
    }
    if (now() - last_success < opts.min_steady_state_interval)
    {
      info("too soon");
      continue;
    }
    if (!tlsdate_retry(&opts, envp))
    {
      last_success = now();
      info("tlsdate succeeded");
      sync_and_save (hwclock_handle, opts.should_save_disk);
      dbus_announce();
    }
  }

  info ("exiting");
  platform->pgrp_kill();
  return 1;
}
#endif /* !TLSDATED_MAIN */
