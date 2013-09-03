/* test returns a "sane" time if the host, port, and proxy
 * are passed in properly on the commandline.  The test
 * is invoked by tlsdated instead of tlsdate.
 * This expects host2, port2, proxy2.
 *
 * Paired with check-host-2.c, it allows for source rotation
 * testing.
 */
#include "config.h"
#include <string.h>
#include <stdio.h>

int main (int argc, char *argv[])
{
  unsigned int t = RECENT_COMPILE_DATE + 1;
  if (argc < 7)
    return 3;
  if (!strcmp (argv[2], "host2")
      && !strcmp (argv[4], "port2")
      && !strcmp (argv[6], "proxy2"))
    {
      fwrite (&t, sizeof (t), 1, stdout);
      return 0;
    }
  return 1;
}
