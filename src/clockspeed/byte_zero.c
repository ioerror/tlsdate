#include "byte.h"

void byte_zero(s,n)
char *s;
register unsigned int n;
{
  for (;;) {
    if (!n) break; *s++ = 0; --n;
    if (!n) break; *s++ = 0; --n;
    if (!n) break; *s++ = 0; --n;
    if (!n) break; *s++ = 0; --n;
  }
}
