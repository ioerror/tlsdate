/*
 * routeup.c - listens for routes coming up, tells stdout
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * We emit 'n' for a route coming up.
 */

#include "config.h"

#ifdef linux
#include <asm/types.h>
#endif

#include <sys/socket.h>   /* needed for linux/if.h for struct sockaddr */
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include "src/util.h"
#include "src/routeup.h"

int verbose;
int verbose_debug;

/*
 * Set up the supplied context by creating and binding its netlink socket.
 * Returns 0 for success, 1 for failure.
 */
int API
routeup_setup (struct routeup *rtc)
{
  struct sockaddr_nl sa;
  memset (&sa, 0, sizeof (sa));
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;
  rtc->netlinkfd = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (rtc->netlinkfd < 0)
    {
      perror ("netlink socket() failed");
      return 1;
    }
  if (bind (rtc->netlinkfd, (struct sockaddr *) &sa, sizeof (sa)) < 0)
    {
      perror ("netlink bind() failed");
      close (rtc->netlinkfd);
      return 1;
    }
  if (fcntl (rtc->netlinkfd, F_SETFL, O_NONBLOCK) < 0)
    {
      perror ("netlink fcntl(O_NONBLOCK) failed");
      close (rtc->netlinkfd);
      return 1;
    }
  return 0;
}

/*
 * Handle a single netlink message.
 * Returns 0 if there was a route status change, 1 if there
 * were no valid nlmsghdrs, and -1 if there was a read error.
 */
int API
routeup_process (struct routeup *rtc)
{
  char buf[4096];
  ssize_t sz;
  struct nlmsghdr *nh;
  if ( (sz = read (rtc->netlinkfd, buf, sizeof (buf))) < 0)
    return -1;
  for (nh = (struct nlmsghdr *) buf; NLMSG_OK (nh, sz);
       nh = NLMSG_NEXT (nh, sz))
    {
      /*
       * Unpack the netlink message into a bunch of... well...
       * netlink messages. The terminology is overloaded. Walk
       * through the message until we find a header of type
       * NLMSG_DONE.
       */
      if (nh->nlmsg_type == NLMSG_DONE)
        break;
      if (nh->nlmsg_type != RTM_NEWROUTE)
        continue;
      /*
       * Clear out the socket so we don't keep old messages
       * queued up and eventually overflow the receive buffer.
       */
      while (read (rtc->netlinkfd, buf, sizeof (buf)) > 0)
        /* loop through receive queue */;
      if (errno != EAGAIN) return -1;
      return 0;
    }
  return 1;
}


/*
 * Blocks until we get a route status change message then calls
 * route_process().  Returns 0 if there was a route state change, 1 if there
 * was a timeout, and -1 if there was a read error.
 */
int API
routeup_once (struct routeup *rtc, unsigned int timeout)
{
  int ret;
  struct timeval remaining;
  struct timeval *rp = timeout ? &remaining : NULL;
  fd_set fds;
  remaining.tv_sec = timeout;
  remaining.tv_usec = 0;
  FD_ZERO (&fds);
  FD_SET (rtc->netlinkfd, &fds);
  while (select (rtc->netlinkfd + 1, &fds, NULL, NULL, rp) >= 0)
    {
      FD_ZERO (&fds);
      FD_SET (rtc->netlinkfd, &fds);
      if (timeout && !remaining.tv_sec && !remaining.tv_usec)
        return 1;
      ret = routeup_process (rtc);
      if (ret == 1)
        continue;
      return ret;
    }
  return -1;
}

/* Tear down the supplied context by closing its netlink socket. */
void API
routeup_teardown (struct routeup *rtc)
{
  close (rtc->netlinkfd);
}

#ifdef ROUTEUP_MAIN
int API
main ()
{
  struct routeup rtc;
  if (routeup_setup (&rtc))
    return 1;
  while (!routeup_once (&rtc, 0))
    {
      printf ("n\n");
      fflush (stdout);
    }
  routeup_teardown (&rtc);
  return 0;
}
#endif /* ROUTEUP_MAIN */
