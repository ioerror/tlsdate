#ifndef SUBSTDIO_H
#define SUBSTDIO_H

typedef struct substdio {
  char *x;
  int p;
  int n;
  int fd;
  int (*op)();
} substdio;

#define SUBSTDIO_FDBUF(op,fd,buf,len) { (buf), 0, (len), (fd), (op) }

extern void substdio_fdbuf();

extern int substdio_flush();
extern int substdio_put();
extern int substdio_bput();
extern int substdio_putflush();
extern int substdio_puts();
extern int substdio_bputs();
extern int substdio_putsflush();

extern int substdio_get();
extern int substdio_bget();
extern int substdio_feed();

extern char *substdio_peek();
extern void substdio_seek();

#define substdio_fileno(s) ((s)->fd)

#define SUBSTDIO_INSIZE 8192
#define SUBSTDIO_OUTSIZE 8192

#define substdio_PEEK(s) ( (s)->x + (s)->n )
#define substdio_SEEK(s,len) ( ( (s)->p -= (len) ) , ( (s)->n += (len) ) )

#define substdio_BPUTC(s,c) \
  ( ((s)->n != (s)->p) \
    ? ( (s)->x[(s)->p++] = (c), 0 ) \
    : substdio_bput((s),&(c),1) \
  )

extern int substdio_copy();

#endif
