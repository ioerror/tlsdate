/*
 * util.c - routeup/tlsdated utility functions
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "src/util.h"

/** helper function to print message and die */
void
die (const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(1);
}

/** helper function for 'verbose' output */
void
verb (const char *fmt, ...)
{
  va_list ap;

  if (! verbose) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
void API logat(int isverbose, const char *fmt, ...)
{
  if (isverbose && !verbose)
    return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  va_start(ap, fmt);
  vsyslog(LOG_INFO, fmt, ap);
  va_end(ap);
}

