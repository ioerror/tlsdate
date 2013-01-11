#include "leapsecs.h"
#include "tai.h"

/* XXX: breaks tai encapsulation */

extern struct tai *leapsecs;
extern int leapsecs_num;

void leapsecs_add(t,hit)
struct tai *t;
int hit;
{
  int i;
  uint64 u;

  if (leapsecs_init() == -1) return;

  u = t->x;

  for (i = 0;i < leapsecs_num;++i) {
    if (u < leapsecs[i].x) break;
    if (!hit || (u > leapsecs[i].x)) ++u;
  }

  t->x = u;
}
