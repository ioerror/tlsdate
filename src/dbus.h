/*
 * dbus.h - event loop dbus integration
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef DBUS_H_
#define DBUS_H_

#include "config.h"

#ifdef HAVE_DBUS
#include <dbus/dbus.h>

#define SET_TIME_OK 0
#define SET_TIME_INVALID 1
#define SET_TIME_NOT_ALLOWED 2
#define SET_TIME_BAD_CALL 3

struct state;
int init_dbus (struct state *state);

struct dbus_state
{
  DBusConnection *conn;
};

struct dbus_event_data
{
  struct dbus_state *state;
  struct event *event;
};

void dbus_announce (struct state *);

#else  /* !HAVE_DBUS */
struct state;
static inline int init_dbus (struct state *state)
{
  return 0;
}
static inline void dbus_announce (struct state *global_state)
{
}
#endif

#endif  /* DBUS_H_ */
