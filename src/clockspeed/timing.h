#ifndef TIMING_H
#define TIMING_H

#include "hasrdtsc.h"
#include "hasgethr.h"
#include <sys/types.h>
#include <sys/time.h>

typedef struct timeval timing_basic;
#define timing_basic_now(x) gettimeofday((x),(struct timezone *) 0)
#define timing_basic_diff(x,y) (1000.0 * ((x)->tv_usec - (double) (y)->tv_usec) + 1000000000.0 * ((x)->tv_sec - (double) (y)->tv_sec))


#ifdef HASRDTSC

typedef struct { unsigned long t[2]; } timing;
#define timing_now(x) asm volatile(".byte 15;.byte 49" : "=a"((x)->t[0]),"=d"((x)->t[1]))
#define timing_diff(x,y) (((x)->t[0] - (double) (y)->t[0]) + 4294967296.0 * ((x)->t[1] - (double) (y)->t[1]))

#else
#ifdef HASGETHRTIME

typedef struct { hrtime_t t; } timing;
#define timing_now(x) ((x)->t = gethrtime())
#define timing_diff(x,y) ((double) ((x)->t - (y)->t))

#else

#define timing timing_basic
#define timing_now timing_basic_now
#define timing_diff timing_basic_diff

#endif
#endif

#endif
