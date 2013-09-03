/*
 * util.c - routeup/tlsdated utility functions
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#ifdef WITH_EVENTS
#include <event2/event.h>
#endif

#include "src/tlsdate.h"
#include "src/util.h"

/** helper function to print message and die */
void
die (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  exit (1);
}

/** helper function for 'verbose' output */
void
verb (const char *fmt, ...)
{
  va_list ap;
  if (! verbose) return;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}
void API logat (int isverbose, const char *fmt, ...)
{
  if (isverbose && !verbose)
    return;
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
  va_start (ap, fmt);
  vsyslog (LOG_INFO, fmt, ap);
  va_end (ap);
}

void
drop_privs_to (const char *user, const char *group)
{
  uid_t uid;
  gid_t gid;
  struct passwd *pw;
  struct group  *gr;
  if (0 != getuid ())
    return; /* not running as root to begin with; should (!) be harmless to continue
         without dropping to 'nobody' (setting time will fail in the end) */
  pw = getpwnam (user);
  gr = getgrnam (group);
  if (NULL == pw)
    die ("Failed to obtain UID for `%s'\n", user);
  if (NULL == gr)
    die ("Failed to obtain GID for `%s'\n", group);
  uid = pw->pw_uid;
  if (0 == uid)
    die ("UID for `%s' is 0, refusing to run SSL\n", user);
  gid = pw->pw_gid;
  if (0 == gid || 0 == gr->gr_gid)
    die ("GID for `%s' is 0, refusing to run SSL\n", user);
  if (pw->pw_gid != gr->gr_gid)
    die ("GID for `%s' is not `%s' as expected, refusing to run SSL\n",
         user, group);
  if (0 != initgroups ( (const char *) user, gr->gr_gid))
    die ("Unable to initgroups for `%s' in group `%s' as expected\n",
         user, group);
#ifdef HAVE_SETRESGID
  if (0 != setresgid (gid, gid, gid))
    die ("Failed to setresgid: %s\n", strerror (errno));
#else
  if (0 != (setgid (gid) | setegid (gid)))
    die ("Failed to setgid: %s\n", strerror (errno));
#endif
#ifdef HAVE_SETRESUID
  if (0 != setresuid (uid, uid, uid))
    die ("Failed to setresuid: %s\n", strerror (errno));
#else
  if (0 != (setuid (uid) | seteuid (uid)))
    die ("Failed to setuid: %s\n", strerror (errno));
#endif
}

/* TODO(wad) rename to schedule_event */
void
trigger_event (struct state *state, enum event_id_t id, int sec)
{
#ifdef WITH_EVENTS
  struct event *e = state->events[id];
  struct timeval delay = { sec, 0 };
  /* Fallthrough to tlsdate if there is no resolver. */
  if (!e && id == E_RESOLVER)
    e = state->events[E_TLSDATE];
  if (!e)
    {
      info ("trigger_event with NULL |e|. I hope this is a test!");
      return;
    }
  if (event_pending (e, EV_READ|EV_WRITE|EV_TIMEOUT|EV_SIGNAL, NULL))
    event_del (e);
  if (sec >= 0)
    event_add (e, &delay);
  else /* Note! This will not fire a TIMEOUT event. */
    event_add (e, NULL);
#endif
}

const char *
sync_type_str (int sync_type)
{
  switch (sync_type)
    {
    case SYNC_TYPE_NONE:
      return "none";
    case SYNC_TYPE_BUILD:
      return "build-timestamp";
    case SYNC_TYPE_DISK:
      return "disk-timestamp";
    case SYNC_TYPE_RTC:
      return "system-clock";
    case SYNC_TYPE_PLATFORM:
      return "platform-feature";
    case SYNC_TYPE_NET:
      return "network";
    default:
      return "error";
    }
}
