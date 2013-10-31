/*
 * util.h - routeup/tlsdated utility functions
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef UTIL_H
#define UTIL_H

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef TARGET_OS_HAIKU
#include <stdarg.h>
#endif

#define API __attribute__((visibility("default")))

extern const char *kTempSuffix;

extern int verbose;
void die (const char *fmt, ...);
void verb (const char *fmt, ...);
extern void logat(int isverbose, const char *fmt, ...);

#define info(fmt, ...) logat(1, fmt, ## __VA_ARGS__)
#define pinfo(fmt, ...) logat(1, fmt ": %s", ## __VA_ARGS__, strerror(errno))
#define error(fmt, ...) logat(0, fmt, ## __VA_ARGS__)
#define perror(fmt, ...) logat(0, fmt ": %s", ## __VA_ARGS__, strerror(errno))
#define fatal(fmt, ...) do { logat(0, fmt, ## __VA_ARGS__); exit(1); } while (0)
#define pfatal(fmt, ...) do { \
  logat(0, fmt ": %s", ## __VA_ARGS__, strerror(errno)); \
  exit(1); \
} while (0)

static inline int min(int x, int y) { return x < y ? x : y; }

void drop_privs_to (const char *user, const char *group);

/* like wait(), but with a timeout. Returns ordinary fork() error codes, or
 * ETIMEDOUT. */
pid_t wait_with_timeout(int *status, int timeout_secs);

struct platform {
	void *(*rtc_open)(void);
	int (*rtc_write)(void *handle, const struct timeval *tv);
	int (*rtc_read)(void *handle, struct timeval *tv);
	int (*rtc_close)(void *handle);

	int (*file_write)(const char *path, void *buf, size_t sz);
	int (*file_read)(const char *path, void *buf, size_t sz);

	int (*time_get)(struct timeval *tv);

	int (*pgrp_enter)(void);
	int (*pgrp_kill)(void);
};

extern struct platform *platform;

#endif /* !UTIL_H */
