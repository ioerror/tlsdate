/*
 * sigchld.c - event for SIGCHLD
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <errno.h>
#include <event2/event.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "src/conf.h"
#include "src/util.h"
#include "src/tlsdate.h"

/* Returns 1 if a death was handled, otherwise 0. */
int
handle_child_death (struct state *state)
{
  siginfo_t info;
  int ret;
  info.si_pid = 0;
  ret = waitid (P_ALL, -1, &info, WEXITED|WNOHANG);
  if (ret == -1)
    {
      if (errno == ECHILD)
        return 0;
      perror ("[event:%s] waitid() failed after SIGCHLD", __func__);
      return 0;
    }
  if (info.si_pid == 0)
    {
      return 0;
    }
  if (info.si_pid == state->setter_pid)
    {
      report_setter_error (&info);
      event_base_loopbreak (state->base);
      return 1;
    }
  if (info.si_pid != state->tlsdate_pid)
    {
      error ("[event:%s] SIGCHLD for an unknown process -- "
             "pid:%d uid:%d status:%d code:%d", __func__,
             info.si_pid, info.si_uid, info.si_status, info.si_code);
      return 1;
    }
  verb ("[event:%s] tlsdate reaped => "
        "pid:%d uid:%d status:%d code:%d", __func__,
        info.si_pid, info.si_uid, info.si_status, info.si_code);

  /* If it was still active, remove it. */
  event_del (state->events[E_TLSDATE_TIMEOUT]);
  state->running = 0;
  state->tlsdate_pid = 0;
  /* Clean exit - don't rerun! */
  if (info.si_status == 0)
    return 1;
  verb_debug ("[event:%s] scheduling a retry", __func__);
  /* Rerun a failed tlsdate */
  if (state->backoff < MAX_SANE_BACKOFF)
    state->backoff *= 2;
  /* If there is no resolver, call tlsdate directly. */
  if (!state->events[E_RESOLVER])
    {
      trigger_event (state, E_TLSDATE, state->backoff);
      return 1;
    }
  /* Run tlsdate even if the resolver doesn't come back. */
  trigger_event (state, E_TLSDATE, RESOLVER_TIMEOUT + state->backoff);
  /* Schedule the resolver.  This is always done after tlsdate in case there
   * is no resolver.
   */
  trigger_event (state, E_RESOLVER, state->backoff);
  return 1;
}

/* Returns 1 if a death was handled, otherwise 0. */
int
handle_child_stop (struct state *state)
{
  /* Handle unexpected external interactions */
  siginfo_t info;
  int ret;
  info.si_pid = 0;
  ret = waitid (P_ALL, -1, &info, WSTOPPED|WCONTINUED|WNOHANG);
  if (ret == -1)
    {
      if (errno == ECHILD)
        return 0;
      perror ("[event:%s] waitid() failed after SIGCHLD", __func__);
      return 0;
    }
  if (info.si_pid == 0)
    return 0;
  info ("[event:%s] a child has been STOPPED or CONTINUED. Killing it.",
         __func__);
  /* Kill it then catch the next SIGCHLD. */
  if (kill (info.si_pid, SIGKILL))
    {
      if (errno == EPERM)
        fatal ("[event:%s] cannot terminate STOPPED privileged child",
               __func__);
      if (errno == ESRCH)
        info ("[event:%s] child gone before we could kill it",
              __func__);
    }
  return 1;
}

void
action_sigchld (evutil_socket_t fd, short what, void *arg)
{
  struct state *state = arg;
  verb_debug ("[event:%s] a child process has SIGCHLD'd!", __func__);
  /* Process SIGCHLDs in two steps: death and stopped until all
   * pending children are sorted.
   */
  if (!handle_child_death (state) && !handle_child_stop (state))
    verb ("[event:%s] SIGCHLD fired but no children ready!", __func__);
  while (handle_child_death (state) || handle_child_stop (state));
}

int
setup_sigchld_event (struct state *state, int persist)
{
  state->events[E_SIGCHLD] = event_new (state->base, SIGCHLD,
                                        EV_SIGNAL| (persist ? EV_PERSIST : 0),
                                        action_sigchld, state);
  if (!state->events[E_SIGCHLD])
    return 1;
  /* Make sure this is lower than SAVE so we get any error
   * messages back from the time setter.
   */
  event_priority_set (state->events[E_SIGCHLD], PRI_NET);
  event_add (state->events[E_SIGCHLD], NULL);
  return 0;
}
