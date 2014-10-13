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
#include <unistd.h>

#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#ifndef PR_SET_NO_NEW_PRIVS
#  define PR_SET_NO_NEW_PRIVS 38
#endif
#ifndef PR_GET_NO_NEW_PRIVS
#  define PR_GET_NO_NEW_PRIVS 39
#endif
#endif

#include "src/rtc.h"

#ifdef TARGET_OS_HAIKU
#include <stdarg.h>
#endif

#define API __attribute__((visibility("default")))

extern const char *kTempSuffix;
#define IGNORE_EINTR(expr) ({ \
  typeof(expr) _r; \
  while ((_r = (expr)) == -1 && errno == EINTR); \
  _r; \
})

extern int verbose;
extern int verbose_debug;
void initalize_syslog (void);
void terminate_syslog (void);
void die (const char *fmt, ...);
void verb (const char *fmt, ...);
extern void logat (int isverbose, const char *fmt, ...);

#define verb_debug debug
#define debug(fmt, ...) if (verbose_debug) logat(1, fmt, ## __VA_ARGS__)
#define info(fmt, ...) logat(0, fmt, ## __VA_ARGS__)
#define pinfo(fmt, ...) logat(1, fmt ": %s", ## __VA_ARGS__, strerror(errno))
#define error(fmt, ...) logat(0, fmt, ## __VA_ARGS__)
#define perror(fmt, ...) logat(0, fmt ": %s", ## __VA_ARGS__, strerror(errno))
#define fatal(fmt, ...) do { logat(0, fmt, ## __VA_ARGS__); exit(1); } while (0)
#define pfatal(fmt, ...) do { \
  logat(0, fmt ": %s", ## __VA_ARGS__, strerror(errno)); \
  exit(1); \
} while (0)

static inline int min (int x, int y)
{
  return x < y ? x : y;
}

void drop_privs_to (const char *user, const char *group);
void no_new_privs (void);
const char *sync_type_str (int sync_type);

struct state;
enum event_id_t;
void trigger_event (struct state *state, enum event_id_t e, int sec);

struct platform {
	int (*rtc_open)(struct rtc_handle *);
	int (*rtc_write)(struct rtc_handle *, const struct timeval *tv);
	int (*rtc_read)(struct rtc_handle *, struct timeval *tv);
	int (*rtc_close)(struct rtc_handle *);

	int (*file_open)(const char *path, int write, int cloexec);
	int (*file_close)(int fd);
	/* Atomic file write and read */
	int (*file_write)(int fd, void *buf, size_t sz);
	int (*file_read)(int fd, void *buf, size_t sz);

	int (*time_get)(struct timeval *tv);

	int (*pgrp_enter)(void);
	int (*pgrp_kill)(void);

	int (*process_signal)(pid_t pid, int sig);
	int (*process_wait)(pid_t pid, int *status, int timeout);
};

extern struct platform *platform;

#endif /* !UTIL_H */
