#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "taia.h"
#include "byte.h"
#include "strerr.h"

#define FATAL "taiclockd: fatal: "

unsigned char packet[256];
struct sockaddr_in sa;
int s;

struct taia ta;

void main(argc,argv)
int argc;
char **argv;
{
  char *x;
  int len;
  int r;

  s = socket(AF_INET,SOCK_DGRAM,0);
  if (s == -1)
    strerr_die2sys(111,FATAL,"unable to create socket: ");

  byte_zero(&sa,sizeof(sa));
  x = (char *) &sa.sin_port;
  x[0] = 15;
  x[1] = 174;
  sa.sin_family = AF_INET;

  if (bind(s,(struct sockaddr *) &sa,sizeof sa) == -1)
    strerr_die2sys(111,FATAL,"unable to bind: ");

  for (;;) {
    len = sizeof sa;
    r = recvfrom(s,packet,sizeof packet,0,(struct sockaddr *) &sa,&len);
    if (r >= 20)
      if (!byte_diff(packet,4,"ctai")) {
	packet[0] = 's';
        taia_now(&ta);
        taia_pack(packet + 4,&ta);
        sendto(s,packet,r,0,(struct sockaddr *) &sa,len);
	  /* if it fails, bummer */
      }
  }
}
