#include <sys/types.h>
#include <sys/time.h>
#include "src/clockspeed/readwrite.h"
#include "src/clockspeed/exit.h"
#include "src/clockspeed/select.h"
#include "src/clockspeed/scan.h"
#include "src/clockspeed/fmt.h"
#include "src/clockspeed/str.h"
#include "src/clockspeed/fifo.h"
#include "src/clockspeed/open.h"
#include "src/clockspeed/error.h"
#include "src/clockspeed/timing.h"
#include "src/configmake.h"

#ifndef HASRDTSC
#ifndef HASGETHRTIME

  Error! Need an unadjusted hardware clock.

#endif
#endif

struct point {
  timing lowlevel;
  timing_basic ostime;
  double adj; /* real - ostime, if flagknown; else 0 */
  int flagknown;
} ;

void now(p)
struct point *p;
{
  timing_now(&p->lowlevel);
  timing_basic_now(&p->ostime);
  p->adj = 0;
  p->flagknown = 0;
}

double nano(buf)
unsigned char buf[16];
{
  unsigned long u;
  double result;

  /* XXX: ignoring buf[0...3] */

  u = buf[12];
  u <<= 8; u += buf[13];
  u <<= 8; u += buf[14];
  u <<= 8; u += buf[15];
  result = u * 0.000000001;

  u = buf[8];
  u <<= 8; u += buf[9];
  u <<= 8; u += buf[10];
  u <<= 8; u += buf[11];
  result += u;

  u = buf[4];
  u <<= 8; u += buf[5];
  u <<= 8; u += buf[6];
  u <<= 8; u += buf[7];
  if (u < 2147483648UL)
    result += 1000000000.0 * u;
  else
    result += 0.0 - 1000000000.0 * (4294967295UL + 1 - u);

  return result;
}

struct point first;
struct point current;
unsigned char buf[16];

double deriv = 0; /* 0 for unknown */

void savederiv()
{
  int fd;
  double z;
  unsigned long u;

  if (deriv <= 0) return;
  if (deriv > 200000000) return; /* 5Hz ticks? be serious */

  fd = open_trunc(TLSDATE_CLOCKSPEED_ATTO_TMP);
  if (fd == -1) return;

  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  buf[4] = 0;
  buf[5] = 0;
  buf[6] = 0;
  buf[7] = 0;

  z = deriv;
  u = z;
  if (u > z) --u;
  if (u > 999999999) u = 999999999;
  z -= u;
  buf[11] = u; u >>= 8;
  buf[10] = u; u >>= 8;
  buf[9] = u; u >>= 8;
  buf[8] = u;

  z *= 1000000000.0;
  u = z;
  if (u > z) --u;
  if (u > 999999999) u = 999999999;
  z -= u;
  buf[15] = u; u >>= 8;
  buf[14] = u; u >>= 8;
  buf[13] = u; u >>= 8;
  buf[12] = u;

  if (write(fd,buf,sizeof buf) < sizeof buf) { close(fd); return; }
  if (fsync(fd) == -1) { close(fd); return; }
  if (close(fd) == -1) return; /* NFS stupidity */

  rename(TLSDATE_CLOCKSPEED_ATTO_TMP, TLSDATE_CLOCKSPEED_ATTO_TMP); /* if it fails, bummer */
}

void main()
{
  struct timeval tvselect;
  fd_set rfds;
  int r;
  double deltareal;
  double deltalowlevel;
  struct timeval tvchange;

  close(0);

  if (chdir(TLSDATE_CLOCKSPEED_HOME) == -1) _exit(1);
  umask(033);

  if (open_read(TLSDATE_CLOCKSPEED_ATTO) == 0) {
    r = read(0,buf,sizeof buf);
    if (r == sizeof buf)
      deriv = nano(buf);
    close(0);
  }

  if (fifo_make("adjust",0600) == -1) if (errno != error_exist) _exit(1);
  if (open_read("adjust") != 0) _exit(1);
  if (open_write("adjust") == -1) _exit(1);

  now(&first);

  for (;;) {
    FD_ZERO(&rfds);
    FD_SET(0,&rfds);

    tvselect.tv_sec = 3;
    tvselect.tv_usec = 0;

    if (select(1,&rfds,(fd_set *) 0,(fd_set *) 0,&tvselect) == 1) {
      r = read(0,buf,sizeof buf);
      if (r <= 0) _exit(1); /* not possible */

      /* XXX: ignoring partial packets */
      if (r == sizeof buf) {
        now(&current);
        current.adj = nano(buf);
        current.flagknown = 1;

        if (!first.flagknown) first = current;

        deltalowlevel = timing_diff(&current.lowlevel,&first.lowlevel);
        deltareal = timing_basic_diff(&current.ostime,&first.ostime);
        deltareal += current.adj - first.adj;
        if (deltareal > 10.0) {
          deriv = deltareal / deltalowlevel;
          savederiv();
        }
      }
    }

    if (deriv) {
      now(&current);
  
      deltalowlevel = timing_diff(&current.lowlevel,&first.lowlevel);
      deltareal = deltalowlevel * deriv;
      deltareal -= timing_basic_diff(&current.ostime,&first.ostime);
      deltareal += first.adj;
  
      deltareal *= 0.001;
      if (deltareal > 99999999.0) deltareal = 99999999.0;
      if (deltareal < -99999999.0) deltareal = -99999999.0;
  
      tvchange.tv_sec = 0;
      tvchange.tv_usec = deltareal;
      while (tvchange.tv_usec < 0) {
        tvchange.tv_sec -= 1;
        tvchange.tv_usec += 1000000;
      }
      while (tvchange.tv_usec > 999999) {
        tvchange.tv_sec += 1;
        tvchange.tv_usec -= 1000000;
      }

      adjtime(&tvchange,(struct timeval *) 0); /* if it fails, bummer */
    }
  }
}
