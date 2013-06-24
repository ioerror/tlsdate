#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "routeup.h"

struct event {
  const char *name;
  int (*fd)(struct event *);
  int (*wait)(struct event *);
  void (*free)(struct event *);
};

struct event_fd {
  struct event event;
  int fd;
  pid_t pid;
};

struct event_composite {
  struct event event;
  struct event **children;
  size_t nchildren;
};

static int _subproc_fd(struct event *e)
{
  struct event_fd *efd = (struct event_fd *)e;
  return efd->fd;
}

static int _subproc_wait(struct event *e)
{
  struct event_fd *efd = (struct event_fd *)e;
  char buf;
  if (read(_subproc_fd(e), &buf, 1) == 1)
    return 1;
  /* fd is broken...? */
  close(efd->fd);
  efd->fd = -1;
  return -1;
}

static const char kZero = '0';

static void _subproc_signal(int fd)
{
  ssize_t _unused = write(fd, &kZero, 1);
  (void)_unused;
}

static void _subproc_free(struct event *e)
{
  struct event_fd *efd = (struct event_fd *)e;
  if (efd->pid > 0) {
    kill(efd->pid, SIGKILL);
    waitpid(efd->pid, NULL, 0);
  }
  close(efd->fd);
  free(efd);
}

static struct event *_event_subproc(void (*func)(void *, int), void *arg)
{
  struct event_fd *ev = malloc(sizeof *ev);
  int fds[2];

  if (!ev)
    return NULL;

  ev->event.fd = _subproc_fd;
  ev->event.wait = _subproc_wait;
  ev->event.free = _subproc_free;

  if (pipe(fds))
  {
    free(ev);
    return NULL;
  }

  ev->pid = fork();
  if (ev->pid < 0)
  {
    close(fds[0]);
    close(fds[1]);
    free(ev);
    return NULL;
  }
  else if (ev->pid == 0)
  {
    close(fds[0]);
    func(arg, fds[1]);
    exit(0);
  }

  close(fds[1]);
  ev->fd = fds[0];

  return &ev->event;
}

static void _routeup(void *arg, int fd)
{
  struct routeup *rtup = arg;
  while (!routeup_once(rtup, 0))
    _subproc_signal(fd);
}

struct event *event_routeup()
{
  struct routeup *rtup = malloc(sizeof *rtup);
  struct event *e;
  if (!rtup)
    return NULL;
  if (!routeup_setup(rtup))
  {
    free(rtup);
    return NULL;
  }
  e = _event_subproc(_routeup, (void*)rtup);
  /* free it in the parent only */
  routeup_teardown(rtup);
  if (e)
    e->name = "routeup";
  return e;
}

static void _every(void *arg, int fd)
{
  int delay = *(int*)arg;
  while (1)
  {
    sleep(delay);
    _subproc_signal(fd);
  }
}

struct event *event_every(int seconds)
{
  struct event *e = _event_subproc(_every, (void *)&seconds);
  e->name = "every";
  return e;
}

static void _suspend(void *arg, int fd)
{
  while (1)
  {
    static const int kInterval = 60;
    static const int kThreshold = 3;
    time_t then = time(NULL);
    time_t now;
    sleep(kInterval);
    now = time(NULL);
    if (abs(now - then - kInterval) > kThreshold)
      _subproc_signal(fd);
  }
}

struct event *event_suspend()
{
  struct event *e = _event_subproc(_suspend, NULL);
  e->name = "suspend";
  return e;
}

struct event *event_fdread(int fd)
{
  struct event_fd *efd = malloc(sizeof *efd);
  efd->event.name = "fdread";
  efd->event.fd = _subproc_fd;
  efd->event.wait = _subproc_wait;
  efd->event.free = _subproc_free;
  efd->fd = fd;
  return &efd->event;
}

int event_wait(struct event *e)
{
  assert(e->wait);
  return e->wait(e);
}

void event_free(struct event *e)
{
  assert(e->free);
  return e->free(e);
}

static int _composite_fd(struct event *e)
{
  return -1;
}

static int _composite_wait(struct event *e)
{
  struct event_composite *ec = (struct event_composite *)e;
  fd_set fds;
  size_t i;
  int n;
  int maxfd = -1;

  FD_ZERO(&fds);
  for (i = 0; i < ec->nchildren; i++)
  {
    struct event *child = ec->children[i];
    int fd = child->fd(child);
    if (fd != -1)
      FD_SET(child->fd(child), &fds);
    if (fd > maxfd)
      maxfd = fd;
  }

  n = select(maxfd + 1, &fds, NULL, NULL, NULL);
  if (n < 0)
    return -1;
  for (i = 0; i < ec->nchildren; i++)
  {
    struct event *child = ec->children[i];
    int fd = child->fd(child);
    if (FD_ISSET(fd, &fds))
    {
      /* won't block, but clears the event */
      int r = child->wait(child);
      if (r)
        /* either error or success, but not 'no event' */
        return r;
    }
  }

  return 0;
}

static void _composite_free(struct event *e)
{
  struct event_composite *ec = (struct event_composite *)e;
  free(ec->children);
  free(ec);
}

struct event *event_composite(void)
{
  struct event_composite *ec = malloc(sizeof *ec);
  if (!ec)
    return NULL;
  ec->event.name = "composite";
  ec->event.fd = _composite_fd;
  ec->event.wait = _composite_wait;
  ec->event.free = _composite_free;
  ec->children = NULL;
  ec->nchildren = 0;
  return &ec->event;
}

int event_composite_add(struct event *comp, struct event *e)
{
  struct event_composite *ec = (struct event_composite *)comp;
  size_t i;
  size_t nsz;
  struct event **nch;
  for (i = 0; i < ec->nchildren; i++)
  {
    if (ec->children[i])
      continue;
    ec->children[i] = e;
    return 0;
  }
  nsz = ec->nchildren + 1;
  nch = realloc(ec->children, nsz * sizeof(struct event *));
  if (!nch)
    return 1;
  ec->nchildren = nsz;
  ec->children = nch;
  ec->children[nsz - 1] = e;
  return 0;
}

int event_composite_del(struct event *comp, struct event *e)
{
  struct event_composite *ec = (struct event_composite *)comp;
  size_t i;
  for (i = 0; i < ec->nchildren; i++)
  {
    if (ec->children[i] != e)
      continue;
    ec->children[i] = NULL;
    return 0;
  }
  return 1;
}
