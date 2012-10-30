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
  int fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0700);
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
  char *args[] = { "/nonexistent", NULL, NULL };
  extern char **environ;
  EXPECT_EQ(1, tlsdate(args, environ, 2, 1));
  args[0] = "/bin/false";
  EXPECT_EQ(1, tlsdate(args, environ, 2, 1));
  args[0] = "/bin/true";
  EXPECT_EQ(0, tlsdate(args, environ, 2, 1));
  args[0] = "/bin/sleep";
  args[1] = "3";
  EXPECT_EQ(-1, tlsdate(args, environ, 2, 1));
  EXPECT_EQ(0, tlsdate(args, environ, 2, 5));
}

TEST_HARNESS_MAIN
