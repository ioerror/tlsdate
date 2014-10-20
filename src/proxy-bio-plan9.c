/*
 * proxy-bio.c - BIO layer for SOCKS4a/5 proxy connections
 *
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file implements a SOCKS4a/SOCKS5 "filter" BIO. In SSL terminology, a BIO
 * is a stackable IO filter, kind of like sysv streams. These filters are
 * inserted into a stream to cause it to run SOCKS over whatever transport is
 * being used. Most commonly, this would be:
 *   SSL BIO (filter) -> SOCKS BIO (filter) -> connect BIO (source/sink)
 * This configuration represents doing an SSL connection through a SOCKS proxy,
 * which is itself connected to in plaintext. You might also do:
 *   SSL BIO -> SOCKS BIO -> SSL BIO -> connect BIO
 * This is an SSL connection through a SOCKS proxy which is itself reached over
 * SSL.
 */

#include <arpa/inet.h>
#include <assert.h>
#ifndef __USE_MISC
#define __USE_MISC
#endif
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef UINT8_MAX
#define UINT8_MAX (255)
#endif

#include <netdb.h>

#include <inttypes.h>

#include "src/proxy-bio-plan9.h"

int socks4a_connect(BIO *b);
int socks5_connect(BIO *b);
int http_connect(BIO *b);

int proxy_new(BIO *b)
{
  struct proxy_ctx *ctx = (struct proxy_ctx *) malloc(sizeof *ctx);
  if (!ctx)
    return 0;
  ctx->connected = 0;
  ctx->connect = NULL;
  ctx->host = NULL;
  ctx->port = 0;
  b->init = 1;
  b->flags = 0;
  b->ptr = ctx;
  return 1;
}

int proxy_free(BIO *b)
{
  struct proxy_ctx *c;
  if (!b || !b->ptr)
    return 1;
  c = (struct proxy_ctx *) b->ptr;
  if (c->host)
    free(c->host);
  c->host = NULL;
  b->ptr = NULL;
  free(c);
  return 1;
}

int socks4a_connect(BIO *b)
{
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;
  int r;
  unsigned char buf[NI_MAXHOST + 16];
  uint16_t port_n = htons(ctx->port);
  size_t sz = 0;

  verb("V: proxy4: connecting %s:%d", ctx->host, ctx->port);

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

  r = BIO_write(b->next_bio, buf, sz);
  if ( -1 == r )
    return -1;
  if ( (size_t) r != sz)
    return 0;

  /* server reply: 1 + 1 + 2 + 4 */
  r = BIO_read(b->next_bio, buf, 8);
  if ( -1 == r )
    return -1;
  if ( (size_t) r != 8)
    return 0;
  if (buf[1] == 0x5a) {
    verb("V: proxy4: connected");
    ctx->connected = 1;
    return 1;
  }
  return 0;
}

int socks5_connect(BIO *b)
{
  unsigned char buf[NI_MAXHOST + 16];
  int r;
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;
  uint16_t port_n = htons(ctx->port);
  size_t sz = 0;

  /* the length for SOCKS addresses is only one byte. */
  if (strlen(ctx->host) == UINT8_MAX + 1)
    return 0;

  verb("V: proxy5: connecting %s:%d", ctx->host, ctx->port);

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

  r = BIO_write(b->next_bio, buf, 3);
  if (r != 3)
    return 0;

  r = BIO_read(b->next_bio, buf, 2);
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

  r = BIO_write(b->next_bio, buf, sz);
  if ( -1 == r )
    return -1;
  if ( (size_t) r != sz)
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
  r = BIO_read(b->next_bio, buf, 4);
  if ( -1 == r )
    return -1;
  if (r != 4)
    return 0;

  if (buf[0] != 0x05 || buf[1] != 0x00) {
    verb("V: proxy5: connect error %02x %02x", buf[0], buf[1]);
    return 0;
  }

  if (buf[3] == 0x03) {
    unsigned int len;
    r = BIO_read(b->next_bio, buf + 4, 1);
    if (r != 1)
      return 0;
    /* host (buf[4] bytes) + port (2 bytes) */
    len = buf[4] + 2;
    while (len) {
      r = BIO_read(b->next_bio, buf + 5, min(len, sizeof(buf)));
      if (r <= 0)
        return 0;
      len -= min(len, r);
    }
  } else if (buf[3] == 0x01) {
    /* 4 bytes ipv4 addr, 2 bytes port */
    r = BIO_read(b->next_bio, buf + 4, 6);
    if (r != 6)
      return 0;
  }

  verb("V: proxy5: connected");
  ctx->connected = 1;
  return 1;
}

