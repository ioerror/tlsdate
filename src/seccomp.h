/*
 * seccomp.h - seccomp functions
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SECCOMP_H
#define SECCOMP_H

#include "config.h"

#include "src/seccomp-compat.h"

#ifdef HAVE_SECCOMP_FILTER
int enable_setter_seccomp (void);
#else  /* HAVE_SECCOMP_FILTER */
static
inline int enable_setter_seccomp (void)
{
  return 0;
}
#endif  /* !HAVE_SECCOMP_FILTER */

#endif
