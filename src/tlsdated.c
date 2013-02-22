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
#include <openssl/rand.h>
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

#include "src/conf.h"
#include "src/routeup.h"
#include "src/util.h"
#include "src/tlsdate.h"

const char *kCacheDir = DEFAULT_DAEMON_CACHEDIR;
const char *kTempSuffix = DEFAULT_DAEMON_TMPSUFFIX;

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
  argc += 6;  /* -H host -p port -x proxy */
  new_argv = malloc (argc * sizeof(char *));
  if (!new_argv)
    fatal ("out of memory building argv");
  for (argc = 0; opts->base_argv[argc]; argc++)
    new_argv[argc] = opts->base_argv[argc];
  new_argv[argc++] = "-H";
  new_argv[argc++] = opts->cur_source->host;
  new_argv[argc++] = "-p";
  new_argv[argc++] = opts->cur_source->port;
  if (opts->cur_source->proxy || opts->proxy) {
    new_argv[argc++] = (char *) "-x";
    new_argv[argc++] = opts->proxy ? opts->proxy : opts->cur_source->proxy;
  }
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
  build_argv(opts);
  
  if ((pid = fork ()) > 0)
    {
      /*
       * We launched tlsdate; wait for up to kMaxTries intervals of
       * kWaitBetweenTries for it to exit, then kill it if it still
       * hasn't.
       */
      int status = -1;
      int i = 0;
      for (i = 0; i < opts->subprocess_tries; ++i)
  {
    info ("wait for child attempt %d", i);
    if (waitpid (-1, &status, WNOHANG) > 0)
      break;
    sleep (opts->subprocess_wait_between_tries);
  }
      if (i == opts->subprocess_tries)
  {
    error ("child hung?");
    kill (pid, SIGKILL);
    /* still have to wait() so we don't leak the child. */
    wait (&status);
    return -1;
  }
      info ("child exited with %d", status);
      return WIFEXITED (status) ? WEXITSTATUS (status) : -1;
    }
  else if (pid < 0)
    {
      pinfo ("fork() failed");
      return -1;
    }
  execve (opts->argv[0], opts->argv, envp);
  pinfo ("execve() failed");
  exit (1);
}

/*
 * Load a time value out of the file named by path. Returns 0 if successful,
 * -1 if not. The file contains the time in seconds since epoch in host byte
 * order.
 */
int
load_disk_timestamp (const char *path, time_t * t)
{
  int fd = open (path, O_RDONLY | O_NOFOLLOW);
  time_t tmpt;
  if (fd < 0)
    {
      perror ("Can't open %s for reading", path);
      return -1;
    }
  if (read (fd, &tmpt, sizeof (tmpt)) != sizeof (tmpt))
    {
      perror ("Can't read seconds from %s", path);
      close (fd);
      return -1;
    }
  close (fd);
  if (!is_sane_time (tmpt))
    {
      perror ("Timevalue not sane: %lu", tmpt);
      return -1;
    }
  *t = tmpt;
  return 0;
}

/* Save a time value to the file named by path. */
void
save_disk_timestamp (const char *path, time_t t)
{
  char tmp[PATH_MAX];
  int fd;

  if (snprintf (tmp, sizeof (tmp), "%s%s", path, kTempSuffix) >= sizeof (tmp))
    {
      pinfo ("Path %s too long to use", path);
      exit (1);
    }

  if ((fd = open (tmp, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC,
      S_IRUSR | S_IWUSR)) < 0)
    {
      pinfo ("open failed");
      return;
    }
  if (write (fd, &t, sizeof (t)) != sizeof (t))
    {
      pinfo ("write failed");
      close (fd);
      return;
    }
  if (close (fd))
    {
      pinfo ("close failed");
      return;
    }
  if (rename (tmp, path))
    pinfo ("rename failed");
}

/*
 * Set the hardware clock referred to by fd (which should be a descriptor to
 * some device that implements the interface documented in rtc(4)) to the system
 * time. See hwclock(8) for details of why this is important. If we fail, we
 * just return - there's nothing the caller can really do about a failure of
 * this function except try later.
 */
void
sync_hwclock (int fd)
{
  struct timeval tv;
  struct tm *tm;
  struct rtc_time rtctm;

  if (gettimeofday (&tv, NULL))
    {
      pinfo ("gettimeofday() failed");
      return;
    }

  tm = gmtime (&tv.tv_sec);

  /* these structs are identical, but separately defined */
  rtctm.tm_sec = tm->tm_sec;
  rtctm.tm_min = tm->tm_min;
  rtctm.tm_hour = tm->tm_hour;
  rtctm.tm_mday = tm->tm_mday;
  rtctm.tm_mon = tm->tm_mon;
  rtctm.tm_year = tm->tm_year;
  rtctm.tm_wday = tm->tm_wday;
  rtctm.tm_yday = tm->tm_yday;
  rtctm.tm_isdst = tm->tm_isdst;

  if (ioctl (fd, RTC_SET_TIME, &rtctm))
    {
      pinfo ("ioctl(%d, RTC_SET_TIME, ...) failed", fd);
      return;
    }

  info ("synced rtc to sysclock");
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
sync_and_save (int hwclock_fd, int should_sync, int should_save)
{
  struct timeval tv;
  if (should_sync)
    sync_hwclock (hwclock_fd);
  if (should_save)
    {
      if (gettimeofday (&tv, NULL))
  pfatal ("gettimeofday() failed");
      save_disk_timestamp (timestamp_path, tv.tv_sec);
    }
}

int
add_jitter (int base, int jitter)
{
  int n = 0;
  if (!jitter)
    return base;
  if (RAND_bytes((unsigned char *)&n, sizeof(n)) != 1)
    fatal ("RAND_bytes() failed");
  return base + (abs(n) % (2 * jitter)) - jitter;
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
  if (gettimeofday (&tv, NULL))
    /* can't use stdio or syslog inside a sig handler */
    exit (2);
  save_disk_timestamp (timestamp_path, tv.tv_sec);
  exit (0);
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
    DEFAULT_TLSDATE, "-H", DEFAULT_HOST, NULL
  };
  opts->max_tries = MAX_TRIES;
  opts->min_steady_state_interval = STEADY_STATE_INTERVAL;
  opts->wait_between_tries = WAIT_BETWEEN_TRIES;
  opts->subprocess_tries = SUBPROCESS_TRIES;
  opts->subprocess_wait_between_tries = SUBPROCESS_WAIT_BETWEEN_TRIES;
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
}

