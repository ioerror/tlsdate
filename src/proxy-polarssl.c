/*
 * proxy-polarssl.c - Net stack layer for SOCKS4a/5 proxy connections
 *
 * Based on proxy-bio.c - Original copyright (c) 2012 The Chromium OS Authors.
 *
 * This file was adapted by Paul Bakker <p.j.bakker@offspark.com>
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file implements a SOCKS4a/SOCKS5 net layer as used by PolarSSL.
 */

#include "config.h"

#include <arpa/inet.h>
#include <assert.h>
#ifndef __USE_MISC
#define __USE_MISC
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>

#ifndef HAVE_STRNLEN
#include "src/common/strnlen.h"
#endif

#include "src/proxy-polarssl.h"
#include "src/util.h"

int socks4a_connect(proxy_polarssl_ctx *ctx)
{
  int r;
  unsigned char buf[NI_MAXHOST + 16];
  uint16_t port_n;
  size_t sz = 0;

  if (!ctx)
    return 0;

  verb("V: proxy4: connecting %s:%d", ctx->host, ctx->port);

  port_n = htons(ctx->port);

  /*
   * Packet layout:
   * 1b: Version (must be 0x04)
   * 1b: command (0x01 is connect)
   * 2b: port number, big-endian
   * 4b: 0x00, 0x00, 0x00, 0x01 (bogus IPv4 addr)
   * 1b: 0x00 (empty 'userid' field)
   * nb: hostname, null-terminated
   */
  buf[0] = 0x04;
  buf[1] = 0x01;
  sz += 2;

  memcpy(buf + 2, &port_n, sizeof(port_n));
  sz += sizeof(port_n);

  buf[4] = 0x00;
  buf[5] = 0x00;
  buf[6] = 0x00;
  buf[7] = 0x01;
  sz += 4;

  buf[8] = 0x00;
  sz += 1;

  memcpy(buf + sz, ctx->host, strlen(ctx->host) + 1);
  sz += strlen(ctx->host) + 1;

  r = ctx->f_send(ctx->p_send, buf, sz);
  if (r != sz)
    return 0;

  /* server reply: 1 + 1 + 2 + 4 */
  r = ctx->f_recv(ctx->p_recv, buf, 8);
  if (r != 8)
    return 0;

  if (buf[1] == 0x5a) {
    verb("V: proxy4: connected");
    ctx->connected = 1;
    return 1;
  }
  return 0;
}

int socks5_connect(proxy_polarssl_ctx *ctx)
{
  unsigned char buf[NI_MAXHOST + 16];
  int r;
  uint16_t port_n;
  size_t sz = 0;

  if (!ctx)
    return 0;

  /* the length for SOCKS addresses is only one byte. */
  if (strnlen(ctx->host, UINT8_MAX + 1) == UINT8_MAX + 1)
    return 0;

  verb("V: proxy5: connecting %s:%d", ctx->host, ctx->port);

  port_n = htons(ctx->port);

  /*
   * Hello packet layout:
   * 1b: Version
   * 1b: auth methods
   * nb: method types
   *
   * We support only one method (no auth, 0x00). Others listed in RFC
   * 1928.
   */
  buf[0] = 0x05;
  buf[1] = 0x01;
  buf[2] = 0x00;

  r = ctx->f_send(ctx->p_send, buf, 3);
  if (r != 3)
    return 0;

  r = ctx->f_recv(ctx->p_recv, buf, 2);
  if (r != 2)
    return 0;

  if (buf[0] != 0x05 || buf[1] != 0x00) {
    verb("V: proxy5: auth error %02x %02x", buf[0], buf[1]);
    return 0;
  }

  /*
   * Connect packet layout:
   * 1b: version
   * 1b: command (0x01 is connect)
   * 1b: reserved, 0x00
   * 1b: addr type (0x03 is domain name)
   * nb: addr len (1b) + addr bytes, no null termination
   * 2b: port, network byte order
   */
  buf[0] = 0x05;
  buf[1] = 0x01;
  buf[2] = 0x00;
  buf[3] = 0x03;
  buf[4] = strlen(ctx->host);
  sz += 5;
  memcpy(buf + 5, ctx->host, strlen(ctx->host));
  sz += strlen(ctx->host);
  memcpy(buf + sz, &port_n, sizeof(port_n));
  sz += sizeof(port_n);

  r = ctx->f_send(ctx->p_send, buf, sz);
  if (r != sz)
    return 0;

  /*
   * Server's response:
   * 1b: version
   * 1b: status (0x00 is okay)
   * 1b: reserved, 0x00
   * 1b: addr type (0x03 is domain name, 0x01 ipv4)
   * nb: addr len (1b) + addr bytes, no null termination
   * 2b: port, network byte order
   */

  /* grab up through the addr type */
  r = ctx->f_recv(ctx->p_recv, buf, 4);
  if (r != 4)
    return 0;

  if (buf[0] != 0x05 || buf[1] != 0x00) {
    verb("V: proxy5: connect error %02x %02x", buf[0], buf[1]);
    return 0;
  }

  if (buf[3] == 0x03) {
    unsigned int len;
    r = ctx->f_recv(ctx->p_recv, buf + 4, 1);
    if (r != 1)
      return 0;
    /* host (buf[4] bytes) + port (2 bytes) */
    len = buf[4] + 2;
    while (len) {
      r = ctx->f_recv(ctx->p_recv, buf + 5, min(len, sizeof(buf)));
      if (r <= 0)
        return 0;
      len -= min(len, r);
    }
  } else if (buf[3] == 0x01) {
    /* 4 bytes ipv4 addr, 2 bytes port */
    r = ctx->f_recv(ctx->p_recv, buf + 4, 6);
    if (r != 6)
      return 0;
  }

  verb("V: proxy5: connected");
  ctx->connected = 1;
  return 1;
}

