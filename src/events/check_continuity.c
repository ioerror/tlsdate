/*
 * check_continuity.c - periodically check local clock deltas
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <time.h>
#include <event2/event.h>

#include "src/conf.h"
#include "src/tlsdate.h"
#include "src/util.h"


/* Returns < 0 on error,
 *           0 on sync'd,
 * and     > 0 on desync'd.
 * Old delta is in |delta|. |delta| is overwritten
 * if >= 0 is returned.
 *
 * This event catches any sort of real-time clock jump.  A jump is observed
 * when settimeofday() or adjtimex() is called, or if the RTC misbehaves on
 * return from suspend.  If a jump is detected between a cycle-oriented clock
 * (MONOTONIC_RAW) and a potentially RTC managed clock (REALTIME), then a
 * network resynchronization will be required.  To avoid requiring this on
 * every resume-from-suspend, a larger delta represents the largest time jump
 * allowed before needing a resync.
 *
 * Note, CLOCK_BOOTTIME does not resolve this on platforms without a persistent
 * clock because the RTC still determines the time considered "suspend time".
 */
int
check_continuity (time_t *delta)
{
  time_t new_delta;
  struct timespec monotonic, real;
  if (clock_gettime (CLOCK_REALTIME, &real) < 0)
    return -1;
  if (clock_gettime (CLOCK_MONOTONIC_RAW, &monotonic) < 0)
    return -1;
  new_delta = real.tv_sec - monotonic.tv_sec;
  if (*delta)
    {
      /* The allowed delta matches the interval for now. */
      static const time_t kDelta = CONTINUITY_INTERVAL;
      if (new_delta < *delta - kDelta || new_delta > *delta + kDelta)
        {
          *delta = new_delta;
          return  1;
        }
    }
  /* First delta after de-sync. */
  *delta = new_delta;
  return 0;
}

/* Sets up a wake event just in case there has not been a wake event
 * recently enough to catch clock desynchronization.  This does not
 * invalidate the time like the action_invalidate_time event.
 */
int setup_event_timer_continuity (struct state *state)
{
  struct event *event;
  struct timeval interval = { state->opts.continuity_interval, 0 };
  event = event_new (state->base, -1, EV_TIMEOUT|EV_PERSIST,
                     action_kickoff_time_sync, state);
  if (!event)
    {
      error ("Failed to create interval event");
      return 1;
    }
  event_priority_set (event, PRI_WAKE);
  return event_add (event, &interval);
}