void
parse_argv(struct opts *opts, int argc, char *argv[])
{
  int opt;

  while ((opt = getopt (argc, argv, "hwrpt:d:T:D:c:a:lsvm:j:f:x:")) != -1)
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
  struct source *source = malloc (sizeof *source);
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
    conf_file = DEFAULT_CONF_FILE;
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
    } else if (!strcmp (e->key, "subprocess-tries") && e->value) {
      opts->subprocess_tries = atoi (e->value);
    } else if (!strcmp (e->key, "subprocess-wait-between-tries") && e->value) {
      opts->subprocess_wait_between_tries = atoi (e->value);
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
  if (!opts->subprocess_tries)
    fatal ("-T argument must be nonzero");
  if (!opts->subprocess_wait_between_tries)
    fatal ("-D argument must be nonzero");
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

int API
main (int argc, char *argv[], char *envp[])
{
  struct routeup rtc;
  int hwclock_fd = -1;
  time_t last_success = 0;
  struct opts opts;
  int wait_time = 0;

  set_conf_defaults(&opts);
  parse_argv(&opts, argc, argv);
  check_conf(&opts);
  load_conf(&opts);
  check_conf(&opts);
  if (!opts.sources)
    add_source_to_conf(&opts, DEFAULT_HOST, DEFAULT_PORT, DEFAULT_PROXY);

  /* grab a handle to /dev/rtc for sync_hwclock() */
  if (opts.should_sync_hwclock && (hwclock_fd = open (DEFAULT_RTC_DEVICE, O_RDONLY)) < 0)
    {
      pinfo ("can't open hwclock fd");
      opts.should_sync_hwclock = 0;
    }

  /* set up a netlink context if we need one */
  if (opts.should_netlink && routeup_setup (&rtc))
    pfatal ("Can't open netlink socket");

  if (!is_sane_time (time (NULL)))
    {
      struct timeval tv = { 0, 0 };
      /*
       * If the time is before the build timestamp, we're probably on
       * a system with a broken rtc. Try loading the timestamp off
       * disk.
       */
      tv.tv_sec = RECENT_COMPILE_DATE;
      if (opts.should_load_disk &&
    load_disk_timestamp (timestamp_path, &tv.tv_sec))
  pinfo ("can't load disk timestamp");
      if (!opts.dry_run && settimeofday (&tv, NULL))
  pfatal ("settimeofday() failed");
      dbus_announce();
      /*
       * don't save here - we either just loaded this time or used the
       * default time, and neither of those are good to save
       */
      sync_and_save (hwclock_fd, opts.should_sync_hwclock, 0);
    }

  /* register a signal handler to save time at shutdown */
  if (opts.should_save_disk)
    signal (SIGTERM, sigterm_handler);

  /*
   * Try once right away. If we fail, wait for a route to appear, then try
   * for a while; repeat whenever another route appears. Try until we
   * succeed.
   */
  if (!tlsdate (&opts, envp)) {
    last_success = time (NULL);
    sync_and_save (hwclock_fd, opts.should_sync_hwclock, opts.should_save_disk);
    dbus_announce();
  }

  /*
   * Loop until we catch a fatal signal or routeup_once() fails. We run
   * tlsdate at least once a day, but possibly as often as routes come up;
   * this should handle cases like a VPN being brought up and down
   * periodically.
   */
  wait_time = add_jitter(opts.steady_state_interval, opts.jitter);
  while (wait_for_event (&rtc, opts.should_netlink, wait_time) >= 0)
    {
      /*
       * If a route just came up, run tlsdate; if it
       * succeeded, then we're good and can keep time locally
       * from now on.
       */
      int i;
      int backoff = opts.wait_between_tries;
      wait_time = add_jitter(opts.steady_state_interval, opts.jitter);
      if (time (NULL) - last_success < opts.min_steady_state_interval)
        continue;
      for (i = 0; i < opts.max_tries && tlsdate (&opts, envp); ++i) {
        if (backoff < 1)
          fatal ("backoff too small? %d", backoff);
        sleep (backoff);
        if (backoff < MAX_SANE_BACKOFF)
          backoff *= 2;
      }
      if (i != opts.max_tries)
      {
        last_success = time (NULL);
        info ("tlsdate succeeded");
        sync_and_save (hwclock_fd, opts.should_sync_hwclock, opts.should_save_disk);
        dbus_announce();
      }
    }

  return 1;
}
#endif /* !TLSDATED_MAIN */
