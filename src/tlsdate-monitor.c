/*
 * tlsdate-monitor.c - tlsdated monitor for tlsdate.
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "src/util.h"
#include "src/tlsdate.h"

static
char **
build_argv (struct opts *opts)
{
  int argc;
  char **new_argv;
  assert (opts->sources);
  /* choose the next source in the list; if we're at the end, start over. */
  if (!opts->cur_source || !opts->cur_source->next)
    opts->cur_source = opts->sources;
  else
    opts->cur_source = opts->cur_source->next;
  for (argc = 0; opts->base_argv[argc]; argc++)
    ;
  /* Put an arbitrary limit on the number of args. */
  if (argc > 1024)
    return NULL;
  argc++; /* uncounted null terminator */
  argc += 9;  /* -H host -p port -x proxy -Vraw -n -l */
  new_argv = malloc (argc * sizeof (char *));
  if (!new_argv)
    return NULL;
  for (argc = 0; opts->base_argv[argc]; argc++)
    new_argv[argc] = opts->base_argv[argc];
  new_argv[argc++] = "-H";
  new_argv[argc++] = opts->cur_source->host;
  new_argv[argc++] = "-p";
  new_argv[argc++] = opts->cur_source->port;
  if (opts->cur_source->proxy || opts->proxy)
    {
      char *proxy = opts->proxy ? opts->proxy : opts->cur_source->proxy;
      if (strcmp (proxy, ""))
        {
          new_argv[argc++] = (char *) "-x";
          new_argv[argc++] = proxy;
        }
    }
  new_argv[argc++] = "-Vraw";
  new_argv[argc++] = "-n";
  if (opts->leap)
    new_argv[argc++] = "-l";
  new_argv[argc++] = NULL;
  return new_argv;
}

/* Run tlsdate and redirects stdout to the monitor_fd */
int
tlsdate (struct state *state)
{
  char **new_argv;
  pid_t pid;
  switch ((pid = fork()))
    {
    case 0: /* child! */
      break;
    case -1:
      perror ("fork() failed!");
      return -1;
    default:
      verb_debug ("[tlsdate-monitor] spawned tlsdate: %d", pid);
      state->tlsdate_pid = pid;
      return 0;
   }
  if (!(new_argv = build_argv (&state->opts)))
    fatal ("out of memory building argv");
  /* Replace stdout with the pipe back to tlsdated */
  if (dup2 (state->tlsdate_monitor_fd, STDOUT_FILENO) < 0)
    {
      perror ("dup2 failed");
      _exit (2);
    }
  execve (new_argv[0], new_argv, state->envp);
  perror ("[tlsdate-monitor] execve() failed");
  _exit (1);
}
