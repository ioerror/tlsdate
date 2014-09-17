/*
 * platform-cros.c - CrOS platform DBus integration
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "config.h"

#include <ctype.h>
#include <dbus/dbus.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <event2/event.h>

#include "src/dbus.h"
#include "src/platform.h"
#include "src/tlsdate.h"
#include "src/util.h"

static const char kMatchFormatData[] = "interface='%s',member='%s',arg0='%s'";
static const char *kMatchFormat = kMatchFormatData;
static const char kMatchNoArgFormatData[] = "interface='%s',member='%s'";
static const char *kMatchNoArgFormat = kMatchNoArgFormatData;

static const char kLibCrosDestData[] = "org.chromium.LibCrosService";
static const char *kLibCrosDest = kLibCrosDestData;
static const char kLibCrosInterfaceData[] = "org.chromium.LibCrosServiceInterface";
static const char *kLibCrosInterface = kLibCrosInterfaceData;
static const char kLibCrosPathData[] = "/org/chromium/LibCrosService";
static const char *kLibCrosPath = kLibCrosPathData;
static const char kResolveNetworkProxyData[] = "ResolveNetworkProxy";
static const char *kResolveNetworkProxy = kResolveNetworkProxyData;

static const char kDBusInterfaceData[] = "org.freedesktop.DBus";
static const char *kDBusInterface = kDBusInterfaceData;
static const char kNameOwnerChangedData[] = "NameOwnerChanged";
static const char *kNameOwnerChanged = kNameOwnerChangedData;
static const char kNameAcquiredData[] = "NameAcquired";
static const char *kNameAcquired = kNameAcquiredData;

static const char kManagerInterfaceData[] = "org.chromium.flimflam.Manager";
static const char *kManagerInterface = kManagerInterfaceData;

static const char kServiceInterfaceData[] = "org.chromium.flimflam.Service";
static const char *kServiceInterface = kServiceInterfaceData;
static const char kMemberData[] = "PropertyChanged";
static const char *kMember = kMemberData;

static const char kProxyConfigData[] = "ProxyConfig";
static const char *kProxyConfig = kProxyConfigData;
static const char kDefaultServiceData[] = "DefaultService";
static const char *kDefaultService = kDefaultServiceData;

static const char kResolveInterfaceData[] = "org.torproject.tlsdate.Resolver";
static const char *kResolveInterface = kResolveInterfaceData;
static const char kResolveMemberData[] = "ProxyChange";
static const char *kResolveMember = kResolveMemberData;

/* TODO(wad) Integrate with cros_system_api/dbus/service_constants.h */
static const char kPowerManagerInterfaceData[] = "org.chromium.PowerManager";
static const char *kPowerManagerInterface = kPowerManagerInterfaceData;
static const char kSuspendDoneData[] = "SuspendDone";
static const char *kSuspendDone = kSuspendDoneData;

static const char kErrorServiceUnknownData[] = "org.freedesktop.DBus.Error.ServiceUnknown";
static const char *kErrorServiceUnknown = kErrorServiceUnknownData;

struct platform_state
{
  struct event_base *base;
  struct state *state;
  DBusMessage **resolve_msg;
  int resolve_msg_count;
  uint32_t resolve_network_proxy_serial;
};

static
bool
get_valid_hostport (const char *hostport, char *out, size_t len)
{
  bool host = true;
  const char *end = hostport + strlen (hostport);
  const char *c;
  *out = '\0';
  /* Hosts begin with alphanumeric only. */
  if (!isalnum (*hostport))
    {
      info ("Host does not start with alnum");
      return false;
    }
  *out++ = *hostport;
  for (c = hostport + 1;  c < end && len > 0; ++c, ++out, --len)
    {
      *out = *c;
      if (host)
        {
          if (isalnum (*c) || *c == '-' || *c == '.')
            {
              continue;
            }
          if (*c == ':')
            {
              host = false;
              continue;
            }
        }
      else
        {
          if (isdigit (*c))
            continue;
        }
      *out = '\0';
      return false;
    }
  *out = '\0';
  return true;
}

