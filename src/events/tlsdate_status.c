/*
 * tlsdate_status.c - handles tlsdate-monitor responses
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <event2/event.h>

#include "src/conf.h"
#include "src/util.h"
#include "src/tlsdate.h"

/* Returns < 0 on error, > 0 on eagain, and 0 on success */
int
read_tlsdate_response (int fd, time_t *t)
{
  /* TLS passes time as a 32-bit value. */
  uint32_t server_time = 0;
  ssize_t ret = IGNORE_EINTR (read (fd, &server_time, sizeof (server_time)));
  if (ret == -1 && errno == EAGAIN)
    {
      /* Full response isn't ready yet. */
      return 1;
    }
  if (ret != sizeof (server_time))
    {
      /* End of pipe (0) or truncated: death probable. */
      error ("[event:(%s)] invalid time read from tlsdate (rd:%d,ret:%zd).",
             __func__, server_time, ret);
      return -1;
    }
  /* uint32_t moves to signed long so there is room for silliness. */
  *t = server_time;
  return 0;
}

void
action_tlsdate_timeout (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  info ("[event:%s] tlsdate timed out", __func__);
  /* Force kill it and let action_sigchld rerun. */
  if (state->tlsdate_pid)
    kill (state->tlsdate_pid, SIGKILL);
}

void
action_tlsdate_status (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  time_t t = 0;
  int ret = read_tlsdate_response (fd, &t);
  verb_debug ("[event:%s] fired", __func__);
  if (ret < 0)
    {
      verb_debug ("[event:%s] forcibly timing out tlsdate", __func__);
      trigger_event (state, E_TLSDATE_TIMEOUT, 0);
      return;
    }
  if (ret)
    {
      /* EAGAIN'd: wait for the rest. */
      trigger_event (state, E_TLSDATE_STATUS, -1);
      return;
    }
  if (is_sane_time (t))
    {
      /* Note that last_time is from an online source */
      state->last_sync_type = SYNC_TYPE_NET;
      state->last_time = t;
      trigger_event (state, E_SAVE, -1);
    }
  else
    {
      error ("[event:%s] invalid time received from tlsdate: %ld",
             __func__, t);
    }
  /* Restore the backoff and tries count on success, insane or not.
   * On failure, the event handler does it.
   */
  state->tries = 0;
  state->backoff = state->opts.wait_between_tries;
  return;
}

/* Returns 0 on success and populates |fds| */
int
new_tlsdate_monitor_pipe (int fds[2])
{
  if (pipe (fds) < 0)
    {
      perror ("pipe failed");
      return -1;
    }
  /* TODO(wad): CLOEXEC, Don't leak these into tlsdate proper. */
  return 0;
}

/* Create a fd pair that the tlsdate runner will communicate over */
int
setup_tlsdate_status (struct state *state)
{
  int fds[2] = { -1, -1 };
  /* One pair of pipes are reused along with the event. */
  if (new_tlsdate_monitor_pipe (fds))
    {
      return -1;
    }
  verb_debug ("[%s] monitor fd pair (%d, %d)", __func__, fds[0], fds[1]);
  /* The fd that the monitor process will write to */
  state->tlsdate_monitor_fd = fds[1];
  /* Make the reader fd non-blocking and not leak into tlsdate. */
  if (fcntl (fds[0], F_SETFL, O_NONBLOCK|O_CLOEXEC) < 0)
    {
      perror ("pipe[0] fcntl(O_NONBLOCK) failed");
      return 1;
    }
  state->events[E_TLSDATE_STATUS] = event_new (state->base, fds[0],
                                    EV_READ,
                                    action_tlsdate_status, state);
  if (!state->events[E_TLSDATE_STATUS])
    {
      error ("Failed to allocate tlsdate status event");
      return 1;
    }
  event_priority_set (state->events[E_TLSDATE_STATUS], PRI_NET);
  state->events[E_TLSDATE_TIMEOUT] = event_new (state->base, -1,
                                     EV_TIMEOUT,
                                     action_tlsdate_timeout, state);
  if (!state->events[E_TLSDATE_TIMEOUT])
    {
      error ("Failed to allocate tlsdate timeout event");
      return 1;
    }
  event_priority_set (state->events[E_TLSDATE_TIMEOUT], PRI_SAVE);
  return 0;
}
