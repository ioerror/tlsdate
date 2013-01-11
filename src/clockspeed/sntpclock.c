#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "strerr.h"
#include "ip.h"
#include "str.h"
#include "byte.h"
#include "substdio.h"
#include "readwrite.h"
#include "select.h"
#include "scan.h"
#include "leapsecs.h"
#include "tai.h"
#include "taia.h"

#define NTP_OFFSET 2208988790UL /* TAI64 baseline - NTP epoch */
/* TAI64 baseline is 1970-01-01 00:00:00 = 4000000000000000 TAI64 */
/* NTP epoch is 1900-01-01 00:00:10 = 3fffffff7c55818a TAI64 */

void ntp_taia(ntp,ta,flagleap)
unsigned char *ntp;
struct taia *ta;
int flagleap;
{
  unsigned char buf[16];
  struct tai t;
  unsigned long u;
  double z;

  u = (unsigned long) ntp[0];
  u <<= 8; u += (unsigned long) ntp[1];
  u <<= 8; u += (unsigned long) ntp[2];
  u <<= 8; u += (unsigned long) ntp[3];
  u -= NTP_OFFSET;

  /* safe to assume that now is past 1970 */

  buf[0] = 64;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  buf[7] = u; u >>= 8;
  buf[6] = u; u >>= 8;
  buf[5] = u; u >>= 8;
  buf[4] = u;

  tai_unpack(buf,&t);
  leapsecs_add(&t,flagleap);
  tai_pack(buf,&t);

  u = (unsigned long) ntp[4];
  u <<= 8; u += (unsigned long) ntp[5];
  u <<= 8; u += (unsigned long) ntp[6];
  u <<= 8; u += (unsigned long) ntp[7];
  z = u / 4294967296.0;

  z *= 1000000000.0;
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
  buf[15] = u; u >>= 8;
  buf[14] = u; u >>= 8;
  buf[13] = u; u >>= 8;
  buf[12] = u;

  taia_unpack(buf,ta);
}

char outbuf[16];
substdio ssout = SUBSTDIO_FDBUF(write,1,outbuf,sizeof outbuf);

#define FATAL "sntpclock: fatal: "
#define WARNING "sntpclock: warning: "

void die_usage()
{
  strerr_die1x(100,"sntpclock: usage: sntpclock ip.ad.dr.ess");
}

char *host;
struct ip_address ipremote;
struct sockaddr_in sa;
int s;

unsigned char query[48];
unsigned char response[128];

char initdeltaoffset[] = {0,0,0,0,0,2,163,0,0,0,0,0,0,0,0,0};
char initdeltamin[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char initdeltamax[] = {0,0,0,0,0,5,70,0,0,0,0,0,0,0,0,0};
char initerrmin[] = {255,255,255,255,255,255,255,254,0,0,0,0,0,0,0,0};
char initerrmax[] = {0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0};
struct taia deltaoffset;
struct taia deltamin;
struct taia deltamax;
struct taia errmin;
struct taia errmax;

struct taia ta0;
struct taia ta1;
struct taia taremote;

struct taia temp1;
struct taia temp2;

unsigned char adj[16];

void main(argc,argv)
int argc;
char **argv;
{
  struct timeval tvselect;
  fd_set rfds;
  char *x;
  unsigned long u;
  int r;
  int loop;
  struct timeval tvcookie;
  int flagleap;

  taia_unpack(initdeltamin,&deltamin);
  taia_unpack(initdeltamax,&deltamax);
  taia_unpack(initdeltaoffset,&deltaoffset);
  taia_unpack(initerrmin,&errmin);
  taia_unpack(initerrmax,&errmax);

  if (leapsecs_init() == -1)
    strerr_die2sys(111,FATAL,"unable to initialize leap seconds: ");

  host = argv[1];
  if (!host) die_usage();
  if (!str_diff(host,"0")) host = "127.0.0.1";
  if (host[ip_scan(host,&ipremote)]) die_usage();

  s = socket(AF_INET,SOCK_DGRAM,0);
  if (s == -1)
    strerr_die2sys(111,FATAL,"unable to create socket: ");

  byte_zero(&sa,sizeof(sa));
  byte_copy(&sa.sin_addr,4,&ipremote);
  x = (char *) &sa.sin_port;
  x[0] = 0;
  x[1] = 123; /* NTP */
  sa.sin_family = AF_INET;

  for (loop = 0;loop < 10;++loop) {
    byte_zero(query,sizeof query);
    query[0] = 27; /* client, NTP version 3 */
    query[2] = 8;
  
    gettimeofday(&tvcookie,(struct timezone *) 0);
    u = tvcookie.tv_sec + NTP_OFFSET;
    query[43] = u; u >>= 8;
    query[42] = u; u >>= 8;
    query[41] = u; u >>= 8;
    query[40] = u;
    u = tvcookie.tv_usec;
    query[45] = u; u >>= 8; /* deliberately inaccurate; this is a cookie */
    query[44] = u;
    u = getpid();
    query[47] = u; u >>= 8;
    query[46] = u;
  
    taia_now(&ta0);
    if (sendto(s,query,sizeof query,0,(struct sockaddr *) &sa,sizeof sa) == -1)
      strerr_die2sys(111,FATAL,"unable to send request: ");
    FD_ZERO(&rfds);
    FD_SET(s,&rfds);
    tvselect.tv_sec = 1;
    tvselect.tv_usec = 0;
    if (select(s + 1,&rfds,(fd_set *) 0,(fd_set *) 0,&tvselect) != 1) {
      strerr_warn2(WARNING,"unable to read clock: timed out",0);
      continue;
    }
    r = recv(s,response,sizeof response,0);
    if (r == -1) {
      strerr_warn2(WARNING,"unable to read clock: ",&strerr_sys);
      continue;
    }
    taia_now(&ta1);
    if (   (r < 48)
	|| (r >= sizeof response)
	|| (((response[0] & 7) != 2) && ((response[0] & 7) != 4))
	|| !(response[0] & 56)
	|| byte_diff(query + 40,8,response + 24)
       ) {
      strerr_warn2(WARNING,"unable to read clock: bad response format",0);
      continue;
    }

    flagleap = ((response[0] & 192) == 64);

    ntp_taia(response + 32,&taremote,flagleap);
    taia_add(&taremote,&taremote,&deltaoffset);

    taia_add(&temp1,&deltamax,&ta0);
    taia_add(&temp2,&deltamin,&ta0);
    if (taia_less(&taremote,&temp1) && !taia_less(&taremote,&temp2)) {
      taia_sub(&temp1,&taremote,&ta0);
      deltamax = temp1;
    }

    ntp_taia(response + 40,&taremote,flagleap);
    taia_add(&taremote,&taremote,&deltaoffset);
  
    taia_add(&temp1,&deltamax,&ta1);
    taia_add(&temp2,&deltamin,&ta1);
    if (taia_less(&temp2,&taremote) && !taia_less(&temp1,&taremote)) {
      taia_sub(&temp2,&taremote,&ta1);
      deltamin = temp2;
    }
  }

  taia_sub(&temp1,&deltamax,&deltamin);
  if (taia_less(&errmax,&temp1) && taia_less(&temp1,&errmin))
    strerr_die2x(111,FATAL,"time uncertainty too large");

  taia_add(&temp1,&deltamax,&deltamin);
  taia_half(&temp1,&temp1);
  taia_sub(&temp1,&temp1,&deltaoffset);

  taia_pack(adj,&temp1);

  if (substdio_putflush(&ssout,adj,sizeof adj) == -1)
    strerr_die2sys(111,FATAL,"unable to write output: ");
  _exit(0);
}
