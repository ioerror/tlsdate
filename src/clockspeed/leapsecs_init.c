#include "leapsecs.h"

static int flaginit = 0;

int leapsecs_init()
{
  if (flaginit) return 0;
  if (leapsecs_read() == -1) return -1;
  flaginit = 1;
  return 0;
}