/* SSL socket BIOs don't support BIO_gets, so... */
int sock_gets(BIO *b, char *buf, size_t sz)
{
  char c;
  while (BIO_read(b, &c, 1) > 0 && sz > 1) {
    *buf++ = c;
    sz--;
    if (c == '\n') {
      *buf = '\0';
      return 0;
    }
  }
  return 1;
}

int http_connect(BIO *b)
{
  int r;
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;
  char buf[4096];
  int retcode;

  snprintf(buf, sizeof(buf), "CONNECT %s:%d HTTP/1.1\r\n",
           ctx->host, ctx->port);
  r = BIO_write(b->next_bio, buf, strlen(buf));
  if ( -1 == r )
    return -1;
  if ( (size_t) r != strlen(buf))
    return 0;
  /* required by RFC 2616 14.23 */
  snprintf(buf, sizeof(buf), "Host: %s:%d\r\n", ctx->host, ctx->port);
  r = BIO_write(b->next_bio, buf, strlen(buf));
  if ( -1 == r )
    return -1;
  if ( (size_t) r != strlen(buf))
    return 0;
  strcpy(buf, "\r\n");
  r = BIO_write(b->next_bio, buf, strlen(buf));
  if ( -1 == r )
    return -1;
  if ( (size_t) r != strlen(buf))
    return 0;

  r = sock_gets(b->next_bio, buf, sizeof(buf));
  if (r)
    return 0;
  /* use %*s to ignore the version */
  if (sscanf(buf, "HTTP/%*s %d", &retcode) != 1)
    return 0;

  if (retcode < 200 || retcode > 299)
    return 0;
  while (!(r = sock_gets(b->next_bio, buf, sizeof(buf)))) {
    if (!strcmp(buf, "\r\n")) {
      /* Done with the header */
      ctx->connected = 1;
      return 1;
    }
  }
  return 0;
}

int proxy_write(BIO *b, const char *buf, int sz)
{
  int r;
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;

  assert(buf);

  if (sz <= 0)
    return 0;

  if (!b->next_bio)
    return 0;

  if (!ctx->connected) {
    assert(ctx->connect);
    if (!ctx->connect(b))
      return 0;
  }

  r = BIO_write(b->next_bio, buf, sz);
  BIO_clear_retry_flags(b);
  BIO_copy_next_retry(b);
  return r;
}

int proxy_read(BIO *b, char *buf, int sz)
{
  int r;
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;

  assert(buf);

  if (!b->next_bio)
    return 0;

  if (!ctx->connected) {
    assert(ctx->connect);
    if (!ctx->connect(b))
      return 0;
  }

  r = BIO_read(b->next_bio, buf, sz);
  BIO_clear_retry_flags(b);
  BIO_copy_next_retry(b);
  return r;
}

long proxy_ctrl(BIO *b, int cmd, long num, void *ptr)
{
  long ret;
  struct proxy_ctx *ctx;
  if (!b->next_bio)
    return 0;
  ctx = (struct proxy_ctx *) b->ptr;
  assert(ctx);

  switch (cmd) {
  case BIO_C_DO_STATE_MACHINE:
    BIO_clear_retry_flags(b);
    ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
    BIO_copy_next_retry(b);
    break;
  case BIO_CTRL_DUP:
    ret = 0;
    break;
  default:
    ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
  }
  return ret;
}

int proxy_gets(BIO *b, char *buf, int size)
{
  return BIO_gets(b->next_bio, buf, size);
}

int proxy_puts(BIO *b, const char *str)
{
  return BIO_puts(b->next_bio, str);
}

long proxy_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp)
{
  if (!b->next_bio)
    return 0;
  return BIO_callback_ctrl(b->next_bio, cmd, fp);
}

BIO_METHOD proxy_methods = {
  BIO_TYPE_MEM,
  "proxy",
  proxy_write,
  proxy_read,
  proxy_puts,
  proxy_gets,
  proxy_ctrl,
  proxy_new,
  proxy_free,
  proxy_callback_ctrl,
};

BIO_METHOD *BIO_f_proxy()
{
  return &proxy_methods;
}

/* API starts here */

BIO API *BIO_new_proxy()
{
  return BIO_new(BIO_f_proxy());
}

int API BIO_proxy_set_type(BIO *b, const char *type)
{
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;
  if (!strcmp(type, "socks5"))
    ctx->connect = socks5_connect;
  else if (!strcmp(type, "socks4a"))
    ctx->connect = socks4a_connect;
  else if (!strcmp(type, "http"))
    ctx->connect = http_connect;
  else
    return 1;
  return 0;
}

int API BIO_proxy_set_host(BIO *b, const char *host)
{
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;
  if (strlen(host) == NI_MAXHOST)
    return 1;
  ctx->host = strdup(host);
  return 0;
}

void API BIO_proxy_set_port(BIO *b, uint16_t port)
{
  struct proxy_ctx *ctx = (struct proxy_ctx *) b->ptr;
  ctx->port = port;
}
