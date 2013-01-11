#ifndef TAI_H
#define TAI_H

#include "uint64.h"

struct tai {
  uint64 x;
} ;

extern void tai_now();

#define tai_approx(t) ((double) ((t)->x))

extern void tai_add();
extern void tai_sub();
#define tai_less(t,u) ((t)->x < (u)->x)

#define TAI_PACK 8
extern void tai_pack();
extern void tai_unpack();

#endif
