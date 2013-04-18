/*
 * Trivial strnlen(3) implementation
 */

#include <stddef.h>

#include "strnlen.h"

size_t
strnlen(const char *s, size_t limit)
{
  size_t len;

  for (len = 0; len < limit; len++)
    if (*s++ == '\0')
      break;

  return len;
}
