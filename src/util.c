/*
 * util.c - routeup/tlsdated utility functions
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "src/util.h"

/** helper function to print message and die */
void
die (const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(1);
}

/** helper function for 'verbose' output */
void
verb (const char *fmt, ...)
{
  va_list ap;

  if (! verbose) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
void API logat(int isverbose, const char *fmt, ...)
{
  if (isverbose && !verbose)
    return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  va_start(ap, fmt);
  vsyslog(LOG_INFO, fmt, ap);
  va_end(ap);
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
  pw = getpwnam(user);
  gr = getgrnam(group);
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

  if (0 != initgroups((const char *)user, gr->gr_gid))
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

pid_t
wait_with_timeout(int *status, int timeout_secs)
{
  int st = 0;
  pid_t exited;
  /* synthesize waiting with a timeout by using a helper process. We
   * launch a child process that will exit in |timeout_secs|, guaranteeing
   * that wait() will return by then. */
  pid_t helper = fork();
  if (helper < 0)
    return helper;
  if (helper == 0)
  {
    sleep(timeout_secs);
    exit(0);
  }

  /* use temporary status to avoid touching it if we do ETIMEDOUT */
  exited = wait(&st);
  if (exited == helper)
    /* helper exited before any other child did */
    return -ETIMEDOUT;

  /* a real child process exited - don't leak the helper */
  kill(helper, SIGKILL);
  waitpid(helper, NULL, 0);
  *status = st;
  return exited;
}
