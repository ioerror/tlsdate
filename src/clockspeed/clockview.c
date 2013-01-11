#include <sys/types.h>
#include <sys/time.h>
#include "substdio.h"
#include "readwrite.h"
#include "strerr.h"
#include "exit.h"
#include "fmt.h"

char outbuf[256];
substdio ssout = SUBSTDIO_FDBUF(write,1,outbuf,sizeof outbuf);

#define FATAL "clockview: fatal: "

time_t when;
unsigned long nano;
unsigned long atto;

void print()
{
  struct tm *tm;
  char strnum[FMT_ULONG];

  tm = localtime(&when);
  substdio_put(&ssout,strnum,fmt_uint0(strnum,1900 + tm->tm_year,4));
  substdio_puts(&ssout,"-");
  substdio_put(&ssout,strnum,fmt_uint0(strnum,1 + tm->tm_mon,2));
  substdio_puts(&ssout,"-");
  substdio_put(&ssout,strnum,fmt_uint0(strnum,tm->tm_mday,2));
  substdio_puts(&ssout," ");
  substdio_put(&ssout,strnum,fmt_uint0(strnum,tm->tm_hour,2));
  substdio_puts(&ssout,":");
  substdio_put(&ssout,strnum,fmt_uint0(strnum,tm->tm_min,2));
  substdio_puts(&ssout,":");
  substdio_put(&ssout,strnum,fmt_uint0(strnum,tm->tm_sec,2));
  substdio_puts(&ssout,".");
  substdio_put(&ssout,strnum,fmt_uint0(strnum,nano,9));
  substdio_put(&ssout,strnum,fmt_uint0(strnum,atto,9));
  substdio_puts(&ssout,"\n");
  substdio_flush(&ssout);
}

unsigned char buf[16];
struct timeval tv;

void main()
{
  unsigned long u;

  if (read(0,buf,sizeof buf) != sizeof buf)
    strerr_die2x(111,FATAL,"data split across packets");

  gettimeofday(&tv,(struct timezone *) 0);
  when = tv.tv_sec;
  nano = tv.tv_usec * 1000;
  atto = 0;
  substdio_puts(&ssout,"before: ");
  print();

  u = buf[12];
  u <<= 8; u += buf[13];
  u <<= 8; u += buf[14];
  u <<= 8; u += buf[15];

  atto += u;
  if (atto > 999999999) {
    atto -= 1000000000;
    ++nano;
  }

  u = buf[8];
  u <<= 8; u += buf[9];
  u <<= 8; u += buf[10];
  u <<= 8; u += buf[11];

  nano += u;
  if (nano > 999999999) {
    nano -= 1000000000;
    ++when;
  }

  u = buf[4];
  u <<= 8; u += buf[5];
  u <<= 8; u += buf[6];
  u <<= 8; u += buf[7];

  if (u < 2147483648UL)
    when += u;
  else
    when -= (4294967295UL + 1 - u);

  substdio_puts(&ssout,"after:  ");
  print();

  _exit(0);
}
