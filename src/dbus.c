/*
 * dbus.c - event loop dbus integration
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <dbus/dbus.h>
#include <event2/event.h>
#include "src/dbus.h"
#include "src/tlsdate.h"
#include "src/util.h"

/* Pointers are needed so that we don't have to deal with array-to-pointer
 * weirdness with DBus argument passing.
 */
static const char kServiceInterfaceData[] = "org.torproject.tlsdate";
static const char *kServiceInterface = kServiceInterfaceData;
static const char kServicePathData[] = "/org/torproject/tlsdate";
static const char *kServicePath = kServicePathData;
static const char kServiceSetTimeData[] = "SetTime";
static const char *kServiceSetTime = kServiceSetTimeData;
static const char kServiceCanSetTimeData[] = "CanSetTime";
static const char *kServiceCanSetTime = kServiceCanSetTimeData;
static const char kServiceLastSyncInfoData[] = "LastSyncInfo";
static const char *kServiceLastSyncInfo = kServiceLastSyncInfoData;

static const char kTimeUpdatedData[] = "TimeUpdated";
static const char *kTimeUpdated = kTimeUpdatedData;

static
short
dbus_to_event (unsigned int flags)
{
  short events = 0;
  if (flags & DBUS_WATCH_READABLE)
    events |= EV_READ;
  if (flags & DBUS_WATCH_WRITABLE)
    events |= EV_WRITE;
  return events;
}

static
unsigned int
event_to_dbus (short events)
{
  unsigned int flags = 0;
  if (events & EV_READ)
    flags |= DBUS_WATCH_READABLE;
  if (events & EV_WRITE)
    flags |= DBUS_WATCH_WRITABLE;
  return flags;
}

static
void
watch_handler (evutil_socket_t fd, short what, void *arg)
{
  DBusWatch *watch = arg;
  struct dbus_event_data *data = dbus_watch_get_data (watch);
  unsigned int flags = event_to_dbus (what);
  dbus_connection_ref (data->state->conn);
  while (!dbus_watch_handle (watch, flags))
    {
      info ("dbus_watch_handle waiting for memory . . .");
      /* TODO(wad) this seems like a bad idea. */
      sleep (1);
    }
  while (dbus_connection_dispatch (data->state->conn) ==
         DBUS_DISPATCH_DATA_REMAINS);
  dbus_connection_unref (data->state->conn);
}

static
dbus_bool_t
add_watch (DBusWatch *watch, void *user_data)
{
  struct state *tlsdate_state = user_data;
  struct dbus_state *state = tlsdate_state->dbus;
  struct dbus_event_data *data;
  /* Don't add anything if it isn't active. */
  data = dbus_malloc0 (sizeof (struct dbus_event_data));
  if (!data)
    return FALSE;
  data->state = state;
  data->event = event_new (tlsdate_state->base,
                           dbus_watch_get_unix_fd (watch),
                           EV_PERSIST|dbus_to_event (dbus_watch_get_flags (watch)),
                           watch_handler,
                           watch);
  if (!data->event)
    {
      dbus_free (data);
      return FALSE;
    }
  event_priority_set (data->event, PRI_WAKE);

  dbus_watch_set_data (watch, data, dbus_free);
  if (!dbus_watch_get_enabled (watch))
    return TRUE;
  /* Only add the event if it is enabled. */
  if (event_add (data->event, NULL))
    {
      error ("Could not add a new watch!");
      event_free (data->event);
      dbus_free (data);
      return FALSE;
    }
  return TRUE;
}

static
void
remove_watch (DBusWatch *watch, void *user_data)
{
  struct dbus_event_data *data = dbus_watch_get_data (watch);
  /* TODO(wad) should this just be in a free_function? */
  if (data && data->event)
    {
      event_del (data->event);
      event_free (data->event);
    }
}

