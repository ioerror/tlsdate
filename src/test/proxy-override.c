#include <string.h>

int main(int argc, char *argv[])
{
  int saw_good_proxy = 0;
  while (argc--) {
    if (!strcmp(argv[0], "socks5://good.proxy"))
      saw_good_proxy = 1;
    if (!strcmp(argv[0], "socks5://bad.proxy"))
      return 2;
    argv++;
  }
  return !saw_good_proxy;
}
