/*
 * seccomp.c - seccomp utility functions
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <asm/unistd.h>
#include <elf.h>
#include <errno.h>
#include <sys/prctl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <linux/audit.h>
#include <linux/filter.h>

#include "src/seccomp.h"

/* Linux seccomp_filter sandbox */
#define SECCOMP_FILTER_FAIL SECCOMP_RET_KILL

/* Use a signal handler to emit violations when debugging */
#ifdef SECCOMP_FILTER_DEBUG
# undef SECCOMP_FILTER_FAIL
# define SECCOMP_FILTER_FAIL SECCOMP_RET_TRAP
#endif /* SANDBOX_SECCOMP_FILTER_DEBUG */

/* Simple helpers to avoid manual errors (but larger BPF programs). */
#define SC_DENY(_nr, _errno) \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ERRNO|(_errno))
#define SC_ALLOW(_nr) \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

#if defined(__i386__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_I386
#elif defined(__x86_64__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__arm__)
# ifndef EM_ARM
#   define EM_ARM 40
# endif
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARM
#else
#  error "Platform does not support seccomp filter yet"
#endif

/* Returns 0 if the the sandbox is enabled using
 * the time setter policy.
 */
int
enable_setter_seccomp (void)
{
  static const struct sock_filter insns[] =
  {
    /* Ensure the syscall arch convention is as expected. */
    BPF_STMT (BPF_LD+BPF_W+BPF_ABS,
    offsetof (struct seccomp_data, arch)),
    BPF_JUMP (BPF_JMP+BPF_JEQ+BPF_K, SECCOMP_AUDIT_ARCH, 1, 0),
    BPF_STMT (BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
    /* Load the syscall number for checking. */
    BPF_STMT (BPF_LD+BPF_W+BPF_ABS,
    offsetof (struct seccomp_data, nr)),

    /* Process ALLOWs as quickly as possible */
    SC_ALLOW (read),
    SC_ALLOW (write),
    SC_ALLOW (pwritev),

    SC_ALLOW (settimeofday),
    SC_ALLOW (ioctl), /* TODO(wad) filter for fd and RTC_SET_TIME */
#ifdef __NR_time /* This is required for x86 systems */
    SC_ALLOW (time),
#endif

    SC_ALLOW (lseek),
    SC_ALLOW (close),
    SC_ALLOW (munmap),

    SC_ALLOW (exit_group),
    SC_ALLOW (exit),

    SC_DENY (open, EINVAL),
    SC_DENY (fcntl, EINVAL),
    SC_DENY (fstat, EINVAL),
#ifdef __NR_mmap
    SC_DENY (mmap, EINVAL),
#endif
#ifdef __NR_mmap2
    SC_DENY (mmap2, EINVAL),
#endif
#ifdef __NR_sendto
    SC_DENY (sendto, EINVAL),
#endif
#ifdef __NR_socket
    SC_DENY (socket, EINVAL),
#endif
#ifdef __NR_socketcall
    SC_DENY (socketcall, EINVAL),
#endif
    BPF_STMT (BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
  };
  static const struct sock_fprog prog =
  {
    .len = (unsigned short) (sizeof (insns) /sizeof (insns[0])),
    .filter = (struct sock_filter *) insns,
  };
  return (prctl (PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) ||
          prctl (PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog));
}
