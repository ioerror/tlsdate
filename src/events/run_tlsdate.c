/*
 * run_tlsdate.c - events for running tlsdate
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <event2/event.h>

#include "src/conf.h"
#include "src/dbus.h"
#include "src/util.h"
#include "src/tlsdate.h"

/* TODO(wad) split out backoff logic to make this testable */
void action_run_tlsdate (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  verb_debug ("[event:%s] fired", __func__);
  if (state->last_sync_type == SYNC_TYPE_NET)
    {
      verb ("[event:%s] called, but network time isn't needed",
            __func__);
      return;
    }
  state->resolving = 0;
  if (state->running)
    {
      /* It's possible that a network or proxy change occurred during a call. If
       * the call succeeded, it doesn't matter.  If the call fails, reissuing
       * the attempt with the new configuration has a chance of succeeding.  To
       * avoid missing a retry, we decrement the try count and reset the
       * backoff.
       */
      if (state->tries > 0)
        {
          state->tries--;
          /* TODO(wad) Make a shorter retry constant for this. */
          state->backoff = state->opts.wait_between_tries;
        }
      info ("[event:%s] requested re-run of tlsdate while tlsdate is running",
            __func__);
      return;
    }
  /* Enforce maximum retries here instead of in sigchld.c */
  if (state->tries < state->opts.max_tries)
    {
      state->tries++;
    }
  else
    {
      state->tries = 0;
      state->backoff = state->opts.wait_between_tries;
      error ("[event:%s] tlsdate tried and failed to get the time", __func__);
      return;
    }
  state->running = 1;
  verb ("[event:%s] attempt %d backoff %d", __func__,
        state->tries, state->backoff);
  /* Setup a timeout before killing tlsdate */
  trigger_event (state, E_TLSDATE_TIMEOUT,
                 state->opts.subprocess_wait_between_tries);
  /* Add the response listener event */
  trigger_event (state, E_TLSDATE_STATUS, -1);
  /* Fire off the child process now! */
  if (tlsdate (state))
    {
      /* TODO(wad) Should this be fatal? */
      error ("[event:%s] tlsdate failed to launch!", __func__);
      state->running = 0;
      state->tries = 0;
      event_del (state->events[E_TLSDATE_TIMEOUT]);
      return;
    }
}