static
void
toggle_watch (DBusWatch *watch, void *user_data)
{
  struct dbus_event_data *data = dbus_watch_get_data (watch);
  if (!data || !data->event)  /* should not be possible */
    return;
  /* If the event is pending, then we have to remove it to
   * disable it or remove it before re-enabling it.
   */
  if (event_pending (data->event,
                     dbus_to_event (dbus_watch_get_flags (watch)), NULL))
    event_del (data->event);
  if (dbus_watch_get_enabled (watch))
    {
      event_add (data->event, NULL);
    }
}

static
void
timeout_handler (evutil_socket_t fd, short what, void *arg)
{
  DBusTimeout *t = arg;
  struct dbus_event_data *data = dbus_timeout_get_data (t);
  dbus_connection_ref (data->state->conn);
  dbus_timeout_handle (t);
  dbus_connection_unref (data->state->conn);
}

static
dbus_bool_t
add_timeout (DBusTimeout *t, void *user_data)
{
  struct state *tlsdate_state = user_data;
  struct dbus_state *state = tlsdate_state->dbus;
  struct dbus_event_data *data;
  int ms = dbus_timeout_get_interval (t);
  struct timeval interval;
  data = dbus_malloc0 (sizeof (struct dbus_event_data));
  if (!data)
    return FALSE;
  interval.tv_sec = ms / 1000;
  interval.tv_usec = (ms % 1000) * 1000;
  data->state = state;
  data->event = event_new (tlsdate_state->base,
                           -1,
                           EV_TIMEOUT|EV_PERSIST,
                           timeout_handler,
                           t);
  if (!data->event)
    {
      dbus_free (data);
      return FALSE;
    }
  event_priority_set (data->event, PRI_WAKE);
  dbus_timeout_set_data (t, data, dbus_free);
  /* Only add it to the queue if it is enabled. */
  if (!dbus_timeout_get_enabled (t))
    return TRUE;
  if (event_add (data->event, &interval))
    {
      error ("Could not add a new timeout!");
      event_free (data->event);
      dbus_free (data);
      return FALSE;
    }
  return TRUE;
}

static
void
remove_timeout (DBusTimeout *t, void *user_data)
{
  struct dbus_event_data *data = dbus_timeout_get_data (t);
  if (data && data->event)
    {
      event_del (data->event);
      event_free (data->event);
    }
}

static
void
toggle_timeout (DBusTimeout *t, void *user_data)
{
  struct dbus_event_data *data = dbus_timeout_get_data (t);
  int ms = dbus_timeout_get_interval (t);
  struct timeval interval;
  /* If the event is pending, then we have to remove it to
   * disable it or remove it before re-enabling it.
   */
  if (evtimer_pending (data->event, NULL))
    event_del (data->event);
  if (dbus_timeout_get_enabled (t))
    {
      interval.tv_sec = ms / 1000;
      interval.tv_usec = (ms % 1000) * 1000;
      event_add (data->event, &interval);
    }
}

void
dbus_announce (struct state *global_state)
{
  struct dbus_state *state = global_state->dbus;
  DBusConnection *conn;
  DBusMessage *msg;
  uint32_t ignored;
  const char *sync_type = sync_type_str (global_state->last_sync_type);

#ifndef TLSDATED_MAIN
  /* Return early if we're not linked to tlsdated. */
  return;
#endif

  conn = state->conn;
  msg = dbus_message_new_signal (kServicePath, kServiceInterface, kTimeUpdated);
  if (!msg)
    {
      error ("[dbus] could not allocate new announce signal");
      return;
    }
  if (!dbus_message_append_args (msg,
                                 DBUS_TYPE_STRING, &sync_type,
                                 DBUS_TYPE_INVALID))
    {
      error ("[dbus] could not allocate new announce args");
      return;
    }
  if (!dbus_connection_send (conn, msg, &ignored))
    {
      error ("[dbus] could not send announce signal");
      return;
    }
}

