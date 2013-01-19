#include <string.h>

int main(int argc, char *argv[])
{
  if (argc < 7)
    return 3;
  if (   !strcmp(argv[2], "host1")
      && !strcmp(argv[4], "port1")
      && !strcmp(argv[6], "proxy1"))
    return 1;
  if (   !strcmp(argv[2], "host2")
      && !strcmp(argv[4], "port2")
      && !strcmp(argv[6], "proxy2"))
    return 2;
  return 4;
}
