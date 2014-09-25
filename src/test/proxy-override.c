/* This test is called in lieu of tlsdate by tlsdated
 * and it returns a timestamp that matches the proxy
 * ordering - global, dynamic, etc.
 * For use, see tlsdated-unittests.c
 */
#include "config.h"

#include <string.h>
#include <stdio.h>

int main (int argc, char *argv[])
{
  /* Unsigned int to match what tlsdate -Vraw returns, not time_t */
  /* TODO(wad) move tlsdated -Vraw to emitting time_t */
  unsigned int t = RECENT_COMPILE_DATE + 1;
  int saw_good_proxy = 0;
  while (argc--)
    {
      if (!strcmp (argv[0], "socks5://good.proxy"))
        saw_good_proxy = 1;
      if (!strcmp (argv[0], "socks5://bad.proxy"))
        {
          t = RECENT_COMPILE_DATE + 3;
          break;
        }
      argv++;
    }
  if (saw_good_proxy)
    t = RECENT_COMPILE_DATE + 2;
  fwrite (&t, sizeof (t), 1, stdout);
  return 0;
}
