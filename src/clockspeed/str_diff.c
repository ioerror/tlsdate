#include "str.h"

int str_diff(s,t)
register char *s;
register char *t;
{
  register char x;

  for (;;) {
    x = *s; if (x != *t) break; if (!x) break; ++s; ++t;
    x = *s; if (x != *t) break; if (!x) break; ++s; ++t;
    x = *s; if (x != *t) break; if (!x) break; ++s; ++t;
    x = *s; if (x != *t) break; if (!x) break; ++s; ++t;
  }
  return ((int)(unsigned int)(unsigned char) x)
       - ((int)(unsigned int)(unsigned char) *t);
}
