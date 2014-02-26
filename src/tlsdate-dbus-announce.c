/*
 * tlsdate-dbus-announce.c - announce date change on dbus
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#include <stdint.h>

int main(void)
{
  DBusConnection *conn = NULL;
  DBusMessage *msg = NULL;
  DBusError error;
  uint32_t ignored;

  dbus_error_init(&error);
  conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  if (!conn)
    return 1;
  if (dbus_bus_request_name(conn, "org.torproject.tlsdate", 0, &error) < 0)
    return 1;
  msg = dbus_message_new_signal("/org/torproject/tlsdate", "org.torproject.tlsdate", "TimeUpdated");
  if (!msg)
    return 1;
  if (!dbus_connection_send(conn, msg, &ignored))
    return 1;
  dbus_connection_flush(conn);
  dbus_message_unref(msg);
  return 0;
}
#else
int main(void)
{
  return 2;
}
#endif