static
DBusHandlerResult
send_time_reply (DBusConnection *connection,
                 DBusMessage *message,
                 dbus_uint32_t code)
{
   DBusMessage *reply;
   DBusMessageIter args;
   dbus_uint32_t serial = dbus_message_get_serial (message);

   reply = dbus_message_new_method_return (message);
   if (!reply)
     {
       error ("[dbus] no memory to reply to SetTime");
       return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
     }

  if (!dbus_message_set_reply_serial (reply, serial))
    {
      error ("[dbus] no memory to set serial for reply to SetTime");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  dbus_message_iter_init_append (reply, &args);
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_UINT32, &code))
    {
      error ("[dbus] no memory to add reply args to SetTime");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  if (!dbus_connection_send (connection, reply, &serial))
   {
      error ("[dbus] unable to send SetTime reply");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
   }
  dbus_connection_flush (connection);
  dbus_message_unref (reply);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
DBusHandlerResult
send_can_reply (DBusConnection *connection,
                DBusMessage *message,
                dbus_bool_t allowed)
{
  DBusMessage *reply;
  DBusMessageIter args;
  dbus_uint32_t serial = dbus_message_get_serial (message);

  reply = dbus_message_new_method_return (message);
  if (!reply)
    {
      error ("[dbus] no memory to reply to CanSetTime");
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  if (!dbus_message_set_reply_serial (reply, serial))
    {
      error ("[dbus] no memory to set serial for reply to CanSetTime");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  dbus_message_iter_init_append (reply, &args);
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_BOOLEAN, &allowed))
    {
      error ("[dbus] no memory to add reply args to CanSetTime");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  if (!dbus_connection_send (connection, reply, &serial))
   {
      error ("[dbus] unable to send CanSetTime reply");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
   }
  dbus_connection_flush (connection);
  dbus_message_unref (reply);
  return DBUS_HANDLER_RESULT_HANDLED;
}

/* Returns 0 if time cannot be set, and 1 otherwise. */
static
int
can_set_time (struct state *state)
{
  time_t delta = state->clock_delta;
  /* Force a synchronization check. */
  if (check_continuity (&delta) > 0)
    {
      info ("[event:%s] clock delta desync detected (%ld != %ld)",
            __func__, state->clock_delta, delta);
      delta = state->clock_delta = 0;
      invalidate_time (state);
    }
  /* Only use the time if we're not synchronized. */
  return !state->clock_delta;
}

static
DBusHandlerResult
handle_set_time (DBusConnection *connection,
                 DBusMessage *message,
                 struct state *state)
{
  DBusMessageIter iter;
  DBusError error;
  dbus_int64_t requested_time = 0;
  verb_debug ("[event:%s]: fired", __func__);
  dbus_error_init (&error);

  /* Expects DBUS_TYPE_INT64:<time_t> */
  if (!dbus_message_iter_init (message, &iter))
    return send_time_reply (connection, message, SET_TIME_BAD_CALL);
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_INT64)
    return send_time_reply (connection, message, SET_TIME_BAD_CALL);
  dbus_message_iter_get_basic (&iter, &requested_time);
  if (!is_sane_time ((time_t) requested_time))
    {
      error ("event:%s] invalid time from user: %ld", __func__,
             (time_t) requested_time);
      return send_time_reply (connection, message, SET_TIME_INVALID);
    }
  if (!can_set_time (state))
    {
      info ("[event:%s]: time is already synchronized.", __func__);
      return send_time_reply (connection, message, SET_TIME_NOT_ALLOWED);
    }

  state->last_time = requested_time;
  state->last_sync_type = SYNC_TYPE_PLATFORM;
  trigger_event (state, E_SAVE, -1);
  /* Kick off a network sync for good measure. */
  action_kickoff_time_sync (-1, EV_TIMEOUT, state);

  return send_time_reply (connection, message, SET_TIME_OK);
}

static
DBusHandlerResult
handle_can_set_time (DBusConnection *connection,
                     DBusMessage *message,
                     struct state *state)
{
  verb_debug ("[event:%s]: fired", __func__);
  return send_can_reply (connection, message, can_set_time (state));
}

static
DBusHandlerResult
handle_last_sync_info (DBusConnection *connection,
                       DBusMessage *message,
                       struct state *state)
{
  DBusMessage *reply;
  DBusMessageIter args;
  dbus_uint32_t serial = dbus_message_get_serial (message);
  dbus_bool_t net_synced = !!state->clock_delta;
  const char *sync = sync_type_str (state->last_sync_type);
  int64_t t = state->last_time;

  verb_debug ("[dbus]: handler fired");
  reply = dbus_message_new_method_return (message);
  if (!reply)
    {
      error ("[dbus] no memory to reply to LastSyncInfo");
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  if (!dbus_message_set_reply_serial (reply, serial))
    {
     error ("[dbus] no memory to set serial for reply to LastSyncInfo");
     dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  dbus_message_iter_init_append (reply, &args);
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_BOOLEAN, &net_synced))
    {
      error ("[dbus] no memory to add reply args to LastSyncInfo");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &sync))
    {
      error ("[dbus] no memory to add reply args to LastSyncInfo");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_INT64, &t))
    {
      error ("[dbus] no memory to add reply args to LastSyncInfo");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

  if (!dbus_connection_send (connection, reply, &serial))
   {
      error ("[dbus] unable to send LastSyncInfo reply");
      dbus_message_unref (reply);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
   }
  dbus_connection_flush (connection);
  dbus_message_unref (reply);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
void
unregister_service (DBusConnection *conn, void *data)
{
  info ("dbus service has been unregistered");
}

static
DBusHandlerResult
service_dispatch (DBusConnection *conn, DBusMessage *msg, void *data)
{
  struct state *state = data;
  const char *interface;
  const char *method;

  verb_debug ("[dbus] service dispatcher called");
  if (dbus_message_get_type (msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  interface = dbus_message_get_interface (msg);
  method = dbus_message_get_member (msg);
  if (!interface || !method)
    {
      verb_debug ("[dbus] service request fired with bogus data");
      /* Consume it */
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  if (strcmp (interface, kServiceInterface))
    {
      verb_debug ("[dbus] invalid interface supplied");
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  if (!strcmp (method, kServiceSetTime))
    return handle_set_time (conn, msg, state);
  else if (!strcmp (method, kServiceCanSetTime))
    return handle_can_set_time (conn, msg, state);
  else if (!strcmp (method, kServiceLastSyncInfo))
    return handle_last_sync_info (conn, msg, state);
  verb_debug ("[dbus] invalid method supplied");
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable service_vtable = {
    .unregister_function = unregister_service,
    .message_function = service_dispatch,
};

int
init_dbus (struct state *tlsdate_state)
{
  DBusError error;
  dbus_error_init (&error);
  struct dbus_state *state = calloc (1, sizeof (struct dbus_state));
  if (!state)
    return 1;
  tlsdate_state->dbus = state;
  state->conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (state->conn == NULL || dbus_error_is_set (&error))
    {
      error ("[dbus] error when connecting to the bus: %s",
             error.message);
      goto err;
    }
  if (!dbus_connection_set_timeout_functions (state->conn, add_timeout,
      remove_timeout, toggle_timeout, tlsdate_state, dbus_free))
    {
      error ("[dbus] dbus_connection_set_timeout_functions failed");
      /* TODO(wad) disconnect from DBus */
      goto err;
    }
  if (!dbus_connection_set_watch_functions (state->conn, add_watch,
      remove_watch, toggle_watch, tlsdate_state, dbus_free))
    {
      error ("[dbus] dbus_connection_set_watch_functions failed");
      goto err;
    }
  if (!dbus_bus_request_name (state->conn, kServiceInterface, 0, &error) ||
      dbus_error_is_set (&error))
    {
      error ("[dbus] failed to get name: %s", error.message);
      goto err;
    }

  /* Setup the vtable for dispatching incoming messages. */
  if (dbus_connection_register_object_path (
          state->conn, kServicePath, &service_vtable, tlsdate_state) == FALSE)
    {
      error ("[dbus] failed to register object path: %s", kServicePath);
      goto err;
    }

  verb_debug ("[dbus] initialized");
  return 0;
err:
  tlsdate_state->dbus = NULL;
  free (state);
  return 1;
}
