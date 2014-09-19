/*
 * kickoff_time_sync.c - network time synchronization
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#ifdef USE_POLARSSL
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#else
#include <openssl/rand.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <event2/event.h>

#include "src/conf.h"
#include "src/util.h"
#include "src/tlsdate.h"

#ifdef USE_POLARSSL
static int random_init = 0;
static entropy_context entropy;
static ctr_drbg_context ctr_drbg;
static char *pers = "tlsdated";
#endif

int
add_jitter (int base, int jitter)
{
  int n = 0;
  if (!jitter)
    return base;
#ifdef USE_POLARSSL
  if (0 == random_init)
  {
    entropy_init(&entropy);
    if (0 > ctr_drbg_init(&ctr_drbg, entropy_func, &entropy,
                          (unsigned char *) pers, strlen(pers)))
    {
      pfatal ("Failed to initialize random source");
    }
    random_init = 1;
  }
  if (0 != ctr_drbg_random(&ctr_drbg, (unsigned char *)&n, sizeof(n)))
    fatal ("ctr_drbg_random() failed");
#else
  if (RAND_bytes ( (unsigned char *) &n, sizeof (n)) != 1)
    fatal ("RAND_bytes() failed");
#endif
  return base + (abs (n) % (2 * jitter)) - jitter;
}

void
invalidate_time (struct state *state)
{
  state->last_sync_type = SYNC_TYPE_RTC;
  state->last_time = time (NULL);
  /* Note(!) this does not invalidate the clock_delta implicitly.
   * This allows forced invalidation to not lose synchronization
   * data.
   */
}

void
action_invalidate_time (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  verb_debug ("[event:%s] fired", __func__);
  /* If time is already invalid and being acquired, do nothing. */
  if (state->last_sync_type == SYNC_TYPE_RTC &&
      event_pending (state->events[E_TLSDATE], EV_TIMEOUT, NULL))
    return;
  /* Time out our trust in network synchronization but don't persist
   * the change to disk or notify the system.  Let a network sync
   * failure or success do that.
   */
  invalidate_time (state);
  /* Then trigger a network sync if possible. */
  action_kickoff_time_sync (-1, EV_TIMEOUT, arg);
}

int
setup_event_timer_sync (struct state *state)
{
  int wait_time = add_jitter (state->opts.steady_state_interval,
                              state->opts.jitter);
  struct timeval interval = { wait_time, 0 };
  state->events[E_STEADYSTATE] = event_new (state->base, -1,
                                 EV_TIMEOUT|EV_PERSIST,
                                 action_invalidate_time, state);
  if (!state->events[E_STEADYSTATE])
    {
      error ("Failed to create interval event");
      return 1;
    }
  event_priority_set (state->events[E_STEADYSTATE], PRI_ANY);
  return event_add (state->events[E_STEADYSTATE], &interval);
}

/* Begins a network synchronization attempt.  If the local clocks
 * are synchronized, then make sure that the _current_ synchronization
 * source is set to the real-time clock and note that the clock_delta
 * is unreliable.  If the clock was in sync and the last synchronization
 * source was the network, then this action does nothing.
 *
 * In the case of desynchronization, the clock_delta value is used as a
 * guard to indicate that even if the synchronization source isn't the
 * network, the source is still tracking the clock delta that was
 * established from a network source.
 * TODO(wad) Change the name of clock_delta to indicate that it is the local
 *           clock delta after the last network sync.
 */
void action_kickoff_time_sync (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  verb_debug ("[event:%s] fired", __func__);
  time_t delta = state->clock_delta;
  int jitter = 0;
  if (check_continuity (&delta) > 0)
    {
      info ("[event:%s] clock delta desync detected (%d != %d)", __func__,
            state->clock_delta, delta);
      /* Add jitter iff we had network synchronization once before. */
      if (state->clock_delta)
        jitter = add_jitter (30, 30); /* TODO(wad) make configurable */
      /* Forget the old delta until we have time again. */
      state->clock_delta = 0;
      invalidate_time (state);
    }
  if (state->last_sync_type == SYNC_TYPE_NET)
    {
      verb_debug ("[event:%s] time in sync. skipping", __func__);
      return;
    }
  /* Keep parity with run_tlsdate: for every wake, allow it to retry again. */
  if (state->tries > 0)
    {
      state->tries -= 1;
      /* Don't bother re-triggering tlsdate */
      verb_debug ("[event:%s] called while tries are in progress", __func__);
      return;
    }
  /* Don't over-schedule if the first attempt hasn't fired. If a wake event
   * impacts the result of a proxy resolution, then the updated value can be
   * acquired on the next run. If the wake comes in after E_TLSDATE is
   * serviced, then the tries count will be decremented.
   */
  if (event_pending (state->events[E_TLSDATE], EV_TIMEOUT, NULL))
    {
      verb_debug ("[event:%s] called while tlsdate is pending", __func__);
      return;
    }
  if (!state->events[E_RESOLVER])
    {
      trigger_event (state, E_TLSDATE, jitter);
      return;
    }
  /* If the resolver relies on an external response, then make sure that a
   * tlsdate event is waiting in the wings if the resolver is too slow.  Even
   * if this fires, it won't stop eventual handling of the resolver since it
   * doesn't event_del() E_RESOLVER.
   */
  trigger_event (state, E_TLSDATE, jitter + RESOLVER_TIMEOUT);
  trigger_event (state, E_RESOLVER, jitter);
}
