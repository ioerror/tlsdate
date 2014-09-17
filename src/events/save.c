/*
 * save.c - send new time to the time setter
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <event2/event.h>

#include "src/conf.h"
#include "src/util.h"
#include "src/tlsdate.h"

void action_sync_and_save (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  time_t t = state->last_time;
  ssize_t bytes;
  verb_debug ("[event:%s] fired", __func__);
  /* For all non-net sources, don't write to disk by
   * flagging the time negative.  We don't use negative
   * times and this won't effect shutdown (0) writes.
   */
  if (state->last_sync_type != SYNC_TYPE_NET)
    t = -t;
  if (what & EV_READ)
    {
      /* EPIPE/EBADF notification */
      error ("[event:%s] time setter is gone!", __func__);
      /* SIGCHLD will handle teardown. */
      return;
    }
  bytes = IGNORE_EINTR (write (fd, &t, sizeof (t)));
  if (bytes == -1)
    {
      if (errno == EPIPE)
        {
          error ("[event:%s] time setter is gone! (EPIPE)", __func__);
          return;
        }
      if (errno == EAGAIN)
        return; /* Get notified again. */
      error ("[event:%s] Unexpected errno %d", __func__, errno);
    }
  if (bytes != sizeof (t))
    pfatal ("[event:%s] unexpected write to time setter (%d)",
            __func__, bytes);
  /* If we're going down and we wrote the time, send a shutdown message. */
  if (state->exitting && t)
    {
      state->last_time = 0;
      action_sync_and_save (fd, what, arg);
      /* TODO(wad) platform->pgrp_kill() ? */
    }
  return;
}
