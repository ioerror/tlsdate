/* Copyright (c) 2012, David Goulet <dgoulet@ev0ke.net>
 *                     Jacob Appelbaum <jacob@torproject.org>
 * Copyright (c) 2012, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
  * \file clock.h
  * \brief Header file for the clock primitives.
  **/

#pragma once
#ifndef CLOCK_HEADER_GUARD
#define CLOCK_HEADER_GUARD 1

#include <src/visibility.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef TARGET_OS_OPENBSD
#include <sys/time.h>
#endif

#ifdef HAVE_MACH_CLOCK_H
#include <mach/clock.h>
#endif
#ifdef HAVE_MACH_MACH_H
#include <mach/mach.h>
#endif

struct tlsdate_time {
#if defined(__linux__) || defined(__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__) || defined (__DragonFly__)
    struct timespec tp;
#elif defined(__APPLE__)
    mach_timespec_t tp;
#elif _WIN32
    void *tp;
#elif TARGET_OS_HAIKU
    struct timespec tp;
#elif TARGET_OS_CYGWIN
    struct timespec tp;
#elif TARGET_OS_MINGW
    struct timespec tp;
#elif TARGET_OS_GNUHURD
    struct timespec tp;
#else
    struct timespec tp;
#endif
};

TLSDATE_API
int clock_get_real_time(struct tlsdate_time *time);

TLSDATE_API
int clock_set_real_time(const struct tlsdate_time *time);

TLSDATE_API
void clock_init_time(struct tlsdate_time *time, time_t sec, long nsec);

/* Helper macros to access time values */
#define CLOCK_SEC(time)  ((time)->tp.tv_sec)
#define CLOCK_MSEC(time) ((time)->tp.tv_nsec / 1000000)
#define CLOCK_USEC(time) ((time)->tp.tv_nsec / 1000)
#define CLOCK_NSEC(time) ((time)->tp.tv_nsec)

/* Helper macros to access time values. TODO: Complete them */
/*
#define CLOCK_SEC(time)
#define CLOCK_MSEC(time)
#define CLOCK_USEC(time)
#define CLOCK_NSEC(time)
*/
#endif // CLOCK_HEADER_GUARD
