/* event.h - event sources */

#ifndef EVENT_H
#define EVENT_H

struct event;

extern struct event *event_fdread(int fd);
extern struct event *event_routeup(void);
extern struct event *event_suspend(void);
extern struct event *event_every(int seconds);

extern struct event *event_composite(void);
extern int event_composite_add(struct event *comp, struct event *e);
extern int event_composite_del(struct event *comp, struct event *e);

extern int event_wait(struct event *e);

extern void event_free(struct event *e);

#endif /* !EVENT_H */
