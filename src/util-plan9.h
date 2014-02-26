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

#if _PLAN9_SOURCE
#define API
#else
#define API __attribute__((visibility("default")))
#endif

extern int verbose;
extern int verbose_debug;
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

#endif /* !UTIL_H */