/* Convert PAC return format to tlsdated url format */
/* TODO(wad) support multiple proxies when Chromium does:
 * PROXY x.x.x.x:yyyy; PROXY z.z.z.z:aaaaa
 */
static
void
canonicalize_pac (const char *pac_fmt, char *proxy_url, size_t len)
{
  size_t type_len;
  size_t copied = 0;
  const char *space;
  /* host[255]:port[6]\0 */
  char hostport[6 + 255 + 2];
  proxy_url[0] = '\0';
  if (len < 1)
    return;
  if (!strcmp (pac_fmt, "DIRECT"))
    {
      return;
    }
  /* Find type */
  space = strchr (pac_fmt, ' ');
  if (!space)
    return;
  type_len = space - pac_fmt;
  if (!get_valid_hostport (space + 1, hostport, sizeof (hostport)))
    {
      error ("invalid host:port: %s", space + 1);
      return;
    }
  proxy_url[0] = '\0';
  if (!strncmp (pac_fmt, "PROXY", type_len))
    {
      copied = snprintf (proxy_url, len, "http://%s", hostport);
    }
  else if (!strncmp (pac_fmt, "SOCKS", type_len))
    {
      copied = snprintf (proxy_url, len, "socks4://%s", hostport);
    }
  else if (!strncmp (pac_fmt, "SOCKS5", type_len))
    {
      copied = snprintf (proxy_url, len, "socks5://%s", hostport);
    }
  else if (!strncmp (pac_fmt, "HTTPS", type_len))
    {
      copied = snprintf (proxy_url, len, "https://%s", hostport);
    }
  else
    {
      error ("pac_fmt unmatched: '%s' %zu", pac_fmt, type_len);
    }
  if (copied >= len)
    {
      error ("canonicalize_pac: truncation '%s'", proxy_url);
      proxy_url[0] = '\0';
      return;
    }
}