/* SSL socket BIOs don't support BIO_gets, so... */
int sock_gets(proxy_polarssl_ctx *ctx, char *buf, size_t sz)
{
  unsigned char c;
  while (ctx->f_recv(ctx->p_recv, &c, 1) > 0 && sz > 1) {
    *buf++ = c;
    sz--;
    if (c == '\n') {
      *buf = '\0';
      return 0;
    }
  }
  return 1;
}

int http_connect(proxy_polarssl_ctx *ctx)
{
  int r;
  char buf[4096];
  int retcode;

  snprintf(buf, sizeof(buf), "CONNECT %s:%d HTTP/1.1\r\n",
           ctx->host, ctx->port);
  r = ctx->f_send(ctx->p_send, (unsigned char *) buf, strlen(buf));
  if (r != strlen(buf))
    return 0;
  /* required by RFC 2616 14.23 */
  snprintf(buf, sizeof(buf), "Host: %s:%d\r\n", ctx->host, ctx->port);
  r = ctx->f_send(ctx->p_send, (unsigned char *) buf, strlen(buf));
  if (r != strlen(buf))
    return 0;
  strcpy(buf, "\r\n");
  r = ctx->f_send(ctx->p_send, (unsigned char *) buf, strlen(buf));
  if (r != strlen(buf))
    return 0;

  r = sock_gets(ctx, buf, sizeof(buf));
  if (r)
    return 0;
  /* use %*s to ignore the version */
  if (sscanf(buf, "HTTP/%*s %d", &retcode) != 1)
    return 0;

  if (retcode < 200 || retcode > 299)
    return 0;
  while (!(r = sock_gets(ctx, buf, sizeof(buf)))) {
    if (!strcmp(buf, "\r\n")) {
      /* Done with the header */
      ctx->connected = 1;
      return 1;
    }
  }
  return 0;
}

int API proxy_polarssl_init(proxy_polarssl_ctx *ctx)
{
  if (!ctx)
    return 0;

  memset(ctx, 0, sizeof(proxy_polarssl_ctx));
  return 1;
}

void API proxy_polarssl_set_bio(proxy_polarssl_ctx *ctx,
                 int (*f_recv)(void *, unsigned char *, size_t), void *p_recv,
                 int (*f_send)(void *, const unsigned char *, size_t), void *p_send)
{
  if (!ctx)
    return;

  ctx->f_recv = f_recv;
  ctx->p_recv = p_recv;
  ctx->f_send = f_send;
  ctx->p_send = p_send;
}

int API proxy_polarssl_free(proxy_polarssl_ctx *ctx)
{
  if (!ctx)
    return 0;

  if (ctx->host)
  {
    free(ctx->host);
    ctx->host = NULL;
  }

  return 1;
}

int API proxy_polarssl_set_scheme(proxy_polarssl_ctx *ctx, const char *scheme)
{
  if (!strcmp(scheme, "socks5"))
    ctx->f_connect = socks5_connect;
  else if (!strcmp(scheme, "socks4"))
    ctx->f_connect = socks4a_connect;
  else if (!strcmp(scheme, "http"))
    ctx->f_connect = http_connect;
  else
    return 1;
  return 0;
}

int API proxy_polarssl_set_host(proxy_polarssl_ctx *ctx, const char *host)
{
  if (strnlen(host, NI_MAXHOST) == NI_MAXHOST)
    return 1;
  ctx->host = strdup(host);
  return 0;
}

void API proxy_polarssl_set_port(proxy_polarssl_ctx *ctx, uint16_t port)
{
  ctx->port = port;
}

int API proxy_polarssl_recv(void *ctx, unsigned char *data, size_t len)
{
  proxy_polarssl_ctx *proxy = (proxy_polarssl_ctx *) ctx;
  int r;

  if (!ctx)
    return -1;

  if (!proxy->connected)
  {
    r = proxy->f_connect(ctx);
    if (r)
      return (r);
  }

  return proxy->f_recv(proxy->p_recv, data, len);
}


int API proxy_polarssl_send(void *ctx, const unsigned char *data, size_t len)
{
  proxy_polarssl_ctx *proxy = (proxy_polarssl_ctx *) ctx;
  int r;

  if (!ctx)
    return -1;

  if (!proxy->connected)
  {
    r = proxy->f_connect(ctx);
    if (r)
      return (r);
  }

  return proxy->f_send(proxy->p_send, data, len);
}
