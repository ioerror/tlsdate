#include "substdio.h"
#include "str.h"
#include "byte.h"
#include "error.h"

static int allwrite(op,fd,buf,len)
register int (*op)();
register int fd;
register char *buf;
register int len;
{
  register int w;

  while (len) {
    w = op(fd,buf,len);
    if (w == -1) {
      if (errno == error_intr) continue;
      return -1; /* note that some data may have been written */
    }
    if (w == 0) ; /* luser's fault */
    buf += w;
    len -= w;
  }
  return 0;
}

int substdio_flush(s)
register substdio *s;
{
  register int p;
 
  p = s->p;
  if (!p) return 0;
  s->p = 0;
  return allwrite(s->op,s->fd,s->x,p);
}

int substdio_bput(s,buf,len)
register substdio *s;
register char *buf;
register int len;
{
  register int n;
 
  while (len > (n = s->n - s->p)) {
    byte_copy(s->x + s->p,n,buf); s->p += n; buf += n; len -= n;
    if (substdio_flush(s) == -1) return -1;
  }
  /* now len <= s->n - s->p */
  byte_copy(s->x + s->p,len,buf);
  s->p += len;
  return 0;
}

int substdio_put(s,buf,len)
register substdio *s;
register char *buf;
register int len;
{
  register int n;
 
  n = s->n;
  if (len > n - s->p) {
    if (substdio_flush(s) == -1) return -1;
    /* now s->p == 0 */
    if (n < SUBSTDIO_OUTSIZE) n = SUBSTDIO_OUTSIZE;
    while (len > s->n) {
      if (n > len) n = len;
      if (allwrite(s->op,s->fd,buf,n) == -1) return -1;
      buf += n;
      len -= n;
    }
  }
  /* now len <= s->n - s->p */
  byte_copy(s->x + s->p,len,buf);
  s->p += len;
  return 0;
}

int substdio_putflush(s,buf,len)
register substdio *s;
register char *buf;
register int len;
{
  if (substdio_flush(s) == -1) return -1;
  return allwrite(s->op,s->fd,buf,len);
}

int substdio_bputs(s,buf)
register substdio *s;
register char *buf;
{
  return substdio_bput(s,buf,str_len(buf));
}

int substdio_puts(s,buf)
register substdio *s;
register char *buf;
{
  return substdio_put(s,buf,str_len(buf));
}

int substdio_putsflush(s,buf)
register substdio *s;
register char *buf;
{
  return substdio_putflush(s,buf,str_len(buf));
}
