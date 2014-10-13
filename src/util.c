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
#include <sys/uio.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#ifdef WITH_EVENTS
#include <event2/event.h>
#endif

#include "src/tlsdate.h"
#include "src/util.h"

#ifdef HAVE_SECCOMP_FILTER
#include "src/seccomp.h"
#endif

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
  fprintf (stderr, "\n");
  va_end (ap);
  exit (1);
}

/* Initalize syslog */
void initalize_syslog (void)
{
  openlog ("tlsdated", LOG_PID, LOG_DAEMON);
}

/* Signal to syslog that we're finished logging */
void terminate_syslog (void)
{
  closelog ();
}

/** helper function for 'verbose' output without syslog support */
void
verb_no_syslog (const char *fmt, ...)
{
  va_list ap;

  if (! verbose ) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end(ap);
}

/** helper function for 'verbose' output */
void
verb (const char *fmt, ...)
{
  va_list ap;

  if (! verbose ) return;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end(ap);
  va_start(ap, fmt);
  vsyslog (LOG_DEBUG, fmt, ap);
  va_end(ap);
}

void API logat (int isverbose, const char *fmt, ...)
{
  va_list ap;
  if (isverbose && !verbose)
    return;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  fprintf (stderr, "\n");
  va_end (ap);
  va_start (ap, fmt);
  vsyslog (LOG_INFO, fmt, ap);
  va_end (ap);
}

void no_new_privs(void)
{
#ifdef TARGET_OS_LINUX
#ifdef HAVE_PRCTL // XXX: Make this specific to PR_SET_NO_NEW_PRIVS
  // Check to see if we're already set PR_SET_NO_NEW_PRIVS
  // This happens in tlsdated earlier than when tlsdate-helper drops
  // privileges.
  if (0 == prctl (PR_GET_NO_NEW_PRIVS)) {
    // Remove the ability to regain privilegess
    if (0 != prctl (PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
      die ("Failed to PR_SET_NO_NEW_PRIVS");
  } else {
    verb ("V: Parent process has already set PR_SET_NO_NEW_PRIVS");
  }
#else
  verb ("V: we are unwilling to set PR_SET_NO_NEW_PRIVS");
#endif
#endif
}

void enable_seccomp(void)
{
#ifdef HAVE_SECCOMP_FILTER
  int status;
  prctl (PR_SET_NAME, "tlsdate seccomp");
  verb ("V: seccomp support is enabled");
  if (enable_setter_seccomp())
  {
    status = SETTER_NO_SBOX;
    _exit (status);
  }
#else
  verb ("V: seccomp support is disabled");
#endif
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
int rtc_open(struct rtc_handle *h)
{
	if (!h)
		return -1;
	h->fd = -1;
	/* TODO: Use platform->file_open but drop NOFOLLOW? */
	h->fd = open(DEFAULT_RTC_DEVICE, O_RDONLY);
	if (h->fd < 0)
	{
		pinfo("can't open rtc");
		return -1;
	}
	return 0;
}

/*
 * Set the hardware clock referred to by fd (which should be a descriptor to
 * some device that implements the interface documented in rtc(4)) to the system
 * time. See hwclock(8) for details of why this is important. If we fail, we
 * just return - there's nothing the caller can really do about a failure of
 * this function except try later.
 */
int rtc_write(struct rtc_handle *handle, const struct timeval *tv)
{
  struct tm tmr;
  struct tm *tm;
  struct rtc_time rtctm;
  int fd = handle->fd;

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

int rtc_read(struct rtc_handle *handle, struct timeval *tv)
{
  struct tm tm;
  struct rtc_time rtctm;
  int fd = handle->fd;

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

int rtc_close(struct rtc_handle *handle)
{
	struct rtc_handle *h = handle;
	platform->file_close(h->fd);
	h->fd = -1;
	return 0;
}
#endif

int file_write(int fd, void *buf, size_t sz)
{
	struct iovec iov[1];
	ssize_t ret;
	iov[0].iov_base = buf;
	iov[0].iov_len = sz;
	ret = IGNORE_EINTR (pwritev (fd, iov, 1, 0));
	if (ret != sz)
	{
		return -1;
	}
	return 0;
}

int file_open(const char *path, int write, int cloexec)
{
	int fd;
	int oflags = cloexec ? O_CLOEXEC : 0;
	if (write)
	{
		int perms = S_IRUSR | S_IWUSR;
		oflags |= O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC;
		/* Rely on atomic write calls rather than rename() calls. */
		fd = open(path, oflags, perms);
	}
	else
	{
		oflags |= O_RDONLY | O_NOFOLLOW;
		fd = open(path, oflags);
	}
	if (fd < 0)
	{
		pinfo("open(%s) failed", path);
		return -1;
	}
	return fd;
}

int file_close(int fd)
{
	return close(fd);
}

int file_read(int fd, void *buf, size_t sz)
{
	struct iovec iov[1];
	iov[0].iov_base = buf;
	iov[0].iov_len = sz;
	if (preadv (fd, iov, 1, 0) != sz)
	{
		/* Returns -1 on read failure */
		return -1;
	}
	/* Returns 0 on a successful buffer fill. */
	return 0;
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

int process_signal(pid_t pid, int signal)
{
	return kill (pid, signal);
}

pid_t process_wait(pid_t pid, int *status, int forever)
{
  int flag = forever ? 0 : WNOHANG;
  return waitpid (pid, status, flag);
}

static struct platform default_platform = {
#ifdef ENABLE_RTC
	.rtc_open = rtc_open,
	.rtc_write = rtc_write,
	.rtc_read = rtc_read,
	.rtc_close = rtc_close,
#endif

	.file_open = file_open,
	.file_close = file_close,
	.file_write = file_write,
	.file_read = file_read,

	.time_get = time_get,

	.pgrp_enter = pgrp_enter,
	.pgrp_kill = pgrp_kill,

	.process_signal = process_signal,
	.process_wait = process_wait
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
