#include <sys/types.h>
#include <sys/time.h>
#include "substdio.h"
#include "readwrite.h"
#include "strerr.h"
#include "exit.h"
#include "select.h"

#define FATAL "clockadd: fatal: "

unsigned char buf[16];
struct timeval tv;

void main()
{
  unsigned long u;
  unsigned long v;
  long adj;
  struct timeval tvselect;

  if (read(0,buf,sizeof buf) != sizeof buf)
    strerr_die2x(111,FATAL,"data split across packets");

  u = buf[4];
  u <<= 8; u += buf[5];
  u <<= 8; u += buf[6];
  u <<= 8; u += buf[7];

  if (u < 2147483648UL)
    adj = u;
  else
    adj = -(4294967295UL + 1 - u);

  v = buf[8];
  v <<= 8; v += buf[9];
  v <<= 8; v += buf[10];
  v <<= 8; v += buf[11];
  v /= 1000;

  /* XXX: Solaris stupidity */
  gettimeofday(&tv,(struct timezone *) 0);
  tvselect.tv_sec = 0;
  tvselect.tv_usec = 1000000 - ((tv.tv_usec + v) % 1000000);
  select(1,(fd_set *) 0,(fd_set *) 0,(fd_set *) 0,&tvselect);

  gettimeofday(&tv,(struct timezone *) 0);
  tv.tv_sec += adj;
  tv.tv_usec += v;
  if (tv.tv_usec > 999999) {
    tv.tv_usec -= 1000000;
    tv.tv_sec += 1;
  }

  if (settimeofday(&tv,(struct timezone *) 0) == -1)
    strerr_die2sys(111,FATAL,"unable to settimeofday: ");

  _exit(0);
}
