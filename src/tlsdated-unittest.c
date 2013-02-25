/*
 * tlsdated-unittest.c - tlsdated unit tests
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include "src/test_harness.h"
#include "src/tlsdate.h"

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

FIXTURE(tempdir) {
  char path[PATH_MAX];
};

FIXTURE_SETUP(tempdir) {
  char *p;
  strncpy(self->path, "/tmp/tlsdated-unit-XXXXXX", sizeof(self->path));
  p = mkdtemp(self->path);
  ASSERT_NE(NULL, p);
}

int rmrf(char *dir) {
  char buf[256];
  snprintf(buf, sizeof(buf), "rm -rf %s", dir);
  return system(buf);
}

FIXTURE_TEARDOWN(tempdir) {
  ASSERT_EQ(0, rmrf(self->path));
}

int write_time(const char *path, time_t time) {
  int fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0600);
  if (fd == -1)
    return 1;
  if (write(fd, &time, sizeof(time)) != sizeof(time)) {
    close(fd);
    return 1;
  }
  return close(fd);
}

int read_time(const char *path, time_t* time) {
  int fd = open(path, O_RDONLY);
  if (fd == -1)
    return 1;
  if (read(fd, time, sizeof(*time)) != sizeof(*time)) {
    close(fd);
    return 1;
  }
  return close(fd);
}

TEST(sane_time) {
  ASSERT_EQ(0, is_sane_time(0));
  ASSERT_EQ(0, is_sane_time(INT_MAX));
}

TEST(sane_host_time) {
  ASSERT_EQ(1, is_sane_time(time(NULL)));
}

TEST_F(tempdir, load_time) {
  char buf[PATH_MAX];
  time_t tm = 3;
  time_t now = time(NULL);
  snprintf(buf, sizeof(buf), "%s/load", self->path);

  ASSERT_EQ(0, write_time(buf, 0));
  ASSERT_EQ(-1, load_disk_timestamp(buf, &tm));
  ASSERT_EQ(3, tm);

  ASSERT_EQ(0, write_time(buf, INT_MAX));
  ASSERT_EQ(-1, load_disk_timestamp(buf, &tm));
  ASSERT_EQ(3, tm);

  ASSERT_EQ(0, write_time(buf, now));
  ASSERT_EQ(0, truncate(buf, 2));
  ASSERT_EQ(-1, load_disk_timestamp(buf, &tm));
  ASSERT_EQ(3, tm);

  ASSERT_EQ(0, unlink(buf));
  ASSERT_EQ(-1, load_disk_timestamp(buf, &tm));
  ASSERT_EQ(3, tm);

  ASSERT_EQ(0, write_time(buf, now));
  ASSERT_EQ(0, load_disk_timestamp(buf, &tm));
  ASSERT_EQ(now, tm);
}

TEST_F(tempdir, save_time) {
  char buf[PATH_MAX];
  time_t now = time(NULL);
  time_t tm;
  snprintf(buf, sizeof(buf), "%s/save", self->path);

  save_disk_timestamp(buf, now);
  ASSERT_EQ(0, read_time(buf, &tm));
  EXPECT_EQ(now, tm);
}

TEST(tlsdate_tests) {
  struct source source = {
    .next = NULL,
    .host = "<host>",
    .port = "<port>",
    .proxy = "<proxy>"
  };
  char *args[] = { "/nonexistent", NULL, NULL };
  struct opts opts;
  memset(&opts, 0, sizeof(opts));
  opts.sources = &source;
  opts.base_argv = args;
  opts.subprocess_tries = 2;
  opts.subprocess_wait_between_tries = 1;
  extern char **environ;
  EXPECT_EQ(1, tlsdate(&opts, environ));
  args[0] = "/bin/false";
  EXPECT_EQ(1, tlsdate(&opts, environ));
  args[0] = "/bin/true";
  EXPECT_EQ(0, tlsdate(&opts, environ));
  args[0] = "src/test/sleep-wrap";
  args[1] = "3";
  EXPECT_EQ(-1, tlsdate(&opts, environ));
  opts.subprocess_wait_between_tries = 5;
  EXPECT_EQ(0, tlsdate(&opts, environ));
}

TEST(jitter) {
  int i = 0;
  int r;
  const int kBase = 100;
  const int kJitter = 25;
  int nonequal = 0;
  for (i = 0; i < 1000; i++) {
    r = add_jitter(kBase, kJitter);
    EXPECT_GE(r, kBase - kJitter);
    EXPECT_LE(r, kBase + kJitter);
    if (r != kBase)
      nonequal++;
  }
  EXPECT_NE(nonequal, 0);
}

TEST(rotate_hosts) {
  struct source s2 = {
    .next = NULL,
    .host = "host2",
    .port = "port2",
    .proxy = "proxy2"
  };
  struct source s1 = {
    .next = &s2,
    .host = "host1",
    .port = "port1",
    .proxy = "proxy1"
  };
  struct opts opts;
  char *args[] = { "src/test/rotate", NULL };
  memset(&opts, 0, sizeof(opts));
  opts.sources = &s1;
  opts.base_argv = args;
  opts.subprocess_tries = 2;
  opts.subprocess_wait_between_tries = 1;
  extern char **environ;
  EXPECT_EQ(1, tlsdate(&opts, environ));
  EXPECT_EQ(2, tlsdate(&opts, environ));
  EXPECT_EQ(1, tlsdate(&opts, environ));
  EXPECT_EQ(2, tlsdate(&opts, environ));
}

TEST(proxy_override) {
  struct source s1 = {
    .next = NULL,
    .host = "host",
    .port = "port",
    .proxy = NULL,
  };
  struct opts opts;
  char *args[] = { "src/test/proxy-override", NULL };
  memset(&opts, 0, sizeof(opts));
  opts.sources = &s1;
  opts.base_argv = args;
  opts.subprocess_tries = 2;
  opts.subprocess_wait_between_tries = 1;
  extern char **environ;
  EXPECT_EQ(1, tlsdate(&opts, environ));
  s1.proxy = "socks5://bad.proxy";
  EXPECT_EQ(2, tlsdate(&opts, environ));
  opts.proxy = "socks5://good.proxy";
  EXPECT_EQ(0, tlsdate(&opts, environ));
}

TEST_HARNESS_MAIN
