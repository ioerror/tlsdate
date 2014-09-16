/*
 * seccomp-compat.h - seccomp defines for bad headers
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SECCOMP_COMPAT_H
#define SECCOMP_COMPAT_H

#include <stdint.h>

#ifndef PR_SET_NO_NEW_PRIVS
#  define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef SECCOMP_MODE_FILTER
#define SECCOMP_MODE_FILTER 2 /* uses user-supplied filter. */
#define SECCOMP_RET_KILL 0x00000000U /* kill the task immediately */
#define SECCOMP_RET_TRAP 0x00030000U /* disallow and force a SIGSYS */
#define SECCOMP_RET_ALLOW 0x7fff0000U /* allow */
#define SECCOMP_RET_ERRNO 0x00050000U /* returns an errno */

struct seccomp_data
{
  int nr;
  uint32_t arch;
  uint64_t instruction_pointer;
  uint64_t args[6];
};
#endif  /* !SECCOMP_MODE_FILTER */

#endif
