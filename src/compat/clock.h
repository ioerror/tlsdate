/* Copyright (c) 2012, David Goulet <dgoulet@ev0ke.net>
 *                     Jacob Appelbaum
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file clock.h
  * \brief Header file for the clock primitives.
  **/

#ifdef __linux__

#include <time.h>

struct tlsdate_time {
    struct timespec tp;
};

extern int clock_get_real_time_linux(struct tlsdate_time *time);
#define clock_get_real_time(time) clock_get_real_time_linux(time)

extern int clock_set_real_time_linux(const struct tlsdate_time *time);
#define clock_set_real_time(time) clock_set_real_time_linux(time)

extern void clock_init_time_linux(struct tlsdate_time *time, time_t sec,
                                  long nsec);
#define clock_init_time(time, sec, nsec) \
        clock_init_time_linux(time, sec, nsec)

/* Helper macros to access time values */
#define CLOCK_SEC(time)  ((time)->tp.tv_sec)
#define CLOCK_MSEC(time) ((time)->tp.tv_nsec / 1000000)
#define CLOCK_USEC(time) ((time)->tp.tv_nsec / 1000)
#define CLOCK_NSEC(time) ((time)->tp.tv_nsec)

#elif _WIN32

struct tlsdate_time {
    /* TODO: Fix Windows support */
};

TLSDATE_API
int clock_get_real_time_win(struct tlsdate_time *time);
#define clock_get_real_time(time) clock_get_real_time_win(time)

extern int clock_set_real_time_win(const struct tlsdate_time *time);
#define clock_set_real_time(time) clock_set_real_time_win(time)

TLSDATE_API
void clock_init_time_win(struct tlsdate_time *time, time_t sec,
                                long nsec);
#define clock_init_time(time, sec, nsec) \
        clock_init_time_win(time, sec, nsec)

/* Helper macros to access time values. TODO: Complete them */
#define CLOCK_SEC(time)
#define CLOCK_MSEC(time)
#define CLOCK_USEC(time)
#define CLOCK_NSEC(time)

#endif /* __linux__ _WIN32 */
