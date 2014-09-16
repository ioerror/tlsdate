/*
 * util.c - routeup/tlsdated utility functions
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "tlsdate.h"

#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#ifdef HAVE_LINUX_RTC_H
#include <linux/rtc.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#ifdef WITH_EVENTS
#include <event2/event.h>
#endif

#include "src/tlsdate.h"
#include "src/util.h"

#if defined(HAVE_STRUCT_RTC_TIME) && defined(RTC_SET_TIME) && defined(RTC_RD_TIME)
#define ENABLE_RTC
#endif

const char *kTempSuffix = DEFAULT_DAEMON_TMPSUFFIX;

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

  if (! verbose ) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

/** helper function for 'verbose' output */
void
verb_debug (const char *fmt, ...)
{
  va_list ap;

  if (! verbose ) return;
  if (! verbose_debug ) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
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

#ifdef ENABLE_RTC
struct rtc_handle
{
	int fd;
};

void *rtc_open()
{
	struct rtc_handle *h = malloc(sizeof *h);
	h->fd = open(DEFAULT_RTC_DEVICE, O_RDONLY);
	if (h->fd < 0)
  {
		pinfo("can't open rtc");
		free(h);
		return NULL;
	}
	return h;
}

int rtc_write(void *handle, const struct timeval *tv)
{
  struct tm tmr;
  struct tm *tm;
  struct rtc_time rtctm;
  int fd = ((struct rtc_handle *)handle)->fd;

  tm = gmtime_r (&tv->tv_sec, &tmr);

  /* these structs are identical, but separately defined */
  rtctm.tm_sec = tm->tm_sec;
  rtctm.tm_min = tm->tm_min;
  rtctm.tm_hour = tm->tm_hour;
  rtctm.tm_mday = tm->tm_mday;
  rtctm.tm_mon = tm->tm_mon;
  rtctm.tm_year = tm->tm_year;
  rtctm.tm_wday = tm->tm_wday;
  rtctm.tm_yday = tm->tm_yday;
  rtctm.tm_isdst = tm->tm_isdst;

  if (ioctl (fd, RTC_SET_TIME, &rtctm))
  {
    pinfo ("ioctl(%d, RTC_SET_TIME, ...) failed", fd);
    return 1;
  }

  info ("synced rtc to sysclock");
  return 0;
}

int rtc_read(void *handle, struct timeval *tv)
{
  struct tm tm;
  struct rtc_time rtctm;
  int fd = ((struct rtc_handle *)handle)->fd;

  if (ioctl (fd, RTC_RD_TIME, &rtctm))
  {
    pinfo ("ioctl(%d, RTC_RD_TIME, ...) failed", fd);
    return 1;
  }

  tm.tm_sec = rtctm.tm_sec;
  tm.tm_min = rtctm.tm_min;
  tm.tm_hour = rtctm.tm_hour;
  tm.tm_mday = rtctm.tm_mday;
  tm.tm_mon = rtctm.tm_mon;
  tm.tm_year = rtctm.tm_year;
  tm.tm_wday = rtctm.tm_wday;
  tm.tm_yday = rtctm.tm_yday;
  tm.tm_isdst = rtctm.tm_isdst;

  tv->tv_sec = mktime(&tm);
  tv->tv_usec = 0;

  return 0;
}

int rtc_close(void *handle)
{
	struct rtc_handle *h = handle;
	close(h->fd);
	free(h);
	return 0;
}
#endif

int file_write(const char *path, void *buf, size_t sz)
{
	char tmp[PATH_MAX];
	int oflags = O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC;
	int perms = S_IRUSR | S_IWUSR;
	int fd;

	if (snprintf(tmp, sizeof(tmp), path, kTempSuffix) >= sizeof(tmp))
  {
		pinfo("path %s too long to use", path);
		exit(1);
	}

	if ((fd = open(tmp, oflags, perms)) < 0)
  {
		pinfo("open(%s) failed", tmp);
		return 1;
	}

	if (write(fd, buf, sz) != sz)
  {
		pinfo("write() failed");
		close(fd);
		return 1;
	}

	if (close(fd))
  {
		pinfo("close() failed");
		return 1;
	}

	if (rename(tmp, path))
  {
		pinfo("rename() failed");
		return 1;
	}

	return 0;
}

int file_read(const char *path, void *buf, size_t sz)
{
	int fd = open(path, O_RDONLY | O_NOFOLLOW);
	if (fd < 0)
  {
		pinfo("open(%s) failed", path);
		return 1;
	}

	if (read(fd, buf, sz) != sz)
  {
		pinfo("read() failed");
		close(fd);
		return 1;
	}

	return close(fd);
}

int time_get(struct timeval *tv)
{
	return gettimeofday(tv, NULL);
}

int pgrp_enter(void)
{
	return setpgid(0, 0);
}

int pgrp_kill(void)
{
	pid_t grp = getpgrp();
	return kill(-grp, SIGKILL);
}

static struct platform default_platform = {
#ifdef ENABLE_RTC
	.rtc_open = rtc_open,
	.rtc_write = rtc_write,
	.rtc_read = rtc_read,
	.rtc_close = rtc_close,
#endif

	.file_write = file_write,
	.file_read = file_read,

	.time_get = time_get,

	.pgrp_enter = pgrp_enter,
	.pgrp_kill = pgrp_kill
};

struct platform *platform = &default_platform;

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
