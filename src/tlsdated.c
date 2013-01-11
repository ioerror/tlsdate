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

/*
 * Run tlsdate in a child process. We fork it off, then wait a specified time
 * for it to exit; if it hasn't exited by then, we kill it and log a baffled
 * message. We return tlsdate's exit code if tlsdate exits normally, and a
 * negative value if we can't launch it or it exits uncleanly, so this function
 * returns 0 for success and nonzero for failure, with >0 being a tlsdate exit
 * code and <0 being an exec/fork/wait error.
 */
int
tlsdate (char *argv[], char *envp[], int tries, int wait_between_tries)
{
  pid_t pid;
  if ((pid = fork ()) > 0)
    {
      /*
       * We launched tlsdate; wait for up to kMaxTries intervals of
       * kWaitBetweenTries for it to exit, then kill it if it still
       * hasn't.
       */
      int status = -1;
      int i = 0;
      for (i = 0; i < tries; ++i)
  {
    info ("wait for child attempt %d", i);
    if (waitpid (-1, &status, WNOHANG) > 0)
      break;
    sleep (wait_between_tries);
  }
      if (i == tries)
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
  execve (argv[0], argv, envp);
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
      S_IRWXU)) < 0)
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
  printf ("  -l        don't load disk timestamps\n");
  printf ("  -s        don't save disk timestamps\n");
  printf ("  -v        be verbose\n");
  printf ("  -h        this\n");
}

int API
main (int argc, char *argv[], char *envp[])
{
  struct routeup rtc;
  int max_tries = MAX_TRIES;
  int min_steady_state_interval = STEADY_STATE_INTERVAL;
  int wait_between_tries = WAIT_BETWEEN_TRIES;
  int subprocess_tries = SUBPROCESS_TRIES;
  int subprocess_wait_between_tries = SUBPROCESS_WAIT_BETWEEN_TRIES;
  int steady_state_interval = STEADY_STATE_INTERVAL;
  const char *base_path = kCacheDir;
  int hwclock_fd = -1;
  static char *kDefaultArgv[] = {
    DEFAULT_TLSDATE, "-H", DEFAULT_HOST, NULL
  };
  char **tlsdate_argv = kDefaultArgv;
  int should_sync_hwclock = DEFAULT_SYNC_HWCLOCK;
  int should_load_disk = DEFAULT_LOAD_FROM_DISK;
  int should_save_disk = DEFAULT_SAVE_TO_DISK;
  int should_netlink = DEFAULT_USE_NETLINK;
  int dry_run = DEFAULT_DRY_RUN;
  time_t last_success = 0;

  /* Parse arguments */
  int opt;
  while ((opt = getopt (argc, argv, "hwrpt:d:T:D:c:a:lsvm:")) != -1)
    {
      switch (opt)
  {
  case 'w':
    should_sync_hwclock = 0;
    break;
  case 'r':
    should_netlink = 0;
    break;
  case 'p':
    dry_run = 1;
    break;
  case 't':
    max_tries = atoi (optarg);
    break;
  case 'd':
    wait_between_tries = atoi (optarg);
    break;
  case 'T':
    subprocess_tries = atoi (optarg);
    break;
  case 'D':
    subprocess_wait_between_tries = atoi (optarg);
    break;
  case 'c':
    base_path = optarg;
    break;
  case 'a':
    steady_state_interval = atoi (optarg);
    break;
  case 'l':
    should_load_disk = 0;
    break;
  case 's':
    should_save_disk = 0;
    break;
  case 'v':
    verbose = 1;
    break;
  case 'm':
    min_steady_state_interval = atoi (optarg);
    break;
  case 'h':
  default:
    usage (argv[0]);
    exit (1);
  }
    }

  if (optind < argc)
    tlsdate_argv = argv + optind;

  /* Validate arguments */
  if (!max_tries)
    fatal ("-t argument must be nonzero");
  if (!wait_between_tries)
    fatal ("-d argument must be nonzero");
  if (!subprocess_tries)
    fatal ("-T argument must be nonzero");
  if (!subprocess_wait_between_tries)
    fatal ("-D argument must be nonzero");
  if (!steady_state_interval)
    fatal ("-a argument must be nonzero");
  if (snprintf (timestamp_path, sizeof (timestamp_path), "%s/timestamp",
    base_path) >= sizeof (timestamp_path))
    fatal ("supplied base path is too long: '%s'", base_path);
  if (strlen (timestamp_path) + strlen (kTempSuffix) >= PATH_MAX)
    fatal ("supplied base path is too long: '%s'", base_path);

  /* grab a handle to /dev/rtc for sync_hwclock() */
  if (should_sync_hwclock && (hwclock_fd = open (DEFAULT_RTC_DEVICE, O_RDONLY)) < 0)
    {
      pinfo ("can't open hwclock fd");
      should_sync_hwclock = 0;
    }

  /* set up a netlink context if we need one */
  if (should_netlink && routeup_setup (&rtc))
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
      if (should_load_disk &&
    load_disk_timestamp (timestamp_path, &tv.tv_sec))
  pinfo ("can't load disk timestamp");
      if (!dry_run && settimeofday (&tv, NULL))
  pfatal ("settimeofday() failed");
      dbus_announce();
      /*
       * don't save here - we either just loaded this time or used the
       * default time, and neither of those are good to save
       */
      sync_and_save (hwclock_fd, should_sync_hwclock, 0);
    }

  /* register a signal handler to save time at shutdown */
  if (should_save_disk)
    signal (SIGTERM, sigterm_handler);

  /*
   * Try once right away. If we fail, wait for a route to appear, then try
   * for a while; repeat whenever another route appears. Try until we
   * succeed.
   */
  if (!tlsdate (tlsdate_argv, envp, subprocess_tries,
    subprocess_wait_between_tries)) {
    last_success = time (NULL);
    sync_and_save (hwclock_fd, should_sync_hwclock, should_save_disk);
    dbus_announce();
  }

  /*
   * Loop until we catch a fatal signal or routeup_once() fails. We run
   * tlsdate at least once a day, but possibly as often as routes come up;
   * this should handle cases like a VPN being brought up and down
   * periodically.
   */
  while (wait_for_event (&rtc, should_netlink, steady_state_interval) >= 0)
    {
      /*
       * If a route just came up, run tlsdate; if it
       * succeeded, then we're good and can keep time locally
       * from now on.
       */
      int i;
      if (time (NULL) - last_success < min_steady_state_interval)
        continue;
      for (i = 0; i < max_tries &&
     tlsdate (tlsdate_argv, envp, subprocess_tries,
        subprocess_wait_between_tries); ++i)
  sleep (wait_between_tries);
      if (i != max_tries)
  {
    last_success = time (NULL);
    info ("tlsdate succeeded");
    sync_and_save (hwclock_fd, should_sync_hwclock, should_save_disk);
    dbus_announce();
  }
    }

  return 1;
}
#endif /* !TLSDATED_MAIN */
