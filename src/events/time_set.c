/*
 * time_set.c - time setting functions
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <event2/event.h>

#include "src/conf.h"
#include "src/dbus.h"
#include "src/tlsdate.h"
#include "src/util.h"

void
handle_time_setter (struct state *state, int status)
{
  switch (status)
    {
    case SETTER_BAD_TIME:
      info ("[event:%s] time setter received bad time", __func__);
      /* This is the leaf node. Failure means that our source
       * tried to walk back in time.
       */
      state->last_sync_type = SYNC_TYPE_RTC;
      state->last_time = time (NULL);
      break;
    case SETTER_TIME_SET:
      info ("[event:%s] time set from the %s (%ld)",
            __func__, sync_type_str (state->last_sync_type), state->last_time);
      if (state->last_sync_type == SYNC_TYPE_NET)
        {
          /* Update the delta so it doesn't fire again immediately. */
          state->clock_delta = 0;
          check_continuity (&state->clock_delta);
          /* Reset the sources list! */
          state->opts.cur_source = NULL;
        }
      /* Share our success. */
      if (state->opts.should_dbus)
        dbus_announce (state);
      break;
    case SETTER_NO_SBOX:
      error ("[event:%s] time setter failed to sandbox", __func__);
      break;
    case SETTER_EXIT:
      error ("[event:%s] time setter exited gracefully", __func__);
      break;
    case SETTER_SET_ERR:
      error ("[event:%s] time setter could not settimeofday()", __func__);
      break;
    case SETTER_NO_RTC:
      error ("[event:%s] time setter could sync rtc", __func__);
      break;
    case SETTER_NO_SAVE:
      error ("[event:%s] time setter could not open save file", __func__);
      break;
    case SETTER_READ_ERR:
      error ("[event:%s] time setter could not read time", __func__);
      break;
    default:
      error ("[event:%s] received bogus status from time setter: %d",
             __func__, status);
      exit (status);
    }
}

void
action_time_set (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  int status = -1;
  ssize_t bytes = 0;
  verb_debug ("[event:%s] fired", __func__);
  bytes = IGNORE_EINTR (read (fd, &status, sizeof (status)));
  if (bytes == -1 && errno == EAGAIN)
    return;  /* Catch next wake up */
  /* Catch the rest of the errnos and any truncation. */
  if (bytes != sizeof (status))
    {
      /* Truncation of an int over a pipe shouldn't happen except in
       * terminal cases.
       */
      perror ("[event:%s] time setter pipe truncated! (%d)", __func__,
              bytes);
      /* Let SIGCHLD do the teardown. */
      close (fd);
      return;
    }
  handle_time_setter (state, status);
}

int
setup_time_setter (struct state *state)
{
  struct event *event;
  int to_fds[2];
  int from_fds[2];
  if (pipe (to_fds) < 0)
    {
      perror ("pipe failed");
      return 1;
    }
  if (pipe (from_fds) < 0)
    {
      perror ("pipe failed");
      close (to_fds[0]);
      close (to_fds[1]);
      return 1;
    }
  /* The fd that tlsdated will write to */
  state->setter_save_fd = to_fds[1];
  state->setter_notify_fd = from_fds[0];
  /* Make the notifications fd non-blocking. */
  if (fcntl (from_fds[0], F_SETFL, O_NONBLOCK) < 0)
    {
      perror ("notifier_fd fcntl(O_NONBLOCK) failed");
      goto close_and_fail;
    }
  /* Make writes non-blocking */
  if (fcntl (to_fds[1], F_SETFL, O_NONBLOCK) < 0)
    {
      perror ("save_fd fcntl(O_NONBLOCK) failed");
      goto close_and_fail;
    }
  event = event_new (state->base, from_fds[0], EV_READ|EV_PERSIST,
                     action_time_set, state);
  if (!event)
    {
      error ("Failed to allocate tlsdate setter event");
      goto close_and_fail;
    }
  event_priority_set (event, PRI_NET);
  event_add (event, NULL);
  /* fork */
  state->setter_pid = fork();
  if (state->setter_pid < 0)
    {
      perror ("fork()ing the time setter failed");
      goto close_and_fail;
    }
  if (state->setter_pid == 0)
    {
      close (to_fds[1]);
      close (from_fds[0]);
      time_setter_coprocess (to_fds[0], from_fds[1], state);
      _exit (1);
    }
  close (from_fds[1]);
  close (to_fds[0]);
  return 0;

close_and_fail:
  close (to_fds[0]);
  close (to_fds[1]);
  close (from_fds[0]);
  close (from_fds[1]);
  return 1;
}
