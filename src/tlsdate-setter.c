/*
 * tlsdate-setter.c - privileged time setter for tlsdated
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <event2/event.h>

#include "src/conf.h"
#include "src/dbus.h"
#include "src/seccomp.h"
#include "src/tlsdate.h"
#include "src/util.h"

/* Atomically writes the timestamp to the specified fd. */
int
save_timestamp_to_fd (int fd, time_t t)
{
  return platform->file_write(fd, &t, sizeof (t));
}

void
report_setter_error (siginfo_t *info)
{
  const char *code;
  int killit = 0;
  switch (info->si_code)
    {
    case CLD_EXITED:
      code = "EXITED";
      break;
    case CLD_KILLED:
      code = "KILLED";
      break;
    case CLD_DUMPED:
      code = "DUMPED";
      break;
    case CLD_STOPPED:
      code = "STOPPED";
      killit = 1;
      break;
    case CLD_TRAPPED:
      code = "TRAPPED";
      killit = 1;
      break;
    case CLD_CONTINUED:
      code = "CONTINUED";
      killit = 1;
      break;
    default:
      code = "???";
      killit = 1;
    }
  info ("tlsdate-setter exitting: code:%s status:%d pid:%d uid:%d",
        code, info->si_status, info->si_pid, info->si_uid);
  if (killit)
    kill (info->si_pid, SIGKILL);
}

void
time_setter_coprocess (int time_fd, int notify_fd, struct state *state)
{
  int save_fd = -1;
  int status;
  prctl (PR_SET_NAME, "tlsdated-setter");
  if (state->opts.should_save_disk && !state->opts.dry_run)
    {
      /* TODO(wad) platform->file_open */
      if ( (save_fd = open (state->timestamp_path,
                            O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC,
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
        {
          /* Attempt to unlink the path on the way out. */
          unlink (state->timestamp_path);
          status = SETTER_NO_SAVE;
          goto notify_and_die;
        }
    }
  /* XXX: Drop all privs but CAP_SYS_TIME */
#ifdef HAVE_SECCOMP_FILTER
  if (enable_setter_seccomp())
    {
      status = SETTER_NO_SBOX;
      goto notify_and_die;
    }
#endif
  while (1)
    {
      struct timeval tv = { 0, 0 };
      /* The wire protocol is a time_t, but the caller should
       * always be the unprivileged tlsdated process which spawned this
       * helper.
       * There are two special messages:
       * (time_t)   0: requests a clean shutdown
       * (time_t) < 0: indicates not to write to disk
       * On Linux, time_t is a signed long.  Expanding the protocol
       * is easy, but writing one long only is ideal.
       */
      ssize_t bytes = read (time_fd, &tv.tv_sec, sizeof (tv.tv_sec));
      int save = 1;
      if (bytes == -1)
        {
          if (errno == EINTR)
            continue;
          status = SETTER_READ_ERR;
          goto notify_and_die;
        }
      if (bytes == 0)
        {
          /* End of pipe */
          status = SETTER_READ_ERR;
          goto notify_and_die;
        }
      if (bytes != sizeof (tv.tv_sec))
        continue;
      if (tv.tv_sec < 0)
        {
          /* Don't write to disk */
          tv.tv_sec = -tv.tv_sec;
          save = 0;
        }
      if (tv.tv_sec == 0)
        {
          status = SETTER_EXIT;
          goto notify_and_die;
        }
      if (is_sane_time (tv.tv_sec))
        {
          /* It would be nice if time was only allowed to move forward, but
           * if a single time source is wrong, then it could make it impossible
           * to recover from once the time is written to disk.
           */
          status = SETTER_BAD_TIME;
          if (!state->opts.dry_run)
            {
              if (settimeofday (&tv, NULL) < 0)
                {
                  status = SETTER_SET_ERR;
                  goto notify_and_die;
                }
              if (state->opts.should_sync_hwclock &&
                  platform->rtc_write(&state->hwclock, &tv))
                {
                  status = SETTER_NO_RTC;
                  goto notify_and_die;
                }
              if (save && save_fd != -1 &&
                  save_timestamp_to_fd (save_fd, tv.tv_sec))
                {
                  status = SETTER_NO_SAVE;
                  goto notify_and_die;
                }
            }
          status = SETTER_TIME_SET;
        }
      /* TODO(wad) platform->file_write */
      IGNORE_EINTR (write (notify_fd, &status, sizeof(status)));
    }
notify_and_die:
  IGNORE_EINTR (write (notify_fd, &status, sizeof(status)));
  close (notify_fd);
  close (save_fd);
  _exit (status);
}
