/* Copyright (c) 2012, David Goulet <dgoulet@ev0ke.net>
 *                     Jacob Appelbaum <jacob@torproject.org>
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file clock-darwin.c
  * \brief Contains clock primitives for Mac OS X (Tested on 10.8.2)
  **/

#include "config.h"
#include "clock.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/clock_priv.h>
#include <mach/mach.h>
#include <mach/clock_types.h>
#include <mach/mach_traps.h>
#include <mach/clock_reply.h>
#include <mach/mach_time.h>
#include <mach/mach_error.h>
#endif

#include <assert.h>

/**
 * Get current real time value and store it into time.
 *
 * @param time where the current time is stored
 * @return clock_gettime syscall return value
 */
int clock_get_real_time(struct tlsdate_time *time)
{
  /* Safety net */
  assert(time);

  kern_return_t r;
  clock_serv_t cclock;
  mach_timespec_t mts;

  r = host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
  if (r != KERN_SUCCESS)
  {
    fprintf(stderr, "host_get_clock_service failed!\n"); 
    return -1;
  }

  r = clock_get_time(cclock, &mts);
  if (r != KERN_SUCCESS)
  {
    fprintf(stderr, "clock_get_time failed!\n"); 
    return -1;
  }

  r = mach_port_deallocate(mach_task_self(), cclock);
  if (r != KERN_SUCCESS)
  {
    fprintf(stderr, "mach_port_deallocate failed!\n"); 
    return -1;
  }

  time->tp.tv_sec = mts.tv_sec;
  time->tp.tv_nsec = mts.tv_nsec;
  return r;
}

/**
 * Set current real time clock using time.
 *
 * @param time where the current time to set is stored
 * @return clock_settime syscall return value
 */
int clock_set_real_time(const struct tlsdate_time *time)
{
  /* Safety net */
  assert(time);

  //printf ("V: server time %u\n", (unsigned int) time->tp.tv_sec);
  int r;
  struct timeval tv = {time->tp.tv_sec, 0};

  r = settimeofday(&tv, NULL);
  if (r != 0)
  {
    fprintf(stderr, "setimeofday failed!\n"); 
    return -1;
  }

  return r;
}

/**
 * Init a tlsdate_time structure.
 *
 * @param sec is the seconds
 * @param nsec is the nanoseconds
 */
void clock_init_time(struct tlsdate_time *time, time_t sec,
                           long nsec)
{
  /* Safety net */
  assert(time);

  time->tp.tv_sec = sec;
  time->tp.tv_nsec = nsec;
}
