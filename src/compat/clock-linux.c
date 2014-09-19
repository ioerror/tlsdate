/* Copyright (c) 2012, David Goulet <dgoulet@ev0ke.net>
 *                     Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file clock-linux.c
  * \brief Contains clock primitives for GNU/Linux OS
  **/

#include "config.h"

#include <assert.h>

#include "src/compat/clock.h"

/**
 * Get current real time value and store it into time.
 *
 * @param time where the current time is stored
 * @return clock_gettime syscall return value
 */
int clock_get_real_time(struct tlsdate_time *time)
{
  /* Safety net */
  assert (time);
  return clock_gettime (CLOCK_REALTIME, &time->tp);
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
  assert (time);
  return clock_settime (CLOCK_REALTIME, &time->tp);
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
  assert (time);
  time->tp.tv_sec = sec;
  time->tp.tv_nsec = nsec;
}
