#include <sys/types.h>
#include <sys/time.h>

main()
{
  hrtime_t t;

  t = gethrtime();
}