static
DBusHandlerResult
handle_service_change (DBusConnection *connection,
                       DBusMessage *message,
                       struct platform_state *ctx)
{
  DBusMessageIter iter, subiter;
  DBusError error;
  const char *pname;
  const char *pval;
  const char *service;
  dbus_error_init (&error);
  verb_debug ("[event:cros:%s]: fired", __func__);
  /* TODO(wad) Track the current DefaultService only fire when it changes */
  service = dbus_message_get_path (message);
  if (!service)
    return DBUS_HANDLER_RESULT_HANDLED;
  /* Shill emits string:ProxyConfig variant string:"..." */
  if (!dbus_message_iter_init (message, &iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&iter, &pname);
  /* Make sure we are only firing on a ProxyConfig property change. */
  if (strcmp (pname, kProxyConfig))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (!dbus_message_iter_next (&iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_VARIANT)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_recurse (&iter, &subiter);
  if (dbus_message_iter_get_arg_type (&subiter) != DBUS_TYPE_STRING)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&subiter, &pval);
  /* Right now, nothing is done with the Shill proxy value because
   * Chromium handles .pac resolution.  This may be more useful for
   * ignoring incomplete proxy values sent while a user is typing.
   */
  action_kickoff_time_sync (-1, EV_TIMEOUT, ctx->state);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
DBusHandlerResult
handle_manager_change (DBusConnection *connection,
                       DBusMessage *message,
                       struct platform_state *ctx)
{
  DBusMessageIter iter, subiter;
  DBusError error;
  const char *pname;
  const char *pval;
  verb_debug ("[event:cros:%s]: fired", __func__);
  dbus_error_init (&error);
  if (!dbus_message_iter_init (message, &iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&iter, &pname);
  /* Make sure we caught the right property. */
  if (strcmp (pname, kDefaultService))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (!dbus_message_iter_next (&iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_VARIANT)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_recurse (&iter, &subiter);
  if (dbus_message_iter_get_arg_type (&subiter) != DBUS_TYPE_OBJECT_PATH)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&subiter, &pval);
  /* TODO(wad) Filter on the currently active service in pval. */
  verb_debug ("[event:cros:%s] service change on path %s",
         __func__, pval);
  action_kickoff_time_sync (-1, EV_TIMEOUT, ctx->state);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
DBusHandlerResult
handle_suspend_done (DBusConnection *connection,
                     DBusMessage *message,
                     struct platform_state *ctx)
{
  DBusMessageIter iter;
  DBusError error;
  const char *pname;
  verb_debug ("[event:cros:%s]: fired", __func__);
  /* Coming back from resume, trigger a continuity and time
   * check just in case none of the other events happen.
   */
  action_kickoff_time_sync (-1, EV_TIMEOUT, ctx->state);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
DBusHandlerResult
handle_proxy_change (DBusConnection *connection,
                     DBusMessage *message,
                     struct platform_state *ctx)
{
  DBusMessageIter iter;
  DBusError error;
  const char *pname;
  const char *pval;
  char time_host[MAX_PROXY_URL];
  int url_len = 0;
  struct source *src = ctx->state->opts.sources;
  verb_debug ("[event:cros:%s]: fired", __func__);
  if (ctx->state->opts.cur_source && ctx->state->opts.cur_source->next)
    src = ctx->state->opts.cur_source->next;
  if (!ctx->state->resolving)
    {
      info ("[event:cros:%s] Unexpected ResolveNetworkProxy signal seen",
            __func__);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  dbus_error_init (&error);
  /* Shill emits string:ProxyConfig variant string:"..." */
  if (!dbus_message_iter_init (message, &iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&iter, &pname);
  /* Make sure this was the resolution we asked for */
  url_len = snprintf (time_host, sizeof (time_host), "https://%s:%s",
                      src->host, src->port);
  if (url_len >= sizeof (time_host))
    {
      error ("[event:cros:%s]: current source url is too long",
             __func__);
    }
  if (strcmp (pname, time_host))
    {
      error ("[event:cros:%s]: resolved host mismatch: %s v %s",
             __func__, pname, time_host);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  if (!dbus_message_iter_next (&iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&iter, &pval);
  ctx->state->resolving = 0;
  canonicalize_pac (pval, ctx->state->dynamic_proxy, sizeof (ctx->state->dynamic_proxy));
  trigger_event (ctx->state, E_TLSDATE, 1);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
DBusHandlerResult
handle_dbus_change (DBusConnection *connection,
                    DBusMessage *message,
                    struct platform_state *ctx)
{
  DBusMessageIter iter;
  DBusError error;
  const char *pname;
  verb_debug ("[event:cros:%s]: fired", __func__);
  dbus_error_init (&error);
  if (!dbus_message_iter_init (message, &iter))
    return DBUS_HANDLER_RESULT_HANDLED;
  if (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_STRING)
    return DBUS_HANDLER_RESULT_HANDLED;
  dbus_message_iter_get_basic (&iter, &pname);
  /* Make sure we caught the right property. */
  if (strcmp (pname, kLibCrosDest))
    return DBUS_HANDLER_RESULT_HANDLED;
  action_kickoff_time_sync (-1, EV_TIMEOUT, ctx->state);
  return DBUS_HANDLER_RESULT_HANDLED;
}

static
void
action_resolve_proxy (evutil_socket_t fd, short what, void *arg)
{
  struct platform_state *ctx = arg;
  struct dbus_state *dbus_state = ctx->state->dbus;
  DBusConnection *conn = dbus_state->conn;
  struct source *src = ctx->state->opts.sources;
  verb_debug ("[event:%s] fired", __func__);
  /* Emulate tlsdate-monitor.c:build_argv and choose the next source */
  if (ctx->state->opts.cur_source && ctx->state->opts.cur_source->next)
    src = ctx->state->opts.cur_source->next;
  if (ctx->state->resolving || ctx->resolve_network_proxy_serial)
    {
      /* Note, this is not the same as the response signal. It just avoids
       * multiple requests in a single dispatch window.
       */
      info ("[event:%s] no resolve_proxy sent; pending method_reply",
            __func__);
      return;
    }
  ctx->state->dynamic_proxy[0] = '\0';
  if (ctx->resolve_msg[src->id] == NULL)
    {
      info ("[event:%s] no dynamic proxy for %s:%s", __func__,
            src->host, src->port);
      trigger_event (ctx->state, E_TLSDATE, 1);
      return;
    }
  info ("[event:%s] resolving proxy for %s:%s", __func__,
        src->host, src->port);
  ctx->state->resolving = 1;
  if (!dbus_connection_send (conn,
                             ctx->resolve_msg[src->id],
                             &ctx->resolve_network_proxy_serial))
    {
      error ("[event:%s] cannot send ResolveNetworkProxy query!", __func__);
      return;
    }
}

static
DBusHandlerResult
dbus_filter (DBusConnection *connection, DBusMessage *message, void *data)
{
  struct platform_state *state = data;
  /* Terminate gracefully if DBus goes away. */
  if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected"))
    {
      error ("[cros] DBus system bus has become inaccessible. Terminating.");
      /* Trigger a graceful teardown. */
      kill (getpid(), SIGINT);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  /* Hand it over to the service dispatcher. */
  if (dbus_message_get_type (message) == DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  /* Handle explicitly defined signals only. */
  if (dbus_message_is_signal (message, kDBusInterface, kNameAcquired))
    {
      info ("[cros] DBus name acquired successfully");
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  if (dbus_message_is_signal (message, kServiceInterface, kMember))
    return handle_service_change (connection, message, state);
  if (dbus_message_is_signal (message, kManagerInterface, kMember))
    return handle_manager_change (connection, message, state);
  if (dbus_message_is_signal (message, kResolveInterface, kResolveMember))
    return handle_proxy_change (connection, message, state);
  if (dbus_message_is_signal (message, kDBusInterface, kNameOwnerChanged))
    return handle_dbus_change (connection, message, state);
  if (dbus_message_is_signal (message, kPowerManagerInterface, kSuspendDone))
    return handle_suspend_done (connection, message, state);
  if (dbus_message_is_error (message, kErrorServiceUnknown))
    {
      info ("[cros] org.chromium.LibCrosService.ResolveNetworkProxy is missing");
      info ("[cros] skipping proxy resolution for now");
      /* Fire off tlsdate rather than letting it fail silently. */
      state->resolve_network_proxy_serial = 0;
      state->state->resolving = 0;
      trigger_event (state->state, E_TLSDATE, 1);
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  /* Indicates a successful resolve request was issued. */
  if (dbus_message_get_type (message) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
      uint32_t serial = dbus_message_get_reply_serial (message);
      if (serial == state->resolve_network_proxy_serial)
        {
          state->resolve_network_proxy_serial = 0;
          return DBUS_HANDLER_RESULT_HANDLED;
        }
      info ("[cros] unknown DBus METHOD_RETURN seen: %u", serial);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
  verb_debug ("[cros] unknown message received: "
         "type=%s dest=%s interface=%s member=%s path=%s sig=%s error_name=%s",
         dbus_message_type_to_string (dbus_message_get_type (message)),
         dbus_message_get_destination (message),
         dbus_message_get_interface (message),
         dbus_message_get_member (message),
         dbus_message_get_path (message),
         dbus_message_get_signature (message),
         dbus_message_get_error_name (message));
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int
add_match (DBusConnection *conn, const char *interface, const char *member,
           const char *arg0)
{
  char match[1024];
  DBusError error;
  int len;
  dbus_error_init (&error);
  if (arg0)
    {
      len = snprintf (match, sizeof (match), kMatchFormat,
                      interface, member, arg0);
    }
  else
    {
      len = snprintf (match, sizeof (match), kMatchNoArgFormat,
                      interface, member);
    }
  if (len >= sizeof (match) || len < 0)
    {
      error ("[dbus] match truncated for '%s,%s'", interface, member);
      return 1;
    }
  dbus_bus_add_match (conn, match, &error);
  if (dbus_error_is_set (&error))
    {
      error ("[dbus] failed to add_match for '%s,%s'; error: %s, %s",
             interface, member, error.name, error.message);
      dbus_error_free (&error);
      return 1;
    }
  return 0;
}

DBusMessage *
new_resolver_message(const struct source *src)
{
  char time_host[MAX_PROXY_URL];
  void *time_host_ptr = &time_host;
  int url_len;
  DBusMessage *res;
  DBusMessageIter args;
  if (!src->proxy || strcmp (src->proxy, "dynamic"))
    {
      return NULL;
    }
  res = dbus_message_new_method_call (kLibCrosDest, kLibCrosPath,
                                      kLibCrosInterface, kResolveNetworkProxy);
  if (!res)
    {
      error ("[cros] could not setup dynamic proxy for source %d", src->id);
      return NULL;
    }
  /* Build the time_host */
  url_len = snprintf (time_host, sizeof (time_host), "https://%s:%s",
                      src->host, src->port);
  if (url_len >= sizeof (time_host))
    {
      fatal ("[cros] source %d url is too long! (%d)", src->id, url_len);
    }
  /* Finish the message */
  dbus_message_iter_init_append (res, &args);
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &time_host_ptr) ||
      !dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &kResolveInterface) ||
      !dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &kResolveMember))
    {
      fatal ("[cros could not append arguments for resolver message");
    }
  return res;
}

int
platform_init_cros (struct state *state)
{
  /* Watch for per-service ProxyConfig property changes */
  struct event_base *base = state->base;
  struct dbus_state *dbus_state = state->dbus;
  struct source *src = NULL;
  int sources = 0;
  DBusConnection *conn = dbus_state->conn;
  DBusError error;
  struct platform_state *platform_state =
      calloc (1, sizeof (struct platform_state));
  if (!platform_state)
    {
      error ("[cros] could not allocate platform_state");
      return -1;
    }
  /* TODO(wad) Follow up with dbus_error_free() where needed. */
  dbus_error_init (&error);
  /* Add watches for: proxy changes, default service changes, proxy resolution,
   * LibCrosService ownership, and power state changes.
   */
  if (add_match (conn, kServiceInterface, kMember, kProxyConfig) ||
      add_match (conn, kManagerInterface, kMember, kDefaultService) ||
      add_match (conn, kResolveInterface, kResolveMember, NULL) ||
      add_match (conn, kDBusInterface, kNameOwnerChanged, kLibCrosDest) ||
      add_match (conn, kPowerManagerInterface, kSuspendDone, NULL))
    return 1;

  /* Allocate one per source */
  for (src = state->opts.sources; src; src = src->next, ++sources);
  platform_state->resolve_msg_count = sources;
  platform_state->resolve_msg = calloc (sources, sizeof (DBusMessage *));
  if (!platform_state->resolve_msg)
    {
      error ("[cros] cannot allocate resolver messages");
      free (platform_state);
      return -1;
    }
  for (src = state->opts.sources; src; src = src->next)
    {
      if (src->id >= sources)
        fatal ("Source ID is greater than available sources!");
      platform_state->resolve_msg[src->id] = new_resolver_message (src);
      if (platform_state->resolve_msg[src->id])
        src->proxy = state->dynamic_proxy;
    }
  state->dynamic_proxy[0] = '\0';
  if (state->opts.proxy && !strcmp (state->opts.proxy, "dynamic"))
    {
      info ("[cros] default dynamic proxy support");
      state->opts.proxy = state->dynamic_proxy;
    }
  platform_state->base = base;
  platform_state->state = state;
  /* Add the dynamic resolver if tlsdate doesn't already have one. */
  if (!state->events[E_RESOLVER])
    {
      state->events[E_RESOLVER] = event_new (base, -1, EV_TIMEOUT,
                                             action_resolve_proxy,
                                             platform_state);
      if (!state->events[E_RESOLVER])
        /* Let's not clean up DBus. */
        fatal ("Could not allocated resolver event");
      /* Wake up as a NET event since it'll self-block until DBus has a chance
       * to send it.
       */
      event_priority_set (state->events[E_RESOLVER], PRI_NET);
    }
  /* Each platform can attach their own filter, but the filter func needs to be
   * willing to DBUS_HANDLER_RESULT_NOT_YET_HANDLED on unexpected events.
   */
  /* TODO(wad) add the clean up function as the callback. */
  if (!dbus_connection_add_filter (conn,
                                   dbus_filter, platform_state, NULL))
    {
      error ("Failed to register signal handler callback");
      return 1;
    }
  return 0;
}
