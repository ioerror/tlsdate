/*
 * route_up.c - wake events for route changes
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <event2/event.h>

#include "src/conf.h"
#include "src/routeup.h"
#include "src/tlsdate.h"
#include "src/util.h"

void action_stdin_wakeup (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  char buf[1];
  verb_debug ("[event:%s] fired", __func__);
  if (what != EV_READ)
    return;
  if (IGNORE_EINTR (read (fd, buf, sizeof (buf))) != sizeof (buf))
    {
      error ("[event:%s] unregistering stdin handler - it's broken!",
             __func__);
      event_del (state->events[E_ROUTEUP]);
      return;
    }
  action_kickoff_time_sync (-1, EV_TIMEOUT, arg);
}

void action_netlink_ready (evutil_socket_t fd, short what, void *arg)
{
  struct routeup routeup_cfg;
  verb_debug ("[event:%s] fired", __func__);
  routeup_cfg.netlinkfd = fd;
  if (what & EV_READ)
    {
      if (routeup_process (&routeup_cfg) == 0)
        {
          verb_debug ("[event:%s] routes changed", __func__);
          /* Fire off a proxy resolution attempt and a new sync request */
          action_kickoff_time_sync (-1, EV_TIMEOUT, arg);
        }
    }
}

int setup_event_route_up (struct state *state)
{
  event_callback_fn handler;
  int fd = -1;
  struct routeup routeup_cfg;
  if (state->opts.should_netlink)
    {
      if (routeup_setup (&routeup_cfg))
        {
          error ("routeup_setup() failed");
          return 1;
        }
      fd = routeup_cfg.netlinkfd;
      handler = action_netlink_ready;
    }
  else      /* Listen for cues from stdin */
    {
      fd = STDIN_FILENO;
      if (fcntl (fd, F_SETFL, O_NONBLOCK) < 0)
        {
          perror ("stdin fcntl(O_NONBLOCK) failed");
          return 1;
        }
      handler = action_stdin_wakeup;
    }
  state->events[E_ROUTEUP] = event_new (state->base, fd,
                                        EV_READ|EV_PERSIST, handler, state);
  if (!state->events[E_ROUTEUP])
    {
      if (state->opts.should_netlink)
        {
          routeup_teardown (&routeup_cfg);
        }
      return 1;
    }
  event_priority_set (state->events[E_ROUTEUP], PRI_WAKE);
  event_add (state->events[E_ROUTEUP], NULL);
  return 0;
}
