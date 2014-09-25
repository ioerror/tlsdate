/* Test invoked by tlsdated instead of tlsdate to
 * show allow arbitrary delays before returning a
 * "sane" time. This makes for easy timeout testing.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
  /* Unsigned int to match what tlsdate -Vraw returns, not time_t */
  unsigned int t = RECENT_COMPILE_DATE + 1;
  if (argc < 2)
    return 1;
  sleep (atoi (argv[1]));
  fwrite (&t, sizeof (t), 1, stdout);
  return 0;
}
