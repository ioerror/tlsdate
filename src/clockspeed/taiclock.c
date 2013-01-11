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
#include "taia.h"

char outbuf[16];
substdio ssout = SUBSTDIO_FDBUF(write,1,outbuf,sizeof outbuf);

#define FATAL "taiclock: fatal: "
#define WARNING "taiclock: warning: "

void die_usage()
{
  strerr_die1x(100,"taiclock: usage: taiclock ip.ad.dr.ess");
}

char *host;
struct ip_address ipremote;
struct sockaddr_in sa;
int s;

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

unsigned char query[32];
unsigned char response[32];
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

  taia_unpack(initdeltamin,&deltamin);
  taia_unpack(initdeltamax,&deltamax);
  taia_unpack(initdeltaoffset,&deltaoffset);
  taia_unpack(initerrmin,&errmin);
  taia_unpack(initerrmax,&errmax);

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
  x[0] = 15;
  x[1] = 174;
  sa.sin_family = AF_INET;

  for (loop = 0;loop < 10;++loop) {
    byte_zero(query,sizeof query);
    query[0] = 'c';
    query[1] = 't';
    query[2] = 'a';
    query[3] = 'i';

    /* XXX: cookie-building time */
    taia_now(&ta0);
    taia_pack(query + 16,&ta0);
    u = getpid();
    query[30] = u; u >>= 8;
    query[31] = u;

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
    if (   (r != sizeof response)
	|| (response[0] != 's')
	|| byte_diff(query + 1,3,response + 1)
	|| byte_diff(query + 20,12,response + 20)
       ) {
      strerr_warn2(WARNING,"unable to read clock: bad response format",0);
      continue;
    }

    taia_unpack(response + 4,&taremote);
    taia_add(&taremote,&taremote,&deltaoffset);

    taia_add(&temp1,&deltamax,&ta0);
    taia_add(&temp2,&deltamin,&ta0);
    if (taia_less(&taremote,&temp1) && !taia_less(&taremote,&temp2)) {
      taia_sub(&temp1,&taremote,&ta0);
      deltamax = temp1;
    }
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
