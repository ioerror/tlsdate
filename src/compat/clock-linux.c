/* Copyright (c) 2012, David Goulet <dgoulet@ev0ke.net>
 *                     Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file clock.c
  * \brief Contains clock primitives for Linux OS
  **/

#include <assert.h>

#include "clock.h"

/**
 * Get current real time value and store it into time.
 *
 * @param time where the current time is stored
 * @return clock_gettime syscall return value
 */
int clock_get_real_time_linux(struct tlsdate_time *the_time)
{
  /* Safety net */
  assert(the_time);

  return clock_gettime(CLOCK_REALTIME, &the_time->tp);
}

/**
 * Set current real time clock using time.
 *
 * @param time where the current time to set is stored
 * @return clock_settime syscall return value
 */
int clock_set_real_time_linux(const struct tlsdate_time *the_time)
{
  /* Safety net */
  assert(the_time);

  return clock_settime(CLOCK_REALTIME, &the_time->tp);
}

/**
 * Init a tlsdate_time structure.
 *
 * @param sec is the seconds
 * @param nsec is the nanoseconds
 */
void clock_init_time_linux(struct tlsdate_time *the_time, time_t sec,
                           long nsec)
{
  /* Safety net */
  assert(the_time);

  the_time->tp.tv_sec = sec;
  the_time->tp.tv_nsec = nsec;
}
