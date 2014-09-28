/*
 * tlsdated-unittest.c - tlsdated unit tests
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include "src/test_harness.h"
#include "src/tlsdate.h"
#include "src/util.h"

#include <event2/event.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

FIXTURE (tempdir)
{
  char path[PATH_MAX];
};

FIXTURE_SETUP (tempdir)
{
  char *p;
  strncpy (self->path, "/tmp/tlsdated-unit-XXXXXX", sizeof (self->path));
  p = mkdtemp (self->path);
  ASSERT_NE (NULL, p);
}

FIXTURE_TEARDOWN(tempdir) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s/load", self->path);
  unlink(buf);
  snprintf(buf, sizeof(buf), "%s/save", self->path);
  unlink(buf);
  ASSERT_EQ(0, rmdir(self->path));
}

int write_time (const char *path, time_t time)
{
  int fd = open (path, O_WRONLY | O_TRUNC | O_CREAT, 0600);
  if (fd == -1)
    return 1;
  if (save_timestamp_to_fd (fd, time))
    return 1;
  if (write (fd, &time, sizeof (time)) != sizeof (time))
    {
      close (fd);
      return 1;
    }
  return close (fd);
}

int read_time (const char *path, time_t* time)
{
  int fd = open (path, O_RDONLY);
  if (fd == -1)
    return 1;
  if (read (fd, time, sizeof (*time)) != sizeof (*time))
    {
      close (fd);
      return 1;
    }
  return close (fd);
}

TEST (sane_time)
{
  ASSERT_EQ (0, is_sane_time (0));
  ASSERT_EQ (0, is_sane_time (INT_MAX));
}

TEST (sane_host_time)
{
  ASSERT_EQ (1, is_sane_time (time (NULL)));
}

TEST_F (tempdir, load_time)
{
  char buf[PATH_MAX];
  time_t tm = 3;
  time_t now = time (NULL);
  snprintf (buf, sizeof (buf), "%s/load", self->path);
  ASSERT_EQ (0, write_time (buf, 0));
  ASSERT_EQ (-1, load_disk_timestamp (buf, &tm));
  ASSERT_EQ (3, tm);
  ASSERT_EQ (0, write_time (buf, INT_MAX));
  ASSERT_EQ (-1, load_disk_timestamp (buf, &tm));
  ASSERT_EQ (3, tm);
  ASSERT_EQ (0, write_time (buf, now));
  ASSERT_EQ (0, truncate (buf, 2));
  ASSERT_EQ (-1, load_disk_timestamp (buf, &tm));
  ASSERT_EQ (3, tm);
  ASSERT_EQ (0, unlink (buf));
  ASSERT_EQ (-1, load_disk_timestamp (buf, &tm));
  ASSERT_EQ (3, tm);
  ASSERT_EQ (0, write_time (buf, now));
  ASSERT_EQ (0, load_disk_timestamp (buf, &tm));
  ASSERT_EQ (now, tm);
}


TEST_F (tempdir, save_time)
{
  char buf[PATH_MAX];
  time_t now = time (NULL);
  time_t tm;
  snprintf (buf, sizeof (buf), "%s/save", self->path);
  ASSERT_EQ (0, write_time (buf, now));
  ASSERT_EQ (0, read_time (buf, &tm));
  EXPECT_EQ (now, tm);
}

FIXTURE (tlsdate)
{
  struct state state;
  struct timeval timeout;
};


FIXTURE_SETUP (tlsdate)
{
  memset (self, 0, sizeof (*self));
  /* TODO(wad) make this use the same function tlsdated uses. */
  self->state.base = event_base_new();
  set_conf_defaults (&self->state.opts);
  ASSERT_NE (NULL, self->state.base);
  event_base_priority_init (self->state.base, MAX_EVENT_PRIORITIES);
  ASSERT_EQ (0, setup_sigchld_event (&self->state, 1));
  self->state.events[E_TLSDATE] = event_new (self->state.base, -1, EV_TIMEOUT,
                                  action_run_tlsdate, &self->state);
  ASSERT_NE (NULL, self->state.events[E_TLSDATE]);
  event_priority_set (self->state.events[E_TLSDATE], PRI_NET);
  /* The timeout and fd will be filled in per-call. */
  ASSERT_EQ (0, setup_tlsdate_status (&self->state));
  self->timeout.tv_sec = 1;
}

FIXTURE_TEARDOWN (tlsdate)
{
  int i;
  for (i = 0; i < E_MAX; ++i)
    {
      struct event *e = self->state.events[i];
      if (e)
        {
          int fd = event_get_fd (e);
          if (fd >= 0 && ! (event_get_events (e) & EV_SIGNAL))
            close (fd);
          event_free (e);
          self->state.events[i] = NULL;
        }
    }
  /* The other half was closed above. */
  close (self->state.tlsdate_monitor_fd);
  if (self->state.tlsdate_pid)
    {
      kill (self->state.tlsdate_pid, SIGKILL);
      waitpid (self->state.tlsdate_pid, NULL, WNOHANG);
    }
  if (self->state.base)
    event_base_free (self->state.base);
}

