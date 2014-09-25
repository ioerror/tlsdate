/*
 * sigterm.c - handler for SIGTERM
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <sys/time.h>

#include <event2/event.h>

#include "src/conf.h"
#include "src/util.h"
#include "src/tlsdate.h"

/* On sigterm, grab the system clock and write it before terminating */
void action_sigterm (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  struct timeval tv;
  info ("[event:%s] starting graceful shutdown . . .", __func__);
  state->exitting = 1;
  if (platform->time_get (&tv))
    {
      pfatal ("[event:%s] couldn't gettimeofday to exit gracefully", __func__);
    }
  /* Don't change the last sync_type */
  state->last_time = tv.tv_sec;
  /* Immediately save and exit. */
  trigger_event (state, E_SAVE, -1);
}
