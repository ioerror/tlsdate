#include "fmt.h"

unsigned int fmt_str(s,t)
register char *s; register char *t;
{
  register unsigned int len;
  char ch;
  len = 0;
  if (s) { while (ch = t[len]) s[len++] = ch; }
  else while (t[len]) len++;
  return len;
}