static int
runner (FIXTURE_DATA (tlsdate) *self, time_t *newtime)
{
  if (newtime)
    *newtime = 0;
  trigger_event (&self->state, E_TLSDATE, 0);
  event_base_loopexit (self->state.base, &self->timeout);
  if (event_base_dispatch (self->state.base))
    return -1;
  if (self->state.last_time)
    {
      if (newtime)
        *newtime = self->state.last_time;
      return 0;
    }
  return 1;
}

TEST_F (tlsdate, runner_multi)
{
  struct source source =
  {
    .next = NULL,
    .host = "host1",
    .port = "port1",
    .proxy = "proxy1"
  };
  char *args[] = { "/nonexistent", NULL, NULL };
  extern char **environ;
  self->state.opts.sources = &source;
  self->state.opts.base_argv = args;
  self->state.opts.subprocess_tries = 2;
  self->state.opts.subprocess_wait_between_tries = 1;
  self->state.opts.max_tries = 3;
  self->state.envp = environ;
  EXPECT_EQ (1, runner (self, NULL));
  args[0] = "/bin/false";
  self->state.tries = 0;
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (1, runner (self, NULL));
  args[0] = "src/test/check-host-1";
  self->state.tries = 0;
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
  args[0] = "src/test/sleep-wrap";
  args[1] = "3";
  self->state.tries = 0;
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
}

TEST (jitter)
{
  int i = 0;
  int r;
  const int kBase = 100;
  const int kJitter = 25;
  int nonequal = 0;
  for (i = 0; i < 1000; i++)
    {
      r = add_jitter (kBase, kJitter);
      EXPECT_GE (r, kBase - kJitter);
      EXPECT_LE (r, kBase + kJitter);
      if (r != kBase)
        nonequal++;
    }
  EXPECT_NE (nonequal, 0);
}

TEST_F (tlsdate, rotate_hosts)
{
  struct source s2 =
  {
    .next = NULL,
    .host = "host2",
    .port = "port2",
    .proxy = "proxy2"
  };
  struct source s1 =
  {
    .next = &s2,
    .host = "host1",
    .port = "port1",
    .proxy = "proxy1"
  };
  char *args[] = { "src/test/check-host-1",  NULL };
  extern char **environ;
  self->state.envp = environ;
  self->state.opts.sources = &s1;
  self->state.opts.base_argv = args;
  self->state.opts.subprocess_tries = 2;
  self->state.opts.subprocess_wait_between_tries = 1;
  self->state.opts.max_tries = 5;
  self->timeout.tv_sec = 2;
  EXPECT_EQ (0, runner (self, NULL));
  self->state.tries = 0;
  args[0] = "src/test/check-host-2";
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
  self->state.tries = 0;
  args[0] = "src/test/check-host-1";
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
  self->state.tries = 0;
  args[0] = "src/test/check-host-2";
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
}

TEST_F (tlsdate, proxy_override)
{
  struct source s1 =
  {
    .next = NULL,
    .host = "host",
    .port = "port",
    .proxy = NULL,
  };
  char *args[] = { "src/test/proxy-override", NULL };
  extern char **environ;
  self->state.envp = environ;
  self->state.opts.sources = &s1;
  self->state.opts.base_argv = args;
  self->state.opts.subprocess_tries = 2;
  self->state.opts.subprocess_wait_between_tries = 1;
  EXPECT_EQ (0, runner (self, NULL));
  EXPECT_EQ (RECENT_COMPILE_DATE + 1, self->state.last_time);
  s1.proxy = "socks5://bad.proxy";
  self->state.tries = 0;
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
  EXPECT_EQ (RECENT_COMPILE_DATE + 3, self->state.last_time);
  self->state.opts.proxy = "socks5://good.proxy";
  self->state.tries = 0;
  self->state.last_sync_type = SYNC_TYPE_NONE;
  EXPECT_EQ (0, runner (self, NULL));
  EXPECT_EQ (RECENT_COMPILE_DATE + 2, self->state.last_time);
}

FIXTURE(mock_platform) {
  struct platform platform;
  struct platform *old_platform;
};

FIXTURE_SETUP(mock_platform) {
  self->old_platform = platform;
  self->platform.rtc_open = NULL;
  self->platform.rtc_write = NULL;
  self->platform.rtc_read = NULL;
  self->platform.rtc_close = NULL;
  platform = &self->platform;
}

FIXTURE_TEARDOWN(mock_platform) {
  platform = self->old_platform;
}

/* TODO: leap_tests, time_setter tests. */

TEST_HARNESS_MAIN
